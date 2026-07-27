#include <iostream>
#include <string>
#include <vector>

#include "commands.h"

namespace {
void PrintHelp() {
    std::cerr << "Usage: coser_cli <command> [args...]\n\n"
                 "Commands:\n"
                 "  ingest   Ingest an image into the store (face + clothing + exact-match routes)\n"
                 "  query    Query the store for matches\n"
                 "  scan     Batch-scan a directory with the CLIP photo-authenticity checker\n\n"
                 "Run 'coser_cli <command>' with no further args to see that command's own usage.\n";
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintHelp();
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        PrintHelp();
        return 0;
    }

    std::vector<char*> subArgs;
    subArgs.push_back(argv[0]);
    for (int i = 2; i < argc; ++i) subArgs.push_back(argv[i]);
    int subArgc = static_cast<int>(subArgs.size());
    char** subArgv = subArgs.data();

    if (cmd == "ingest") return RunIngest(subArgc, subArgv);
    if (cmd == "query") return RunQuery(subArgc, subArgv);
    if (cmd == "scan") return RunScan(subArgc, subArgv);

    std::cerr << "Unknown command: " << cmd << "\n\n";
    PrintHelp();
    return 1;
}
