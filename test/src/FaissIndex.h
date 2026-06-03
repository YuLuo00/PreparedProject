#pragma once
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>

/// 封装单个 Faiss IndexFlatIP（内积/余弦相似度）
class FaissIndex {
public:
    explicit FaissIndex(int dim);
    ~FaissIndex() = default;

    /// 加载已有索引文件，不存在则创建空索引
    bool Load(const std::string& indexPath);

    /// 保存索引到文件
    bool Save(const std::string& indexPath);

    /// 添加向量（已 L2 归一化），返回分配的 faiss id
    /// @param vec  float 向量（长度必须等于 dim_）
    /// @param id   外部指定的 id（用于 IDMap）
    bool AddVector(const std::vector<float>& vec, int64_t id);

    /// 批量添加
    bool AddVectors(const std::vector<std::vector<float>>& vecs,
                    const std::vector<int64_t>& ids);

    /// 搜索最近邻
    /// @param query  查询向量（已 L2 归一化）
    /// @param topK   返回数量
    /// @param outIds     输出 id 列表
    /// @param outScores  输出相似度分数列表
    bool Search(const std::vector<float>& query, int topK,
                std::vector<int64_t>& outIds,
                std::vector<float>& outScores);

    /// 重置索引（清空所有向量）
    void Reset();

    int64_t GetCount() const;
    int GetDim() const { return dim_; }

    /// L2 归一化（原地）
    static void L2Normalize(std::vector<float>& vec);

private:
    int dim_;
    std::string indexPath_;
    std::unique_ptr<faiss::IndexIDMap> index_;
    mutable std::mutex mutex_;
};

/// 管理三个索引：clip(768d) / wd14(1024d) / face(512d)
class FaissIndexManager {
public:
    FaissIndexManager();

    bool Init(const std::string& indexDir);

    FaissIndex* GetClipIndex()  { return clipIndex_.get(); }
    FaissIndex* GetWd14Index()  { return wd14Index_.get(); }
    FaissIndex* GetFaceIndex()  { return faceIndex_.get(); }

    bool SaveAll();

    static constexpr int CLIP_DIM = 768;
    static constexpr int WD14_DIM = 1024;
    static constexpr int FACE_DIM = 512;

private:
    std::string indexDir_;
    std::unique_ptr<FaissIndex> clipIndex_;
    std::unique_ptr<FaissIndex> wd14Index_;
    std::unique_ptr<FaissIndex> faceIndex_;
};
