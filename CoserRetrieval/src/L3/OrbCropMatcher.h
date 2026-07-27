#pragma once
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>

namespace coser {

/// ORB feature matching for precise confirmation of pHash short-list candidates
/// (handles crop/recompression cases that a pure hash can't distinguish).
class OrbCropMatcher {
public:
    OrbCropMatcher();

    /// Returns the number of good (Hamming-distance-filtered) matches between
    /// query and candidate. Higher = more likely the same original image.
    /// Caller decides the pass/fail threshold.
    int MatchScore(const cv::Mat& query, const cv::Mat& candidate) const;

private:
    cv::Ptr<cv::ORB> orb_;
};

}  // namespace coser
