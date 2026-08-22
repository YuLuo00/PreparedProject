#include <windows.h>
#include <cstring>
#include <string>

#include "../src/L1/FileHasher.h"

namespace {
std::string WideToUtf8(const wchar_t* value) {
    if (!value) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string result(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);
    return result;
}

bool WriteWide(const std::wstring& value, wchar_t* output, int capacity) {
    if (!output || capacity <= 0 || static_cast<int>(value.size()) >= capacity) return false;
    memcpy(output, value.c_str(), (value.size() + 1) * sizeof(wchar_t));
    return true;
}
}  // namespace

extern "C" __declspec(dllexport) int CoserBridge_GetBuildInfo(wchar_t* output, int capacity) {
    return WriteWide(L"CoserRetrievalCore bridge: FileHasher ready", output, capacity) ? 0 : -1;
}

extern "C" __declspec(dllexport) int CoserBridge_ComputeMd5(
    const wchar_t* filePath, wchar_t* output, int capacity) {
    std::string error;
    auto hash = coser::ComputeFileMd5(WideToUtf8(filePath), &error);
    if (!hash) return -2;
    return WriteWide(std::wstring(hash->begin(), hash->end()), output, capacity) ? 0 : -1;
}
