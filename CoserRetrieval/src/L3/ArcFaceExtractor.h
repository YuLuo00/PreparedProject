#pragma once
#include "IEmbeddingExtractor.h"
#include <string>
#include <onnxruntime_cxx_api.h>

namespace coser {

class ArcFaceExtractor : public IEmbeddingExtractor {
public:
    explicit ArcFaceExtractor(const std::string& modelPath, int dim = 512);

    std::vector<float> Extract(const cv::Mat& alignedFace) override;
    int GetDim() const override { return dim_; }

private:
    Ort::Env env_;
    Ort::Session session_;
    Ort::MemoryInfo memInfo_;
    int dim_;
};

}  // namespace coser
