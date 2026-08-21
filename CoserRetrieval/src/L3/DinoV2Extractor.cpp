#include "DinoV2Extractor.h"
#include <opencv2/imgproc.hpp>
#include <array>
#include <algorithm>
#include <cmath>

namespace coser {

namespace {
std::wstring ToWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

// ImageNet normalization, used by the HuggingFace Dinov2 preprocessor.
constexpr float kMean[3] = {0.485f, 0.456f, 0.406f};
constexpr float kStd[3] = {0.229f, 0.224f, 0.225f};
}  // namespace

DinoV2Extractor::DinoV2Extractor(const std::string& modelPath, int inputSize, int dim)
    : env_(ORT_LOGGING_LEVEL_WARNING, "DinoV2Extractor"),
      session_(nullptr),
      memInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      inputSize_(inputSize),
      dim_(dim) {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    session_ = Ort::Session(env_, ToWide(modelPath).c_str(), opts);
}

std::vector<float> DinoV2Extractor::Extract(const cv::Mat& crop) {
    return Run(crop, nullptr);
}

std::vector<float> DinoV2Extractor::ExtractMasked(const cv::Mat& crop, const cv::Mat& mask) {
    return Run(crop, &mask);
}

std::vector<float> DinoV2Extractor::Run(const cv::Mat& crop, const cv::Mat* mask) {
    cv::Mat resized;
    cv::resize(crop, resized, cv::Size(inputSize_, inputSize_), 0, 0, cv::INTER_LINEAR);

    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    std::vector<float> input(3 * inputSize_ * inputSize_);
    for (int c = 0; c < 3; ++c) {
        for (int y = 0; y < inputSize_; ++y) {
            for (int x = 0; x < inputSize_; ++x) {
                float v = rgb.at<cv::Vec3f>(y, x)[c];
                input[c * inputSize_ * inputSize_ + y * inputSize_ + x] = (v - kMean[c]) / kStd[c];
            }
        }
    }

    std::array<int64_t, 4> inputShape{1, 3, inputSize_, inputSize_};
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memInfo_, input.data(), input.size(), inputShape.data(), inputShape.size());

    Ort::AllocatorWithDefaultOptions allocator;
    auto inputName = session_.GetInputNameAllocated(0, allocator);
    const char* inputNames[] = {inputName.get()};
    const char* outputNames[] = {mask ? "last_hidden_state" : "pooler_output"};

    auto outputs = session_.Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);

    float* outData = outputs[0].GetTensorMutableData<float>();
    std::vector<float> emb(dim_, 0.0f);
    if (!mask) {
        std::copy(outData, outData + dim_, emb.begin());
    } else {
        constexpr int kPatchSize = 14;
        const int grid = inputSize_ / kPatchSize;
        cv::Mat resizedMask;
        cv::resize(*mask, resizedMask, cv::Size(grid, grid), 0, 0, cv::INTER_AREA);
        float totalWeight = 0.0f;
        for (int y = 0; y < grid; ++y) {
            for (int x = 0; x < grid; ++x) {
                const float weight = resizedMask.at<unsigned char>(y, x) / 255.0f;
                if (weight <= 0.0f) continue;
                const int token = 1 + y * grid + x;
                for (int d = 0; d < dim_; ++d) emb[d] += weight * outData[token * dim_ + d];
                totalWeight += weight;
            }
        }
        if (totalWeight <= 1e-6f) return {};
        for (float& value : emb) value /= totalWeight;
    }

    float norm = 0.0f;
    for (float v : emb) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-10f) for (float& v : emb) v /= norm;

    return emb;
}

}  // namespace coser
