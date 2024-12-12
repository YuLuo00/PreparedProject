#include "CommonTool.h"

#include <Windows.h>

#include <fstream>
#include <codecvt>
#include <corecrt_wstring.h>

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
