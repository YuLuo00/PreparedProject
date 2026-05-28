#include "ArchiveTool.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <cstring>
namespace fs = std::filesystem;

#include <bit7z/bitfileextractor.hpp>
#include <archive.h>
#include <archive_entry.h>
#include <fmt/format.h>

#include "Global.h"
#include "CommonTool.h"
#include "PwdManager.h"
#include "ArchiveType.h"
#include "ArchiveMsg.h"

// -----------------------------------------------------------------------
// 内部辅助：将字符串写入缓冲区，返回实际字符数
// -----------------------------------------------------------------------
static int WriteStrBuf(const std::string &src, char *buf, int bufSize)
{
    int len = static_cast<int>(src.size());
    if (buf && bufSize > 0) {
        int copy = (len < bufSize - 1) ? len : bufSize - 1;
        std::memcpy(buf, src.c_str(), copy);
        buf[copy] = '\0';
    }
    return len;
}

static int WriteWStrBuf(const std::wstring &src, wchar_t *buf, int bufSize)
{
    int len = static_cast<int>(src.size());
    if (buf && bufSize > 0) {
        int copy = (len < bufSize - 1) ? len : bufSize - 1;
        std::wmemcpy(buf, src.c_str(), copy);
        buf[copy] = L'\0';
    }
    return len;
}

// -----------------------------------------------------------------------
// 实现
// -----------------------------------------------------------------------

ZYB_ARCHIVE_TOOL_API int ArchiveExtraTest(
    const wchar_t *file, const wchar_t *passwd, const wchar_t *type)
{
    std::wstring wfile(file ? file : L"");
    std::wstring wpasswd(passwd ? passwd : L"");
    std::wstring wtype(type ? type : L"Auto");

    std::string typeU8 = CommonTool::Wstr2Utf8(wtype);
    const bit7z::BitInFormat *format = ArchiveType::Ins().GetFormat(typeU8);

    try {
        using namespace bit7z;
        BitFileExtractor extractor{::Get7zLibrary(), *format};
        if (!wpasswd.empty()) {
            extractor.setPassword(CommonTool::Wstr2Utf8(wpasswd));
        }
        extractor.test(CommonTool::Wstr2Utf8(wfile));
    }
    catch (...) {
        return 0;
    }
    return 1;
}

ZYB_ARCHIVE_TOOL_API int check_format(const char *filePath, char *buf, int bufSize)
{
    struct archive *a = archive_read_new();
    archive_read_support_format_all(a);

    if (archive_read_open_filename(a, filePath, 500 * 1240) != ARCHIVE_OK) {
        archive_read_free(a);
        return 0;
    }

    struct archive_entry *entry = nullptr;
    archive_read_next_header(a, &entry);

    const char *fmt = archive_format_name(a);
    std::string result = fmt ? fmt : "";

    archive_read_close(a);
    archive_read_free(a);

    return WriteStrBuf(result, buf, bufSize);
}

ZYB_ARCHIVE_TOOL_API int TryDetermineType(const wchar_t *filePath, char *buf, int bufSize)
{
    std::wstring wpath(filePath ? filePath : L"");
    ArchiveMsg::Ins().Clear();

    std::vector<std::string> keys = ArchiveType::Ins().GetKeys();
    std::partition(keys.begin(), keys.end(), [](const std::string &s) {
        return s != "Auto";
    });

    std::string result = "Auto";
    for (const std::string &type : keys) {
        const bit7z::BitInFormat *format = ArchiveType::Ins().GetFormat(type);
        bit7z::BitFileExtractor extractor{::Get7zLibrary(), *format};
        try {
            extractor.test(CommonTool::Wstr2Utf8(wpath));
            result = type;
            break;
        }
        catch (const bit7z::BitException &ex) {
            std::string exMsg = ex.what();
            std::error_code code = ex.code();
            ArchiveMsg::Ins().AddLine(fmt::format("[{}]::[{}]{}", type, code.value(), exMsg));
            int cd = code.value();
            if (exMsg.find("A password is required but none was provided") != std::string::npos
                || cd == 5 || cd == 9) {
                result = type;
                break;
            }
        }
    }

    return WriteStrBuf(result, buf, bufSize);
}

