#include "ScrfdFaceDetector.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace coser {

namespace {
std::wstring ToWide(const std::string& s) {
    return std::wstring(s.begin(), s.end());
}

struct StrideDecoded {
    int stride;
    std::vector<FaceDetection> dets;
};

float IoU(const cv::Rect2f& a, const cv::Rect2f& b) {
    float interArea = (a & b).area();
    float unionArea = a.area() + b.area() - interArea;
    return unionArea > 0 ? interArea / unionArea : 0.0f;
}

std::vector<FaceDetection> NmsMerge(std::vector<FaceDetection> dets, float nmsThreshold) {
    std::sort(dets.begin(), dets.end(),
        [](const FaceDetection& a, const FaceDetection& b) { return a.score > b.score; });
    std::vector<FaceDetection> kept;
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

ScrfdFaceDetector::ScrfdFaceDetector(const std::string& modelPath, int inputSize,
                                      float scoreThreshold, float nmsThreshold)
    : env_(ORT_LOGGING_LEVEL_WARNING, "ScrfdFaceDetector"),
      session_(nullptr),
      memInfo_(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)),
      inputSize_(inputSize),
      scoreThreshold_(scoreThreshold),
      nmsThreshold_(nmsThreshold) {
    Ort::SessionOptions opts;
    opts.SetIntraOpNumThreads(1);
    session_ = Ort::Session(env_, ToWide(modelPath).c_str(), opts);
}

std::vector<FaceDetection> ScrfdFaceDetector::Detect(const cv::Mat& image) {
    // letterbox resize to inputSize_ x inputSize_
    float scale = std::min((float)inputSize_ / image.cols, (float)inputSize_ / image.rows);
    int newW = (int)std::round(image.cols * scale);
    int newH = (int)std::round(image.rows * scale);
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(newW, newH));
    cv::Mat canvas(inputSize_, inputSize_, image.type(), cv::Scalar(0, 0, 0));
    resized.copyTo(canvas(cv::Rect(0, 0, newW, newH)));

    cv::Mat rgb;
    cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 128.0, -127.5 / 128.0);

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
    const char* inputNames[] = {inputName.get()};

    size_t numOutputs = session_.GetOutputCount();
    std::vector<Ort::AllocatedStringPtr> outputNameHolders;
    std::vector<const char*> outputNames;
    for (size_t i = 0; i < numOutputs; ++i) {
        outputNameHolders.push_back(session_.GetOutputNameAllocated(i, allocator));
        outputNames.push_back(outputNameHolders.back().get());
    }

    auto outputs = session_.Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1,
                                 outputNames.data(), numOutputs);

    // 按 shape 分类每个输出: 末维 1=score, 4=bbox-delta, 10=kps-delta；
    // 首维 (12800/3200/800 @640输入) 对应 stride 8/16/32，每格 2 个 anchor
    std::vector<FaceDetection> allDets;
    struct StrideBuffers {
        const float* score = nullptr;
        const float* bbox = nullptr;
        const float* kps = nullptr;
        int64_t numAnchors = 0;
    };
    std::vector<int> strides = {8, 16, 32};
    std::vector<StrideBuffers> buffers(strides.size());

    for (size_t i = 0; i < numOutputs; ++i) {
        auto shape = outputs[i].GetTensorTypeAndShapeInfo().GetShape();
        int64_t lastDim = shape.back();
        int64_t firstDim = shape[0] == 1 && shape.size() > 1 ? shape[1] : shape[0];
        // shape 通常是 [N, C] 或 [N]；取非批次维的锚点数
        int64_t numAnchors = shape.size() >= 2 ? shape[shape.size() - 2] : shape[0];

        int strideIdx = -1;
        for (size_t s = 0; s < strides.size(); ++s) {
            int gridSize = inputSize_ / strides[s];
            int64_t expected = (int64_t)gridSize * gridSize * 2;
            if (numAnchors == expected) { strideIdx = (int)s; break; }
        }
        if (strideIdx < 0) continue;

        const float* data = outputs[i].GetTensorData<float>();
        if (lastDim == 1) buffers[strideIdx].score = data;
        else if (lastDim == 4) buffers[strideIdx].bbox = data;
        else if (lastDim == 10) buffers[strideIdx].kps = data;
        buffers[strideIdx].numAnchors = numAnchors;
    }

    for (size_t s = 0; s < strides.size(); ++s) {
        auto& buf = buffers[s];
        if (!buf.score || !buf.bbox || !buf.kps) continue;
        int stride = strides[s];
        int gridSize = inputSize_ / stride;

        for (int64_t idx = 0; idx < buf.numAnchors; ++idx) {
            float score = buf.score[idx];
            if (score < scoreThreshold_) continue;

            int64_t gridIdx = idx / 2;
            int col = (int)(gridIdx % gridSize);
            int row = (int)(gridIdx / gridSize);
            float cx = (col + 0.5f) * stride;
            float cy = (row + 0.5f) * stride;

            const float* bboxDelta = buf.bbox + idx * 4;
            float l = bboxDelta[0] * stride;
            float t = bboxDelta[1] * stride;
            float r = bboxDelta[2] * stride;
            float b = bboxDelta[3] * stride;

            FaceDetection det;
            det.score = score;
            float x1 = (cx - l) / scale;
            float y1 = (cy - t) / scale;
            float x2 = (cx + r) / scale;
            float y2 = (cy + b) / scale;
            det.box = cv::Rect2f(x1, y1, x2 - x1, y2 - y1);

            const float* kpsDelta = buf.kps + idx * 10;
            for (int k = 0; k < 5; ++k) {
                float kx = (cx + kpsDelta[k * 2] * stride) / scale;
                float ky = (cy + kpsDelta[k * 2 + 1] * stride) / scale;
                det.keypoints.emplace_back(kx, ky);
            }

            allDets.push_back(det);
        }
    }

    return NmsMerge(allDets, nmsThreshold_);
}

}  // namespace coser
