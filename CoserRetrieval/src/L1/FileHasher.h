#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace coser {

// Returns the lowercase hexadecimal MD5 of a file without loading it all into memory.
std::optional<std::string> ComputeFileMd5(const std::filesystem::path& path,
                                          std::string* error = nullptr);

}  // namespace coser
