#pragma once
#include "IDetector.h"
#include <memory>
#include <string>
#include <onnxruntime_cxx_api.h>

namespace coser {

class ScrfdFaceDetector : public IDetector {
public:
    explicit ScrfdFaceDetector(const std::string& modelPath,
                                int inputSize = 640,
                                float scoreThreshold = 0.5f,
                                float nmsThreshold = 0.4f);

    std::vector<FaceDetection> Detect(const cv::Mat& image) override;

private:
    Ort::Env env_;
    Ort::Session session_;
    Ort::MemoryInfo memInfo_;

    int inputSize_;
    float scoreThreshold_;
    float nmsThreshold_;
};

}  // namespace coser
