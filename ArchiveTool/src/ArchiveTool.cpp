#include "ArchiveTool.h"

#include <iostream>
#include <map>
#include <string>
#include <filesystem>
#include <thread>
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

ZYB_ARCHIVE_TOOL_API bool
ArchiveExtraTest(const std::wstring &file, const std::wstring &passwd, const std::wstring &type)
{
    std::string typeU8 = CommonTool::Wstr2Utf8(type);
    const bit7z::BitInFormat *format = ArchiveType::Ins().GetFormat(typeU8);
    if (passwd == L"reduwallpaper") {
        int i = 0;
    }
    try { // bit7z classes can throw BitException objects
        using namespace bit7z;

        //Bit7zLibrary lib{ "7zip.dll" };
        //BitFileExtractor extractor{ lib, BitFormat::Auto };
        BitFileExtractor extractor{::Get7zLibrary(), *format};
        if (!passwd.empty()) {
            std::string passwdU8 = CommonTool::Wstr2Utf8(passwd);
            extractor.setPassword(passwdU8);
        }
        std::string fileUtf8 = CommonTool::Wstr2Utf8(file);
        extractor.test(fileUtf8);
    }
    catch (const bit7z::BitException &ex) {
        std::string exMsg = ex.what();
        std::error_code code = ex.code();
        int cd = code.value();
        std::cout << exMsg << std::endl;
        return false;
    }
    catch (std::exception &ex) {
        std::string exMsg = ex.what();
        std::cout << exMsg << std::endl;
        return false;
    }
    catch (...) {
        return false;
    }

    return true;
}

ZYB_ARCHIVE_TOOL_API bool AddNewPwd(const std::string &pwd)
{
    return PwdManager::Ins().AddNewPwd(pwd);
}

ZYB_ARCHIVE_TOOL_API std::vector<std::string> GetAllPwd()
{
    return PwdManager::Ins().GetAllPwd();
}

ZYB_ARCHIVE_TOOL_API std::string check_format(const std::string &filePath)
{
    struct archive *a;
    struct archive_entry *entry;
    int r;
    a = archive_read_new();
    r = archive_read_support_format_all(a);

    if (r != ARCHIVE_OK) {
        return "";
    }

    const char *format = NULL;

    r = archive_read_open_filename(a, filePath.c_str(), 500 * 1240); //            test.zip
    if (r != ARCHIVE_OK) {
        auto errInfo = archive_error_string(a);
        std::cout << "Error opening compressed file." << std::endl;
        return "";
    }

    bool is_encrypted = archive_read_has_encrypted_entries(a);

    r = archive_read_next_header(a, &entry);
    if (r != ARCHIVE_OK) {
        const char *errMsg = archive_error_string(a);
        if (errMsg != nullptr) {
            std::string err = errMsg;
        }
        //return err;
    }
    format = archive_format_name(a);
    std::string ret = format;

    archive_read_close(a);
    archive_read_free(a);

    return ret;
}

ZYB_ARCHIVE_TOOL_API std::vector<std::string> GetKeys()
{
    return ArchiveType::Ins().GetKeys();
}

ZYB_ARCHIVE_TOOL_API void UpdateTable(const std::string &key, const std::string &type)
{
    return ArchiveType::Ins().UpdateTable(key, type);
}

ZYB_ARCHIVE_TOOL_API std::string TryDetermineType(const std::wstring &filePath)
{
    ArchiveMsg::Ins().Clear();

    std::vector<std::string> keys = ArchiveType::Ins().GetKeys();
    std::partition(keys.begin(), keys.end(), [](const std::string &s) {
        return s != "Auto"; // 保留所有非"Auto"的元素
    });
    int i = 0;
    for (size_t i = 0; i < keys.size(); i++) {
        const std::string &type = keys[i];
        const bit7z::BitInFormat *format = ArchiveType::Ins().GetFormat(type);
        bit7z::BitFileExtractor extractor{::Get7zLibrary(), *format};
        std::string filePathUtf8 = CommonTool::Wstr2Utf8(filePath);
        try {
            extractor.test(filePathUtf8);
            return type;
        }
        catch (const bit7z::BitException &ex) {
            std::string exMsg = ex.what();
            std::cout << exMsg << std::endl;
            std::error_code code = ex.code();
            ArchiveMsg::Ins().AddLine(fmt::format("[{}]::[{}]{}", type, code.value(), exMsg));
            int cd = code.value();
            if (exMsg.find("A password is required but none was provided") != std::string::npos) {
                return type;
            }
            switch (cd) {
                case 5: { // 未提供密码
                    return type;
                }
                case 9: { // 密码错误
                    return type;
                }
                case 1: { // 类型不正确
                    continue;
                }
                default: {
                    continue;
                }
            }
        }
    }
    return "Auto";
}


