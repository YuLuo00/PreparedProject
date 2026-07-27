#pragma once
#include "../L2/MetadataStore.h"
#include "../L2/PHashIndex.h"
#include "../L3/PHasher.h"
#include "../L3/OrbCropMatcher.h"
#include "FaceRecognitionPipeline.h"  // reuse PipelineMatch / IngestResult
#include <opencv2/core.hpp>

namespace coser {

class ImageMatchPipeline {
public:
    ImageMatchPipeline(PHasher* hasher, OrbCropMatcher* orbMatcher, PHashIndex* phashIndex,
                        MetadataStore* store, int maxHammingDist = 10, int minOrbInliers = 15);

    /// Computes pHash and registers it. Practically never fails (image already decoded upstream).
    IngestResult Ingest(const cv::Mat& image, int64_t imageId);

    /// pHash coarse filter -> ORB precise confirmation on the short list.
    std::vector<PipelineMatch> Query(const cv::Mat& image);

private:
    PHasher* hasher_;
    OrbCropMatcher* orbMatcher_;
    PHashIndex* phashIndex_;
    MetadataStore* store_;
    int maxHammingDist_;
    int minOrbInliers_;
};

}  // namespace coser
