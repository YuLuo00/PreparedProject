#pragma once


#include <string>
#include <fstream>
#include <string>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <vector>
namespace fs = std::filesystem;


#include <nlohmann/json.hpp>
nlohmann::json;
using json = nlohmann::json;


namespace Tools
{
    std::string AvErrorCode2Str(int errCode);
    //inline bool StringFromFile(const std::string &file, std::string &cotent)
    //{
    //    return false;
    //}
    bool StringFromFile(const std::wstring &file, std::string &cotent);

    bool        TextFromFile(const std::string &path, std::string &outText, std::string *errMsg = nullptr);
    std::string TextFromFile(const std::string &path);

    bool ParseJsonSafe(const std::string &s, nlohmann::json &out, std::string *err = nullptr);

    // 返回 true 表示文件是合法的 UTF-8（允许可选 BOM）
    bool IsUtf8File(const std::string &path, bool allow_bom = true);

    // 检查 path/filename 是否存在且为常规文件
    bool file_exists(const fs::path &dir, const std::string &filename);

    // 判断某个目录 dir 是否包含至少一个子目录 child，且 child 中同时包含 video.m4s, audio.m4s, index.json
    bool has_media_subdir(const fs::path &dir);

    // 从 root 开始递归扫描，返回所有符合条件的目录路径
    std::vector<fs::path> CollectBiliFolders(const fs::path &root);

    json LoadJsonFromFile(const std::filesystem::path &path);

    std::string sanitize_windows_filename(const std::string &input);

    std::wstring utf8_to_wstring(const std::string &str);
    std::string wstring_to_utf8(const std::wstring &wstr);
    std::string Utf8ToLocal(const std::string &utf8);

    std::wstring sanitize_windows_filename(const std::wstring &input);
    inline std::wstring tolower_wstring(const std::wstring &s, const std::locale &loc = std::locale())
    {
        std::wstring out;
        out.reserve(s.size());
        const std::ctype<wchar_t> &ct = std::use_facet<std::ctype<wchar_t>>(loc);
        for (wchar_t wc : s)
            out.push_back(ct.tolower(wc));
        return out;
    }

 inline bool iequals_wstring(const std::wstring &a, const std::wstring &b, const std::locale &loc = std::locale())
    {
        if (a.size() != b.size())
            return false;
        const std::ctype<wchar_t> &ct = std::use_facet<std::ctype<wchar_t>>(loc);
        for (size_t i = 0; i < a.size(); ++i) {
            if (ct.tolower(a[i]) != ct.tolower(b[i]))
                return false;
        }
        return true;
    }


    }


