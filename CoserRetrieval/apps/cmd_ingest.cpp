#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <opencv2/imgcodecs.hpp>

#include "commands.h"
#include "../src/L1/FileHasher.h"
#include "../src/L2/MetadataStore.h"
#include "../src/L2/FaissFlatIpIndex.h"
#include "../src/L2/PHashIndex.h"
#include "../src/L3/ScrfdFaceDetector.h"
#include "../src/L3/ArcFaceExtractor.h"
#include "../src/L3/AdaFaceExtractor.h"
#include "../src/L3/IEmbeddingExtractor.h"
#include "../src/L3/PhotoAuthenticityChecker.h"
#include "../src/L3/YoloPoseDetector.h"
#include "../src/L3/HumanParsingSegmenter.h"
#include "../src/L3/DinoV2Extractor.h"
#include "../src/L3/PHasher.h"
#include "../src/L3/OrbCropMatcher.h"
#include "../src/L4/FaceRecognitionPipeline.h"
#include "../src/L4/ClothingRecognitionPipeline.h"
#include "../src/L4/ImageMatchPipeline.h"
#include "../src/L5/ResultFusion.h"
#include "../src/L5/RetrievalOrchestrator.h"

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

bool IsImageFile(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".webp";
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
    std::string roleName = get("role");
    std::string faceModel = get("face-model", "arcface");
    std::string imagePath = get("image");
    std::string dirPath = get("dir");
    std::string filePrefix = get("file-prefix");

    bool includeFace = !personName.empty();
    if (faceModel != "arcface" && faceModel != "adaface") {
        std::cerr << "Unsupported --face-model: " << faceModel << " (use arcface or adaface)\n";
        return 1;
    }
    if (dbPath.empty() || (includeFace && faceIndexPath.empty()) || modelsDir.empty() || (imagePath.empty() == dirPath.empty()) ||
        (personName.empty() && roleName.empty())) {
        std::cerr << "Usage: coser_cli ingest --db <path> [--index <path>] --models-dir <dir> "
                     "(--image <path> | --dir <directory>) "
                     "[--person \"<Name>\"] [--role \"<Character>\"] "
                     "[--clothing-index <path>]\n"
                     "  At least one of --person or --role is required.\n"
                     "  --dir recursively imports supported image files with one shared person/role label.\n"
                     "  --file-prefix filters directory imports by filename prefix.\n"
                     "  --index is required only when --person is supplied.\n"
                     "  --face-model arcface|adaface selects the face embedding model (default: arcface).\n"
                     "  <models-dir> must contain face/, pose/, clothing/, clip/ subdirectories.\n";
        return 1;
    }

    std::vector<fs::path> imagePaths;
    if (!imagePath.empty()) {
        imagePaths.emplace_back(imagePath);
    } else {
        if (!fs::is_directory(dirPath)) {
            std::cerr << "Not a directory: " << dirPath << "\n";
            return 1;
        }
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (!entry.is_regular_file() || !IsImageFile(entry.path())) continue;
            std::string filename = entry.path().filename().string();
            if (!filePrefix.empty() && filename.rfind(filePrefix, 0) != 0) continue;
            imagePaths.push_back(entry.path());
        }
        std::sort(imagePaths.begin(), imagePaths.end());
        if (imagePaths.empty()) {
            std::cerr << "No supported image files found in: " << dirPath << "\n";
            return 1;
        }
    }

    MetadataStore store;
    if (!store.Open(dbPath)) {
        std::cerr << "Failed to open db: " << store.GetLastError() << "\n";
        return 1;
    }

    FaissFlatIpIndex clothingIndex(384);
    if (!clothingIndex.Load(clothingIndexPath)) {
        std::cerr << "Failed to load/create clothing index: " << clothingIndexPath << "\n";
        return 1;
    }

    PHashIndex phashIndex;
    phashIndex.LoadFrom(store.LoadAllPHashes());

    YoloPoseDetector poseDetector(modelsDir + "/pose/yolov8n-pose.onnx");
    HumanParsingSegmenter humanParser(get("human-parsing-model",
        modelsDir + "/clothing/human_parsing_lip_resnet101.onnx"));
    DinoV2Extractor dinoExtractor(modelsDir + "/clothing/dinov2_vits14.onnx");
    ClothingRecognitionPipeline clothingPipeline(&poseDetector, &dinoExtractor, &clothingIndex, &store,
                                                 &humanParser);

    PHasher hasher;
    OrbCropMatcher orbMatcher;
    ImageMatchPipeline imageMatchPipeline(&hasher, &orbMatcher, &phashIndex, &store);

    std::unique_ptr<FaissFlatIpIndex> faceIndex;
    std::unique_ptr<ScrfdFaceDetector> faceDetector;
    std::unique_ptr<IEmbeddingExtractor> faceExtractor;
    std::unique_ptr<PhotoAuthenticityChecker> authChecker;
    std::unique_ptr<FaceRecognitionPipeline> facePipeline;
    if (includeFace) {
        faceIndex = std::make_unique<FaissFlatIpIndex>(512);
        if (!faceIndex->Load(faceIndexPath)) {
            std::cerr << "Failed to load/create face index: " << faceIndexPath << "\n";
            return 1;
        }
        faceDetector = std::make_unique<ScrfdFaceDetector>(modelsDir + "/face/det_10g.onnx");
        std::string faceModelPath = get("face-model-path", faceModel == "adaface"
            ? modelsDir + "/face/adaface_ir18_webface4m.onnx"
            : modelsDir + "/face/w600k_r50.onnx");
        if (faceModel == "adaface") {
            faceExtractor = std::make_unique<AdaFaceExtractor>(faceModelPath);
        } else {
            faceExtractor = std::make_unique<ArcFaceExtractor>(faceModelPath);
        }
        authChecker = std::make_unique<PhotoAuthenticityChecker>(
            get("clip-model", modelsDir + "/clip/clip_vision_quantized.onnx"));
        facePipeline = std::make_unique<FaceRecognitionPipeline>(
            faceDetector.get(), faceExtractor.get(), faceIndex.get(), &store, authChecker.get());
    }

    ResultFusion fusion;
    RetrievalOrchestrator orchestrator(
        facePipeline.get(), &clothingPipeline, &imageMatchPipeline, &store, &fusion);

    int64_t personId = 0;
    if (!personName.empty()) {
        personId = store.InsertPerson(personName);
        if (personId < 0) {
            std::cerr << "Failed to insert person: " << store.GetLastError() << "\n";
            return 1;
        }
    }

    int64_t roleId = 0;
    if (!roleName.empty()) {
        roleId = store.InsertRole(roleName);
        if (roleId < 0) {
            std::cerr << "Failed to insert role: " << store.GetLastError() << "\n";
            return 1;
        }
    }

    int succeeded = 0;
    int skipped = 0;
    int failed = 0;
    for (const auto& path : imagePaths) {
        std::string pathString = path.string();
        std::string hashError;
        auto md5 = ComputeFileMd5(path, &hashError);
        if (!md5) {
            std::cerr << "Failed to calculate MD5: " << hashError << " in " << pathString << "\n";
            ++failed;
            continue;
        }
        auto duplicate = store.FindImageByMd5(*md5);
        if (duplicate) {
            std::cout << "Duplicate skipped: image_id=" << duplicate->image_id
                      << " existing_path=" << duplicate->file_path
                      << " path=" << pathString << "\n";
            ++skipped;
            continue;
        }

        cv::Mat image = cv::imread(pathString);
        if (image.empty()) {
            std::cerr << "Failed to read image: " << pathString << "\n";
            ++failed;
            continue;
        }

        ImageRow row;
        row.person_id = personId;
        row.role_id = roleId;
        row.file_path = pathString;
        row.md5 = *md5;
        row.width = image.cols;
        row.height = image.rows;
        row.ingest_status = "pending";
        int64_t imageId = store.InsertImage(row);
        if (imageId < 0) {
            std::cerr << "Failed to insert image: " << store.GetLastError() << " in " << pathString << "\n";
            ++failed;
            continue;
        }

        IngestResult result = orchestrator.IngestImage(image, imageId, includeFace);
        if (!result.ok) {
            std::cerr << "Ingest failed: " << result.reason << " in " << pathString << "\n";
            ++failed;
            continue;
        }
        if (!result.reason.empty()) {
            std::cerr << "Ingest committed with partial route failures: " << result.reason
                      << " in " << pathString << "\n";
        }

        std::cout << "Ingested";
        if (personId > 0) std::cout << " person_id=" << personId;
        if (roleId > 0) std::cout << " role_id=" << roleId << " role=\"" << roleName << "\"";
        std::cout << " image_id=" << imageId << " md5=" << *md5 << " path=" << pathString << "\n";
        ++succeeded;
    }

    if (faceIndex && !faceIndex->Save(faceIndexPath)) {
        std::cerr << "Failed to save face index: " << faceIndexPath << "\n";
        return 1;
    }
    if (!clothingIndex.Save(clothingIndexPath)) {
        std::cerr << "Failed to save clothing index: " << clothingIndexPath << "\n";
        return 1;
    }

    if (!dirPath.empty()) {
        std::cout << "Batch complete: total=" << imagePaths.size()
                  << " succeeded=" << succeeded << " skipped=" << skipped << " failed=" << failed << "\n";
    }
    return failed == 0 ? 0 : 1;
} catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << "\n";
    return 2;
} catch (...) {
    std::cerr << "Unhandled unknown exception\n";
    return 2;
}
