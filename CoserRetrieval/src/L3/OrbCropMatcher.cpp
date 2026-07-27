#include "OrbCropMatcher.h"
#include <opencv2/imgproc.hpp>

namespace coser {

OrbCropMatcher::OrbCropMatcher() {
    orb_ = cv::ORB::create(500);
}

int OrbCropMatcher::MatchScore(const cv::Mat& query, const cv::Mat& candidate) const {
    cv::Mat grayQuery, grayCandidate;
    cv::cvtColor(query, grayQuery, cv::COLOR_BGR2GRAY);
    cv::cvtColor(candidate, grayCandidate, cv::COLOR_BGR2GRAY);

    std::vector<cv::KeyPoint> kpQuery, kpCandidate;
    cv::Mat descQuery, descCandidate;
    orb_->detectAndCompute(grayQuery, cv::noArray(), kpQuery, descQuery);
    orb_->detectAndCompute(grayCandidate, cv::noArray(), kpCandidate, descCandidate);

    if (descQuery.empty() || descCandidate.empty()) return 0;

    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector<std::vector<cv::DMatch>> knnMatches;
    matcher.knnMatch(descQuery, descCandidate, knnMatches, 2);

    int goodCount = 0;
    for (const auto& m : knnMatches) {
        if (m.size() < 2) continue;
        if (m[0].distance < 0.75f * m[1].distance) {
            ++goodCount;
        }
    }
    return goodCount;
}

}  // namespace coser
