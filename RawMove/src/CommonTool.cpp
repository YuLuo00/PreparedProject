#include "CommonTool.h"

#include <Windows.h>

#include <codecvt>
#include <corecrt_wstring.h>
#include <fstream>

std::string CommonTool::Local2Utf8(const std::string &str)
{
    int size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), NULL, NULL);
    if (size <= 0) {
        return "";
    }
    std::wstring wstr(size, L'\0');
    size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), str.size(), wstr.data(), wstr.size());

    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    std::string ret = Wstr2Utf8(wstr);
    return ret;
}

std::string CommonTool::Utf82Local(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    std::wstring wstr = cvt.from_bytes(str);
    std::string strLocal = Wstr2Local(wstr);
    return strLocal;
}

std::string CommonTool::Wstr2Utf8(const std::wstring &wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    std::string ret = cvt.to_bytes(wstr);

    return ret;
}

std::wstring CommonTool::Utf82Wstr(const std::string &str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    return cvt.from_bytes(str);
}

std::wstring CommonTool::Local2Wstr(const std::string &str)
{
    std::string strU8 = Local2Utf8(str);
    return Utf82Wstr(strU8);
}

std::string CommonTool::Wstr2Local(const std::wstring &wstr)
{
    int size = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), NULL, 0, NULL, NULL);
    if (size <= 0) {
        return "";
    }

    std::string ret(size, '\0');
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), wstr.size(), ret.data(), ret.size(), NULL, NULL);

    return ret;
}

bool CommonTool::StrIsUtf8Bom(const std::string &str)
{
    if (str.size() < 3) {
        return false;
    }

    bool ret = str[0] == 0xEF && str[1] == 0xBB && str[2] == 0xBF;
    return false;
}

bool CommonTool::FileIsUtf8Bom(const std::string &filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // 检查文件大小是否足够
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    if (fileSize < 3) {
        return false; // 文件不足3字节，不可能包含 BOM
    }

    // 读取 BOM
    file.seekg(0, std::ios::beg);
    unsigned char bom[3] = {0};
    file.read(reinterpret_cast<char *>(bom), 3);

    // 检查 BOM
    bool ret = bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF;
    return ret;
}

std::vector<std::string> CommonTool::ReadFileTxtAsLocal(const std::string &filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return {};
    }

    std::vector<std::string> ret;
    std::string line;
    if (CommonTool::FileIsUtf8Bom(filePath)) {
        std::getline(file, line);
        line = std::string(line.begin() + 3, line.end());
        line = CommonTool::Utf82Local(line);
        ret.push_back(line);
        while (std::getline(file, line)) { // 按行读取
            line = CommonTool::Utf82Local(line);
            ret.push_back(line);
        }
    }
    else {
        while (std::getline(file, line)) { // 按行读取
            ret.push_back(line);
        }
    }

    file.close();
    return ret;
}

#include <filesystem>
#include <iostream>
#include <queue>
#include <stack>
#include <string>
#include <vector>
namespace fs = std::filesystem;

#include <windows.h>

#include "CommonTool.h"



/// <summary>
///
/// </summary>
/// <param name="skipThisFolder"></param>
void FindFilesDfs(const std::wstring &directory,
                  std::function<GoOnFind(const std::wstring &folderPath, const WIN32_FIND_DATAW &item)> itemCallback,
                  std::function<void(bool inInto, const std::wstring&folder)> folderInOutCallback)
{
    if (fs::exists(directory) == false || fs::is_directory(directory) == false) {
        return;
    }
    std::wstring dirFullPath = fs::absolute(directory).wstring();
    std::stack<std::wstring> subFolders({dirFullPath});
    while (subFolders.empty() == false) {
        std::wstring curFolder = subFolders.top();
        subFolders.pop();

        // call back for on and off folder
        if (folderInOutCallback) {
            folderInOutCallback(true, curFolder);
        }
        CallByRAII triggerFolderOffCb([&]() {
            if (folderInOutCallback) {
                folderInOutCallback(false, curFolder);
            }
        });

        std::wstring searchPath = curFolder + L"\\*";
        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE) {
            continue;
        }
        do {
            // 排除 "." 和 ".." 目录
            if (lstrcmpW(findData.cFileName, L".") == 0 || lstrcmpW(findData.cFileName, L"..") == 0) {
                continue;
            }
            std::wstring fullPath = curFolder + L"\\" + findData.cFileName;
            GoOnFind goOnFind = itemCallback == nullptr ? GoOnFind::CONTINUE : itemCallback(curFolder, findData);
            switch (goOnFind) {
                case GoOnFind::SKIP:
                    continue;
                case GoOnFind::BREAK:
                    return;
                case GoOnFind::CONTINUE:
                default: {
                    if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        subFolders.push(fullPath);
                        continue;
                    }
                    break;
                }
            };
            continue;
        } while (FindNextFileW(hFind, &findData) != 0);
        FindClose(hFind);
    }
}

void CommonTool::getFilesInDirectory(const std::string &directory)
{
    //FindFilesDfs(directory, [](const std::wstring &file, const WIN32_FIND_DATAW &findData) {
    //    std::string fileStr = CommonTool::Wstr2Utf8(file);
    //    std::wstring fileWStr = CommonTool::Utf82Wstr(fileStr);
    //    return GoOnFind::CONTINUE;
    //});
}
