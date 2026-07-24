#include "FaissFlatIpIndex.h"
#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>
#include <faiss/impl/IDSelector.h>
#include <filesystem>
#include <cmath>
#include <algorithm>

namespace fs = std::filesystem;

namespace coser {

// 实现方式与 test\src\FaissIndex 一致（Step1-Step9 已验证）：
//   创建空索引：IndexFlatIP + IndexIDMap(栈上) + write_index → read_index
//   不 delete read_index 返回的指针（避免跨堆崩溃）

FaissFlatIpIndex::FaissFlatIpIndex(int dim) : dim_(dim), count_(0), index_(nullptr) {}

FaissFlatIpIndex::~FaissFlatIpIndex() {
    index_ = nullptr;  // 不 delete，避免 MSVC delete MinGW 堆上的对象
}

bool FaissFlatIpIndex::Load(const std::string& indexPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    indexPath_ = indexPath;

    if (fs::exists(indexPath)) {
        faiss::Index* loaded = faiss::read_index(indexPath.c_str());
        index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
        count_ = index_ ? index_->ntotal : 0;
        return index_ != nullptr;
    }

    try {
        fs::path p(indexPath);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        faiss::IndexFlatIP flat(dim_);
        faiss::IndexIDMap idxmap(&flat);
        faiss::write_index(&idxmap, indexPath.c_str());
    } catch (...) {
        return false;
    }

    faiss::Index* loaded = faiss::read_index(indexPath.c_str());
    index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
    count_ = 0;
    return index_ != nullptr;
}

bool FaissFlatIpIndex::Save(const std::string& indexPath) {
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

void FaissFlatIpIndex::L2Normalize(std::vector<float>& vec) {
    float norm = 0.0f;
    for (float v : vec) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-10f) for (float& v : vec) v /= norm;
}

bool FaissFlatIpIndex::Add(int64_t id, const std::vector<float>& vec) {
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

void FaissFlatIpIndex::Remove(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!index_) return;
    faiss::idx_t fid = id;
    faiss::IDSelectorArray sel(1, &fid);
    try {
        size_t removed = index_->remove_ids(sel);
        if (removed > 0) count_ -= static_cast<int64_t>(removed);
    } catch (...) {}
}

bool FaissFlatIpIndex::Search(const std::vector<float>& query, int topK,
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

int64_t FaissFlatIpIndex::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

}  // namespace coser
