#include "ClothingRecognitionPipeline.h"
#include <algorithm>
#include <unordered_map>
#include <opencv2/imgproc.hpp>

namespace coser {

namespace {
cv::Mat CropBox(const cv::Mat& image, const cv::Rect2f& box) {
    cv::Rect roi(static_cast<int>(std::max(0.0f, box.x)),
                 static_cast<int>(std::max(0.0f, box.y)),
                 static_cast<int>(box.width),
                 static_cast<int>(box.height));
    roi &= cv::Rect(0, 0, image.cols, image.rows);
    return image(roi).clone();
}

cv::Mat CropAppearance(const cv::Mat& image, const PersonDetection& detection) {
    cv::Mat crop = CropBox(image, detection.box);
    if (crop.empty()) return crop;

    // COCO keypoints 0..4 are nose, eyes and ears. Mask the face itself but
    // retain the surrounding/top hair, so role matching has no facial identity
    // signal while still retaining hairstyle and clothing appearance.
    std::vector<Keypoint> facePoints;
    for (size_t i = 0; i < std::min<size_t>(5, detection.keypoints.size()); ++i) {
        if (detection.keypoints[i].score >= 0.25f) facePoints.push_back(detection.keypoints[i]);
    }

    cv::Point center;
    cv::Size axes;
    if (facePoints.empty()) {
        // A head-only or distant pose can lack reliable facial keypoints. Use
        // a conservative top-center mask rather than leaking facial pixels.
        center = cv::Point(crop.cols / 2, static_cast<int>(crop.rows * 0.18f));
        axes = cv::Size(std::max(1, static_cast<int>(crop.cols * 0.22f)),
                        std::max(1, static_cast<int>(crop.rows * 0.16f)));
    } else {
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
    cv::ellipse(crop, center, axes, 0.0, 0.0, 360.0, cv::Scalar::all(0), cv::FILLED);
    return crop;
}

std::vector<float> FlattenKeypoints(const std::vector<Keypoint>& keypoints) {
    std::vector<float> flat;
    flat.reserve(keypoints.size() * 3);
    for (const auto& kp : keypoints) {
        flat.push_back(kp.x);
        flat.push_back(kp.y);
        flat.push_back(kp.score);
    }
    return flat;
}
}  // namespace

ClothingRecognitionPipeline::ClothingRecognitionPipeline(IPoseDetector* poseDetector,
                                                           IEmbeddingExtractor* extractor,
                                                           IVectorIndex* index,
                                                           MetadataStore* store)
    : poseDetector_(poseDetector), extractor_(extractor), index_(index), store_(store) {}

IngestResult ClothingRecognitionPipeline::Ingest(const cv::Mat& image, int64_t imageId) {
    auto dets = poseDetector_->Detect(image);
    if (dets.empty()) return {false, "no person detected"};

    auto best = std::max_element(dets.begin(), dets.end(),
        [](const PersonDetection& a, const PersonDetection& b) { return a.score < b.score; });

    cv::Mat crop = CropAppearance(image, *best);
    if (crop.empty()) return {false, "no person detected"};

    std::vector<float> emb = extractor_->Extract(crop);

    ClothingEmbeddingRef ref;
    ref.image_id = imageId;
    ref.bbox_x = best->box.x;
    ref.bbox_y = best->box.y;
    ref.bbox_w = best->box.width;
    ref.bbox_h = best->box.height;
    ref.pose_keypoints = FlattenKeypoints(best->keypoints);

    int64_t embId = store_->InsertClothingEmbeddingRef(ref);
    if (embId < 0) return {false, "metadata insert failed"};

    if (!index_->Add(embId, emb)) return {false, "index add failed"};
    return {true, ""};
}

std::vector<PipelineMatch> ClothingRecognitionPipeline::Query(const cv::Mat& image, int topK) {
    std::vector<PipelineMatch> results;

    auto dets = poseDetector_->Detect(image);
    if (dets.empty()) return results;

    auto best = std::max_element(dets.begin(), dets.end(),
        [](const PersonDetection& a, const PersonDetection& b) { return a.score < b.score; });

    cv::Mat crop = CropAppearance(image, *best);
    if (crop.empty()) return results;

    std::vector<float> emb = extractor_->Extract(crop);

    std::vector<int64_t> ids;
    std::vector<float> scores;
    if (!index_->Search(emb, topK, ids, scores)) return results;

    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] < 0) continue;
        auto resolved = store_->ResolveClothingEmbedding(ids[i]);
        if (!resolved) continue;
        PipelineMatch m;
        m.image_id = resolved->image_id;
        m.person_id = resolved->person_id;
        m.display_name = resolved->display_name;
        m.score = scores[i];
        results.push_back(m);
    }
    return results;
}

std::vector<PipelineMatch> ClothingRecognitionPipeline::QueryRoles(const cv::Mat& image, int topK) {
    std::vector<PipelineMatch> results;
    auto dets = poseDetector_->Detect(image);
    if (dets.empty()) return results;

    auto best = std::max_element(dets.begin(), dets.end(),
        [](const PersonDetection& a, const PersonDetection& b) { return a.score < b.score; });
    cv::Mat crop = CropAppearance(image, *best);
    if (crop.empty()) return results;

    std::vector<float> emb = extractor_->Extract(crop);
    std::vector<int64_t> ids;
    std::vector<float> scores;
    // Fetch extra reference images so several photos of one role do not crowd
    // out other role labels before aggregation.
    if (!index_->Search(emb, std::max(topK * 5, topK), ids, scores)) return results;

    std::unordered_map<int64_t, PipelineMatch> bestByRole;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] < 0) continue;
        auto resolved = store_->ResolveClothingEmbedding(ids[i]);
        if (!resolved || resolved->role_id <= 0 || resolved->role_name.empty()) continue;

        PipelineMatch candidate;
        candidate.image_id = resolved->image_id;
        candidate.person_id = resolved->person_id;
        candidate.display_name = resolved->display_name;
        candidate.role_id = resolved->role_id;
        candidate.role_name = resolved->role_name;
        candidate.score = scores[i];

        auto it = bestByRole.find(candidate.role_id);
        if (it == bestByRole.end() || candidate.score > it->second.score) {
            bestByRole[candidate.role_id] = candidate;
        }
    }
    for (const auto& [_, match] : bestByRole) results.push_back(match);
    std::sort(results.begin(), results.end(),
        [](const PipelineMatch& a, const PipelineMatch& b) { return a.score > b.score; });
    if (static_cast<int>(results.size()) > topK) results.resize(topK);
    return results;
}

}  // namespace coser
