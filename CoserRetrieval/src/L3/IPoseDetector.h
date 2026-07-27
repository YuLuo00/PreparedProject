#pragma once
#include <opencv2/core.hpp>
#include <vector>

namespace coser {

struct Keypoint {
    float x = 0.0f;
    float y = 0.0f;
    float score = 0.0f;
};

struct PersonDetection {
    cv::Rect2f box;
    float score = 0.0f;
    // 17 COCO body keypoints (YOLOv8-pose convention).
    std::vector<Keypoint> keypoints;
};

class IPoseDetector {
public:
    virtual ~IPoseDetector() = default;
    virtual std::vector<PersonDetection> Detect(const cv::Mat& image) = 0;
};

}  // namespace coser
