#pragma once

#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>
#include <string>

namespace coser {

/// LIP human parsing model. The returned mask preserves hair and wearable
/// apparel pixels only; face, skin, limbs and background are excluded.
class HumanParsingSegmenter {
public:
    explicit HumanParsingSegmenter(const std::string& modelPath, int inputSize = 473);

    /// Returns LIP class ids aligned with personCrop (0=background, 2=hair,
    /// 5..12=main apparel, 13=face; see README for the complete table).
    cv::Mat ParseLabels(const cv::Mat& personCrop);
    /// Returns wearable apparel pixels only. Hair is deliberately excluded so
    /// callers can reject images that contain no identifiable clothing.
    cv::Mat ExtractApparelMask(const cv::Mat& personCrop);
    cv::Mat ExtractAppearanceMask(const cv::Mat& personCrop);

private:
    Ort::Env env_;
    Ort::Session session_;
    Ort::MemoryInfo memInfo_;
    int inputSize_;
};

}  // namespace coser
