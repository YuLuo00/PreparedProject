#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct MediaInfo;

class BiliApiClient
{
public:
    struct AuthorInfo
    {
        struct OfficialInfo
        {
            int role = 0;
            int type = 0;
            std::wstring title;
            std::wstring desc;
        };

        struct LiveRoomInfo
        {
            int room_status = 0;
            int live_status = 0;
            int round_status = 0;
            std::int64_t room_id = 0;
            std::wstring url;
            std::wstring title;
            std::wstring cover;
        };

        std::int64_t mid = 0;
        std::wstring name;
        std::wstring sex;
        std::wstring face;
        std::wstring sign;
        int rank = 0;
        int level = 0;
        bool fans_badge = false;
        std::wstring top_photo;
        std::wstring birthday;
        OfficialInfo official;
        LiveRoomInfo live_room;
        std::vector<std::wstring> tags;
    };

    struct HttpResponse
    {
        long status_code = 0;
        std::string content_type;
        std::string final_url;
        std::vector<std::uint8_t> body;
    };

    struct DownloadOptions
    {
        long connect_timeout_ms = 10000;
        long request_timeout_ms = 30000;
        bool follow_redirects = true;
    };

    BiliApiClient() = default;
    explicit BiliApiClient(DownloadOptions options);

    const DownloadOptions &GetOptions() const;
    void SetOptions(const DownloadOptions &options);

    // Download the image pointed to by MediaInfo.cover into memory.
    bool DownloadCover(const MediaInfo &mediaInfo,
                       HttpResponse &outResponse,
                       std::string *errMsg = nullptr) const;

    // Download MediaInfo.cover and save it to the specified file path.
    bool DownloadCoverToFile(const MediaInfo &mediaInfo,
                             const std::filesystem::path &outputPath,
                             std::string *errMsg = nullptr) const;

    // Download MediaInfo.cover and auto-generate a file name under outputDir.
    bool DownloadCoverToDirectory(const MediaInfo &mediaInfo,
                                  const std::filesystem::path &outputDir,
                                  std::filesystem::path &outPath,
                                  std::string *errMsg = nullptr) const;

    // Query author info by Bilibili mid.
    bool GetAuthorInfo(int ownerId, AuthorInfo &outInfo, std::string *errMsg = nullptr) const;

    // Query author info from MediaInfo.owner_id.
    bool GetAuthorInfo(const MediaInfo &mediaInfo, AuthorInfo &outInfo, std::string *errMsg = nullptr) const;

    // Build a default cover file path from MediaInfo without doing network IO.
    std::filesystem::path BuildCoverFilePath(const MediaInfo &mediaInfo,
                                             const std::filesystem::path &outputDir) const;

private:
    bool HttpGetBinary(const std::string &url, HttpResponse &outResponse, std::string *errMsg) const;
    bool ParseAuthorInfoResponse(const std::string &body, AuthorInfo &outInfo, std::string *errMsg) const;

    DownloadOptions m_options;
};
