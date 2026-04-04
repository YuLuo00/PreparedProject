extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/avutil.h"
}

#include <fstream>
#include <iostream>
#include <streambuf>
#include <sstream>
#include <string>
#include <atomic>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "tools.h"
using namespace Tools;

namespace
{
std::atomic<bool> g_logUtf8ToLocalEnabled{true};
}

std::string Tools::AvErrorCode2Str(int errCode)
{
    char buf[256];
    av_strerror(errCode, buf, sizeof(buf));
    return std::string(buf);
}

static void print_av_error(int err)
{
    std::cerr << "FFmpeg error: " << Tools::AvErrorCode2Str(err) << "\n";
}

bool Tools::StringFromFile(const std::wstring &file, std::string &content)
{
    std::string fileU8 = Tools::wstring_to_utf8(file);
    std::string fileLoc = Tools::Utf8ToLocal(fileU8);
    std::ifstream ifs(fileLoc, std::ios::in | std::ios::binary);
    if (!ifs) {
        // 打开失败
        return false;
    }

    // 获取文件大小并一次性分配
    ifs.seekg(0, std::ios::end);
    std::streampos size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    if (size > 0) {
        content.resize(static_cast<size_t>(size));
        ifs.read(&content[0], size);
        // 如果读取失败（例如中途出错），可以根据需要处理：
        if (!ifs) {
            // 读取不完整，返回已读部分或空字符串；这里返回已读部分
            content.resize(static_cast<size_t>(ifs.gcount()));
        }
    }
    else {
        // 文件大小为0或 tellg 不支持（例如某些流），退回到流式读取
        std::ostringstream ss;
        ss << ifs.rdbuf();
        content = ss.str();
    }

    return true;
}

// 读取文本文件到 outText，返回 true 表示成功。
// 如果 errMsg 不为空且发生错误，会写入错误描述。
// 会去除 UTF-8 BOM（如果存在）并把 CRLF 转为 LF。
bool Tools::TextFromFile(const std::string &path, std::string &outText, std::string *errMsg)
{
    outText.clear();
    std::ifstream ifs(path, std::ios::in | std::ios::binary);
    if (!ifs) {
        if (errMsg)
            *errMsg = "Failed to open file: " + path;
        return false;
    }

    // 读取全部内容
    ifs.seekg(0, std::ios::end);
    std::streampos sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    if (sz > 0) {
        outText.resize(static_cast<size_t>(sz));
        ifs.read(&outText[0], sz);
        if (!ifs) {
            // 读取可能不完整，保留已读部分
            outText.resize(static_cast<size_t>(ifs.gcount()));
        }
    }
    else {
        // 文件为空或 tellg 不支持，退回到流式读取
        std::ostringstream ss;
        ss << ifs.rdbuf();
        outText = ss.str();
    }

    // 去除 UTF-8 BOM（0xEF 0xBB 0xBF）
    if (outText.size() >= 3 && static_cast<unsigned char>(outText[0]) == 0xEF &&
        static_cast<unsigned char>(outText[1]) == 0xBB && static_cast<unsigned char>(outText[2]) == 0xBF) {
        outText.erase(0, 3);
    }

    //// 规范化 CRLF -> LF（只在需要时做替换）
    //// 如果文件很大且担心性能，可改为按块处理
    //std::string::size_type pos = 0;
    //while ((pos = outText.find("\r\n", pos)) != std::string::npos) {
    //    outText.replace(pos, 2, "\n");
    //    pos += 1; // 跳过刚替换的 '\n'
    //}
    //// 可选：把孤立的 '\r' 也替换为 '\n'
    //pos = 0;
    //while ((pos = outText.find('\r', pos)) != std::string::npos) {
    //    outText[pos] = '\n';
    //    pos += 1;
    //}

    return true;
}

// 简单包装：失败返回空字符串（无法区分空文件与失败）
std::string Tools::TextFromFile(const std::string &path)
{
    std::string out;
    if (Tools::TextFromFile(path, out, nullptr))
        return out;
    return std::string();
}

bool Tools::ParseJsonSafe(const std::string &s, nlohmann::json &out, std::string *err)
{
    try {
        out = nlohmann::json::parse(s);
        return true;
    }
    catch (const nlohmann::json::parse_error &e) {
        if (err)
            *err = e.what(); // 包含错误位置等信息
        return false;
    }
    catch (const std::exception &e) {
        if (err)
            *err = e.what();
        return false;
    }
}

