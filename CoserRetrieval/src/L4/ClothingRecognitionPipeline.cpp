#include "ClothingRecognitionPipeline.h"
#include <algorithm>

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

    cv::Mat crop = CropBox(image, best->box);
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

    cv::Mat crop = CropBox(image, best->box);
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

}  // namespace coser
