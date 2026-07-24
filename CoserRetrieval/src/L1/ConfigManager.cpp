#include "ConfigManager.h"
#include <fstream>

namespace coser {

ConfigManager& ConfigManager::Instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::Load(const std::string& configPath) {
    std::ifstream f(configPath);
    if (!f.is_open()) return false;
    try {
        f >> root_;
    } catch (...) {
        return false;
    }
    return true;
}

}  // namespace coser
