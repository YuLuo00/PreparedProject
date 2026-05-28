#include "CommonTool.h"

#include <Windows.h>

#include <fstream>

std::string CommonTool::Local2Utf8(const std::string &str)
{
    if (str.empty()) return "";
    int size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), nullptr, 0);
    if (size <= 0) return "";
    std::wstring wstr(size, L'\0');
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), wstr.data(), size);
    return Wstr2Utf8(wstr);
}

std::string CommonTool::Utf82Local(const std::string &str)
{
    std::wstring wstr = Utf82Wstr(str);
    return Wstr2Local(wstr);
}

std::string CommonTool::Wstr2Utf8(const std::wstring &wstr)
{
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string ret(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), ret.data(), size, nullptr, nullptr);
    return ret;
}

std::wstring CommonTool::Utf82Wstr(const std::string &str)
{
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring ret(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), ret.data(), size);
    return ret;
}

std::wstring CommonTool::Local2Wstr(const std::string &str)
{
    std::string strU8 = Local2Utf8(str);
    return Utf82Wstr(strU8);
}

std::string CommonTool::Wstr2Local(const std::wstring &wstr)
{
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string ret(size, '\0');
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), (int)wstr.size(), ret.data(), size, nullptr, nullptr);
    return ret;
}

bool CommonTool::StrIsUtf8Bom(const std::string &str)
{
    if (str.size() < 3) {
        return false;
    }
    return (unsigned char)str[0] == 0xEF &&
           (unsigned char)str[1] == 0xBB &&
           (unsigned char)str[2] == 0xBF;
}

bool CommonTool::FileIsUtf8Bom(const std::string &filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    if (fileSize < 3) {
        return false;
    }

    file.seekg(0, std::ios::beg);
    unsigned char bom[3] = {0};
    file.read(reinterpret_cast<char *>(bom), 3);

    return bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF;
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
        while (std::getline(file, line)) {
            line = CommonTool::Utf82Local(line);
            ret.push_back(line);
        }
    }
    else {
        while (std::getline(file, line)) {
            ret.push_back(line);
        }
    }

    file.close();
    return ret;
}
