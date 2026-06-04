#pragma once
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>

/// 封装单个 Faiss IndexFlatIP（内积/余弦相似度）
/// 实现方式经过逐步验证（Step1-Step9 全部通过）：
///   创建空索引：IndexFlatIP + IndexIDMap(栈上) + write_index → read_index
///   加载索引：read_index → ntotal 可正常读取
///   不 delete read_index 返回的指针（避免跨堆崩溃）
class FaissIndex {
public:
    explicit FaissIndex(int dim);
    ~FaissIndex();  // 不 delete index_，避免跨堆崩溃

    /// 加载已有索引文件，不存在则创建空索引
    bool Load(const std::string& indexPath);

    /// 保存索引到文件
    bool Save(const std::string& indexPath = "");

    /// 添加向量（自动 L2 归一化）
    bool AddVector(const std::vector<float>& vec, int64_t id);

    /// 批量添加
    bool AddVectors(const std::vector<std::vector<float>>& vecs,
                    const std::vector<int64_t>& ids);

    /// 搜索最近邻
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
    faiss::IndexIDMap* index_ = nullptr;  // raw ptr，不跨堆 delete
    int64_t count_ = 0;
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
