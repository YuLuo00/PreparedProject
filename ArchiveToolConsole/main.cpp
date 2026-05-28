#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <windows.h>

#define ZYB_ARCHIVE_TOOL_API __declspec(dllimport)
#include "ArchiveTool.h"

// 将 UTF-8 字符串转为宽字符串
static std::wstring Utf8ToWstr(const std::string &str)
{
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    if (size <= 0) return L"";
    std::wstring ret(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), ret.data(), size);
    return ret;
}

// 将宽字符串转为 UTF-8
static std::string WstrToUtf8(const std::wstring &wstr)
{
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string ret(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), ret.data(), size, nullptr, nullptr);
    return ret;
}

// 分割字符串
static std::vector<std::string> Split(const std::string &s)
{
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

static void PrintHelp()
{
    std::cout << "\n可用命令：\n"
              << "  check <file>                    检查文件格式\n"
              << "  test <file> [passwd] [type]     测试压缩包完整性\n"
              << "  type <file>                     自动识别压缩格式\n"
              << "  keys                            列出所有支持的格式\n"
              << "  msg                             显示上次操作的日志\n"
              << "  help                            显示此帮助\n"
              << "  q                               退出\n\n";
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "=== ArchiveTool Console ===\n";
    std::cout << "输入 'help' 查看命令，'q' 退出\n\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        std::cout.flush();
        if (!std::getline(std::cin, line)) break;

        auto args = Split(line);
        if (args.empty()) continue;

        const std::string &cmd = args[0];

        if (cmd == "q" || cmd == "quit" || cmd == "exit") {
            std::cout << "再见！\n";
            break;
        }
        else if (cmd == "help") {
            PrintHelp();
        }
        else if (cmd == "keys") {
            auto keys = GetKeys();
            std::cout << "支持的格式（" << keys.size() << " 个）：\n";
            for (const auto &k : keys) {
                std::cout << "  " << k << "\n";
            }
        }
        else if (cmd == "check") {
            if (args.size() < 2) {
                std::cout << "用法: check <file>\n";
                continue;
            }
            std::string result = check_format(args[1]);
            std::cout << "格式: " << (result.empty() ? "(未识别)" : result) << "\n";
        }
        else if (cmd == "type") {
            if (args.size() < 2) {
                std::cout << "用法: type <file>\n";
                continue;
            }
            std::wstring wfile = Utf8ToWstr(args[1]);
            std::string result = TryDetermineType(wfile);
            std::cout << "识别类型: " << result << "\n";

            auto msgs = ArchiveToolMsg();
            if (!msgs.empty()) {
                std::cout << "详细日志：\n";
                for (const auto &m : msgs) std::cout << "  " << m << "\n";
            }
        }
        else if (cmd == "test") {
            if (args.size() < 2) {
                std::cout << "用法: test <file> [passwd] [type]\n";
                continue;
            }
            std::wstring wfile   = Utf8ToWstr(args[1]);
            std::wstring wpasswd = args.size() >= 3 ? Utf8ToWstr(args[2]) : L"";
            std::wstring wtype   = args.size() >= 4 ? Utf8ToWstr(args[3]) : L"Auto";

            std::cout << "测试中...\n";
            bool ok = ArchiveExtraTest(wfile, wpasswd, wtype);
            std::cout << "结果: " << (ok ? "✓ 通过" : "✗ 失败") << "\n";
        }
        else if (cmd == "msg") {
            auto msgs = ArchiveToolMsg();
            if (msgs.empty()) {
                std::cout << "(无日志)\n";
            } else {
                for (const auto &m : msgs) std::cout << m << "\n";
            }
        }
        else {
            std::cout << "未知命令: " << cmd << "，输入 'help' 查看帮助\n";
        }
    }

    return 0;
}
