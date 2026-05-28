#include "ArchiveTool.h"

#include <iostream>
#include <map>
#include <string>
#include <filesystem>
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