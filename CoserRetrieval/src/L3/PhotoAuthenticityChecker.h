#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <onnxruntime_cxx_api.h>

namespace coser {

class PhotoAuthenticityChecker {
public:
    explicit PhotoAuthenticityChecker(const std::string& modelPath, int inputSize = 224);

    // 返回 true 表示更接近"真实照片"，false 表示更接近"插画/CG"。
    // outScore 可选返回 (real_similarity - illustration_similarity)，用于日志/调试。
    bool IsRealPhoto(const cv::Mat& image, float* outScore = nullptr);

private:
    Ort::Env env_;
    Ort::Session session_;
    Ort::MemoryInfo memInfo_;
    int inputSize_;
};

}  // namespace coser
