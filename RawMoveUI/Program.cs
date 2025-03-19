

using System.Runtime.InteropServices;

namespace RawMoveUI
{
    internal static class Program
    {
        static void ErrMsgBack(IntPtr msg, IntPtr obj, int level)
        {
            string? message = Marshal.PtrToStringUni(msg);
            Console.WriteLine(message);
        }

        static public NS_RawMoveCLR.RawMoveCLR clr = new();
        /// <summary>
        ///  The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            AppDomain.CurrentDomain.UnhandledException += (sender, e) =>
            {
                Console.WriteLine($"未捕获的异常: {e.ExceptionObject}");
                Environment.Exit(1); // 强制退出程序，模拟崩溃
            };

            NS_RawMoveCLR.RawMoveCLR.CallbackDelegate callbackDelegate = ErrMsgBack;
            clr.SetCbErrMsg(callbackDelegate);

            NS_RawMoveCLR.RawMoveCLR.test();

            // To customize application configuration such as set high DPI settings or default font,
            // see https://aka.ms/applicationconfiguration.
            ApplicationConfiguration.Initialize();
            Application.Run(new Form1());
        }
    }
}