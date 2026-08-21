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

    /// Detect the highest-score person, mask facial pixels, then extract the
    /// clothing/hair appearance embedding and write it to index/metadata.
    IngestResult Ingest(const cv::Mat& image, int64_t imageId);

    std::vector<PipelineMatch> Query(const cv::Mat& image, int topK);

    /// Role retrieval only uses the face-masked clothing/hair index. Results
    /// without an ingest-time role label are ignored and duplicate references
    /// for the same role are collapsed to their best score.
    std::vector<PipelineMatch> QueryRoles(const cv::Mat& image, int topK);

private:
    IPoseDetector* poseDetector_;
    IEmbeddingExtractor* extractor_;
    IVectorIndex* index_;
    MetadataStore* store_;
};

}  // namespace coser
