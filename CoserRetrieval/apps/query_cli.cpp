#include <iostream>
#include <string>
#include <unordered_map>
#include <opencv2/imgcodecs.hpp>

#include "../src/L2/MetadataStore.h"
#include "../src/L2/FaissFlatIpIndex.h"
#include "../src/L3/ScrfdFaceDetector.h"
#include "../src/L3/ArcFaceExtractor.h"
#include "../src/L4/FaceRecognitionPipeline.h"

using namespace coser;

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
}  // namespace

int main(int argc, char** argv) {
    auto args = ParseArgs(argc, argv);
    auto get = [&](const std::string& key, const std::string& def = "") {
        auto it = args.find(key);
        return it != args.end() ? it->second : def;
    };

    std::string dbPath = get("db");
    std::string indexPath = get("index");
    std::string modelsDir = get("models-dir");
    std::string imagePath = get("image");
    int topK = std::stoi(get("topk", "5"));

    if (dbPath.empty() || indexPath.empty() || modelsDir.empty() || imagePath.empty()) {
        std::cerr << "Usage: query_cli --db <path> --index <path> --models-dir <dir> --image <path> --topk <N>\n";
        return 1;
    }

    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "Failed to read image: " << imagePath << "\n";
        return 1;
    }

    MetadataStore store;
    if (!store.Open(dbPath)) {
        std::cerr << "Failed to open db: " << store.GetLastError() << "\n";
        return 1;
    }

    FaissFlatIpIndex index(512);
    if (!index.Load(indexPath)) {
        std::cerr << "Failed to load index: " << indexPath << "\n";
        return 1;
    }

    ScrfdFaceDetector detector(modelsDir + "/det_10g.onnx");
    ArcFaceExtractor extractor(modelsDir + "/w600k_r50.onnx");
    FaceRecognitionPipeline pipeline(&detector, &extractor, &index, &store);

    auto matches = pipeline.Query(image, topK);
    if (matches.empty()) {
        std::cout << "No matches (no face detected or empty index)\n";
        return 0;
    }

    for (const auto& m : matches) {
        std::cout << "person_id=" << m.person_id
                   << " display_name=" << m.display_name
                   << " image_id=" << m.image_id
                   << " score=" << m.score << "\n";
    }
    return 0;
}
