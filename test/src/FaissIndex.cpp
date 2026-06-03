#include "FaissIndex.h"
#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>
#include <filesystem>
#include <cmath>
#include <stdexcept>

namespace fs = std::filesystem;

FaissIndex::FaissIndex(int dim) : dim_(dim) {
    auto* flat = new faiss::IndexFlatIP(dim);
    index_ = std::make_unique<faiss::IndexIDMap>(flat);
}

bool FaissIndex::Load(const std::string& indexPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    indexPath_ = indexPath;
    if (fs::exists(indexPath)) {
        try {
            faiss::Index* loaded = faiss::read_index(indexPath.c_str());
            index_.reset(dynamic_cast<faiss::IndexIDMap*>(loaded));
            if (!index_) {
                // 如果类型不匹配，重新创建
                delete loaded;
                auto* flat = new faiss::IndexFlatIP(dim_);
                index_ = std::make_unique<faiss::IndexIDMap>(flat);
            }
        } catch (...) {
            auto* flat = new faiss::IndexFlatIP(dim_);
            index_ = std::make_unique<faiss::IndexIDMap>(flat);
        }
    }
    return true;
}

bool FaissIndex::Save(const std::string& indexPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string path = indexPath.empty() ? indexPath_ : indexPath;
    if (path.empty()) return false;
    try {
        // 确保目录存在
        fs::path p(path);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }
        faiss::write_index(index_.get(), path.c_str());
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
    if ((int)vec.size() != dim_) return false;
    std::vector<float> normalized = vec;
    L2Normalize(normalized);
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        index_->add_with_ids(1, normalized.data(), &id);
        return true;
    } catch (...) {
        return false;
    }
}

bool FaissIndex::AddVectors(const std::vector<std::vector<float>>& vecs,
                             const std::vector<int64_t>& ids) {
    if (vecs.empty() || vecs.size() != ids.size()) return false;
    // 展平并归一化
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
        return true;
    } catch (...) {
        return false;
    }
}

bool FaissIndex::Search(const std::vector<float>& query, int topK,
                         std::vector<int64_t>& outIds,
                         std::vector<float>& outScores) {
    if ((int)query.size() != dim_) return false;
    std::vector<float> normalized = query;
    L2Normalize(normalized);

    outIds.resize(topK, -1);
    outScores.resize(topK, 0.0f);

    std::lock_guard<std::mutex> lock(mutex_);
    if (index_->ntotal == 0) return true;

    int actualK = std::min(topK, (int)index_->ntotal);
    outIds.resize(actualK);
    outScores.resize(actualK);

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
    auto* flat = new faiss::IndexFlatIP(dim_);
    index_ = std::make_unique<faiss::IndexIDMap>(flat);
}

int64_t FaissIndex::GetCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return index_ ? index_->ntotal : 0;
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
