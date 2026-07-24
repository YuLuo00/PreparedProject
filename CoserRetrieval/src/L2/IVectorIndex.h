#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace coser {

class IVectorIndex {
public:
    virtual ~IVectorIndex() = default;

    virtual bool Load(const std::string& indexPath) = 0;
    virtual bool Save(const std::string& indexPath = "") = 0;

    virtual bool Add(int64_t id, const std::vector<float>& vec) = 0;
    virtual void Remove(int64_t id) = 0;

    virtual bool Search(const std::vector<float>& query, int topK,
                         std::vector<int64_t>& outIds,
                         std::vector<float>& outScores) = 0;

    virtual int64_t Size() const = 0;
    virtual int GetDim() const = 0;
};

}  // namespace coser
