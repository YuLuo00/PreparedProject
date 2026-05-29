#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <windows.h>

#define ZYB_ARCHIVE_TOOL_API __declspec(dllimport)
#include "ArchiveTool.h"

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
            std::vector<std::string> keys;
            GetKeys([](const char *key, void *ud) {
                static_cast<std::vector<std::string>*>(ud)->push_back(key);
            }, &keys);
            std::cout << "支持的格式（" << keys.size() << " 个）：\n";
            for (const auto &k : keys) std::cout << "  " << k << "\n";
        }
        else if (cmd == "check") {
            if (args.size() < 2) { std::cout << "用法: check <file>\n"; continue; }
            char buf[256] = {};
            int len = check_format(args[1].c_str(), buf, sizeof(buf));
            std::string result = len > 0 ? buf : "";
            std::cout << "格式: " << (result.empty() ? "(未识别)" : result) << "\n";
        }
        else if (cmd == "type") {
            if (args.size() < 2) { std::cout << "用法: type <file>\n"; continue; }
            char buf[256] = {};
            TryDetermineType(args[1].c_str(), buf, sizeof(buf));
            std::cout << "识别类型: " << buf << "\n";

            std::cout << "详细日志：\n";
            ArchiveToolMsg([](const char *msg, void *) {
                std::cout << "  " << msg << "\n";
            }, nullptr);
        }
        else if (cmd == "test") {
            if (args.size() < 2) { std::cout << "用法: test <file> [passwd] [type]\n"; continue; }
            const char *file   = args[1].c_str();
            const char *passwd = args.size() >= 3 ? args[2].c_str() : "";
            const char *type   = args.size() >= 4 ? args[3].c_str() : "Auto";

            std::cout << "测试中...\n";
            int ok = ArchiveExtraTest(file, passwd, type);
            std::cout << "结果: " << (ok ? "✓ 通过" : "✗ 失败") << "\n";
        }
        else if (cmd == "msg") {
            bool hasMsg = false;
            ArchiveToolMsg([](const char *msg, void *has) {
                std::cout << msg << "\n";
                *static_cast<bool*>(has) = true;
            }, &hasMsg);
            if (!hasMsg) std::cout << "(无日志)\n";
        }
        else {
            std::cout << "未知命令: " << cmd << "，输入 'help' 查看帮助\n";
        }
    }

    return 0;
}
