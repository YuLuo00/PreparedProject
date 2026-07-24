#pragma once
#include <opencv2/core.hpp>
#include <vector>

namespace coser {

/// 5点相似变换对齐到 ArcFace 标准 112x112 模板
cv::Mat AlignFace(const cv::Mat& image, const std::vector<cv::Point2f>& keypoints);

}  // namespace coser
