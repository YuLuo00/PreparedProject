#include "PHashIndex.h"
#include "../L3/PHasher.h"
#include <algorithm>

namespace coser {

void PHashIndex::Add(int64_t imageId, uint64_t hash) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.emplace_back(imageId, hash);
}

void PHashIndex::Remove(int64_t imageId) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
        [imageId](const auto& entry) { return entry.first == imageId; }), entries_.end());
}

void PHashIndex::LoadFrom(const std::vector<std::pair<int64_t, int64_t>>& imageIdHashPairs) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
    entries_.reserve(imageIdHashPairs.size());
    for (const auto& p : imageIdHashPairs) {
        entries_.emplace_back(p.first, static_cast<uint64_t>(p.second));
    }
}

std::vector<PHashHit> PHashIndex::Search(uint64_t queryHash, int maxHammingDist) const {
    std::vector<PHashHit> hits;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : entries_) {
        int dist = PHasher::HammingDistance(entry.second, queryHash);
        if (dist <= maxHammingDist) {
            hits.push_back({entry.first, dist});
        }
    }
    std::sort(hits.begin(), hits.end(), [](const PHashHit& a, const PHashHit& b) {
        return a.hamming_dist < b.hamming_dist;
    });
    return hits;
}

}  // namespace coser
