#include "FaissIndex.h"
#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

// ─── FaissIndex ───────────────────────────────────────────────────────────────
// 实现方式经过逐步验证（Step1-Step9 全部通过）：
//   创建空索引：IndexFlatIP + IndexIDMap(栈上) + write_index → read_index
//   加载索引：read_index → ntotal 可正常读取
//   不 delete read_index 返回的指针（避免跨堆崩溃）

FaissIndex::FaissIndex(int dim) : dim_(dim), count_(0), index_(nullptr) {}

FaissIndex::~FaissIndex() {
    // 不 delete index_，避免 MSVC delete MinGW 堆上的对象
    index_ = nullptr;
}

bool FaissIndex::Load(const std::string& indexPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    indexPath_ = indexPath;

    if (fs::exists(indexPath)) {
        faiss::Index* loaded = faiss::read_index(indexPath.c_str());
        index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
        count_ = index_ ? index_->ntotal : 0;
        return index_ != nullptr;
    }

    // 文件不存在：创建空索引文件再加载
    try {
        fs::path p(indexPath);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        faiss::IndexFlatIP flat(dim_);
        faiss::IndexIDMap idxmap(&flat);  // own_fields=false，不会 delete flat
        faiss::write_index(&idxmap, indexPath.c_str());
    } catch (...) {
        return false;
    }

    faiss::Index* loaded = faiss::read_index(indexPath.c_str());
    index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
    count_ = 0;
    return index_ != nullptr;
}

bool FaissIndex::Save(const std::string& indexPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = indexPath.empty() ? indexPath_ : indexPath;
    if (path.empty() || !index_) return false;
    try {
        fs::path p(path);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
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
    if (norm > 1e-10f) for (float& v : vec) v /= norm;
}

bool FaissIndex::AddVector(const std::vector<float>& vec, int64_t id) {
    if ((int)vec.size() != dim_ || !index_) return false;
    std::vector<float> n = vec;
    L2Normalize(n);
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        index_->add_with_ids(1, n.data(), &id);
        ++count_;
        return true;
    } catch (...) { return false; }
}

bool FaissIndex::AddVectors(const std::vector<std::vector<float>>& vecs,
                             const std::vector<int64_t>& ids) {
    if (vecs.empty() || vecs.size() != ids.size() || !index_) return false;
    std::vector<float> flat;
    flat.reserve(vecs.size() * dim_);
    for (auto& v : vecs) {
        if ((int)v.size() != dim_) return false;
        std::vector<float> n = v;
        L2Normalize(n);
        flat.insert(flat.end(), n.begin(), n.end());
    }
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        index_->add_with_ids((faiss::idx_t)vecs.size(), flat.data(), ids.data());
        count_ += (int64_t)vecs.size();
        return true;
    } catch (...) { return false; }
}

bool FaissIndex::Search(const std::vector<float>& query, int topK,
                         std::vector<int64_t>& outIds,
                         std::vector<float>& outScores) {
    if ((int)query.size() != dim_ || !index_) return false;
    std::vector<float> n = query;
    L2Normalize(n);
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ == 0) { outIds.clear(); outScores.clear(); return true; }
    int actualK = std::min(topK, (int)count_);
    outIds.resize(actualK, -1);
    outScores.resize(actualK, 0.0f);
    try {
        index_->search(1, n.data(), actualK, outScores.data(), outIds.data());
        return true;
    } catch (...) { return false; }
}

void FaissIndex::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    // 重新创建空索引（不 delete 旧的，避免跨堆崩溃）
    if (!indexPath_.empty()) {
        try {
            faiss::IndexFlatIP flat(dim_);
            faiss::IndexIDMap idxmap(&flat);
            faiss::write_index(&idxmap, indexPath_.c_str());
            faiss::Index* loaded = faiss::read_index(indexPath_.c_str());
            index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
        } catch (...) {}
    }
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
