#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <tbb/parallel_pipeline.h>
#include <opencv2/imgcodecs.hpp>

#include "commands.h"
#include "../src/L1/FileHasher.h"
#include "../src/L1/TaskProgress.h"
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
    std::string taskId = get("task-id", "ingest");
    std::string skipDuplicatesValue = get("skip-duplicates", "true");
    std::transform(skipDuplicatesValue.begin(), skipDuplicatesValue.end(), skipDuplicatesValue.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (skipDuplicatesValue != "true" && skipDuplicatesValue != "false") {
        std::cerr << "--skip-duplicates must be true or false\n";
        return 1;
    }
    const bool skipDuplicates = skipDuplicatesValue == "true";

    bool includeFace = !personName.empty();
    if (faceModel != "arcface" && faceModel != "adaface") {
        std::cerr << "Unsupported --face-model: " << faceModel << " (use arcface or adaface)\n";
        return 1;
    }
    if (dbPath.empty() || ((includeFace || !skipDuplicates) && faceIndexPath.empty()) || modelsDir.empty() || (imagePath.empty() == dirPath.empty()) ||
        (personName.empty() && roleName.empty())) {
        std::cerr << "Usage: coser_cli ingest --db <path> [--index <path>] --models-dir <dir> "
                     "(--image <path> | --dir <directory>) "
                     "[--person \"<Name>\"] [--role \"<Character>\"] "
                     "[--clothing-index <path>]\n"
                     "  At least one of --person or --role is required.\n"
                     "  --dir recursively imports supported image files with one shared person/role label.\n"
                     "  --file-prefix filters directory imports by filename prefix.\n"
                     "  --index is required only when --person is supplied.\n"
                     "  --skip-duplicates true|false skips matching MD5 files (default: true); false replaces the old record.\n"
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
    if (includeFace || !skipDuplicates) {
        faceIndex = std::make_unique<FaissFlatIpIndex>(512);
        if (!faceIndex->Load(faceIndexPath)) {
            std::cerr << "Failed to load/create face index: " << faceIndexPath << "\n";
            return 1;
        }
    }
    if (includeFace) {
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

    struct BatchItem {
        fs::path path;
        std::string pathString;
        std::string md5;
        std::string error;
        cv::Mat image;
        int64_t imageId = 0;
        int64_t replacedImageId = 0;
        IngestResult result{false, ""};
        enum class State { Ready, Skipped, Failed, PendingInference, Complete } state = State::Ready;
    };

    std::atomic<size_t> nextPath{0};
    int succeeded = 0;
    int skipped = 0;
    int failed = 0;
    std::unordered_set<int64_t> batchImageIds;
    std::mutex outputMutex;
    auto reportProgress = [&](TaskStatus status, const std::string& path = "",
                              const std::string& message = "") {
        TaskProgressHub::Instance().Notify({taskId, "ingest", status,
            static_cast<int>(imagePaths.size()), succeeded + skipped + failed,
            succeeded, skipped, failed, path, message});
    };
    reportProgress(TaskStatus::Started, "", "Batch ingest started");

    // Seven in-flight tokens bound memory and ONNX CPU contention. A completed
    // token immediately frees a slot for the next source image.
    tbb::parallel_pipeline(7,
        tbb::make_filter<void, BatchItem>(tbb::filter_mode::serial_in_order,
            [&](tbb::flow_control& control) {
                const size_t index = nextPath.fetch_add(1);
                if (index >= imagePaths.size()) {
                    control.stop();
                    return BatchItem{};
                }
                BatchItem item;
                item.path = imagePaths[index];
                item.pathString = item.path.string();
                return item;
            }) &
        tbb::make_filter<BatchItem, BatchItem>(tbb::filter_mode::parallel,
            [&](BatchItem item) {
                std::string hashError;
                auto md5 = ComputeFileMd5(item.path, &hashError);
                if (!md5) {
                    item.error = "Failed to calculate MD5: " + hashError;
                    item.state = BatchItem::State::Failed;
                    return item;
                }
                item.md5 = *md5;
                item.image = cv::imread(item.pathString);
                if (item.image.empty()) {
                    item.error = "Failed to read image";
                    item.state = BatchItem::State::Failed;
                }
                return item;
            }) &
        tbb::make_filter<BatchItem, BatchItem>(tbb::filter_mode::serial_in_order,
            [&](BatchItem item) {
                if (item.state == BatchItem::State::Failed) return item;
                auto duplicate = store.FindImageByMd5(item.md5);
                if (duplicate) {
                    if (skipDuplicates || batchImageIds.count(duplicate->image_id) != 0) {
                        item.error = "Duplicate skipped: image_id=" + std::to_string(duplicate->image_id) +
                            " existing_path=" + duplicate->file_path;
                        item.state = BatchItem::State::Skipped;
                        return item;
                    }
                    RemovedImageEmbeddings removed;
                    if (!store.DeleteImageAndEmbeddings(duplicate->image_id, removed)) {
                        item.error = "Failed to replace duplicate image_id=" + std::to_string(duplicate->image_id) +
                            ": " + store.GetLastError();
                        item.state = BatchItem::State::Failed;
                        return item;
                    }
                    for (int64_t embeddingId : removed.face_embedding_ids) faceIndex->Remove(embeddingId);
                    for (int64_t embeddingId : removed.clothing_embedding_ids) clothingIndex.Remove(embeddingId);
                    phashIndex.Remove(duplicate->image_id);
                    item.replacedImageId = duplicate->image_id;
                }
                ImageRow row;
                row.person_id = personId;
                row.role_id = roleId;
                row.file_path = item.pathString;
                row.md5 = item.md5;
                row.width = item.image.cols;
                row.height = item.image.rows;
                row.ingest_status = "pending";
                item.imageId = store.InsertImage(row);
                if (item.imageId < 0) {
                    item.error = "Failed to insert image: " + store.GetLastError();
                    item.state = BatchItem::State::Failed;
                } else {
                    batchImageIds.insert(item.imageId);
                    item.state = BatchItem::State::PendingInference;
                }
                return item;
            }) &
        tbb::make_filter<BatchItem, BatchItem>(tbb::filter_mode::parallel,
            [&](BatchItem item) {
                if (item.state != BatchItem::State::PendingInference) return item;
                item.result = orchestrator.IngestImage(item.image, item.imageId, includeFace);
                item.state = item.result.ok ? BatchItem::State::Complete : BatchItem::State::Failed;
                if (!item.result.ok) item.error = "Ingest failed: " + item.result.reason;
                return item;
            }) &
        tbb::make_filter<BatchItem, void>(tbb::filter_mode::serial_in_order,
            [&](BatchItem item) {
                std::lock_guard<std::mutex> lock(outputMutex);
                if (item.state == BatchItem::State::Skipped) {
                    std::cout << item.error << " path=" << item.pathString << "\n";
                    ++skipped;
                } else if (item.state == BatchItem::State::Failed) {
                    std::cerr << item.error << " in " << item.pathString << "\n";
                    ++failed;
                } else {
                    if (!item.result.reason.empty()) {
                        std::cerr << "Ingest committed with partial route failures: " << item.result.reason
                                  << " in " << item.pathString << "\n";
                    }
                    std::cout << "Ingested";
                    if (item.replacedImageId > 0) std::cout << " replaced_image_id=" << item.replacedImageId;
                    if (personId > 0) std::cout << " person_id=" << personId;
                    if (roleId > 0) std::cout << " role_id=" << roleId << " role=\"" << roleName << "\"";
                    std::cout << " image_id=" << item.imageId << " md5=" << item.md5
                              << " path=" << item.pathString << "\n";
                    ++succeeded;
                }
                reportProgress(TaskStatus::Running, item.pathString, item.error);
            }));

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
    const bool success = failed == 0;
    reportProgress(success ? TaskStatus::Completed : TaskStatus::Failed, "",
                   success ? "Batch ingest completed" : "Batch ingest completed with failures");
    return success ? 0 : 1;
} catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << "\n";
    return 2;
} catch (...) {
    std::cerr << "Unhandled unknown exception\n";
    return 2;
}
