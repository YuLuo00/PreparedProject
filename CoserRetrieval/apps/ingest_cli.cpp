#include <iostream>
#include <string>
#include <unordered_map>
#include <opencv2/imgcodecs.hpp>

#include "../src/L2/MetadataStore.h"
#include "../src/L2/FaissFlatIpIndex.h"
#include "../src/L3/ScrfdFaceDetector.h"
#include "../src/L3/ArcFaceExtractor.h"
#include "../src/L3/PhotoAuthenticityChecker.h"
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
    std::string personName = get("person");
    std::string imagePath = get("image");

    if (dbPath.empty() || indexPath.empty() || modelsDir.empty() || personName.empty() || imagePath.empty()) {
        std::cerr << "Usage: ingest_cli --db <path> --index <path> --models-dir <dir> --person \"<Name>\" --image <path>\n";
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
        std::cerr << "Failed to load/create index: " << indexPath << "\n";
        return 1;
    }

    ScrfdFaceDetector detector(modelsDir + "/det_10g.onnx");
    ArcFaceExtractor extractor(modelsDir + "/w600k_r50.onnx");
    PhotoAuthenticityChecker authChecker(get("clip-model", modelsDir + "/../clip/clip_vision_quantized.onnx"));
    FaceRecognitionPipeline pipeline(&detector, &extractor, &index, &store, &authChecker);

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

    IngestResult result = pipeline.Ingest(image, imageId);
    store.UpdateImageStatus(imageId, result.ok ? "committed" : "failed", result.ok ? "" : result.reason);
    if (!result.ok) {
        std::cerr << "Ingest failed: " << result.reason << " in " << imagePath << "\n";
        return 1;
    }

    if (!index.Save(indexPath)) {
        std::cerr << "Failed to save index: " << indexPath << "\n";
        return 1;
    }

    std::cout << "Ingested person_id=" << personId << " image_id=" << imageId << "\n";
    return 0;
}
