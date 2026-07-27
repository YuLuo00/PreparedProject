#include <iostream>
#include <string>
#include <unordered_map>
#include <opencv2/imgcodecs.hpp>

#include "commands.h"
#include "../src/L2/MetadataStore.h"
#include "../src/L2/FaissFlatIpIndex.h"
#include "../src/L2/PHashIndex.h"
#include "../src/L3/ScrfdFaceDetector.h"
#include "../src/L3/ArcFaceExtractor.h"
#include "../src/L3/YoloPoseDetector.h"
#include "../src/L3/DinoV2Extractor.h"
#include "../src/L3/PHasher.h"
#include "../src/L3/OrbCropMatcher.h"
#include "../src/L4/FaceRecognitionPipeline.h"
#include "../src/L4/ClothingRecognitionPipeline.h"
#include "../src/L4/ImageMatchPipeline.h"
#include "../src/L5/ResultFusion.h"
#include "../src/L5/RetrievalOrchestrator.h"

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

QueryMode ParseMode(const std::string& mode) {
    if (mode == "exact") return QueryMode::ExactOnly;
    if (mode == "face") return QueryMode::FaceOnly;
    if (mode == "clothing") return QueryMode::ClothingOnly;
    return QueryMode::AllLinked;
}

void PrintMatches(const std::string& label, const std::vector<PipelineMatch>& matches) {
    std::cout << "-- " << label << " (" << matches.size() << ") --\n";
    for (const auto& m : matches) {
        std::cout << "  person_id=" << m.person_id
                   << " display_name=" << m.display_name
                   << " image_id=" << m.image_id
                   << " score=" << m.score << "\n";
    }
}
}  // namespace

int RunQuery(int argc, char** argv) try {
    auto args = ParseArgs(argc, argv);
    auto get = [&](const std::string& key, const std::string& def = "") {
        auto it = args.find(key);
        return it != args.end() ? it->second : def;
    };

    std::string dbPath = get("db");
    std::string faceIndexPath = get("index");
    std::string clothingIndexPath = get("clothing-index", "coser_clothing.index");
    std::string modelsDir = get("models-dir");
    std::string imagePath = get("image");
    int topK = std::stoi(get("topk", "5"));
    QueryMode mode = ParseMode(get("mode", "all"));

    if (dbPath.empty() || faceIndexPath.empty() || modelsDir.empty() || imagePath.empty()) {
        std::cerr << "Usage: coser_cli query --db <path> --index <path> --models-dir <dir> --image <path> --topk <N> "
                     "[--mode exact|face|clothing|all] [--clothing-index <path>]\n"
                     "  <models-dir> must contain face/, pose/, clothing/ subdirectories.\n";
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

    FaissFlatIpIndex faceIndex(512);
    if (!faceIndex.Load(faceIndexPath)) {
        std::cerr << "Failed to load face index: " << faceIndexPath << "\n";
        return 1;
    }

    FaissFlatIpIndex clothingIndex(384);
    if (!clothingIndex.Load(clothingIndexPath)) {
        std::cerr << "Failed to load clothing index: " << clothingIndexPath << "\n";
        return 1;
    }

    PHashIndex phashIndex;
    phashIndex.LoadFrom(store.LoadAllPHashes());

    ScrfdFaceDetector faceDetector(modelsDir + "/face/det_10g.onnx");
    ArcFaceExtractor faceExtractor(modelsDir + "/face/w600k_r50.onnx");
    FaceRecognitionPipeline facePipeline(&faceDetector, &faceExtractor, &faceIndex, &store);

    YoloPoseDetector poseDetector(modelsDir + "/pose/yolov8n-pose.onnx");
    DinoV2Extractor dinoExtractor(modelsDir + "/clothing/dinov2_vits14.onnx");
    ClothingRecognitionPipeline clothingPipeline(&poseDetector, &dinoExtractor, &clothingIndex, &store);

    PHasher hasher;
    OrbCropMatcher orbMatcher;
    ImageMatchPipeline imageMatchPipeline(&hasher, &orbMatcher, &phashIndex, &store);

    ResultFusion fusion;
    RetrievalOrchestrator orchestrator(&facePipeline, &clothingPipeline, &imageMatchPipeline, &store, &fusion);

    QueryRequest request;
    request.image = image;
    request.mode = mode;
    request.topK = topK;

    QueryResponse response = orchestrator.Query(request);

    PrintMatches("exact", response.exactMatches);
    PrintMatches("face", response.faceMatches);
    PrintMatches("clothing", response.clothingMatches);
    PrintMatches("fused", response.fused);

    if (response.exactMatches.empty() && response.faceMatches.empty() && response.clothingMatches.empty()) {
        std::cout << "No matches (no face/person detected or all indices empty)\n";
    }
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << "\n";
    return 2;
} catch (...) {
    std::cerr << "Unhandled unknown exception\n";
    return 2;
}
