#pragma once
#include "IVectorIndex.h"
#include <mutex>
#include <faiss/IndexIDMap.h>

namespace coser {

/// 封装单个 Faiss IndexFlatIP（内积/余弦相似度），512 维人脸向量专用
/// 不 delete read_index 返回的指针（避免跨堆崩溃，MinGW faiss vs MSVC 调用方）
class FaissFlatIpIndex : public IVectorIndex {
public:
    explicit FaissFlatIpIndex(int dim);
    ~FaissFlatIpIndex() override;

    bool Load(const std::string& indexPath) override;
    bool Save(const std::string& indexPath = "") override;

    bool Add(int64_t id, const std::vector<float>& vec) override;
    void Remove(int64_t id) override;

    bool Search(const std::vector<float>& query, int topK,
                std::vector<int64_t>& outIds,
                std::vector<float>& outScores) override;

    int64_t Size() const override;
    int GetDim() const override { return dim_; }

    static void L2Normalize(std::vector<float>& vec);

private:
    int dim_;
    std::string indexPath_;
    faiss::IndexIDMap* index_ = nullptr;
    int64_t count_ = 0;
    mutable std::mutex mutex_;
};

}  // namespace coser
