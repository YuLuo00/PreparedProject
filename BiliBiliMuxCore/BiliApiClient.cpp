#include "BiliApiClient.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "Logger.h"
#include "attach.h"
#include "tools.h"

namespace
{
std::once_flag g_curlInitOnce;
CURLcode g_curlInitCode = CURLE_OK;

void SetError(std::string *errMsg, const std::string &message)
{
    if (errMsg) {
        *errMsg = message;
    }
    LOG_ERROR(LogGroup::IO, "{}", message);
}

bool EnsureCurlInitialized(std::string *errMsg)
{
    std::call_once(g_curlInitOnce, []() {
        g_curlInitCode = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (g_curlInitCode == CURLE_OK) {
            std::atexit([]() { curl_global_cleanup(); });
        }
    });

    if (g_curlInitCode != CURLE_OK) {
        SetError(errMsg, "curl_global_init failed: " + std::string(curl_easy_strerror(g_curlInitCode)));
        return false;
    }
    return true;
}

void SetErrorWithPrefix(std::string *errMsg, const std::string &prefix, const std::string &message)
{
    SetError(errMsg, prefix + message);
}

std::wstring JsonStringToWString(const nlohmann::json &j, const char *key)
{
    if (!j.is_object()) {
        return {};
    }

    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) {
        return {};
    }

    return Tools::utf8_to_wstring(it->get<std::string>());
}

int JsonIntOrDefault(const nlohmann::json &j, const char *key, int defaultValue = 0)
{
    if (!j.is_object()) {
        return defaultValue;
    }

    const auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) {
        return defaultValue;
    }

    return it->get<int>();
}

std::int64_t JsonInt64OrDefault(const nlohmann::json &j, const char *key, std::int64_t defaultValue = 0)
{
    if (!j.is_object()) {
        return defaultValue;
    }

    const auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) {
        return defaultValue;
    }

    return it->get<std::int64_t>();
}

bool JsonBoolOrDefault(const nlohmann::json &j, const char *key, bool defaultValue = false)
{
    if (!j.is_object()) {
        return defaultValue;
    }

    const auto it = j.find(key);
    if (it == j.end() || !it->is_boolean()) {
        return defaultValue;
    }

    return it->get<bool>();
}

std::string BuildAuthorInfoUrl(int ownerId)
{
    std::ostringstream oss;
    oss << "https://api.bilibili.com/x/space/acc/info?mid=" << ownerId << "&jsonp=jsonp";
    return oss.str();
}

size_t WriteToVector(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    if (!ptr || !userdata) {
        return 0;
    }

    const size_t bytes = size * nmemb;
    auto *buffer = static_cast<std::vector<std::uint8_t> *>(userdata);
    const auto *begin = static_cast<const std::uint8_t *>(ptr);
    buffer->insert(buffer->end(), begin, begin + bytes);
    return bytes;
}

std::string NormalizeUrl(const std::string &url)
{
    if (url.rfind("//", 0) == 0) {
        return "https:" + url;
    }
    return url;
}

std::string TrimQueryAndFragment(const std::string &url)
{
    size_t end = url.find_first_of("?#");
    if (end == std::string::npos) {
        end = url.size();
    }
    return url.substr(0, end);
}

bool IsAsciiAlphaNum(char ch)
{
    const unsigned char uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) != 0;
}

bool IsSafeExtension(const std::string &ext)
{
    if (ext.size() < 2 || ext.size() > 6 || ext.front() != '.') {
        return false;
    }

    for (size_t i = 1; i < ext.size(); ++i) {
        if (!IsAsciiAlphaNum(ext[i])) {
            return false;
        }
    }
    return true;
}

std::string ToLowerAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::string GuessExtensionFromUrl(const std::string &url)
{
    const std::string cleanUrl = TrimQueryAndFragment(url);
    const size_t slashPos = cleanUrl.find_last_of('/');
    const size_t dotPos = cleanUrl.find_last_of('.');
    if (dotPos == std::string::npos) {
        return {};
    }
    if (slashPos != std::string::npos && dotPos < slashPos) {
        return {};
    }

    std::string ext = ToLowerAscii(cleanUrl.substr(dotPos));
    if (!IsSafeExtension(ext)) {
        return {};
    }

    return ext;
}

