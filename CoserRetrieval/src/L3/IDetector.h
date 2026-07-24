#pragma once
#include <opencv2/core.hpp>
#include <vector>

namespace coser {

struct FaceDetection {
    cv::Rect2f box;
    float score = 0.0f;
    // 5 landmarks: left-eye, right-eye, nose, left-mouth, right-mouth
    std::vector<cv::Point2f> keypoints;
};

class IDetector {
public:
    virtual ~IDetector() = default;
    virtual std::vector<FaceDetection> Detect(const cv::Mat& image) = 0;
};

}  // namespace coser