ZYB_ARCHIVE_TOOL_API void GetKeys(EnumKeysCallback callback, void *userData)
{
    if (!callback) return;
    auto keys = ArchiveType::Ins().GetKeys();
    for (const auto &k : keys) {
        callback(k.c_str(), userData);
    }
}

ZYB_ARCHIVE_TOOL_API void UpdateTable(const char *key, const char *type)
{
    if (key && type) {
        ArchiveType::Ins().UpdateTable(std::string(key), std::string(type));
    }
}

ZYB_ARCHIVE_TOOL_API void ArchiveToolMsg(EnumMsgCallback callback, void *userData)
{
    if (!callback) return;
    for (const auto &msg : ArchiveMsg::Ins().MsgLines()) {
        callback(msg.c_str(), userData);
    }
}

ZYB_ARCHIVE_TOOL_API int AddNewPwd(const char *pwd)
{
    if (!pwd) return 0;
    return PwdManager::Ins().AddNewPwd(std::string(pwd)) ? 1 : 0;
}

ZYB_ARCHIVE_TOOL_API void GetAllPwd(EnumPwdCallback callback, void *userData)
{
    if (!callback) return;
    for (const auto &pwd : PwdManager::Ins().GetAllPwd()) {
        callback(pwd.c_str(), userData);
    }
}

ZYB_ARCHIVE_TOOL_API int FindFirstPassword(
    const wchar_t *filePath, wchar_t *buf, int bufSize)
{
    std::wstring wpath(filePath ? filePath : L"");

    char typeBuf[256] = {};
    TryDetermineType(filePath, typeBuf, sizeof(typeBuf));
    std::wstring wtype = CommonTool::Utf82Wstr(typeBuf);

    for (const auto &pwd : PwdManager::Ins().GetAllPwd()) {
        std::wstring wpwd = CommonTool::Utf82Wstr(pwd);
        if (ArchiveExtraTest(filePath, wpwd.c_str(), wtype.c_str())) {
            return WriteWStrBuf(wpwd, buf, bufSize);
        }
    }
    return 0;
}

ZYB_ARCHIVE_TOOL_API void FindPasswordAsync(
    const wchar_t        *filePath,
    FindPasswordCallback  callback,
    void                 *userData,
    int                   findAll)
{
    if (!filePath || !callback) return;
    std::wstring filePathStr(filePath);

    std::thread([filePathStr, callback, userData, findAll]() {
        // 1. 确定压缩类型
        char typeBuf[256] = {};
        TryDetermineType(filePathStr.c_str(), typeBuf, sizeof(typeBuf));
        std::wstring wtype = CommonTool::Utf82Wstr(typeBuf);

        // 2. 获取密码本
        std::vector<std::string> pwds = PwdManager::Ins().GetAllPwd();
        int total = static_cast<int>(pwds.size());

        // 3. 逐个尝试密码
        for (int i = 0; i < total; ++i) {
            std::wstring wpwd = CommonTool::Utf82Wstr(pwds[i]);
            int found = ArchiveExtraTest(filePathStr.c_str(), wpwd.c_str(), wtype.c_str());

            FindPasswordProgress progress{};
            progress.current  = i + 1;
            progress.total    = total;
            progress.pwd      = wpwd.c_str();
            progress.type     = typeBuf;
            progress.found    = found;
            progress.finished = 0;

            if (!callback(&progress, userData)) return;
            if (!findAll && found) return;
        }

        // 4. 完成通知
        FindPasswordProgress done{};
        done.current  = total;
        done.total    = total;
        done.pwd      = L"";
        done.type     = typeBuf;
        done.found    = 0;
        done.finished = 1;
        callback(&done, userData);

    }).detach();
}