ZYB_ARCHIVE_TOOL_API const std::vector<std::string> &ArchiveToolMsg()
{
    return ArchiveMsg::Ins().MsgLines();
}

ZYB_ARCHIVE_TOOL_API std::wstring FindFirstPassword(const std::wstring &filePath)
{
    // 1. 先确定压缩类型
    std::string type = TryDetermineType(filePath);
    std::wstring wtype = CommonTool::Utf82Wstr(type);

    // 2. 遍历密码本，找到第一个能解压的密码
    std::vector<std::string> pwds = PwdManager::Ins().GetAllPwd();
    for (const std::string &pwd : pwds) {
        std::wstring wpwd = CommonTool::Utf82Wstr(pwd);
        if (ArchiveExtraTest(filePath, wpwd, wtype)) {
            return wpwd;
        }
    }
    return L""; // 未找到
}

ZYB_ARCHIVE_TOOL_API std::vector<std::wstring> FindAllPasswords(const std::wstring &filePath)
{
    // 1. 先确定压缩类型
    std::string type = TryDetermineType(filePath);
    std::wstring wtype = CommonTool::Utf82Wstr(type);

    // 2. 遍历密码本，收集所有能解压的密码
    std::vector<std::wstring> result;
    std::vector<std::string> pwds = PwdManager::Ins().GetAllPwd();
    for (const std::string &pwd : pwds) {
        std::wstring wpwd = CommonTool::Utf82Wstr(pwd);
        if (ArchiveExtraTest(filePath, wpwd, wtype)) {
            result.push_back(wpwd);
        }
    }
    return result;
}

ZYB_ARCHIVE_TOOL_API void FindPasswordAsync(
    const wchar_t        *filePath,
    FindPasswordCallback  callback,
    void                 *userData,
    bool                  findAll)
{
    // 捕获参数，在后台线程中执行
    std::wstring filePathStr(filePath);

    std::thread([filePathStr, callback, userData, findAll]() {
        // 1. 确定压缩类型
        std::string typeU8 = TryDetermineType(filePathStr);
        std::wstring wtype = CommonTool::Utf82Wstr(typeU8);

        // 2. 获取密码本
        std::vector<std::string> pwds = PwdManager::Ins().GetAllPwd();
        int total = static_cast<int>(pwds.size());

        // 3. 逐个尝试密码
        for (int i = 0; i < total; ++i) {
            std::wstring wpwd = CommonTool::Utf82Wstr(pwds[i]);
            bool found = ArchiveExtraTest(filePathStr, wpwd, wtype);

            FindPasswordProgress progress;
            progress.current  = i + 1;
            progress.total    = total;
            progress.pwd      = wpwd.c_str();
            progress.type     = wtype.c_str();
            progress.found    = found;
            progress.finished = false;

            // 调用回调，返回 false 则中止
            if (!callback(&progress, userData)) {
                return;
            }

            // findAll=false 且找到密码，停止
            if (!findAll && found) {
                return;
            }
        }

        // 4. 全部遍历完成，发送 finished 通知
        FindPasswordProgress done{};
        done.current  = total;
        done.total    = total;
        done.pwd      = L"";
        done.type     = wtype.c_str();
        done.found    = false;
        done.finished = true;
        callback(&done, userData);

    }).detach(); // 后台线程，自动分离
}
