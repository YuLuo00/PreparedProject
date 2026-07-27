#include "ResultFusion.h"
#include <algorithm>
#include <unordered_map>

namespace coser {

ResultFusion::ResultFusion(const FusionConfig& config) : config_(config) {}

std::vector<PipelineMatch> ResultFusion::Fuse(const std::vector<PipelineMatch>& exactMatches,
                                               const std::vector<PipelineMatch>& faceMatches,
                                               const std::vector<PipelineMatch>& clothingMatches) const {
    std::vector<PipelineMatch> sortedExact = exactMatches;
    std::sort(sortedExact.begin(), sortedExact.end(),
        [](const PipelineMatch& a, const PipelineMatch& b) { return a.score > b.score; });

    if (!sortedExact.empty() && sortedExact.front().score >= config_.exact_score_threshold) {
        return sortedExact;
    }

    struct Agg {
        float score = 0.0f;
        int64_t image_id = 0;
        std::string display_name;
    };
    std::unordered_map<int64_t, Agg> agg;

    for (const auto& m : faceMatches) {
        auto& a = agg[m.person_id];
        a.score += config_.face_weight * m.score;
        if (a.display_name.empty()) {
            a.display_name = m.display_name;
            a.image_id = m.image_id;
        }
    }
    for (const auto& m : clothingMatches) {
        auto& a = agg[m.person_id];
        a.score += config_.clothing_weight * m.score;
        if (a.display_name.empty()) {
            a.display_name = m.display_name;
            a.image_id = m.image_id;
        }
    }

    std::vector<PipelineMatch> fused;
    fused.reserve(agg.size());
    for (const auto& [personId, a] : agg) {
        PipelineMatch m;
        m.person_id = personId;
        m.image_id = a.image_id;
        m.display_name = a.display_name;
        m.score = a.score;
        fused.push_back(m);
    }
    std::sort(fused.begin(), fused.end(),
        [](const PipelineMatch& a, const PipelineMatch& b) { return a.score > b.score; });
    return fused;
}

}  // namespace coser
