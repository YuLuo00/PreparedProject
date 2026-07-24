#include "PhotoAuthenticityChecker.h"
#include "clip_text_embeddings.inc"
#include <opencv2/imgproc.hpp>
#include <array>
#include <cmath>
#include <stdexcept>

namespace coser {

namespace {
std::wstring ToWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

// CLIP (openai/clip-vit-base-patch32) 视觉侧预处理参数，来自 preprocessor_config.json。
constexpr float kMean[3] = {0.48145466f, 0.4578275f, 0.40821073f};
constexpr float kStd[3] = {0.26862954f, 0.26130258f, 0.27577711f};

float Dot(const std::vector<float>& a, const std::array<float, 512>& b) {
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) sum += a[i] * b[i];
    return sum;
}
}  // namespace

PhotoAuthenticityChecker::PhotoAuthenticityChecker(const std::string& modelPath, int inputSize)
    : env_(ORT_LOGGING_LEVEL_WARNING, "PhotoAuthenticityChecker"),
      session_(nullptr),
      memInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      inputSize_(inputSize) {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    session_ = Ort::Session(env_, ToWide(modelPath).c_str(), opts);
}

bool PhotoAuthenticityChecker::IsRealPhoto(const cv::Mat& image, float* outScore) {
    // resize shortest edge to inputSize_, then center-crop to inputSize_ x inputSize_.
    int w = image.cols, h = image.rows;
    float scale = static_cast<float>(inputSize_) / static_cast<float>(std::min(w, h));
    int newW = static_cast<int>(std::round(w * scale));
    int newH = static_cast<int>(std::round(h * scale));

    cv::Mat resized;
    cv::resize(image, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

    int x0 = (newW - inputSize_) / 2;
    int y0 = (newH - inputSize_) / 2;
    cv::Mat cropped = resized(cv::Rect(x0, y0, inputSize_, inputSize_)).clone();

    cv::Mat rgb;
    cv::cvtColor(cropped, rgb, cv::COLOR_BGR2RGB);
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
    auto outputName = session_.GetOutputNameAllocated(0, allocator);
    const char* inputNames[] = {inputName.get()};
    const char* outputNames[] = {outputName.get()};

    auto outputs = session_.Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);

    float* outData = outputs[0].GetTensorMutableData<float>();
    std::vector<float> emb(outData, outData + 512);

    float norm = 0.0f;
    for (float v : emb) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-10f) for (float& v : emb) v /= norm;

    float realScore = Dot(emb, kRealPhotoEmbedding);
    float illustScore = Dot(emb, kIllustrationEmbedding);

    if (outScore) *outScore = realScore - illustScore;
    return realScore > illustScore;
}

}  // namespace coser
