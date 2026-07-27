#pragma once
#include "IEmbeddingExtractor.h"
#include <string>
#include <onnxruntime_cxx_api.h>

namespace coser {

/// DINOv2 ViT-S/14 visual feature extractor for Route B (clothing/character crop).
/// Input: any crop (typically a person bounding-box crop, not face-aligned).
class DinoV2Extractor : public IEmbeddingExtractor {
public:
    explicit DinoV2Extractor(const std::string& modelPath, int inputSize = 224, int dim = 384);

    std::vector<float> Extract(const cv::Mat& crop) override;
    int GetDim() const override { return dim_; }

private:
    Ort::Env env_;
    Ort::Session session_;
    Ort::MemoryInfo memInfo_;
    int inputSize_;
    int dim_;
};

}  // namespace coser
