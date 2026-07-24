#include "FaceAlign.h"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace coser {

namespace {
// 标准 ArcFace 112x112 参考模板 (left-eye, right-eye, nose, left-mouth, right-mouth)
const std::vector<cv::Point2f> kArcFaceTemplate = {
    {38.2946f, 51.6963f},
    {73.5318f, 51.5014f},
    {56.0252f, 71.7366f},
    {41.5493f, 92.3655f},
    {70.7299f, 92.2041f},
};
}  // namespace

cv::Mat AlignFace(const cv::Mat& image, const std::vector<cv::Point2f>& keypoints) {
    if (keypoints.size() != 5) {
        throw std::invalid_argument("AlignFace requires exactly 5 keypoints");
    }
    cv::Mat transform = cv::estimateAffinePartial2D(keypoints, kArcFaceTemplate);
    cv::Mat aligned;
    cv::warpAffine(image, aligned, transform, cv::Size(112, 112));
    return aligned;
}

}  // namespace coser
