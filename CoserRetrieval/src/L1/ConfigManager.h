#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace coser {

class ConfigManager {
public:
    static ConfigManager& Instance();

    bool Load(const std::string& configPath);

    template <typename T>
    T Get(const std::string& dottedKey, const T& defaultValue) const {
        const nlohmann::json* node = &root_;
        size_t start = 0;
        while (start <= dottedKey.size()) {
            size_t dot = dottedKey.find('.', start);
            std::string part = dottedKey.substr(start, dot == std::string::npos ? std::string::npos : dot - start);
            if (!node->is_object() || !node->contains(part)) return defaultValue;
            node = &(*node)[part];
            if (dot == std::string::npos) break;
            start = dot + 1;
        }
        try {
            return node->get<T>();
        } catch (...) {
            return defaultValue;
        }
    }

private:
    ConfigManager() = default;
    nlohmann::json root_;
};

}  // namespace coser
