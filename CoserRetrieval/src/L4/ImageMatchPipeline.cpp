#include "ImageMatchPipeline.h"
#include <opencv2/imgcodecs.hpp>

namespace coser {

ImageMatchPipeline::ImageMatchPipeline(PHasher* hasher, OrbCropMatcher* orbMatcher, PHashIndex* phashIndex,
                                        MetadataStore* store, int maxHammingDist, int minOrbInliers)
    : hasher_(hasher), orbMatcher_(orbMatcher), phashIndex_(phashIndex), store_(store),
      maxHammingDist_(maxHammingDist), minOrbInliers_(minOrbInliers) {}

IngestResult ImageMatchPipeline::Ingest(const cv::Mat& image, int64_t imageId) {
    uint64_t hash = hasher_->Compute(image);
    phashIndex_->Add(imageId, hash);
    if (!store_->UpdateImagePHash(imageId, static_cast<int64_t>(hash))) {
        return {false, "phash metadata write failed"};
    }
    return {true, ""};
}

std::vector<PipelineMatch> ImageMatchPipeline::Query(const cv::Mat& image) {
    std::vector<PipelineMatch> results;

    uint64_t queryHash = hasher_->Compute(image);
    auto candidates = phashIndex_->Search(queryHash, maxHammingDist_);

    for (const auto& candidate : candidates) {
        auto filePath = store_->GetImageFilePath(candidate.image_id);
        if (!filePath) continue;

        cv::Mat candidateImage = cv::imread(*filePath);
        if (candidateImage.empty()) continue;

        int inliers = orbMatcher_->MatchScore(image, candidateImage);
        if (inliers < minOrbInliers_) continue;

        auto personRow = store_->GetPersonByImageId(candidate.image_id);
        if (!personRow) continue;

        PipelineMatch m;
        m.image_id = personRow->image_id;
        m.person_id = personRow->person_id;
        m.display_name = personRow->display_name;
        m.score = static_cast<float>(inliers);
        results.push_back(m);
    }
    return results;
}

}  // namespace coser