std::string GuessExtensionFromContentType(const std::string &contentType)
{
    const std::string lowerType = ToLowerAscii(contentType);
    if (lowerType.find("image/jpeg") != std::string::npos || lowerType.find("image/jpg") != std::string::npos) {
        return ".jpg";
    }
    if (lowerType.find("image/png") != std::string::npos) {
        return ".png";
    }
    if (lowerType.find("image/webp") != std::string::npos) {
        return ".webp";
    }
    if (lowerType.find("image/gif") != std::string::npos) {
        return ".gif";
    }
    if (lowerType.find("image/bmp") != std::string::npos) {
        return ".bmp";
    }
    if (lowerType.find("image/tiff") != std::string::npos) {
        return ".tiff";
    }
    return {};
}

std::wstring BuildCoverBaseName(const MediaInfo &mediaInfo)
{
    if (!mediaInfo.download_title.empty()) {
        return mediaInfo.download_title;
    }
    if (!mediaInfo.title.empty()) {
        return mediaInfo.title;
    }
    if (!mediaInfo.bvid.empty()) {
        return mediaInfo.bvid;
    }
    if (mediaInfo.avid > 0) {
        return L"av" + std::to_wstring(mediaInfo.avid);
    }
    return L"cover";
}

std::filesystem::path BuildCoverFilePathImpl(const MediaInfo &mediaInfo,
                                             const std::filesystem::path &outputDir,
                                             const std::string &preferredExt)
{
    std::wstring baseName = Tools::sanitize_windows_filename(BuildCoverBaseName(mediaInfo));
    if (baseName.empty()) {
        baseName = L"cover";
    }

    std::string ext = preferredExt;
    if (ext.empty()) {
        try {
            ext = GuessExtensionFromUrl(Tools::wstring_to_utf8(mediaInfo.cover));
        }
        catch (...) {
            ext.clear();
        }
    }
    if (ext.empty()) {
        ext = ".jpg";
    }

    const std::string fileNameUtf8 = Tools::wstring_to_utf8(baseName) + "_cover" + ext;
    return outputDir / std::filesystem::path(fileNameUtf8);
}

bool WriteBytesToFile(const std::filesystem::path &outputPath,
                      const std::vector<std::uint8_t> &body,
                      std::string *errMsg)
{
    try {
        const std::filesystem::path parent = outputPath.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        const std::string outputPathUtf8 = outputPath.u8string();
        const std::string outputPathLocal = Tools::Utf8ToLocal(outputPathUtf8);

        std::ofstream ofs(outputPathLocal, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            SetError(errMsg, "Failed to open output file: " + outputPathUtf8);
            return false;
        }

        if (!body.empty()) {
            ofs.write(reinterpret_cast<const char *>(body.data()), static_cast<std::streamsize>(body.size()));
        }

        if (!ofs.good()) {
            SetError(errMsg, "Failed to write output file: " + outputPathUtf8);
            return false;
        }

        return true;
    }
    catch (const std::exception &e) {
        SetError(errMsg, "Write cover file failed: " + std::string(e.what()));
        return false;
    }
}
} // namespace

BiliApiClient::BiliApiClient(DownloadOptions options)
    : m_options(std::move(options))
{
}

const BiliApiClient::DownloadOptions &BiliApiClient::GetOptions() const
{
    return m_options;
}

void BiliApiClient::SetOptions(const DownloadOptions &options)
{
    m_options = options;
}

