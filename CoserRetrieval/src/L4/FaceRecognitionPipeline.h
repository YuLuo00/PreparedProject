#pragma once
#include "../L2/IVectorIndex.h"
#include "../L2/MetadataStore.h"
#include "../L3/IDetector.h"
#include "../L3/IEmbeddingExtractor.h"
#include "../L3/PhotoAuthenticityChecker.h"
#include <memory>
#include <opencv2/core.hpp>

namespace coser {

struct PipelineMatch {
    int64_t image_id = 0;
    int64_t person_id = 0;
    std::string display_name;
    int64_t role_id = 0;
    std::string role_name;
    float score = 0.0f;
};

struct IngestResult {
    bool ok = false;
    std::string reason;
};

class FaceRecognitionPipeline {
public:
    FaceRecognitionPipeline(IDetector* detector,
                             IEmbeddingExtractor* extractor,
                             IVectorIndex* index,
                             MetadataStore* store,
                             PhotoAuthenticityChecker* authChecker = nullptr);

    /// 检测最高分人脸 -> (可选)真人照过滤 -> 对齐 -> 提取 -> 写入 index/metadata
    IngestResult Ingest(const cv::Mat& image, int64_t imageId);

    std::vector<PipelineMatch> Query(const cv::Mat& image, int topK);

private:
    IDetector* detector_;
    IEmbeddingExtractor* extractor_;
    IVectorIndex* index_;
    MetadataStore* store_;
    PhotoAuthenticityChecker* authChecker_;
};

}  // namespace coser
