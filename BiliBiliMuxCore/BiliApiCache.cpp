#include "BiliApiCache.h"

#include <ctime>
#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

#include "Logger.h"
#include "attach.h"
#include "tools.h"

namespace
{
using json = nlohmann::json;

void SetError(std::string *errMsg, const std::string &message)
{
    if (errMsg) {
        *errMsg = message;
    }
}

std::string WStringToUtf8(const std::wstring &text)
{
    return Tools::wstring_to_utf8(text);
}

std::wstring Utf8ToWString(const std::string &text)
{
    return Tools::utf8_to_wstring(text);
}

std::string PathToUtf8(const std::filesystem::path &path)
{
    return path.u8string();
}

std::string PathToLocal(const std::filesystem::path &path)
{
    return Tools::Utf8ToLocal(PathToUtf8(path));
}

json AuthorInfoToJson(const BiliApiClient::AuthorInfo &info)
{
    json j;
    j["mid"] = info.mid;
    j["name"] = WStringToUtf8(info.name);
    j["sex"] = WStringToUtf8(info.sex);
    j["face"] = WStringToUtf8(info.face);
    j["sign"] = WStringToUtf8(info.sign);
    j["rank"] = info.rank;
    j["level"] = info.level;
    j["fans_badge"] = info.fans_badge;
    j["top_photo"] = WStringToUtf8(info.top_photo);
    j["birthday"] = WStringToUtf8(info.birthday);

    j["official"] = {
        { "role", info.official.role },
        { "type", info.official.type },
        { "title", WStringToUtf8(info.official.title) },
        { "desc", WStringToUtf8(info.official.desc) },
    };

    j["live_room"] = {
        { "room_status", info.live_room.room_status },
        { "live_status", info.live_room.live_status },
        { "round_status", info.live_room.round_status },
        { "room_id", info.live_room.room_id },
        { "url", WStringToUtf8(info.live_room.url) },
        { "title", WStringToUtf8(info.live_room.title) },
        { "cover", WStringToUtf8(info.live_room.cover) },
    };

    json tags = json::array();
    for (const auto &tag : info.tags) {
        tags.push_back(WStringToUtf8(tag));
    }
    j["tags"] = std::move(tags);

    return j;
}

bool JsonToAuthorInfo(const json &j, BiliApiClient::AuthorInfo &outInfo, std::string *errMsg)
{
    outInfo = {};

    try {
        if (!j.is_object()) {
            SetError(errMsg, "Cached author info json is not an object.");
            return false;
        }

        if (const auto it = j.find("mid"); it != j.end() && it->is_number_integer()) {
            outInfo.mid = it->get<std::int64_t>();
        }
        if (const auto it = j.find("name"); it != j.end() && it->is_string()) {
            outInfo.name = Utf8ToWString(it->get<std::string>());
        }
        if (const auto it = j.find("sex"); it != j.end() && it->is_string()) {
            outInfo.sex = Utf8ToWString(it->get<std::string>());
        }
        if (const auto it = j.find("face"); it != j.end() && it->is_string()) {
            outInfo.face = Utf8ToWString(it->get<std::string>());
        }
        if (const auto it = j.find("sign"); it != j.end() && it->is_string()) {
            outInfo.sign = Utf8ToWString(it->get<std::string>());
        }
        if (const auto it = j.find("rank"); it != j.end() && it->is_number_integer()) {
            outInfo.rank = it->get<int>();
        }
        if (const auto it = j.find("level"); it != j.end() && it->is_number_integer()) {
            outInfo.level = it->get<int>();
        }
        if (const auto it = j.find("fans_badge"); it != j.end() && it->is_boolean()) {
            outInfo.fans_badge = it->get<bool>();
        }
        if (const auto it = j.find("top_photo"); it != j.end() && it->is_string()) {
            outInfo.top_photo = Utf8ToWString(it->get<std::string>());
        }
        if (const auto it = j.find("birthday"); it != j.end() && it->is_string()) {
            outInfo.birthday = Utf8ToWString(it->get<std::string>());
        }

        if (const auto it = j.find("official"); it != j.end() && it->is_object()) {
            const json &official = *it;
            if (const auto v = official.find("role"); v != official.end() && v->is_number_integer()) {
                outInfo.official.role = v->get<int>();
            }
            if (const auto v = official.find("type"); v != official.end() && v->is_number_integer()) {
                outInfo.official.type = v->get<int>();
            }
            if (const auto v = official.find("title"); v != official.end() && v->is_string()) {
                outInfo.official.title = Utf8ToWString(v->get<std::string>());
            }
            if (const auto v = official.find("desc"); v != official.end() && v->is_string()) {
                outInfo.official.desc = Utf8ToWString(v->get<std::string>());
            }
        }

        if (const auto it = j.find("live_room"); it != j.end() && it->is_object()) {
            const json &liveRoom = *it;
            if (const auto v = liveRoom.find("room_status"); v != liveRoom.end() && v->is_number_integer()) {
                outInfo.live_room.room_status = v->get<int>();
            }
            if (const auto v = liveRoom.find("live_status"); v != liveRoom.end() && v->is_number_integer()) {
                outInfo.live_room.live_status = v->get<int>();
            }
            if (const auto v = liveRoom.find("round_status"); v != liveRoom.end() && v->is_number_integer()) {
                outInfo.live_room.round_status = v->get<int>();
            }
            if (const auto v = liveRoom.find("room_id"); v != liveRoom.end() && v->is_number_integer()) {
                outInfo.live_room.room_id = v->get<std::int64_t>();
            }
            if (const auto v = liveRoom.find("url"); v != liveRoom.end() && v->is_string()) {
                outInfo.live_room.url = Utf8ToWString(v->get<std::string>());
            }
            if (const auto v = liveRoom.find("title"); v != liveRoom.end() && v->is_string()) {
                outInfo.live_room.title = Utf8ToWString(v->get<std::string>());
            }
            if (const auto v = liveRoom.find("cover"); v != liveRoom.end() && v->is_string()) {
                outInfo.live_room.cover = Utf8ToWString(v->get<std::string>());
            }
        }

        if (const auto it = j.find("tags"); it != j.end() && it->is_array()) {
            outInfo.tags.reserve(it->size());
            for (const auto &tag : *it) {
                if (!tag.is_string()) {
                    continue;
                }
                outInfo.tags.push_back(Utf8ToWString(tag.get<std::string>()));
            }
        }

        return true;
    }
    catch (const std::exception &e) {
        SetError(errMsg, "Failed to parse cached author info: " + std::string(e.what()));
        return false;
    }
}
} // namespace

