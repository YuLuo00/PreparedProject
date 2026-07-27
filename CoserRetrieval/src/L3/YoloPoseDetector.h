#pragma once
#include "IPoseDetector.h"
#include <string>
#include <onnxruntime_cxx_api.h>

namespace coser {

class YoloPoseDetector : public IPoseDetector {
public:
    explicit YoloPoseDetector(const std::string& modelPath,
                               int inputSize = 640,
                               float scoreThreshold = 0.5f,
                               float nmsThreshold = 0.45f);

    std::vector<PersonDetection> Detect(const cv::Mat& image) override;

private:
    Ort::Env env_;
    Ort::Session session_;
    Ort::MemoryInfo memInfo_;

    int inputSize_;
    float scoreThreshold_;
    float nmsThreshold_;
};

}  // namespace coser
