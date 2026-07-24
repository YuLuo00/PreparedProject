#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <sqlite3.h>

namespace coser {

struct Person {
    int64_t person_id = 0;
    std::string display_name;
    std::string category;
};

struct ImageRow {
    int64_t image_id = 0;
    int64_t person_id = 0;
    std::string file_path;
    int width = 0;
    int height = 0;
    std::string format;
    std::string ingest_status = "pending";
    std::string fail_reason;
};

struct FaceEmbeddingRef {
    int64_t embedding_id = 0;
    int64_t image_id = 0;
    float bbox_x = 0, bbox_y = 0, bbox_w = 0, bbox_h = 0;
    float det_score = 0;
};

struct FaceMatchRow {
    int64_t embedding_id = 0;
    int64_t image_id = 0;
    int64_t person_id = 0;
    std::string display_name;
};

class MetadataStore {
public:
    MetadataStore();
    ~MetadataStore();

    bool Open(const std::string& dbPath);
    void Close();

    int64_t InsertPerson(const std::string& displayName, const std::string& category = "");
    int64_t InsertImage(const ImageRow& row);
    bool UpdateImageStatus(int64_t imageId, const std::string& status, const std::string& failReason = "");

    int64_t InsertFaceEmbeddingRef(const FaceEmbeddingRef& ref);
    std::optional<FaceMatchRow> ResolveFaceEmbedding(int64_t embeddingId);

    std::string GetLastError() const { return lastError_; }

private:
    sqlite3* db_ = nullptr;
    std::string lastError_;

    bool Execute(const std::string& sql);
    bool CreateTables();
};

}  // namespace coser
