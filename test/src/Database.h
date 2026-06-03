#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <sqlite3.h>

struct ImageRecord {
    int64_t id = 0;
    std::string path;
    std::string filename;
    std::string shoot_date;
    double rating = 0.0;
    std::string path_prefix;
    std::string scene_tags;   // JSON array string
    std::string style_tags;   // JSON array string
    std::string mood_tags;    // JSON array string
};

struct CoserFace {
    int64_t id = 0;
    std::string coser_name;
    std::string reference_image;
    std::vector<float> face_vector; // 512-dim ArcFace
};

struct QueryFilter {
    std::vector<std::string> tags;
    std::string date_from;
    std::string date_to;
    std::string path_prefix;
    double rating_min = 0.0;
    int top_k = 20;
};

class Database {
public:
    Database();
    ~Database();

    bool Open(const std::string& dbPath);
    void Close();
    bool IsOpen() const { return db_ != nullptr; }

    // 建表
    bool CreateTables();

    // 图片库操作
    int64_t InsertImage(const ImageRecord& rec);
    bool UpdateImageFeatures(int64_t imageId,
                             const std::vector<float>& clipVec,
                             const std::vector<float>& wd14Vec,
                             int64_t clipFaissId,
                             int64_t wd14FaissId);
    bool ImageExists(const std::string& path);
    std::vector<ImageRecord> QueryImages(const QueryFilter& filter);
    std::optional<ImageRecord> GetImageById(int64_t id);

    // 标签操作
    bool InsertTags(int64_t imageId, const std::vector<std::pair<std::string,float>>& tags);
    std::vector<std::pair<std::string,float>> GetTags(int64_t imageId);

    // Faiss id 映射
    // index_type: "clip" / "wd14" / "face"
    bool InsertFaissMapping(int64_t faissId, int64_t entityId, const std::string& indexType);
    int64_t GetEntityIdByFaissId(int64_t faissId, const std::string& indexType);
    // 获取所有向量用于重建索引
    bool LoadAllVectors(const std::string& indexType,
                        std::vector<int64_t>& outFaissIds,
                        std::vector<std::vector<float>>& outVectors);

    // Coser 人脸操作
    int64_t InsertCoserFace(const CoserFace& face);
    std::vector<CoserFace> GetAllCoserFaces();
    bool CoserExists(const std::string& name);

    // 获取下一个 Faiss ID（自增）
    int64_t GetNextFaissId(const std::string& indexType);

    // 事务
    bool BeginTransaction();
    bool CommitTransaction();
    bool RollbackTransaction();

    std::string GetLastError() const { return lastError_; }

private:
    sqlite3* db_ = nullptr;
    std::string lastError_;

    bool Execute(const std::string& sql);
    bool Prepare(const std::string& sql, sqlite3_stmt** stmt);
};
