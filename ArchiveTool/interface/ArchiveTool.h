#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// 所有字符串参数/返回值均为 UTF-8 编码的 char*

// -----------------------------------------------------------------------
// 压缩包测试
// -----------------------------------------------------------------------

// 测试压缩包是否能用指定密码和类型解压（0=失败，1=成功）
ZYB_ARCHIVE_TOOL_API int ArchiveExtraTest(
    const char *file,
    const char *passwd,
    const char *type);

// 用 libarchive 检测文件格式，写入 buf（UTF-8），返回实际字符数
ZYB_ARCHIVE_TOOL_API int check_format(
    const char *filePath,
    char       *buf,
    int         bufSize);

// 用 bit7z 确定压缩类型，写入 buf（UTF-8），返回实际字符数
ZYB_ARCHIVE_TOOL_API int TryDetermineType(
    const char *filePath,
    char       *buf,
    int         bufSize);

// -----------------------------------------------------------------------
// 格式表管理
// -----------------------------------------------------------------------

typedef void (*EnumKeysCallback)(const char *key, void *userData);
ZYB_ARCHIVE_TOOL_API void GetKeys(EnumKeysCallback callback, void *userData);

ZYB_ARCHIVE_TOOL_API void UpdateTable(const char *key, const char *type);

// -----------------------------------------------------------------------
// 日志
// -----------------------------------------------------------------------

typedef void (*EnumMsgCallback)(const char *msg, void *userData);
ZYB_ARCHIVE_TOOL_API void ArchiveToolMsg(EnumMsgCallback callback, void *userData);

// -----------------------------------------------------------------------
// 密码本管理
// -----------------------------------------------------------------------

ZYB_ARCHIVE_TOOL_API int AddNewPwd(const char *pwd);

typedef void (*EnumPwdCallback)(const char *pwd, void *userData);
ZYB_ARCHIVE_TOOL_API void GetAllPwd(EnumPwdCallback callback, void *userData);

// -----------------------------------------------------------------------
// 密码搜索
// -----------------------------------------------------------------------

// 同步：找到第一个有效密码，写入 buf（UTF-8），返回实际字符数（0=未找到）
ZYB_ARCHIVE_TOOL_API int FindFirstPassword(
    const char *filePath,
    char       *buf,
    int         bufSize);

// 异步进度回调数据（所有字符串均为 UTF-8）
struct FindPasswordProgress {
    int         current;   // 当前序号（1-based）
    int         total;     // 密码本总数
    const char *pwd;       // 当前尝试的密码（UTF-8，回调期间有效）
    const char *type;      // 确定的压缩类型（UTF-8）
    int         found;     // 当前密码是否有效（0/1）
    int         finished;  // 是否全部完成（0/1）
};

// 回调函数类型：返回 1 继续，返回 0 中止
typedef int (*FindPasswordCallback)(const struct FindPasswordProgress *progress, void *userData);

// 异步搜索密码（后台线程执行）
// findAll: 1=全部遍历，0=找到第一个就停止
// 返回任务 ID（>0），可用于 CancelFindPassword 取消
ZYB_ARCHIVE_TOOL_API int FindPasswordAsync(
    const char           *filePath,
    FindPasswordCallback  callback,
    void                 *userData,
    int                   findAll);

// 取消指定任务（taskId 由 FindPasswordAsync 返回）
ZYB_ARCHIVE_TOOL_API void CancelFindPassword(int taskId);

#ifdef __cplusplus
}
#endif
