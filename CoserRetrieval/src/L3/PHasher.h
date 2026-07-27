#pragma once
#include <cstdint>
#include <opencv2/core.hpp>

namespace coser {

/// DCT-based perceptual hash (64-bit), for coarse "same original image" filtering.
class PHasher {
public:
    uint64_t Compute(const cv::Mat& image) const;
    static int HammingDistance(uint64_t a, uint64_t b);
};

}  // namespace coser
