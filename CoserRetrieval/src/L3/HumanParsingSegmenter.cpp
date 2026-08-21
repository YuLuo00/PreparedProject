#include "HumanParsingSegmenter.h"

#include <array>
#include <stdexcept>
#include <vector>
#include <opencv2/imgproc.hpp>

namespace coser {
namespace {
std::wstring ToWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

// LIP labels: hat, hair, glove, upper-clothes, dress, coat, socks, pants,
// jumpsuits, scarf, skirt and shoes. Face, sunglasses, skin and background
// deliberately do not appear here.
bool IsApparelClass(int label) {
    switch (label) {
        case 1: case 3: case 5: case 6: case 7: case 8: case 9:
        case 10: case 11: case 12: case 18: case 19:
            return true;
        default:
            return false;
    }
}

bool IsAppearanceClass(int label) {
    return label == 2 || IsApparelClass(label);  // hair + wearable apparel
}
}  // namespace

HumanParsingSegmenter::HumanParsingSegmenter(const std::string& modelPath, int inputSize)
    : env_(ORT_LOGGING_LEVEL_WARNING, "HumanParsingSegmenter"),
      session_(nullptr),
      memInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      inputSize_(inputSize) {
    Ort::SessionOptions options;
    options.SetIntraOpNumThreads(1);
    session_ = Ort::Session(env_, ToWide(modelPath).c_str(), options);
    if (session_.GetOutputCount() < 2) {
        throw std::runtime_error("Human parsing model must provide the LIP fusion output");
    }
}

cv::Mat HumanParsingSegmenter::ParseLabels(const cv::Mat& personCrop) {
    if (personCrop.empty()) return {};

    cv::Mat resized;
    cv::resize(personCrop, resized, cv::Size(inputSize_, inputSize_), 0, 0, cv::INTER_LINEAR);
    resized.convertTo(resized, CV_32F, 1.0 / 255.0);

    // The published LIP ONNX model uses BGR input and BGR ImageNet statistics.
    constexpr float kMean[3] = {0.406f, 0.456f, 0.485f};
    constexpr float kStd[3] = {0.225f, 0.224f, 0.229f};
    std::vector<float> input(3 * inputSize_ * inputSize_);
    for (int channel = 0; channel < 3; ++channel) {
        for (int y = 0; y < inputSize_; ++y) {
            for (int x = 0; x < inputSize_; ++x) {
                float value = resized.at<cv::Vec3f>(y, x)[channel];
                input[channel * inputSize_ * inputSize_ + y * inputSize_ + x] =
                    (value - kMean[channel]) / kStd[channel];
            }
        }
    }

    const std::array<int64_t, 4> inputShape{1, 3, inputSize_, inputSize_};
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memInfo_, input.data(), input.size(), inputShape.data(), inputShape.size());
    Ort::AllocatorWithDefaultOptions allocator;
    auto inputName = session_.GetInputNameAllocated(0, allocator);
    auto fusionName = session_.GetOutputNameAllocated(1, allocator);
    const char* inputNames[] = {inputName.get()};
    const char* outputNames[] = {fusionName.get()};
    auto output = session_.Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1,
                               outputNames, 1);

    const auto shape = output[0].GetTensorTypeAndShapeInfo().GetShape();
    if (shape.size() != 4 || shape[0] != 1 || shape[1] != 20 || shape[2] <= 0 || shape[3] <= 0) {
        throw std::runtime_error("Unexpected LIP fusion output shape");
    }
    const int outputHeight = static_cast<int>(shape[2]);
    const int outputWidth = static_cast<int>(shape[3]);
    const float* logits = output[0].GetTensorData<float>();

    cv::Mat labels(outputHeight, outputWidth, CV_8U);
    const int plane = outputHeight * outputWidth;
    for (int y = 0; y < outputHeight; ++y) {
        for (int x = 0; x < outputWidth; ++x) {
            const int pixel = y * outputWidth + x;
            int bestLabel = 0;
            float bestScore = logits[pixel];
            for (int label = 1; label < 20; ++label) {
                const float score = logits[label * plane + pixel];
                if (score > bestScore) {
                    bestScore = score;
                    bestLabel = label;
                }
            }
            labels.at<unsigned char>(y, x) = static_cast<unsigned char>(bestLabel);
        }
    }

    cv::Mat resizedLabels;
    cv::resize(labels, resizedLabels, personCrop.size(), 0, 0, cv::INTER_NEAREST);
    return resizedLabels;
}

cv::Mat HumanParsingSegmenter::ExtractAppearanceMask(const cv::Mat& personCrop) {
    return BuildAppearanceMask(ParseLabels(personCrop));
}

cv::Mat HumanParsingSegmenter::BuildAppearanceMask(const cv::Mat& labels) const {
    if (labels.empty()) return {};
    cv::Mat mask(labels.size(), CV_8U, cv::Scalar(0));
    for (int y = 0; y < mask.rows; ++y) {
        for (int x = 0; x < mask.cols; ++x) {
            if (IsAppearanceClass(labels.at<unsigned char>(y, x))) {
                mask.at<unsigned char>(y, x) = 255;
            }
        }
    }
    return mask;
}

cv::Mat HumanParsingSegmenter::ExtractApparelMask(const cv::Mat& personCrop) {
    return BuildApparelMask(ParseLabels(personCrop));
}

cv::Mat HumanParsingSegmenter::BuildApparelMask(const cv::Mat& labels) const {
    if (labels.empty()) return {};
    cv::Mat mask(labels.size(), CV_8U, cv::Scalar(0));
    for (int y = 0; y < mask.rows; ++y) {
        for (int x = 0; x < mask.cols; ++x) {
            if (IsApparelClass(labels.at<unsigned char>(y, x))) {
                mask.at<unsigned char>(y, x) = 255;
            }
        }
    }
    return mask;
}

}  // namespace coser