// 返回 true 表示文件是合法的 UTF-8（允许可选 BOM）
bool Tools::IsUtf8File(const std::string &path, bool allow_bom)
{
    const size_t BUF_SIZE = 64 * 1024;
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
        return false;

    std::vector<unsigned char> buf;
    buf.reserve(BUF_SIZE);

    // 用于跨块保存未完成的起始字节
    std::vector<unsigned char> pending;

    // 读取第一个块，检查 BOM（如果允许）
    ifs.seekg(0, std::ios::beg);

    while (true) {
        buf.clear();
        // 先把 pending 拷贝到 buf 开头
        for (unsigned char c : pending)
            buf.push_back(c);
        pending.clear();

        // 读入更多字节填满 buf
        size_t need = BUF_SIZE;
        std::vector<char> tmp(need);
        ifs.read(tmp.data(), static_cast<std::streamsize>(need));
        std::streamsize r = ifs.gcount();
        if (r > 0) {
            buf.insert(buf.end(), tmp.begin(), tmp.begin() + r);
        }

        if (buf.empty())
            break; // 文件为空或读完

        size_t i = 0;
        // 如果是文件开头并允许 BOM，检查并跳过 BOM
        static bool checked_bom = false;
        if (!checked_bom) {
            checked_bom = true;
            if (allow_bom && buf.size() >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
                i = 3; // 跳过 BOM
            }
        }

        while (i < buf.size()) {
            unsigned char c = buf[i];

            if (c <= 0x7F) {
                // ASCII
                ++i;
                continue;
            }

            // 多字节起始字节判断
            if ((c & 0xE0) == 0xC0) {
                // 2 字节序列: 110xxxxx 10xxxxxx
                // 禁止 0xC0,0xC1（过长编码）
                if (c == 0xC0 || c == 0xC1)
                    return false;
                size_t need_cont = 1;
                if (i + need_cont >= buf.size()) {
                    // 不足，保存剩余到 pending 并跳出读取下一块
                    pending.insert(pending.end(), buf.begin() + i, buf.end());
                    goto read_next_chunk;
                }
                // 检查 continuation bytes
                for (size_t k = 1; k <= need_cont; ++k) {
                    unsigned char cc = buf[i + k];
                    if ((cc & 0xC0) != 0x80)
                        return false;
                }
                i += 1 + need_cont;
                continue;
            }
            else if ((c & 0xF0) == 0xE0) {
                // 3 字节序列: 1110xxxx 10xxxxxx 10xxxxxx
                size_t need_cont = 2;
                if (i + need_cont >= buf.size()) {
                    pending.insert(pending.end(), buf.begin() + i, buf.end());
                    goto read_next_chunk;
                }
                unsigned char c1 = buf[i + 1];
                // 特殊检查避免过长或 surrogate
                if (c == 0xE0) {
                    if (c1 < 0xA0 || c1 > 0xBF)
                        return false;
                }
                else if (c == 0xED) {
                    if (c1 < 0x80 || c1 > 0x9F)
                        return false; // 避免 UTF-16 surrogate
                }
                else {
                    if ((c1 & 0xC0) != 0x80)
                        return false;
                }
                unsigned char c2 = buf[i + 2];
                if ((c2 & 0xC0) != 0x80)
                    return false;
                i += 3;
                continue;
            }
            else if ((c & 0xF8) == 0xF0) {
                // 4 字节序列: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                size_t need_cont = 3;
                if (i + need_cont >= buf.size()) {
                    pending.insert(pending.end(), buf.begin() + i, buf.end());
                    goto read_next_chunk;
                }
                unsigned char c1 = buf[i + 1];
                if (c == 0xF0) {
                    if (c1 < 0x90 || c1 > 0xBF)
                        return false;
                }
                else if (c == 0xF4) {
                    if (c1 < 0x80 || c1 > 0x8F)
                        return false; // 最大 U+10FFFF
                }
                else {
                    if ((c1 & 0xC0) != 0x80)
                        return false;
                }
                // 检查其余 continuation bytes
                if ((buf[i + 2] & 0xC0) != 0x80)
                    return false;
                if ((buf[i + 3] & 0xC0) != 0x80)
                    return false;
                i += 4;
                continue;
            }
            else {
                // 起始字节非法（例如 0xF5-0xFF 或 0x80-0xBF 单独出现）
                return false;
            }
        }

    read_next_chunk:
        // 如果文件已读完且 pending 非空，说明末尾有不完整序列 -> 非法
        if (ifs.eof()) {
            if (!pending.empty())
                return false;
            break;
        }
        // 否则继续循环读取下一块（pending 已保留）
        if (!ifs)
            break;
    }

    return true;
}

// 检查 path/filename 是否存在且为常规文件
bool Tools::file_exists(const fs::path &dir, const std::string &filename)
{
    try {
        fs::path p = dir / filename;
        return fs::exists(p) && fs::is_regular_file(p);
    }
    catch (const fs::filesystem_error &) {
        return false;
    }
}

// 判断某个目录 dir 是否包含至少一个子目录 child，且 child 中同时包含 video.m4s, audio.m4s, index.json
bool Tools::has_media_subdir(const fs::path &dir)
{
    try {
        for (auto const &entry : fs::directory_iterator(dir)) {
            if (!entry.is_directory())
                continue;
            fs::path child = entry.path();
            if (file_exists(child, "video.m4s") && file_exists(child, "audio.m4s") &&
                file_exists(child, "index.json")) {
                return true;
            }
        }
    }
    catch (const fs::filesystem_error &) {
        // 忽略无法访问的目录
    }
    return false;
}

