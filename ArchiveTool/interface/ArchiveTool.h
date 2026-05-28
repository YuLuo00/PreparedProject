#pragma once
#include <string>
#include <vector>

ZYB_ARCHIVE_TOOL_API bool ArchiveExtraTest(const std::wstring &file, const std::wstring &passwd, const std::wstring &type);

ZYB_ARCHIVE_TOOL_API std::string check_format(const std::string &filePath);

ZYB_ARCHIVE_TOOL_API std::vector<std::string> GetKeys();
ZYB_ARCHIVE_TOOL_API void UpdateTable(const std::string &key, const std::string &type);
ZYB_ARCHIVE_TOOL_API std::string TryDetermineType(const std::wstring &filePath);
ZYB_ARCHIVE_TOOL_API const std::vector<std::string> &ArchiveToolMsg();

ZYB_ARCHIVE_TOOL_API bool AddNewPwd(const std::string &pwd);
ZYB_ARCHIVE_TOOL_API std::vector<std::string> GetAllPwd();

// 同步：找到第一个能解压的密码（空字符串表示未找到）
ZYB_ARCHIVE_TOOL_API std::wstring FindFirstPassword(const std::wstring &filePath);

// 同步：返回所有能解压的密码（全部遍历，处理误报）
ZYB_ARCHIVE_TOOL_API std::vector<std::wstring> FindAllPasswords(const std::wstring &filePath);

// -----------------------------------------------------------------------
// 异步密码搜索
// -----------------------------------------------------------------------

// 进度回调数据（每次尝试一个密码时触发）
struct FindPasswordProgress {
    int     current;        // 当前尝试的密码序号（从 1 开始）
    int     total;          // 密码本总数
    const wchar_t *pwd;     // 当前尝试的密码（回调期间有效）
    const wchar_t *type;    // 已确定的压缩类型（第一次回调后设置）
    bool    found;          // 当前密码是否有效
    bool    finished;       // 是否已全部遍历完成
};

// 回调函数类型
// 返回 true  继续搜索
// 返回 false 中止搜索
typedef bool (*FindPasswordCallback)(const FindPasswordProgress *progress, void *userData);

// 异步搜索密码（在后台线程执行，通过回调实时返回进度）
// findAll: true  = 全部遍历，收集所有有效密码（finished=true 时结束）
//          false = 找到第一个有效密码即停止
ZYB_ARCHIVE_TOOL_API void FindPasswordAsync(
    const wchar_t        *filePath,
    FindPasswordCallback  callback,
    void                 *userData,
    bool                  findAll
);
