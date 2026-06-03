#include "FaissIndex.h"
#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>
#include <faiss/index_factory.h>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

// 使用 index_factory 在 faiss DLL 内部堆创建索引，避免 MSVC/MinGW 跨堆问题
static faiss::IndexIDMap* CreateFaissIndex(int dim) {
    faiss::Index* idx = faiss::index_factory(dim, "IDMap,Flat",
                                              faiss::METRIC_INNER_PRODUCT);
    return dynamic_cast<faiss::IndexIDMap*>(idx);
}

FaissIndex::FaissIndex(int dim) : dim_(dim) {
    index_ = CreateFaissIndex(dim);
}

// 析构：不 delete index_，避免 MSVC delete MinGW 堆上的对象导致 STATUS_HEAP_CORRUPTION
FaissIndex::~FaissIndex() {
    index_ = nullptr;
}

bool FaissIndex::Load(const std::string& indexPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    indexPath_ = indexPath;
    if (fs::exists(indexPath)) {
        try {
            faiss::Index* loaded = faiss::read_index(indexPath.c_str());
            auto* asIDMap = dynamic_cast<faiss::IndexIDMap*>(loaded);
            if (asIDMap) {
                index_ = asIDMap;
                // 读取 ntotal 更新 count_（faiss DLL 内部读取，安全）
                // 通过虚函数调用获取 ntotal，避免直接访问成员偏移量问题
                count_ = loaded->ntotal;
            } else {
                index_ = CreateFaissIndex(dim_);
                count_ = 0;
            }
        } catch (...) {
            index_ = CreateFaissIndex(dim_);
            count_ = 0;
        }
    }
    return true;
}

bool FaissIndex::Save(const std::string& indexPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = indexPath.empty() ? indexPath_ : indexPath;
    if (path.empty() || !index_) return false;
    try {
        fs::path p(path);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
        faiss::write_index(index_, path.c_str());
        return true;
    } catch (...) {
        return false;
    }
}

void FaissIndex::L2Normalize(std::vector<float>& vec) {
    float norm = 0.0f;
    for (float v : vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-10f) {
        for (float& v : vec) v /= norm;
    }
}

bool FaissIndex::AddVector(const std::vector<float>& vec, int64_t id) {
    if ((int)vec.size() != dim_ || !index_) return false;
    std::vector<float> normalized = vec;
    L2Normalize(normalized);
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        index_->add_with_ids(1, normalized.data(), &id);
        ++count_;
        return true;
    } catch (...) {
        return false;
    }
}

bool FaissIndex::AddVectors(const std::vector<std::vector<float>>& vecs,
                             const std::vector<int64_t>& ids) {
    if (vecs.empty() || vecs.size() != ids.size() || !index_) return false;
    std::vector<float> flat;
    flat.reserve(vecs.size() * dim_);
    for (auto& v : vecs) {
        if ((int)v.size() != dim_) return false;
        std::vector<float> norm = v;
        L2Normalize(norm);
        flat.insert(flat.end(), norm.begin(), norm.end());
    }
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        index_->add_with_ids((faiss::idx_t)vecs.size(), flat.data(), ids.data());
        count_ += (int64_t)vecs.size();
        return true;
    } catch (...) {
        return false;
    }
}

bool FaissIndex::Search(const std::vector<float>& query, int topK,
                         std::vector<int64_t>& outIds,
                         std::vector<float>& outScores) {
    if ((int)query.size() != dim_ || !index_) return false;
    std::vector<float> normalized = query;
    L2Normalize(normalized);

    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0) {
        outIds.clear();
        outScores.clear();
        return true;
    }

    int actualK = std::min(topK, (int)count_);
    outIds.resize(actualK, -1);
    outScores.resize(actualK, 0.0f);

    try {
        index_->search(1, normalized.data(), actualK,
                       outScores.data(), outIds.data());
        return true;
    } catch (...) {
        return false;
    }
}

void FaissIndex::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    index_ = CreateFaissIndex(dim_);
    count_ = 0;
}

int64_t FaissIndex::GetCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

// ─── FaissIndexManager ────────────────────────────────────────────────────────

FaissIndexManager::FaissIndexManager()
    : clipIndex_(std::make_unique<FaissIndex>(CLIP_DIM))
    , wd14Index_(std::make_unique<FaissIndex>(WD14_DIM))
    , faceIndex_(std::make_unique<FaissIndex>(FACE_DIM)) {
}

bool FaissIndexManager::Init(const std::string& indexDir) {
    indexDir_ = indexDir;
    fs::create_directories(indexDir);

    bool ok = true;
    ok &= clipIndex_->Load(indexDir + "/clip.index");
    ok &= wd14Index_->Load(indexDir + "/wd14.index");
    ok &= faceIndex_->Load(indexDir + "/face.index");
    return ok;
}

bool FaissIndexManager::SaveAll() {
    bool ok = true;
    ok &= clipIndex_->Save(indexDir_ + "/clip.index");
    ok &= wd14Index_->Save(indexDir_ + "/wd14.index");
    ok &= faceIndex_->Save(indexDir_ + "/face.index");
    return ok;
}
