#include "PHasher.h"
#include <opencv2/imgproc.hpp>

namespace coser {

uint64_t PHasher::Compute(const cv::Mat& image) const {
    cv::Mat gray;
    if (image.channels() == 3 || image.channels() == 4) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image;
    }

    cv::Mat resized;
    cv::resize(gray, resized, cv::Size(32, 32), 0, 0, cv::INTER_AREA);

    cv::Mat floatImg;
    resized.convertTo(floatImg, CV_32F);

    cv::Mat dctMat;
    cv::dct(floatImg, dctMat);

    cv::Mat low = dctMat(cv::Rect(0, 0, 8, 8)).clone();

    double sum = 0.0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (x == 0 && y == 0) continue;  // skip DC term
            sum += low.at<float>(y, x);
        }
    }
    double avg = sum / 63.0;

    uint64_t hash = 0;
    int bit = 0;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            if (low.at<float>(y, x) > avg) {
                hash |= (uint64_t(1) << bit);
            }
            ++bit;
        }
    }
    return hash;
}

int PHasher::HammingDistance(uint64_t a, uint64_t b) {
    uint64_t x = a ^ b;
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
#endif
}

}  // namespace coser
