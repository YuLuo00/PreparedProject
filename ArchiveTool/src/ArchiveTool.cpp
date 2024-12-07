#include "ArchiveTool.h"

#include <iostream>
#include <map>
#include <string>

#include <bit7z/bitfileextractor.hpp>
#include <archive.h>
#include <archive_entry.h>

#include "Global.h"
#include "CommonTool.h"
#include "PwdManager.h"

ZYB_ARCHIVE_TOOL_API bool ArchiveExtraTest(const std::string &file, const std::string passwd, const std::string &type)
{
    try { // bit7z classes can throw BitException objects
        using namespace bit7z;

        //Bit7zLibrary lib{ "7zip.dll" };
        //BitFileExtractor extractor{ lib, BitFormat::Auto };
        BitFileExtractor extractor{ ::Get7zLibrary(), BitFormat::Auto};
        if (!passwd.empty()) {
            extractor.setPassword(passwd);
        }
        extractor.test(file);
    }
    catch (const bit7z::BitException &ex) {
        std::cout << ex.what() << std::endl;
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

    archive_read_close(a);
    archive_read_free(a);

    if (format) {
        return format;
    }

    return "";
}