BiliApiCache::BiliApiCache(Options options)
    : m_options(std::move(options))
{
}

BiliApiCache::BiliApiCache(BiliApiClient client, Options options)
    : m_options(std::move(options))
    , m_client(std::move(client))
{
}

const BiliApiCache::Options &BiliApiCache::GetOptions() const
{
    return m_options;
}

void BiliApiCache::SetOptions(const Options &options)
{
    m_options = options;
}

BiliApiClient &BiliApiCache::GetClient()
{
    return m_client;
}

const BiliApiClient &BiliApiCache::GetClient() const
{
    return m_client;
}

std::filesystem::path BiliApiCache::BuildAuthorInfoCachePath(int ownerId) const
{
    return m_options.cache_root / "author_info" / (std::to_string(ownerId) + ".json");
}

bool BiliApiCache::HasAuthorInfoCache(int ownerId) const
{
    try {
        const std::filesystem::path cachePath = BuildAuthorInfoCachePath(ownerId);
        return std::filesystem::exists(cachePath) && std::filesystem::is_regular_file(cachePath);
    }
    catch (...) {
        return false;
    }
}

bool BiliApiCache::GetAuthorInfo(int ownerId,
                                 AuthorInfo &outInfo,
                                 bool *loadedFromCache,
                                 std::string *errMsg)
{
    if (errMsg) {
        errMsg->clear();
    }

    if (loadedFromCache) {
        *loadedFromCache = false;
    }

    if (ownerId <= 0) {
        SetError(errMsg, "ownerId must be greater than zero.");
        return false;
    }

    if (HasAuthorInfoCache(ownerId)) {
        std::string cacheErr;
        if (LoadAuthorInfo(ownerId, outInfo, &cacheErr)) {
            if (loadedFromCache) {
                *loadedFromCache = true;
            }
            LOG_INFO(LogGroup::IO,
                     "Author info cache hit: owner_id={}, name='{}'",
                     ownerId,
                     LOGUTF8(outInfo.name));
            return true;
        }

        LOG_WARN(LogGroup::IO,
                 "Author info cache is invalid, refetching: owner_id={}, err={}",
                 ownerId,
                 cacheErr);
    }

    if (!m_client.GetAuthorInfo(ownerId, outInfo, errMsg)) {
        return false;
    }

    std::string saveErr;
    if (!SaveAuthorInfo(ownerId, outInfo, &saveErr)) {
        LOG_WARN(LogGroup::IO,
                 "Author info fetched but cache save failed: owner_id={}, err={}",
                 ownerId,
                 saveErr);
    }

    return true;
}

