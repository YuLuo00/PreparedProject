using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace ArchiveToolUI
{
    // -----------------------------------------------------------------------
    // P/Invoke：对应 ArchiveTool.h 的 C 接口
    // -----------------------------------------------------------------------
    internal static class ArchiveToolNative
    {
        private const string DllName = "libArchiveTool.dll";

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int TryDetermineType(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string filePath,
            byte[] buf, int bufSize);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int FindPasswordAsync(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string filePath,
            FindPasswordCallback callback,
            IntPtr userData,
            int findAll);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int FindPasswordAsyncQueue(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string filePath,
            FindPasswordSignalCallback signalCallback,
            IntPtr userData,
            int findAll);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int GetLatestFindPasswordProgress(
            int taskId,
            ref FindPasswordProgressOut progress);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void CancelFindPassword(int taskId);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int AddNewPwd(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string pwd);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void EnumKeysCallback(
            [MarshalAs(UnmanagedType.LPUTF8Str)] string key, IntPtr userData);

        [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void GetKeys(EnumKeysCallback callback, IntPtr userData);

        // 回调委托
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int FindPasswordCallback(
            ref FindPasswordProgress progress, IntPtr userData);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void FindPasswordSignalCallback(int taskId, IntPtr userData);
    }

    // -----------------------------------------------------------------------
    // 原生结构体（对应 C 接口）
    // -----------------------------------------------------------------------
    [StructLayout(LayoutKind.Sequential)]
    public struct FindPasswordProgress
    {
        public int     current;
        public int     total;
        public IntPtr  pwd;    // const char* UTF-8
        public IntPtr  type;   // const char* UTF-8
        public int     found;
        public int     finished;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct FindPasswordProgressOut
    {
        public int current;
        public int total;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)]
        public string pwd;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string type;
        public int found;
        public int finished;
    }

    // -----------------------------------------------------------------------
    // UI 层可订阅的进度数据
    // -----------------------------------------------------------------------
    public class PasswordSearchProgress
    {
        public int    Current  { get; init; }
        public int    Total    { get; init; }
        public string Password { get; init; } = "";
        public string Type     { get; init; } = "";
        public bool   Found    { get; init; }
        public bool   Finished { get; init; }
    }

    // -----------------------------------------------------------------------
    // 业务层：ArchiveToolService
    // -----------------------------------------------------------------------
    public class ArchiveToolService
    {
        // ── 事件：UI 层订阅 ──────────────────────────────────────────────

        /// <summary>功能1完成：文件类型检测结果（type 字符串，空字符串表示失败）</summary>
        public event Action<string>? OnTypeDetected;

        /// <summary>功能3：密码搜索进度更新</summary>
        public event Action<PasswordSearchProgress>? OnPasswordProgress;

        // ── 内部状态 ─────────────────────────────────────────────────────
        private int _currentTaskId = 0;

        // 保持委托引用，防止 GC 回收
        private ArchiveToolNative.FindPasswordCallback?       _callbackRef;
        private ArchiveToolNative.FindPasswordSignalCallback? _signalCallbackRef;

        // ── 功能1：检测文件类型（后台执行，完成后触发 OnTypeDetected）────

        /// <summary>
        /// 传入文件路径，后台检测压缩类型，完成后通过 OnTypeDetected 返回结果。
        /// </summary>
        public void DetectFileType(string filePath)
        {
            Task.Run(() =>
            {
                byte[] buf = new byte[256];
                int len = ArchiveToolNative.TryDetermineType(filePath, buf, buf.Length);
                string type = len > 0
                    ? System.Text.Encoding.UTF8.GetString(buf, 0, len)
                    : "";
                OnTypeDetected?.Invoke(type);
            });
        }

        // ── 功能2+3：密码搜索（异步，进度通过 OnPasswordProgress 推送）──

        /// <summary>
        /// 传入文件路径和类型，后台遍历密码本。
        /// 每次尝试密码时触发 OnPasswordProgress。
        /// findAll: true=全部遍历，false=找到第一个就停止。
        /// 返回 taskId，可用于 Cancel。
        /// </summary>
        public int StartPasswordSearch(string filePath, bool findAll = false)
        {
            // 取消上一个任务
            if (_currentTaskId != 0)
            {
                ArchiveToolNative.CancelFindPassword(_currentTaskId);
                _currentTaskId = 0;
            }

            // 保持委托引用（防止 GC）
            _callbackRef = OnNativeCallback;

            int taskId = ArchiveToolNative.FindPasswordAsync(
                filePath,
                _callbackRef,
                IntPtr.Zero,
                findAll ? 1 : 0);

            _currentTaskId = taskId;
            return taskId;
        }

        /// <summary>取消当前密码搜索任务</summary>
        public void CancelSearch()
        {
            if (_currentTaskId != 0)
            {
                ArchiveToolNative.CancelFindPassword(_currentTaskId);
                _currentTaskId = 0;
            }
        }

        // ── 原生回调（在后台线程触发，UI 层需注意线程切换）──────────────

        private int OnNativeCallback(ref FindPasswordProgress p, IntPtr _)
        {
            string pwd  = p.pwd  != IntPtr.Zero ? Marshal.PtrToStringUTF8(p.pwd)  ?? "" : "";
            string type = p.type != IntPtr.Zero ? Marshal.PtrToStringUTF8(p.type) ?? "" : "";

            var progress = new PasswordSearchProgress
            {
                Current  = p.current,
                Total    = p.total,
                Password = pwd,
                Type     = type,
                Found    = p.found    != 0,
                Finished = p.finished != 0,
            };

            OnPasswordProgress?.Invoke(progress);

            // 返回 1 继续，0 中止（这里始终继续，取消通过 CancelFindPassword）
            return 1;
        }
    }
}
