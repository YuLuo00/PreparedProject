#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <opencv2/imgcodecs.hpp>

#include "commands.h"
#include "../src/L2/MetadataStore.h"
#include "../src/L2/FaissFlatIpIndex.h"
#include "../src/L2/PHashIndex.h"
#include "../src/L3/ScrfdFaceDetector.h"
#include "../src/L3/ArcFaceExtractor.h"
#include "../src/L3/AdaFaceExtractor.h"
#include "../src/L3/IEmbeddingExtractor.h"
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

std::string CsvEscape(const std::string& value) {
    std::string escaped = "\"";
    for (char c : value) {
        if (c == '\"') escaped += "\"\"";
        else escaped += c;
    }
    return escaped + "\"";
}

QueryMode ParseMode(const std::string& mode) {
    if (mode == "exact") return QueryMode::ExactOnly;
    if (mode == "face") return QueryMode::FaceOnly;
    if (mode == "clothing") return QueryMode::ClothingOnly;
    if (mode == "role") return QueryMode::RoleOnly;
    return QueryMode::AllLinked;
}

void PrintMatches(const std::string& label, const std::vector<PipelineMatch>& matches, MetadataStore* store) {
    std::cout << "-- " << label << " (" << matches.size() << ") --\n";
    for (const auto& m : matches) {
        std::cout << "  person_id=" << m.person_id
                   << " display_name=" << m.display_name
                   << " image_id=" << m.image_id
                   << " score=" << m.score;
        if (store) {
            auto path = store->GetImageFilePath(m.image_id);
            if (path) std::cout << " reference_path=" << *path;
        }
        std::cout << "\n";
    }
}

void PrintRoleMatches(const std::vector<PipelineMatch>& matches, MetadataStore* store) {
    std::cout << "-- role (" << matches.size() << ") --\n";
    for (const auto& m : matches) {
        std::cout << "  role_id=" << m.role_id
                  << " role_name=" << m.role_name
                  << " reference_image_id=" << m.image_id
                  << " score=" << m.score;
        if (store) {
            auto path = store->GetImageFilePath(m.image_id);
            if (path) std::cout << " reference_path=" << *path;
        }
        std::cout << "\n";
    }
}

const PipelineMatch* TopMatch(QueryMode mode, const QueryResponse& response) {
    const std::vector<PipelineMatch>* matches = nullptr;
    switch (mode) {
        case QueryMode::ExactOnly: matches = &response.exactMatches; break;
        case QueryMode::FaceOnly: matches = &response.faceMatches; break;
        case QueryMode::ClothingOnly: matches = &response.clothingMatches; break;
        case QueryMode::RoleOnly: matches = &response.roleMatches; break;
        case QueryMode::AllLinked: matches = &response.fused; break;
    }
    return matches && !matches->empty() ? &matches->front() : nullptr;
}

