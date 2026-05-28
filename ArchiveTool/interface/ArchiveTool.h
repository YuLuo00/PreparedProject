#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------
// 压缩包测试
// -----------------------------------------------------------------------

// 测试压缩包是否能用指定密码和类型解压（0=失败，1=成功）
ZYB_ARCHIVE_TOOL_API int ArchiveExtraTest(
    const wchar_t *file,
    const wchar_t *passwd,
    const wchar_t *type);

// 用 libarchive 检测文件格式，写入 buf，返回实际字符数（buf=NULL 时只返回所需长度）
ZYB_ARCHIVE_TOOL_API int check_format(
    const char *filePath,
    char       *buf,
    int         bufSize);

// 用 bit7z 确定压缩类型，写入 buf（UTF-8），返回实际字符数
ZYB_ARCHIVE_TOOL_API int TryDetermineType(
    const wchar_t *filePath,
    char          *buf,
    int            bufSize);

// -----------------------------------------------------------------------
// 格式表管理
// -----------------------------------------------------------------------

// 遍历所有支持的格式，每个 key 触发一次回调
typedef void (*EnumKeysCallback)(const char *key, void *userData);
ZYB_ARCHIVE_TOOL_API void GetKeys(EnumKeysCallback callback, void *userData);

// 更新格式表（key -> type 映射）
ZYB_ARCHIVE_TOOL_API void UpdateTable(const char *key, const char *type);

// -----------------------------------------------------------------------
// 日志
// -----------------------------------------------------------------------

// 遍历上次操作的日志消息
typedef void (*EnumMsgCallback)(const char *msg, void *userData);
ZYB_ARCHIVE_TOOL_API void ArchiveToolMsg(EnumMsgCallback callback, void *userData);

// -----------------------------------------------------------------------
// 密码本管理
// -----------------------------------------------------------------------

// 添加新密码（0=失败，1=成功）
ZYB_ARCHIVE_TOOL_API int AddNewPwd(const char *pwd);

// 遍历密码本，每个密码触发一次回调
typedef void (*EnumPwdCallback)(const char *pwd, void *userData);
ZYB_ARCHIVE_TOOL_API void GetAllPwd(EnumPwdCallback callback, void *userData);

// -----------------------------------------------------------------------
// 密码搜索
// -----------------------------------------------------------------------

// 同步：找到第一个有效密码，写入 buf（wchar_t），返回实际字符数（0=未找到）
ZYB_ARCHIVE_TOOL_API int FindFirstPassword(
    const wchar_t *filePath,
    wchar_t       *buf,
    int            bufSize);

// 异步进度回调数据
struct FindPasswordProgress {
    int            current;   // 当前序号（1-based）
    int            total;     // 密码本总数
    const wchar_t *pwd;       // 当前尝试的密码（回调期间有效）
    const char    *type;      // 确定的压缩类型（UTF-8）
    int            found;     // 当前密码是否有效（0/1）
    int            finished;  // 是否全部完成（0/1）
};

// 回调函数类型：返回 1 继续，返回 0 中止
typedef int (*FindPasswordCallback)(const struct FindPasswordProgress *progress, void *userData);

// 异步搜索密码（后台线程执行，通过回调实时返回进度）
// findAll: 1=全部遍历，0=找到第一个就停止
ZYB_ARCHIVE_TOOL_API void FindPasswordAsync(
    const wchar_t        *filePath,
    FindPasswordCallback  callback,
    void                 *userData,
    int                   findAll);

#ifdef __cplusplus
}
#endif
