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
#include "../src/L3/HumanParsingSegmenter.h"

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
    std::string humanParsingModel = get("human-parsing-model");
    if (humanParsingModel.empty()) {
        humanParsingModel = modelsDir + "/clothing/human_parsing_lip_resnet101.onnx";
    }
    HumanParsingSegmenter parser(humanParsingModel);
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

    cv::Mat labels = parser.ParseLabels(image(person));
    cv::Mat overlay = image.clone();
    for (int y = 0; y < labels.rows; ++y) {
        for (int x = 0; x < labels.cols; ++x) {
            const unsigned char label = labels.at<unsigned char>(y, x);
            cv::Vec3b& pixel = overlay.at<cv::Vec3b>(person.y + y, person.x + x);
            if (label == 2) {  // hair
                pixel = cv::Vec3b(0, 0, 255);
            } else if (label == 13) {  // face
                pixel = cv::Vec3b(255, 255, 255);
            } else if (label == 1 || label == 3 || (label >= 5 && label <= 12) ||
                       label == 18 || label == 19) {
                pixel = cv::Vec3b(0, 0, 255);
            }
        }
    }
    cv::addWeighted(overlay, 0.45, image, 0.55, 0.0, overlay);

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