void PrintResponse(QueryMode mode, const QueryResponse& response, MetadataStore* store) {
    if (mode == QueryMode::RoleOnly) {
        PrintRoleMatches(response.roleMatches, store);
    } else {
        PrintMatches("exact", response.exactMatches, store);
        PrintMatches("face", response.faceMatches, store);
        PrintMatches("clothing", response.clothingMatches, store);
        PrintMatches("fused", response.fused, store);
    }
    if (response.exactMatches.empty() && response.faceMatches.empty() && response.clothingMatches.empty() &&
        response.roleMatches.empty()) {
        std::cout << "No matches (no face/person detected or all indices empty)\n";
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
    std::string dirPath = get("dir");
    std::string reportPath = get("report");
    std::string faceModel = get("face-model", "arcface");
    int topK = std::stoi(get("topk", "5"));
    QueryMode mode = ParseMode(get("mode", "all"));

    bool roleOnly = mode == QueryMode::RoleOnly;
    if (faceModel != "arcface" && faceModel != "adaface") {
        std::cerr << "Unsupported --face-model: " << faceModel << " (use arcface or adaface)\n";
        return 1;
    }
    if (dbPath.empty() || (!roleOnly && faceIndexPath.empty()) || modelsDir.empty() ||
        (imagePath.empty() == dirPath.empty())) {
        std::cerr << "Usage: coser_cli query --db <path> [--index <path>] --models-dir <dir> "
                     "(--image <path> | --dir <directory>) --topk <N> "
                     "[--mode exact|face|clothing|role|all] [--clothing-index <path>] [--report <csv>]\n"
                     "  Use --mode role to match only face-masked clothing and hairstyle against --role labels.\n"
                     "  --face-model arcface|adaface selects the face embedding model (default: arcface).\n"
                     "  --dir recursively queries supported image files with models loaded once; --report writes top-1 rows to CSV.\n"
                     "  <models-dir> must contain face/, pose/, clothing/ subdirectories.\n";
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
            if (entry.is_regular_file() && IsImageFile(entry.path())) imagePaths.push_back(entry.path());
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
        std::cerr << "Failed to load clothing index: " << clothingIndexPath << "\n";
        return 1;
    }

    YoloPoseDetector poseDetector(modelsDir + "/pose/yolov8n-pose.onnx");
    DinoV2Extractor dinoExtractor(modelsDir + "/clothing/dinov2_vits14.onnx");
    ClothingRecognitionPipeline clothingPipeline(&poseDetector, &dinoExtractor, &clothingIndex, &store);

    std::unique_ptr<FaissFlatIpIndex> faceIndex;
    std::unique_ptr<ScrfdFaceDetector> faceDetector;
    std::unique_ptr<IEmbeddingExtractor> faceExtractor;
    std::unique_ptr<FaceRecognitionPipeline> facePipeline;
    std::unique_ptr<PHashIndex> phashIndex;
    std::unique_ptr<PHasher> hasher;
    std::unique_ptr<OrbCropMatcher> orbMatcher;
    std::unique_ptr<ImageMatchPipeline> imageMatchPipeline;

    if (!roleOnly) {
        faceIndex = std::make_unique<FaissFlatIpIndex>(512);
        if (!faceIndex->Load(faceIndexPath)) {
            std::cerr << "Failed to load face index: " << faceIndexPath << "\n";
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
        facePipeline = std::make_unique<FaceRecognitionPipeline>(
            faceDetector.get(), faceExtractor.get(), faceIndex.get(), &store);

        phashIndex = std::make_unique<PHashIndex>();
        phashIndex->LoadFrom(store.LoadAllPHashes());
        hasher = std::make_unique<PHasher>();
        orbMatcher = std::make_unique<OrbCropMatcher>();
        imageMatchPipeline = std::make_unique<ImageMatchPipeline>(
            hasher.get(), orbMatcher.get(), phashIndex.get(), &store);
    }

    ResultFusion fusion;
    RetrievalOrchestrator orchestrator(
        facePipeline.get(), &clothingPipeline, imageMatchPipeline.get(), &store, &fusion);

    std::ofstream report;
    if (!reportPath.empty()) {
        report.open(reportPath, std::ios::binary);
        if (!report) {
            std::cerr << "Failed to open report: " << reportPath << "\n";
            return 1;
        }
        report << "file_path,mode,top1_label,top1_image_id,top1_reference_path,top1_score,matched\n";
    }

    int processed = 0;
    int unreadable = 0;
    int matched = 0;
    for (const auto& path : imagePaths) {
        std::string pathString = path.string();
        cv::Mat image = cv::imread(pathString);
        if (image.empty()) {
            std::cerr << "Failed to read image: " << pathString << "\n";
            ++unreadable;
            if (report) report << CsvEscape(pathString) << ',' << CsvEscape(get("mode", "all"))
                               << ",,,,,false\n";
            continue;
        }

        QueryRequest request;
        request.image = image;
        request.mode = mode;
        request.topK = topK;
        QueryResponse response = orchestrator.Query(request);

        if (!dirPath.empty()) std::cout << "=== Query: " << pathString << " ===\n";
        PrintResponse(mode, response, &store);

        const PipelineMatch* top = TopMatch(mode, response);
        if (top) ++matched;
        if (report) {
            std::string label = top ? (mode == QueryMode::RoleOnly ? top->role_name : top->display_name) : "";
            std::string referencePath;
            if (top) {
                auto path = store.GetImageFilePath(top->image_id);
                if (path) referencePath = *path;
            }
            report << CsvEscape(pathString) << ',' << CsvEscape(get("mode", "all")) << ','
                   << CsvEscape(label) << ',';
            if (top) report << top->image_id;
            report << ',' << CsvEscape(referencePath) << ',';
            if (top) report << top->score;
            report << ',' << (top ? "true" : "false") << '\n';
        }
        ++processed;
    }

    if (!dirPath.empty()) {
        std::cout << "Batch complete: total=" << imagePaths.size()
                  << " processed=" << processed << " matched=" << matched
                  << " unreadable=" << unreadable << "\n";
    }
    return unreadable == 0 ? 0 : 1;
} catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << "\n";
    return 2;
} catch (...) {
    std::cerr << "Unhandled unknown exception\n";
    return 2;
}
