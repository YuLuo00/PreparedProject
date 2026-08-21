#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

#include "commands.h"

namespace {
void PrintHelp() {
    std::cerr << "Usage: coser_cli <command> [args...]\n\n"
                 "Commands:\n"
                 "  ingest   Ingest an image with person and optional role labels\n"
                 "  query    Query person, role, clothing or exact-image matches\n"
                 "  scan     Batch-scan a directory with the CLIP photo-authenticity checker\n"
                 "  visualize  Render clothing/hair and face-mask regions on an image\n\n"
                 "Run 'coser_cli <command>' with no further args to see that command's own usage.\n";
}
}  // namespace

std::string WideToUtf8(const wchar_t* value) {
    if (!value) return {};
    int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), length, nullptr, nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::vector<std::string> utf8Args;
    utf8Args.reserve(argc);
    for (int i = 0; i < argc; ++i) utf8Args.push_back(WideToUtf8(argv[i]));

    if (argc < 2) {
        PrintHelp();
        return 1;
    }

    std::string cmd = utf8Args[1];
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        PrintHelp();
        return 0;
    }

    std::vector<char*> subArgs;
    subArgs.push_back(utf8Args[0].data());
    for (int i = 2; i < argc; ++i) subArgs.push_back(utf8Args[i].data());
    int subArgc = static_cast<int>(subArgs.size());
    char** subArgv = subArgs.data();

    if (cmd == "ingest") return RunIngest(subArgc, subArgv);
    if (cmd == "query") return RunQuery(subArgc, subArgv);
    if (cmd == "scan") return RunScan(subArgc, subArgv);
    if (cmd == "visualize") return RunVisualize(subArgc, subArgv);

    std::cerr << "Unknown command: " << cmd << "\n\n";
    PrintHelp();
    return 1;
}
