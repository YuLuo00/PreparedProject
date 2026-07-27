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
#include "../src/L3/PhotoAuthenticityChecker.h"
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
}  // namespace

int RunIngest(int argc, char** argv) try {
    auto args = ParseArgs(argc, argv);
    auto get = [&](const std::string& key, const std::string& def = "") {
        auto it = args.find(key);
        return it != args.end() ? it->second : def;
    };

    std::string dbPath = get("db");
    std::string faceIndexPath = get("index");
    std::string clothingIndexPath = get("clothing-index", "coser_clothing.index");
    std::string modelsDir = get("models-dir");
    std::string personName = get("person");
    std::string imagePath = get("image");

    if (dbPath.empty() || faceIndexPath.empty() || modelsDir.empty() || personName.empty() || imagePath.empty()) {
        std::cerr << "Usage: coser_cli ingest --db <path> --index <path> --models-dir <dir> --person \"<Name>\" --image <path> "
                     "[--clothing-index <path>]\n"
                     "  <models-dir> must contain face/, pose/, clothing/, clip/ subdirectories.\n";
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
        std::cerr << "Failed to load/create face index: " << faceIndexPath << "\n";
        return 1;
    }

    FaissFlatIpIndex clothingIndex(384);
    if (!clothingIndex.Load(clothingIndexPath)) {
        std::cerr << "Failed to load/create clothing index: " << clothingIndexPath << "\n";
        return 1;
    }

    PHashIndex phashIndex;
    phashIndex.LoadFrom(store.LoadAllPHashes());

    ScrfdFaceDetector faceDetector(modelsDir + "/face/det_10g.onnx");
    ArcFaceExtractor faceExtractor(modelsDir + "/face/w600k_r50.onnx");
    PhotoAuthenticityChecker authChecker(get("clip-model", modelsDir + "/clip/clip_vision_quantized.onnx"));
    FaceRecognitionPipeline facePipeline(&faceDetector, &faceExtractor, &faceIndex, &store, &authChecker);

    YoloPoseDetector poseDetector(modelsDir + "/pose/yolov8n-pose.onnx");
    DinoV2Extractor dinoExtractor(modelsDir + "/clothing/dinov2_vits14.onnx");
    ClothingRecognitionPipeline clothingPipeline(&poseDetector, &dinoExtractor, &clothingIndex, &store);

    PHasher hasher;
    OrbCropMatcher orbMatcher;
    ImageMatchPipeline imageMatchPipeline(&hasher, &orbMatcher, &phashIndex, &store);

    ResultFusion fusion;
    RetrievalOrchestrator orchestrator(&facePipeline, &clothingPipeline, &imageMatchPipeline, &store, &fusion);

    int64_t personId = store.InsertPerson(personName);
    if (personId < 0) {
        std::cerr << "Failed to insert person: " << store.GetLastError() << "\n";
        return 1;
    }

    ImageRow row;
    row.person_id = personId;
    row.file_path = imagePath;
    row.width = image.cols;
    row.height = image.rows;
    row.ingest_status = "pending";
    int64_t imageId = store.InsertImage(row);
    if (imageId < 0) {
        std::cerr << "Failed to insert image: " << store.GetLastError() << "\n";
        return 1;
    }

    IngestResult result = orchestrator.IngestImage(image, imageId);
    if (!result.ok) {
        std::cerr << "Ingest failed: " << result.reason << " in " << imagePath << "\n";
        return 1;
    }
    if (!result.reason.empty()) {
        std::cerr << "Ingest committed with partial route failures: " << result.reason << "\n";
    }

    if (!faceIndex.Save(faceIndexPath)) {
        std::cerr << "Failed to save face index: " << faceIndexPath << "\n";
        return 1;
    }
    if (!clothingIndex.Save(clothingIndexPath)) {
        std::cerr << "Failed to save clothing index: " << clothingIndexPath << "\n";
        return 1;
    }

    std::cout << "Ingested person_id=" << personId << " image_id=" << imageId << "\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << "\n";
    return 2;
} catch (...) {
    std::cerr << "Unhandled unknown exception\n";
    return 2;
}
