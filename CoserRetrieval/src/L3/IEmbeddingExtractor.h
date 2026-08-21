#pragma once
#include <opencv2/core.hpp>
#include <vector>

namespace coser {

class IEmbeddingExtractor {
public:
    virtual ~IEmbeddingExtractor() = default;
    // 输入: 已对齐的人脸裁剪图 (112x112)。输出: L2 归一化后的向量。
    virtual std::vector<float> Extract(const cv::Mat& alignedFace) = 0;
    virtual std::vector<float> ExtractMasked(const cv::Mat& image, const cv::Mat& mask) {
        (void)mask;
        return Extract(image);
    }
    virtual int GetDim() const = 0;
};

}  // namespace coser
