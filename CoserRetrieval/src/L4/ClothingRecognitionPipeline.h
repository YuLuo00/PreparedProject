#pragma once
#include "../L2/IVectorIndex.h"
#include "../L2/MetadataStore.h"
#include "../L3/IPoseDetector.h"
#include "../L3/IEmbeddingExtractor.h"
#include "FaceRecognitionPipeline.h"  // reuse PipelineMatch / IngestResult
#include <opencv2/core.hpp>

namespace coser {

class ClothingRecognitionPipeline {
public:
    ClothingRecognitionPipeline(IPoseDetector* poseDetector,
                                 IEmbeddingExtractor* extractor,
                                 IVectorIndex* index,
                                 MetadataStore* store);

    /// 检测最高分人体框 -> 整框裁剪 -> 提取 -> 写入 index/metadata
    IngestResult Ingest(const cv::Mat& image, int64_t imageId);

    std::vector<PipelineMatch> Query(const cv::Mat& image, int topK);

private:
    IPoseDetector* poseDetector_;
    IEmbeddingExtractor* extractor_;
    IVectorIndex* index_;
    MetadataStore* store_;
};

}  // namespace coser
