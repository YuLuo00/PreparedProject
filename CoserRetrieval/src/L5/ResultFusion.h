#pragma once
#include "../L4/FaceRecognitionPipeline.h"  // reuse PipelineMatch
#include <vector>

namespace coser {

struct FusionConfig {
    float face_weight = 0.7f;
    float clothing_weight = 0.3f;
    // NOTE: exact matches carry OrbCropMatcher's raw inlier count as their score
    // (not a normalized 0-1 confidence), so in practice this threshold is already
    // satisfied by anything ImageMatchPipeline::Query returns (it pre-filters via
    // its own minOrbInliers). Kept configurable for when that score is normalized.
    float exact_score_threshold = 0.85f;
};

/// Implements HLD 5.3 fusion rules: exact-match short-circuit, otherwise
/// person_id-weighted aggregation of face + clothing matches.
class ResultFusion {
public:
    explicit ResultFusion(const FusionConfig& config = FusionConfig());

    std::vector<PipelineMatch> Fuse(const std::vector<PipelineMatch>& exactMatches,
                                     const std::vector<PipelineMatch>& faceMatches,
                                     const std::vector<PipelineMatch>& clothingMatches) const;

private:
    FusionConfig config_;
};

}  // namespace coser
