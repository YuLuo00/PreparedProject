#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "commands.h"
#include "../src/L3/YoloPoseDetector.h"

using namespace coser;
namespace fs = std::filesystem;

namespace {
std::unordered_map<std::string, std::string> ParseArgs(int argc, char** argv) {
    std::unordered_map<std::string, std::string> args;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string key = argv[i];
        if (key.rfind("--", 0) == 0) args[key.substr(2)] = argv[i + 1];
    }
    return args;
}

void FaceMaskEllipse(const cv::Size& cropSize, const PersonDetection& detection,
                     cv::Point& center, cv::Size& axes) {
    std::vector<Keypoint> facePoints;
    for (size_t i = 0; i < std::min<size_t>(5, detection.keypoints.size()); ++i) {
        if (detection.keypoints[i].score >= 0.25f) facePoints.push_back(detection.keypoints[i]);
    }
    if (facePoints.empty()) {
        center = cv::Point(cropSize.width / 2, static_cast<int>(cropSize.height * 0.18f));
        axes = cv::Size(std::max(1, static_cast<int>(cropSize.width * 0.22f)),
                        std::max(1, static_cast<int>(cropSize.height * 0.16f)));
        return;
    }

    float minX = facePoints.front().x, maxX = minX;
    float minY = facePoints.front().y, maxY = minY;
    float sumX = 0.0f, sumY = 0.0f;
    for (const auto& point : facePoints) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
        sumX += point.x;
        sumY += point.y;
    }
    float span = std::max({maxX - minX, maxY - minY, detection.box.width * 0.12f});
    center = cv::Point(static_cast<int>(sumX / facePoints.size() - detection.box.x),
                       static_cast<int>(sumY / facePoints.size() - detection.box.y + span * 0.12f));
    axes = cv::Size(static_cast<int>(span * 0.72f), static_cast<int>(span * 0.85f));
}
}  // namespace

int RunVisualize(int argc, char** argv) try {
    auto args = ParseArgs(argc, argv);
    auto get = [&](const std::string& key) {
        auto it = args.find(key);
        return it == args.end() ? std::string{} : it->second;
    };
    std::string modelsDir = get("models-dir");
    std::string imagePath = get("image");
    std::string outputPath = get("output");
    if (modelsDir.empty() || imagePath.empty() || outputPath.empty()) {
        std::cerr << "Usage: coser_cli visualize --models-dir <dir> --image <path> --output <png>\n";
        return 1;
    }

    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "Failed to read image: " << imagePath << "\n";
        return 1;
    }
    YoloPoseDetector detector(modelsDir + "/pose/yolov8n-pose.onnx");
    auto detections = detector.Detect(image);
    if (detections.empty()) {
        std::cerr << "No person detected: " << imagePath << "\n";
        return 1;
    }
    auto best = std::max_element(detections.begin(), detections.end(),
        [](const PersonDetection& a, const PersonDetection& b) { return a.score < b.score; });
    cv::Rect person(static_cast<int>(std::max(0.0f, best->box.x)),
                    static_cast<int>(std::max(0.0f, best->box.y)),
                    static_cast<int>(best->box.width), static_cast<int>(best->box.height));
    person &= cv::Rect(0, 0, image.cols, image.rows);
    if (person.empty()) {
        std::cerr << "Invalid person crop: " << imagePath << "\n";
        return 1;
    }

    cv::Mat overlay = image.clone();
    cv::rectangle(overlay, person, cv::Scalar(0, 0, 255), cv::FILLED);
    cv::addWeighted(overlay, 0.35, image, 0.65, 0.0, overlay);

    cv::Point faceCenter;
    cv::Size faceAxes;
    FaceMaskEllipse(person.size(), *best, faceCenter, faceAxes);
    faceCenter += person.tl();
    cv::Mat faceLayer = overlay.clone();
    cv::ellipse(faceLayer, faceCenter, faceAxes, 0.0, 0.0, 360.0, cv::Scalar(255, 255, 255), cv::FILLED);
    cv::addWeighted(faceLayer, 0.65, overlay, 0.35, 0.0, overlay);
    cv::rectangle(overlay, person, cv::Scalar(0, 0, 255), 2);

    fs::path output(outputPath);
    if (output.has_parent_path()) fs::create_directories(output.parent_path());
    if (!cv::imwrite(outputPath, overlay)) {
        std::cerr << "Failed to write visualization: " << outputPath << "\n";
        return 1;
    }
    std::cout << "Wrote visualization: " << outputPath << "\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "Unhandled exception: " << e.what() << "\n";
    return 2;
}
