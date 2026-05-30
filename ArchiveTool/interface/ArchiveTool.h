#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// 所有字符串参数/返回值均为 UTF-8 编码的 char*

// 显式初始化（在后台线程调用，触发 7-Zip 库加载）
ZYB_ARCHIVE_TOOL_API void InitArchiveTool();

// -----------------------------------------------------------------------
// 压缩包测试
// -----------------------------------------------------------------------

ZYB_ARCHIVE_TOOL_API int ArchiveExtraTest(
    const char *file,
    const char *passwd,
    const char *type);

ZYB_ARCHIVE_TOOL_API int check_format(
    const char *filePath,
    char       *buf,
    int         bufSize);

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
ZYB_ARCHIVE_TOOL_API int LookupTypeByFormat(const char *formatStr, char *buf, int bufSize);

// -----------------------------------------------------------------------
// 日志
// -----------------------------------------------------------------------

typedef void (*EnumMsgCallback)(const char *msg, void *userData);
ZYB_ARCHIVE_TOOL_API void ArchiveToolMsg(EnumMsgCallback callback, void *userData);

// -----------------------------------------------------------------------
// 密码本管理
// -----------------------------------------------------------------------

ZYB_ARCHIVE_TOOL_API int AddNewPwd(const char *pwd);

// 将指定密码移到密码本最前面（匹配成功后调用，提升下次命中速度）
ZYB_ARCHIVE_TOOL_API int PromotePwd(const char *pwd);

typedef void (*EnumPwdCallback)(const char *pwd, void *userData);
ZYB_ARCHIVE_TOOL_API void GetAllPwd(EnumPwdCallback callback, void *userData);

// 测试密码在所有类型下的结果回调
typedef void (*TestPasswordTypeCallback)(const char *type, int success, void *userData);


// -----------------------------------------------------------------------
// 密码搜索 - 公共数据结构
// -----------------------------------------------------------------------

// 方式一回调数据（字符串指针，回调期间有效）
struct FindPasswordProgress {
    int         current;
    int         total;
    const char *pwd;      // UTF-8，回调期间有效
    const char *type;     // UTF-8
    int         found;
    int         finished;
};

// 方式二队列数据（固定缓冲区，可安全跨线程读取）
struct FindPasswordProgressOut {
    int  current;
    int  total;
    char pwd[512];   // UTF-8 密码副本
    char type[64];   // 压缩类型副本
    int  found;
    int  finished;
};

// -----------------------------------------------------------------------
// 密码搜索 - 方式一：完整回调（每次尝试都触发，传递完整进度）
// -----------------------------------------------------------------------

// 回调返回 1 继续，返回 0 中止
typedef int (*FindPasswordCallback)(const struct FindPasswordProgress *progress, void *userData);

ZYB_ARCHIVE_TOOL_API int FindFirstPassword(
    const char *filePath,
    char       *buf,
    int         bufSize);

// 返回 taskId（>0）
ZYB_ARCHIVE_TOOL_API int FindPasswordAsync(
    const char           *filePath,
    const char           *type,
    FindPasswordCallback  callback,
    void                 *userData,
    int                   findAll);

// -----------------------------------------------------------------------
// 密码搜索 - 方式二：信号 + 队列（只回调信号，进度存入线程安全队列）
// -----------------------------------------------------------------------

// 信号回调：仅通知"有新数据"，UI 端收到后调用 PopFindPasswordProgress 读取
typedef void (*FindPasswordSignalCallback)(int taskId, void *userData);

// 启动异步搜索（方式二），返回 taskId
ZYB_ARCHIVE_TOOL_API int FindPasswordAsyncQueue(
    const char                 *filePath,
    const char                 *type,
    FindPasswordSignalCallback  signalCallback,
    void                       *userData,
    int                         findAll);

// 测试：使用指定密码，遍历所有已知解压类型尝试解压，
// 回调每种类型的结果（同步调用）。完成后回调一次 type==NULL, success==2 表示结束。
ZYB_ARCHIVE_TOOL_API void TestPasswordAcrossAllTypes(
    const char *filePath,
    const char *passwd,
    TestPasswordTypeCallback callback,
    void *userData);

// 读取最新进度（覆盖写，始终返回最新一条；返回 1=有数据，0=尚无数据）
ZYB_ARCHIVE_TOOL_API int GetLatestFindPasswordProgress(
    int                          taskId,
    struct FindPasswordProgressOut *out);

// -----------------------------------------------------------------------
// 取消（方式一和方式二通用）
// -----------------------------------------------------------------------

ZYB_ARCHIVE_TOOL_API void CancelFindPassword(int taskId);

#ifdef __cplusplus
}
#endif
