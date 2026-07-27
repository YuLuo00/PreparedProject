#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <opencv2/imgcodecs.hpp>

#include "../src/L3/PhotoAuthenticityChecker.h"

using namespace coser;
namespace fs = std::filesystem;

namespace {
std::unordered_map<std::string, std::string> ParseArgs(int argc, char** argv) {
    std::unordered_map<std::string, std::string> args;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string key = argv[i];
        if (key.size() > 2 && key[0] == '-' && key[1] == '-') {
            args[key.substr(2)] = argv[i + 1];
        }
    }
    return args;
}

bool IsImageFile(const fs::path& p) {
    std::string ext = p.extension().string();
    for (auto& c : ext) c = static_cast<char>(::tolower(c));
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp";
}
}  // namespace

int main(int argc, char** argv) try {
    auto args = ParseArgs(argc, argv);
    auto get = [&](const std::string& key, const std::string& def = "") {
        auto it = args.find(key);
        return it != args.end() ? it->second : def;
    };

    std::string dir = get("dir");
    std::string clipModel = get("clip-model");

    if (dir.empty() || clipModel.empty()) {
        std::cerr << "Usage: scan_authenticity_cli --dir <directory> --clip-model <path>\n";
        return 1;
    }

    PhotoAuthenticityChecker checker(clipModel);

    int total = 0, illustrationCount = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file() || !IsImageFile(entry.path())) continue;
        total++;
        std::string path = entry.path().string();
        cv::Mat image = cv::imread(path);
        if (image.empty()) {
            std::cout << "SKIP (unreadable): " << path << "\n";
            continue;
        }
        float score = 0.0f;
        bool isReal = checker.IsRealPhoto(image, &score);
        if (!isReal) {
            illustrationCount++;
            std::cout << "ILLUSTRATION score=" << score << " " << path << "\n";
        } else {
            std::cout << "real_photo   score=" << score << " " << path << "\n";
        }
    }

    std::cout << "---\nTotal scanned: " << total << ", flagged as illustration/CG: " << illustrationCount << "\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << "\n";
    return 2;
} catch (...) {
    std::cerr << "Unhandled unknown exception\n";
    return 2;
}
