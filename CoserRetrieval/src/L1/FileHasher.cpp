#include "FileHasher.h"

#include <array>
#include <fstream>
#include <vector>
#include <windows.h>
#include <bcrypt.h>

namespace coser {

std::optional<std::string> ComputeFileMd5(const std::filesystem::path& path, std::string* error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "cannot open file for hashing";
        return std::nullopt;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectLength = 0;
    DWORD resultLength = 0;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0);
    if (status < 0) {
        if (error) *error = "failed to initialize MD5 provider";
        return std::nullopt;
    }

    auto closeHandles = [&]() {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    };

    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                               reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength),
                               &resultLength, 0);
    if (status < 0) {
        closeHandles();
        if (error) *error = "failed to read MD5 provider properties";
        return std::nullopt;
    }

    std::vector<UCHAR> hashObject(objectLength);
    status = BCryptCreateHash(algorithm, &hash, hashObject.data(), objectLength, nullptr, 0, 0);
    if (status < 0) {
        closeHandles();
        if (error) *error = "failed to create MD5 hash";
        return std::nullopt;
    }

    std::array<char, 64 * 1024> buffer;
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        std::streamsize count = input.gcount();
        if (count <= 0) break;
        status = BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()),
                                static_cast<ULONG>(count), 0);
        if (status < 0) {
            closeHandles();
            if (error) *error = "failed while calculating MD5";
            return std::nullopt;
        }
    }
    if (!input.eof()) {
        closeHandles();
        if (error) *error = "failed while reading file for MD5";
        return std::nullopt;
    }

    std::array<UCHAR, 16> digest{};
    status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    closeHandles();
    if (status < 0) {
        if (error) *error = "failed to finalize MD5";
        return std::nullopt;
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string result;
    result.reserve(digest.size() * 2);
    for (UCHAR byte : digest) {
        result.push_back(kHex[byte >> 4]);
        result.push_back(kHex[byte & 0x0f]);
    }
    return result;
}

}  // namespace coser
