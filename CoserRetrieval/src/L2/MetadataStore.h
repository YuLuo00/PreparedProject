#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <mutex>
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
    int64_t role_id = 0;
    std::string file_path;
    std::string md5;
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

struct ClothingEmbeddingRef {
    int64_t embedding_id = 0;
    int64_t image_id = 0;
    float bbox_x = 0, bbox_y = 0, bbox_w = 0, bbox_h = 0;
    std::vector<float> pose_keypoints;  // flattened 17x3 (x,y,score) from YOLOv8-pose
};

struct ClothingMatchRow {
    int64_t embedding_id = 0;
    int64_t image_id = 0;
    int64_t person_id = 0;
    std::string display_name;
    int64_t role_id = 0;
    std::string role_name;
};

struct ImagePersonRow {
    int64_t image_id = 0;
    int64_t person_id = 0;
    std::string display_name;
};

struct ImageMd5Row {
    int64_t image_id = 0;
    std::string file_path;
};

struct RemovedImageEmbeddings {
    std::vector<int64_t> face_embedding_ids;
    std::vector<int64_t> clothing_embedding_ids;
};

class MetadataStore {
public:
    MetadataStore();
    ~MetadataStore();

    bool Open(const std::string& dbPath);
    void Close();

    int64_t InsertPerson(const std::string& displayName, const std::string& category = "");
    int64_t InsertRole(const std::string& displayName);
    int64_t InsertImage(const ImageRow& row);
    std::optional<ImageMd5Row> FindImageByMd5(const std::string& md5);
    bool DeleteImageAndEmbeddings(int64_t imageId, RemovedImageEmbeddings& removed);
    bool UpdateImageStatus(int64_t imageId, const std::string& status, const std::string& failReason = "");

    int64_t InsertFaceEmbeddingRef(const FaceEmbeddingRef& ref);
    std::optional<FaceMatchRow> ResolveFaceEmbedding(int64_t embeddingId);

    int64_t InsertClothingEmbeddingRef(const ClothingEmbeddingRef& ref);
    std::optional<ClothingMatchRow> ResolveClothingEmbedding(int64_t embeddingId);

    bool UpdateImagePHash(int64_t imageId, int64_t phash);
    std::vector<std::pair<int64_t, int64_t>> LoadAllPHashes();  // (image_id, phash)
    std::optional<std::string> GetImageFilePath(int64_t imageId);
    std::optional<ImagePersonRow> GetPersonByImageId(int64_t imageId);  // used by exact-match route (no embedding involved)

    std::string GetLastError() const { return lastError_; }

private:
    sqlite3* db_ = nullptr;
    std::string lastError_;
    mutable std::recursive_mutex mutex_;

    bool Execute(const std::string& sql);
    bool CreateTables();
    bool ColumnExists(const std::string& table, const std::string& column);
};

}  // namespace coser
