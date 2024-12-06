#include "ArchiveTool.h"

#include <iostream>

#include <bit7z/bitfileextractor.hpp>

#include "Global.h"

ZYB_ARCHIVE_TOOL_API bool ArchiveExtraTest(const std::string &file, const std::string passwd)
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