bool BiliApiCache::GetAuthorInfo(const MediaInfo &mediaInfo,
                                 AuthorInfo &outInfo,
                                 bool *loadedFromCache,
                                 std::string *errMsg)
{
    return GetAuthorInfo(mediaInfo.owner_id, outInfo, loadedFromCache, errMsg);
}

bool BiliApiCache::LoadAuthorInfo(int ownerId, AuthorInfo &outInfo, std::string *errMsg) const
{
    if (errMsg) {
        errMsg->clear();
    }

    if (ownerId <= 0) {
        SetError(errMsg, "ownerId must be greater than zero.");
        return false;
    }

    try {
        const std::filesystem::path cachePath = BuildAuthorInfoCachePath(ownerId);
        const std::string pathLocal = PathToLocal(cachePath);

        std::ifstream ifs(pathLocal, std::ios::binary);
        if (!ifs) {
            SetError(errMsg, "Failed to open cache file: " + PathToUtf8(cachePath));
            return false;
        }

        std::ostringstream oss;
        oss << ifs.rdbuf();
        const json root = json::parse(oss.str());

        const auto dataIt = root.find("data");
        if (dataIt == root.end() || !dataIt->is_object()) {
            SetError(errMsg, "Cache file does not contain a valid data object: " + PathToUtf8(cachePath));
            return false;
        }

        if (!JsonToAuthorInfo(*dataIt, outInfo, errMsg)) {
            return false;
        }

        return true;
    }
    catch (const std::exception &e) {
        SetError(errMsg, "Failed to load author info cache: " + std::string(e.what()));
        return false;
    }
}

bool BiliApiCache::SaveAuthorInfo(int ownerId, const AuthorInfo &info, std::string *errMsg) const
{
    if (errMsg) {
        errMsg->clear();
    }

    if (ownerId <= 0) {
        SetError(errMsg, "ownerId must be greater than zero.");
        return false;
    }

    try {
        const std::filesystem::path cachePath = BuildAuthorInfoCachePath(ownerId);
        if (!EnsureCacheDirectory(cachePath.parent_path(), errMsg)) {
            return false;
        }

        json root;
        root["schema_version"] = 1;
        root["owner_id"] = ownerId;
        root["saved_at_unix"] = static_cast<std::int64_t>(std::time(nullptr));
        root["data"] = AuthorInfoToJson(info);

        const std::string pathLocal = PathToLocal(cachePath);
        std::ofstream ofs(pathLocal, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            SetError(errMsg, "Failed to open cache file for writing: " + PathToUtf8(cachePath));
            return false;
        }

        ofs << root.dump(2);
        if (!ofs.good()) {
            SetError(errMsg, "Failed to write cache file: " + PathToUtf8(cachePath));
            return false;
        }

        LOG_INFO(LogGroup::IO, "Author info cache saved: {}", PathToUtf8(cachePath));
        return true;
    }
    catch (const std::exception &e) {
        SetError(errMsg, "Failed to save author info cache: " + std::string(e.what()));
        return false;
    }
}

bool BiliApiCache::EnsureCacheDirectory(const std::filesystem::path &dir, std::string *errMsg) const
{
    if (dir.empty()) {
        return true;
    }

    if (!m_options.create_directories) {
        if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
            return true;
        }

        SetError(errMsg, "Cache directory does not exist: " + PathToUtf8(dir));
        return false;
    }

    try {
        std::filesystem::create_directories(dir);
        return true;
    }
    catch (const std::exception &e) {
        SetError(errMsg, "Failed to create cache directory: " + std::string(e.what()));
        return false;
    }
}