bool BiliApiClient::DownloadCover(const MediaInfo &mediaInfo, HttpResponse &outResponse, std::string *errMsg) const
{
    outResponse = {};

    try {
        std::string url = NormalizeUrl(Tools::wstring_to_utf8(mediaInfo.cover));
        if (url.empty()) {
            SetError(errMsg, "MediaInfo.cover is empty.");
            return false;
        }

        if (!HttpGetBinary(url, outResponse, errMsg)) {
            return false;
        }

        LOG_INFO(LogGroup::IO,
                 "Cover downloaded: title='{}', bytes={}, status={}",
                 LOGUTF8(mediaInfo.title),
                 outResponse.body.size(),
                 outResponse.status_code);
        return true;
    }
    catch (const std::exception &e) {
        SetError(errMsg, "DownloadCover failed: " + std::string(e.what()));
        return false;
    }
}

bool BiliApiClient::DownloadCoverToFile(const MediaInfo &mediaInfo,
                                        const std::filesystem::path &outputPath,
                                        std::string *errMsg) const
{
    HttpResponse response;
    if (!DownloadCover(mediaInfo, response, errMsg)) {
        return false;
    }

    if (!WriteBytesToFile(outputPath, response.body, errMsg)) {
        return false;
    }

    LOG_INFO(LogGroup::IO, "Cover saved: {}", outputPath.u8string());
    return true;
}

bool BiliApiClient::DownloadCoverToDirectory(const MediaInfo &mediaInfo,
                                             const std::filesystem::path &outputDir,
                                             std::filesystem::path &outPath,
                                             std::string *errMsg) const
{
    HttpResponse response;
    if (!DownloadCover(mediaInfo, response, errMsg)) {
        return false;
    }

    const std::string ext = GuessExtensionFromContentType(response.content_type);
    outPath = BuildCoverFilePathImpl(mediaInfo, outputDir, ext);
    if (!WriteBytesToFile(outPath, response.body, errMsg)) {
        outPath.clear();
        return false;
    }

    LOG_INFO(LogGroup::IO, "Cover saved: {}", outPath.u8string());
    return true;
}

bool BiliApiClient::GetAuthorInfo(int ownerId, AuthorInfo &outInfo, std::string *errMsg) const
{
    outInfo = {};

    if (ownerId <= 0) {
        SetError(errMsg, "ownerId must be greater than zero.");
        return false;
    }

    HttpResponse response;
    if (!HttpGetBinary(BuildAuthorInfoUrl(ownerId), response, errMsg)) {
        return false;
    }

    if (!ParseAuthorInfoResponse(std::string(response.body.begin(), response.body.end()), outInfo, errMsg)) {
        return false;
    }

    LOG_INFO(LogGroup::IO,
             "Author info loaded: mid={}, name='{}'",
             outInfo.mid,
             LOGUTF8(outInfo.name));
    return true;
}

bool BiliApiClient::GetAuthorInfo(const MediaInfo &mediaInfo, AuthorInfo &outInfo, std::string *errMsg) const
{
    return GetAuthorInfo(mediaInfo.owner_id, outInfo, errMsg);
}

std::filesystem::path BiliApiClient::BuildCoverFilePath(const MediaInfo &mediaInfo,
                                                        const std::filesystem::path &outputDir) const
{
    return BuildCoverFilePathImpl(mediaInfo, outputDir, std::string());
}

