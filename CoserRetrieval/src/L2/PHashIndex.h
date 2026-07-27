#pragma once
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace coser {

struct PHashHit {
    int64_t image_id = 0;
    int hamming_dist = 0;
};

/// In-memory brute-force pHash index. Hamming distance isn't natively SQL-indexable,
/// so candidates are loaded once at startup (tens of thousands scale, <1ms scan).
class PHashIndex {
public:
    void Add(int64_t imageId, uint64_t hash);
    void LoadFrom(const std::vector<std::pair<int64_t, int64_t>>& imageIdHashPairs);
    std::vector<PHashHit> Search(uint64_t queryHash, int maxHammingDist) const;

private:
    std::vector<std::pair<int64_t, uint64_t>> entries_;
    mutable std::mutex mutex_;
};

}  // namespace coser
