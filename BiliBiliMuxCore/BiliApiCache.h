#pragma once

#include <filesystem>
#include <string>

#include "BiliApiClient.h"

struct MediaInfo;

class BiliApiCache
{
public:
    using AuthorInfo = BiliApiClient::AuthorInfo;

    struct Options
    {
        std::filesystem::path cache_root = std::filesystem::path("cache/bilibili_api");
        bool create_directories = true;
    };

    BiliApiCache() = default;
    explicit BiliApiCache(Options options);
    BiliApiCache(BiliApiClient client, Options options = {});

    const Options &GetOptions() const;
    void SetOptions(const Options &options);

    BiliApiClient &GetClient();
    const BiliApiClient &GetClient() const;

    std::filesystem::path BuildAuthorInfoCachePath(int ownerId) const;
    bool HasAuthorInfoCache(int ownerId) const;

    bool GetAuthorInfo(int ownerId,
                       AuthorInfo &outInfo,
                       bool *loadedFromCache = nullptr,
                       std::string *errMsg = nullptr);

    bool GetAuthorInfo(const MediaInfo &mediaInfo,
                       AuthorInfo &outInfo,
                       bool *loadedFromCache = nullptr,
                       std::string *errMsg = nullptr);

    bool LoadAuthorInfo(int ownerId, AuthorInfo &outInfo, std::string *errMsg = nullptr) const;
    bool SaveAuthorInfo(int ownerId, const AuthorInfo &info, std::string *errMsg = nullptr) const;

    // Cover cache methods
    std::filesystem::path BuildCoverCachePath(const std::string& coverUrl) const;
    bool HasCoverCache(const std::string& coverUrl) const;
    bool DownloadCoverToCache(const std::string& coverUrl,
                              std::filesystem::path& outPath,
                              std::string* errMsg = nullptr);

private:
    bool EnsureCacheDirectory(const std::filesystem::path &dir, std::string *errMsg) const;

    Options m_options;
    BiliApiClient m_client;
};
