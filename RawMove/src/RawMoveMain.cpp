#include <iostream>
#include <string>

#include <Windows.h>

// DLL 加载时调用
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            //std::cout << "Library Loaded!" << std::endl;
            break;
        case DLL_PROCESS_DETACH:
            //std::cout << "Library Unloaded!" << std::endl;
            break;
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }
    return TRUE;
}

#include "RawMoveMain.h"

#include <filesystem>
namespace fs = std::filesystem;
#include <map>
#include <set>
#include <vector>

#include "CommonTool.h"

int RawMoveCore::GetNum()
{
    return 99;
}

bool RawMoveCore::MoveAllRawToStorePath()
{
    // check folder [rawsStore]
    if (fs::exists(m_RawsStorePath) == false) {
        fs::create_directory(m_RawsStorePath);
    }

    if (fs::is_directory(m_RawsStorePath) == false) {
        return false;
    }

    bool isInRawStoreDir = false;
    std::map<std::wstring, std::wstring, CaseInsCmpW> rawInStoreStem2FullPath;
    std::map<std::wstring, std::wstring, CaseInsCmpW> rawAllStem2FullPath;
    std::map<std::wstring, std::wstring, CaseInsCmpW> picStem2FullPath;

    std::wstring fold = L"(D:\\_File\\a6700\\深圳AB动漫展_2025-03-15)";
    fold = m_rootDirPath;
    std::wstring store = fs::path(L"D:\\_File\\a6700\\深圳AB动漫展_2025-03-15\\ALL_RAW🚀").generic_wstring();
    store = this->m_RawsStorePath;

    bool hasSearchedTheStoreFolder = false;
    auto folderOnOffCb = [&](bool isInto, const std::wstring &folder) {
        if (fs::canonical(folder) == fs::canonical(store)) {
            hasSearchedTheStoreFolder = true;
            isInRawStoreDir = isInto;
        }
    };

    FindFilesDfs(
        fold,
        [&](const std::wstring &folderPath, const WIN32_FIND_DATAW &findData) {
            std::wstring fileName(findData.cFileName);
            fs::path path(fileName);
            std::wstring stem = path.filename().stem().wstring();
            std::wstring ext = path.extension().wstring();
            // record the raw under the store directory
            if (lstrcmpiW(ext.c_str(), L".arw") == 0) {
                rawAllStem2FullPath[stem] = folderPath + L"\\" + fileName;
                if (isInRawStoreDir) {
                    rawInStoreStem2FullPath[stem] = folderPath + L"\\" + fileName;
                }
            }
            // record the pic
            static const std::set<std::wstring, CaseInsCmpW> picExts = {
                L".jpg",
                L".jpeg",
                L".png",
            };
            if (picExts.count(ext) > 0) {
                picStem2FullPath[stem] = folderPath + L"\\" + fileName;
            }

            return GoOnFind::CONTINUE;
        },
        folderOnOffCb);
    for (const auto &[rawstem, fullpath] : rawAllStem2FullPath) {
        std::wstring newPath = store + L"\\" + fs::path(fullpath).filename().wstring();
        try {
            fs::rename(fullpath, newPath);
        }
        catch (const std::exception &e) {
            std::cerr << "发生错误: " << e.what() << std::endl;
        }
    }
}

ZYB_RAWMOVE_API void say_hello()
{
    bool isInRawStoreDir = false;
    std::map<std::wstring, std::wstring, CaseInsCmpW> rawStem2FullPath;
    std::map<std::wstring, std::wstring, CaseInsCmpW> picStem2FullPath;

    std::wstring fold = CommonTool::Local2Wstr(R"(D:\_File\a6700\深圳AB动漫展_2025-03-15)");
    std::wstring store = fs::path(L"D:\\_File\\a6700\\深圳AB动漫展_2025-03-15\\ALL_RAW🚀").generic_wstring();

    auto folderOnOffCb = [&](bool isInto, const std::wstring &folder) {
        if (fs::canonical(folder) == fs::canonical(store)) {
            isInRawStoreDir = isInto;
        }
    };

    FindFilesDfs(
        fold,
        [&](const std::wstring &folderPath, const WIN32_FIND_DATAW &findData) {
            std::wstring fileName(findData.cFileName);
            fs::path path(fileName);
            std::wstring stem = path.filename().stem().wstring();
            std::wstring ext = path.extension().wstring();
            // record the raw under the store directory
            if (lstrcmpiW(ext.c_str(), L".arw") == 0) {
                if (isInRawStoreDir) {
                    rawStem2FullPath[stem] = folderPath + L"\\" + fileName;
                }
            }
            // record the pic
            static const std::set<std::wstring, CaseInsCmpW> picExts = {
                L".jpg",
                L".jpeg",
                L".png",
            };
            if (picExts.count(ext) > 0) {
                picStem2FullPath[stem] = folderPath + L"\\" + fileName;
            }

            return GoOnFind::CONTINUE;
        },
        folderOnOffCb);

    std::cout << "Hello from MyLibrary!" << std::endl;
}
