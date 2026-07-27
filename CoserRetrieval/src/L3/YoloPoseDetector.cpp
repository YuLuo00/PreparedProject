#include "YoloPoseDetector.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace coser {

namespace {
std::wstring ToWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

float IoU(const cv::Rect2f& a, const cv::Rect2f& b) {
    float interArea = (a & b).area();
    float unionArea = a.area() + b.area() - interArea;
    return unionArea > 0 ? interArea / unionArea : 0.0f;
}

std::vector<PersonDetection> NmsMerge(std::vector<PersonDetection> dets, float nmsThreshold) {
    std::sort(dets.begin(), dets.end(),
        [](const PersonDetection& a, const PersonDetection& b) { return a.score > b.score; });
    std::vector<PersonDetection> kept;
    std::vector<bool> suppressed(dets.size(), false);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (suppressed[i]) continue;
        kept.push_back(dets[i]);
        for (size_t j = i + 1; j < dets.size(); ++j) {
            if (!suppressed[j] && IoU(dets[i].box, dets[j].box) > nmsThreshold) {
                suppressed[j] = true;
            }
        }
    }
    return kept;
}
}  // namespace

YoloPoseDetector::YoloPoseDetector(const std::string& modelPath, int inputSize,
                                    float scoreThreshold, float nmsThreshold)
    : env_(ORT_LOGGING_LEVEL_WARNING, "YoloPoseDetector"),
      session_(nullptr),
      memInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      inputSize_(inputSize),
      scoreThreshold_(scoreThreshold),
      nmsThreshold_(nmsThreshold) {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    session_ = Ort::Session(env_, ToWide(modelPath).c_str(), opts);
}

std::vector<PersonDetection> YoloPoseDetector::Detect(const cv::Mat& image) {
    // letterbox resize to inputSize_ x inputSize_ (top-left aligned, same as ScrfdFaceDetector).
    float scale = std::min((float)inputSize_ / image.cols, (float)inputSize_ / image.rows);
    int newW = (int)std::round(image.cols * scale);
    int newH = (int)std::round(image.rows * scale);
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(newW, newH));
    cv::Mat canvas(inputSize_, inputSize_, image.type(), cv::Scalar(0, 0, 0));
    resized.copyTo(canvas(cv::Rect(0, 0, newW, newH)));

    cv::Mat rgb;
    cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    std::vector<float> input(3 * inputSize_ * inputSize_);
    for (int c = 0; c < 3; ++c) {
        for (int y = 0; y < inputSize_; ++y) {
            for (int x = 0; x < inputSize_; ++x) {
                input[c * inputSize_ * inputSize_ + y * inputSize_ + x] = rgb.at<cv::Vec3f>(y, x)[c];
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

    // output0: [1, 56, N] — already decoded to pixel space by the Ultralytics ONNX export.
    // Channels: 0-3 = box(cx,cy,w,h), 4 = score, 5-55 = 17 keypoints * (x,y,conf).
    auto shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
    int64_t numAnchors = shape[2];
    const float* data = outputs[0].GetTensorData<float>();
    const int kNumKeypoints = 17;

    std::vector<PersonDetection> allDets;
    for (int64_t i = 0; i < numAnchors; ++i) {
        float score = data[4 * numAnchors + i];
        if (score < scoreThreshold_) continue;

        float cx = data[0 * numAnchors + i];
        float cy = data[1 * numAnchors + i];
        float w = data[2 * numAnchors + i];
        float h = data[3 * numAnchors + i];

        PersonDetection det;
        det.score = score;
        float x1 = (cx - w / 2.0f) / scale;
        float y1 = (cy - h / 2.0f) / scale;
        det.box = cv::Rect2f(x1, y1, w / scale, h / scale);

        det.keypoints.reserve(kNumKeypoints);
        for (int k = 0; k < kNumKeypoints; ++k) {
            int64_t base = 5 + k * 3;
            Keypoint kp;
            kp.x = data[base * numAnchors + i] / scale;
            kp.y = data[(base + 1) * numAnchors + i] / scale;
            kp.score = data[(base + 2) * numAnchors + i];
            det.keypoints.push_back(kp);
        }

        allDets.push_back(det);
    }

    return NmsMerge(allDets, nmsThreshold_);
}

}  // namespace coser