bool BiliApiClient::ParseAuthorInfoResponse(const std::string &body,
                                            AuthorInfo &outInfo,
                                            std::string *errMsg) const
{
    outInfo = {};

    try {
        const nlohmann::json root = nlohmann::json::parse(body);

        const int code = JsonIntOrDefault(root, "code", -1);
        if (code != 0) {
            const std::wstring messageW = JsonStringToWString(root, "message");
            const std::string message = messageW.empty() ? std::string("unknown") : Tools::wstring_to_utf8(messageW);
            SetErrorWithPrefix(errMsg, "Bilibili author info API returned error: ", message);
            return false;
        }

        const auto dataIt = root.find("data");
        if (dataIt == root.end() || !dataIt->is_object()) {
            SetError(errMsg, "Bilibili author info API response does not contain a valid data object.");
            return false;
        }

        const nlohmann::json &data = *dataIt;
        outInfo.mid = JsonInt64OrDefault(data, "mid");
        outInfo.name = JsonStringToWString(data, "name");
        outInfo.sex = JsonStringToWString(data, "sex");
        outInfo.face = JsonStringToWString(data, "face");
        outInfo.sign = JsonStringToWString(data, "sign");
        outInfo.rank = JsonIntOrDefault(data, "rank");
        outInfo.level = JsonIntOrDefault(data, "level");
        outInfo.fans_badge = JsonBoolOrDefault(data, "fans_badge");
        outInfo.top_photo = JsonStringToWString(data, "top_photo");
        outInfo.birthday = JsonStringToWString(data, "birthday");

        const auto officialIt = data.find("official");
        if (officialIt != data.end() && officialIt->is_object()) {
            outInfo.official.role = JsonIntOrDefault(*officialIt, "role");
            outInfo.official.type = JsonIntOrDefault(*officialIt, "type");
            outInfo.official.title = JsonStringToWString(*officialIt, "title");
            outInfo.official.desc = JsonStringToWString(*officialIt, "desc");
        }

        const auto liveRoomIt = data.find("live_room");
        if (liveRoomIt != data.end() && liveRoomIt->is_object()) {
            outInfo.live_room.room_status = JsonIntOrDefault(*liveRoomIt, "roomStatus");
            outInfo.live_room.live_status = JsonIntOrDefault(*liveRoomIt, "liveStatus");
            outInfo.live_room.round_status = JsonIntOrDefault(*liveRoomIt, "roundStatus");
            outInfo.live_room.room_id = JsonInt64OrDefault(*liveRoomIt, "roomid");
            outInfo.live_room.url = JsonStringToWString(*liveRoomIt, "url");
            outInfo.live_room.title = JsonStringToWString(*liveRoomIt, "title");
            outInfo.live_room.cover = JsonStringToWString(*liveRoomIt, "cover");
        }

        const auto tagsIt = data.find("tags");
        if (tagsIt != data.end() && tagsIt->is_array()) {
            outInfo.tags.reserve(tagsIt->size());
            for (const auto &tag : *tagsIt) {
                if (!tag.is_string()) {
                    continue;
                }
                outInfo.tags.push_back(Tools::utf8_to_wstring(tag.get<std::string>()));
            }
        }

        return true;
    }
    catch (const std::exception &e) {
        SetErrorWithPrefix(errMsg, "Parse author info response failed: ", e.what());
        return false;
    }
}

bool BiliApiClient::HttpGetBinary(const std::string &url, HttpResponse &outResponse, std::string *errMsg) const
{
    outResponse = {};

    if (!EnsureCurlInitialized(errMsg)) {
        return false;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        SetError(errMsg, "curl_easy_init failed.");
        return false;
    }

    std::array<char, CURL_ERROR_SIZE> errorBuffer{};
    curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/134.0.0.0 Safari/537.36");
    headers = curl_slist_append(headers, "Referer: https://www.bilibili.com/");
    headers = curl_slist_append(headers, "Accept: */*");

    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer.data());
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, m_options.follow_redirects ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, m_options.connect_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, m_options.request_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteToVector);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outResponse.body);

    const CURLcode performCode = curl_easy_perform(curl);
    if (performCode != CURLE_OK) {
        std::ostringstream oss;
        oss << "HTTP request failed: " << url << ", curl="
            << (errorBuffer[0] ? errorBuffer.data() : curl_easy_strerror(performCode));
        SetError(errMsg, oss.str());
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    char *contentType = nullptr;
    char *effectiveUrl = nullptr;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &outResponse.status_code);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effectiveUrl);

    if (contentType) {
        outResponse.content_type = contentType;
    }
    if (effectiveUrl) {
        outResponse.final_url = effectiveUrl;
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (outResponse.status_code < 200 || outResponse.status_code >= 300) {
        std::ostringstream oss;
        oss << "HTTP status is not successful: " << outResponse.status_code << ", url=" << url;
        SetError(errMsg, oss.str());
        return false;
    }

    return true;
}
