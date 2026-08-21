#pragma once

#include "IEmbeddingExtractor.h"
#include <onnxruntime_cxx_api.h>
#include <string>

namespace coser {

// AdaFace IR-18 uses BGR input normalized to [-1, 1], unlike the RGB
// InsightFace ArcFace model. Keep it separate to prevent preprocessing drift.
class AdaFaceExtractor : public IEmbeddingExtractor {
public:
    explicit AdaFaceExtractor(const std::string& modelPath, int dim = 512);

    std::vector<float> Extract(const cv::Mat& alignedFace) override;
    int GetDim() const override { return dim_; }

private:
    Ort::Env env_;
    Ort::Session session_{nullptr};
    Ort::MemoryInfo memInfo_;
    int dim_;
};

}  // namespace coser
