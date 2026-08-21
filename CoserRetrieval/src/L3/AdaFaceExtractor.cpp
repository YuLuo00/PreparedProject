#include "AdaFaceExtractor.h"

#include <array>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace coser {

namespace {
std::wstring ToWide(const std::string& value) {
    return std::wstring(value.begin(), value.end());
}
}  // namespace

AdaFaceExtractor::AdaFaceExtractor(const std::string& modelPath, int dim)
    : env_(ORT_LOGGING_LEVEL_WARNING, "AdaFaceExtractor"),
      memInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      dim_(dim) {
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(1);
    session_ = Ort::Session(env_, ToWide(modelPath).c_str(), options);
}

std::vector<float> AdaFaceExtractor::Extract(const cv::Mat& alignedFace) {
    if (alignedFace.empty()) throw std::runtime_error("AdaFace received an empty aligned face");

    cv::Mat bgr;
    if (alignedFace.cols != 112 || alignedFace.rows != 112) {
        cv::resize(alignedFace, bgr, cv::Size(112, 112));
    } else {
        bgr = alignedFace;
    }
    bgr.convertTo(bgr, CV_32F, 1.0 / 127.5, -1.0);

    std::vector<float> input(3 * 112 * 112);
    for (int channel = 0; channel < 3; ++channel) {
        for (int y = 0; y < 112; ++y) {
            for (int x = 0; x < 112; ++x) {
                input[channel * 112 * 112 + y * 112 + x] = bgr.at<cv::Vec3f>(y, x)[channel];
            }
        }
    }

    std::array<int64_t, 4> inputShape{1, 3, 112, 112};
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memInfo_, input.data(), input.size(), inputShape.data(), inputShape.size());

    Ort::AllocatorWithDefaultOptions allocator;
    auto inputName = session_.GetInputNameAllocated(0, allocator);
    auto outputName = session_.GetOutputNameAllocated(0, allocator);
    const char* inputNames[] = {inputName.get()};
    const char* outputNames[] = {outputName.get()};
    auto outputs = session_.Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);

    float* output = outputs[0].GetTensorMutableData<float>();
    std::vector<float> embedding(output, output + dim_);
    float norm = 0.0f;
    for (float value : embedding) norm += value * value;
    norm = std::sqrt(norm);
    if (norm > 1e-10f) {
        for (float& value : embedding) value /= norm;
    }
    return embedding;
}

}  // namespace coser