// 从 root 开始递归扫描，返回所有符合条件的目录路径
std::vector<fs::path> Tools::CollectBiliFolders(const fs::path &root)
{
    std::vector<fs::path> result;
    if (!fs::exists(root) || !fs::is_directory(root))
        return result;

    try {
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied), end; it != end;
             ++it) {
            try {
                if (!it->is_directory())
                    continue;
                fs::path dir = it->path();

                // 必须同时存在 entry.json 和 danmaku.xml（在 dir 下）
                if (!file_exists(dir, "entry.json"))
                    continue;
                if (!file_exists(dir, "danmaku.xml"))
                    continue;

                // 并且存在至少一个子目录包含 video.m4s/audio.m4s/index.json
                if (has_media_subdir(dir)) {
                    result.push_back(dir);
                    // 如果不想在该目录下继续递归（避免重复发现子目录中的子目录），可以跳过递归：
                    it.disable_recursion_pending();
                }
            }
            catch (const fs::filesystem_error &) {
                // 忽略单个条目错误，继续扫描
            }
        }
    }
    catch (const fs::filesystem_error &) {
        // 根目录不可读或其他全局错误，直接返回已收集的结果（可能为空）
    }

    return result;
}

json Tools::LoadJsonFromFile(const std::filesystem::path &path)
{
    // 打开文件
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    // 解析 JSON
    try {
        json j;
        ifs >> j; // nlohmann::json 支持直接从流解析
        return j;
    }
    catch (const json::parse_error &e) {
        throw std::runtime_error("JSON parse error in file " + path.string() + ": " + e.what());
    }
}

std::string Tools::sanitize_windows_filename(const std::string &input)
{
    std::string result = input;

    const std::string invalid_chars = "\\/:*?\"<>|";

    for (char &c : result) {
        // 替换非法字符
        if (invalid_chars.find(c) != std::string::npos || static_cast<unsigned char>(c) < 32) {
            c = '？';
        }
    }

    return result;
}

std::wstring Tools::utf8_to_wstring(const std::string &str)
{
    if (str.empty())
        return L"";

    // 先计算需要的长度
    int size_needed = MultiByteToWideChar(CP_UTF8, // 输入是 UTF-8
                                          0,
                                          str.data(),
                                          (int)str.size(),
                                          nullptr,
                                          0);

    if (size_needed <= 0) {
        throw std::runtime_error("MultiByteToWideChar failed");
    }

    std::wstring result(size_needed, 0);

    // 真正转换
    MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), &result[0], size_needed);

    return result;
}

std::string Tools::wstring_to_utf8(const std::wstring &wstr)
{
    if (wstr.empty())
        return "";

    // 先计算需要的长度
    int size_needed = WideCharToMultiByte(CP_UTF8, // 输出 UTF-8
                                          0,
                                          wstr.data(),
                                          (int)wstr.size(),
                                          nullptr,
                                          0,
                                          nullptr,
                                          nullptr);

    if (size_needed <= 0) {
        throw std::runtime_error("WideCharToMultiByte failed");
    }

    std::string result(size_needed, 0);

    // 真正转换
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), (int)wstr.size(), &result[0], size_needed, nullptr, nullptr);

    return result;
}

std::wstring Tools::sanitize_windows_filename(const std::wstring &input)
{
    std::wstring result = input;

    const std::wstring invalid_chars = L"\\/～:*?\"<>|";

    for (wchar_t &c : result) {
        // 替换非法字符 + 控制字符
        if (invalid_chars.find(c) != std::wstring::npos || c < 32) {
            c = L'？';
        }
    }

    return result;
}

std::string Tools::Utf8ToLocal(const std::string &utf8)
{
#ifdef _WIN32
    // UTF-8 → UTF-16
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (wlen <= 0)
        return "";

    std::wstring wbuf(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wbuf[0], wlen);

    // UTF-16 → 本地编码（CP_ACP = 系统默认编码，如 GBK）
    int len = WideCharToMultiByte(CP_ACP, 0, wbuf.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return "";

    std::string buf(len, 0);
    WideCharToMultiByte(CP_ACP, 0, wbuf.c_str(), -1, &buf[0], len, nullptr, nullptr);

    return buf;
#else
    // Linux/macOS 默认 UTF-8，不需要转换
    return utf8;
#endif
}

void Tools::SetLogUtf8ToLocalEnabled(bool enabled)
{
    g_logUtf8ToLocalEnabled.store(enabled, std::memory_order_relaxed);
}

bool Tools::IsLogUtf8ToLocalEnabled()
{
    return g_logUtf8ToLocalEnabled.load(std::memory_order_relaxed);
}

std::string Tools::LogUtf8(const std::string &utf8)
{
    if (IsLogUtf8ToLocalEnabled()) {
        return Utf8ToLocal(utf8);
    }
    return utf8;
}












