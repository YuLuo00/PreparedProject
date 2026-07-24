#include "ArcFaceExtractor.h"
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <stdexcept>

namespace coser {

namespace {
std::wstring ToWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}
}  // namespace

ArcFaceExtractor::ArcFaceExtractor(const std::string& modelPath, int dim)
    : env_(ORT_LOGGING_LEVEL_WARNING, "ArcFaceExtractor"),
      session_(nullptr),
      memInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      dim_(dim) {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    session_ = Ort::Session(env_, ToWide(modelPath).c_str(), opts);
}

std::vector<float> ArcFaceExtractor::Extract(const cv::Mat& alignedFace) {
    cv::Mat rgb;
    cv::cvtColor(alignedFace, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 127.5, -1.0);

    std::vector<float> input(3 * 112 * 112);
    for (int c = 0; c < 3; ++c) {
        for (int y = 0; y < 112; ++y) {
            for (int x = 0; x < 112; ++x) {
                input[c * 112 * 112 + y * 112 + x] = rgb.at<cv::Vec3f>(y, x)[c];
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

    float* outData = outputs[0].GetTensorMutableData<float>();
    std::vector<float> emb(outData, outData + dim_);

    float norm = 0.0f;
    for (float v : emb) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-10f) for (float& v : emb) v /= norm;

    return emb;
}

}  // namespace coser
