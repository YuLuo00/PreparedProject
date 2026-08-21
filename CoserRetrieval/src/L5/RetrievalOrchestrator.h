#pragma once
#include "../L2/MetadataStore.h"
#include "../L4/FaceRecognitionPipeline.h"
#include "../L4/ClothingRecognitionPipeline.h"
#include "../L4/ImageMatchPipeline.h"
#include "ResultFusion.h"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace coser {

enum class QueryMode {
    ExactOnly,
    FaceOnly,
    ClothingOnly,
    RoleOnly,
    AllLinked
};

struct QueryRequest {
    cv::Mat image;
    QueryMode mode = QueryMode::AllLinked;
    int topK = 10;
};

struct QueryResponse {
    std::vector<PipelineMatch> exactMatches;
    std::vector<PipelineMatch> faceMatches;
    std::vector<PipelineMatch> clothingMatches;
    std::vector<PipelineMatch> roleMatches;
    std::vector<PipelineMatch> fused;
};

/// Single entry point wiring the three independent L4 retrieval routes together.
/// Ingest always runs all three routes (route failure is non-fatal per-route);
/// Query runs only the routes selected by QueryMode.
class RetrievalOrchestrator {
public:
    RetrievalOrchestrator(FaceRecognitionPipeline* facePipeline,
                           ClothingRecognitionPipeline* clothingPipeline,
                           ImageMatchPipeline* imageMatchPipeline,
                           MetadataStore* store,
                           ResultFusion* fusion);

    /// Runs clothing/exact and optionally face routes against the image, then
    /// marks the image row committed/failed. Role-only ingest can skip face
    /// embeddings entirely when no person label was supplied.
    IngestResult IngestImage(const cv::Mat& image, int64_t imageId, bool includeFace = true);

    QueryResponse Query(const QueryRequest& request);

private:
    FaceRecognitionPipeline* facePipeline_;
    ClothingRecognitionPipeline* clothingPipeline_;
    ImageMatchPipeline* imageMatchPipeline_;
    MetadataStore* store_;
    ResultFusion* fusion_;
};

}  // namespace coser
