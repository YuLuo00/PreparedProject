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

    /// Runs all three routes against the image, then marks the image row
    /// committed/failed. Per-route failure reasons are joined with "; ".
    IngestResult IngestImage(const cv::Mat& image, int64_t imageId);

    QueryResponse Query(const QueryRequest& request);

private:
    FaceRecognitionPipeline* facePipeline_;
    ClothingRecognitionPipeline* clothingPipeline_;
    ImageMatchPipeline* imageMatchPipeline_;
    MetadataStore* store_;
    ResultFusion* fusion_;
};

}  // namespace coser
