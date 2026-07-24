#include "FaceRecognitionPipeline.h"
#include <algorithm>
#include <sstream>
#include "../L3/FaceAlign.h"

namespace coser {

FaceRecognitionPipeline::FaceRecognitionPipeline(IDetector* detector,
                                                  IEmbeddingExtractor* extractor,
                                                  IVectorIndex* index,
                                                  MetadataStore* store,
                                                  PhotoAuthenticityChecker* authChecker)
    : detector_(detector), extractor_(extractor), index_(index), store_(store),
      authChecker_(authChecker) {}

IngestResult FaceRecognitionPipeline::Ingest(const cv::Mat& image, int64_t imageId) {
    auto dets = detector_->Detect(image);
    if (dets.empty()) return {false, "no face detected"};

    auto best = std::max_element(dets.begin(), dets.end(),
        [](const FaceDetection& a, const FaceDetection& b) { return a.score < b.score; });

    cv::Mat aligned = AlignFace(image, best->keypoints);

    if (authChecker_) {
        float clipScore = 0.0f;
        bool isReal = authChecker_->IsRealPhoto(aligned, &clipScore);
        if (!isReal) {
            std::ostringstream oss;
            oss << "rejected: not a real photo (clip_score=" << clipScore << ")";
            return {false, oss.str()};
        }
    }

    std::vector<float> emb = extractor_->Extract(aligned);

    FaceEmbeddingRef ref;
    ref.image_id = imageId;
    ref.bbox_x = best->box.x;
    ref.bbox_y = best->box.y;
    ref.bbox_w = best->box.width;
    ref.bbox_h = best->box.height;
    ref.det_score = best->score;

    int64_t embId = store_->InsertFaceEmbeddingRef(ref);
    if (embId < 0) return {false, "metadata insert failed"};

    if (!index_->Add(embId, emb)) return {false, "index add failed"};
    return {true, ""};
}

std::vector<PipelineMatch> FaceRecognitionPipeline::Query(const cv::Mat& image, int topK) {
    std::vector<PipelineMatch> results;

    auto dets = detector_->Detect(image);
    if (dets.empty()) return results;

    auto best = std::max_element(dets.begin(), dets.end(),
        [](const FaceDetection& a, const FaceDetection& b) { return a.score < b.score; });

    cv::Mat aligned = AlignFace(image, best->keypoints);
    std::vector<float> emb = extractor_->Extract(aligned);

    std::vector<int64_t> ids;
    std::vector<float> scores;
    if (!index_->Search(emb, topK, ids, scores)) return results;

    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] < 0) continue;
        auto resolved = store_->ResolveFaceEmbedding(ids[i]);
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
