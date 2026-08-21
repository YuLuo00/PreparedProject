#include "RetrievalOrchestrator.h"

namespace coser {

RetrievalOrchestrator::RetrievalOrchestrator(FaceRecognitionPipeline* facePipeline,
                                              ClothingRecognitionPipeline* clothingPipeline,
                                              ImageMatchPipeline* imageMatchPipeline,
                                              MetadataStore* store,
                                              ResultFusion* fusion)
    : facePipeline_(facePipeline),
      clothingPipeline_(clothingPipeline),
      imageMatchPipeline_(imageMatchPipeline),
      store_(store),
      fusion_(fusion) {}

IngestResult RetrievalOrchestrator::IngestImage(const cv::Mat& image, int64_t imageId, bool includeFace) {
    IngestResult faceResult{true, ""};
    if (includeFace) faceResult = facePipeline_->Ingest(image, imageId);
    IngestResult clothingResult = clothingPipeline_->Ingest(image, imageId);
    IngestResult exactResult = imageMatchPipeline_->Ingest(image, imageId);

    // exact-match (pHash registration) failing is treated as fatal — it means we
    // couldn't even record basic metadata for the image. Face/clothing route
    // failures (e.g. "no face detected") are expected/non-fatal on their own but
    // still reported so the caller can see why those routes didn't contribute.
    bool ok = exactResult.ok;

    std::string reason;
    auto appendReason = [&reason](const std::string& r) {
        if (r.empty()) return;
        if (!reason.empty()) reason += "; ";
        reason += r;
    };
    if (!faceResult.ok) appendReason("face: " + faceResult.reason);
    if (!clothingResult.ok) appendReason("clothing: " + clothingResult.reason);
    if (!exactResult.ok) appendReason("exact: " + exactResult.reason);

    store_->UpdateImageStatus(imageId, ok ? "committed" : "failed", reason);
    return {ok, reason};
}

QueryResponse RetrievalOrchestrator::Query(const QueryRequest& request) {
    QueryResponse response;

    if (request.mode == QueryMode::ExactOnly || request.mode == QueryMode::AllLinked) {
        response.exactMatches = imageMatchPipeline_->Query(request.image);
    }
    if (request.mode == QueryMode::FaceOnly || request.mode == QueryMode::AllLinked) {
        response.faceMatches = facePipeline_->Query(request.image, request.topK);
    }
    if (request.mode == QueryMode::ClothingOnly || request.mode == QueryMode::AllLinked) {
        response.clothingMatches = clothingPipeline_->Query(request.image, request.topK);
    }

    // Role mode deliberately does not invoke ArcFace or exact-image matching.
    // It only searches face-masked clothing/hair appearance embeddings.
    if (request.mode == QueryMode::RoleOnly) {
        response.roleMatches = clothingPipeline_->QueryRoles(request.image, request.topK);
        response.fused = response.roleMatches;
        return response;
    }

    response.fused = fusion_->Fuse(response.exactMatches, response.faceMatches, response.clothingMatches);
    return response;
}

}  // namespace coser
