#include "attach.h"
#include <map>

namespace
{
std::wstring JsonStringToWString(const json &j, const char *key)
{
    if (!j.contains(key)) {
        return {};
    }
    return Tools::utf8_to_wstring(j.at(key).get<std::string>());
}

void SetMetadataWString(AVDictionary **metadata, const char *key, const std::wstring &value)
{
    if (!metadata || value.empty()) {
        return;
    }

    std::string utf8 = Tools::wstring_to_utf8(value);
    av_dict_set(metadata, key, utf8.c_str(), 0);
}

std::wstring GetMetadataWString(const AVDictionary *metadata, const char *key)
{
    if (!metadata) {
        return {};
    }

    const AVDictionaryEntry *entry = av_dict_get(metadata, key, nullptr, 0);
    if (!entry || !entry->value) {
        return {};
    }

    return Tools::utf8_to_wstring(entry->value);
}

bool TryParseInt(const char *text, int &out)
{
    if (!text) {
        return false;
    }

    try {
        size_t pos = 0;
        int value = std::stoi(text, &pos);
        if (pos != std::string(text).size()) {
            return false;
        }
        out = value;
        return true;
    }
    catch (...) {
        return false;
    }
}

void GetMetadataInt(const AVDictionary *metadata, const char *key, int &out)
{
    if (!metadata) {
        return;
    }

    const AVDictionaryEntry *entry = av_dict_get(metadata, key, nullptr, 0);
    if (!entry || !entry->value) {
        return;
    }

    TryParseInt(entry->value, out);
}

bool ParseIndexedMetadataKey(const char *key, const char *prefix, size_t &index, std::string &field)
{
    if (!key || !prefix) {
        return false;
    }

    const std::string keyText(key);
    const std::string prefixText(prefix);

    if (keyText.rfind(prefixText, 0) != 0) {
        return false;
    }

    const size_t indexBegin = prefixText.size();
    const size_t fieldSep = keyText.find('.', indexBegin);
    if (fieldSep == std::string::npos || fieldSep == indexBegin) {
        return false;
    }

    try {
        size_t pos = 0;
        index = static_cast<size_t>(std::stoull(keyText.substr(indexBegin, fieldSep - indexBegin), &pos));
        if (pos != fieldSep - indexBegin) {
            return false;
        }
    }
    catch (...) {
        return false;
    }

    field = keyText.substr(fieldSep + 1);
    return !field.empty();
}

void ApplyEntryField(EntryItem &item, const std::string &field, const char *value)
{
    if (!value) {
        return;
    }

    if (field == "md5") {
        item.md5 = Tools::utf8_to_wstring(value);
        return;
    }

    if (field == "base_url") {
        item.base_url = Tools::utf8_to_wstring(value);
        return;
    }

    if (field == "id") {
        TryParseInt(value, item.id);
    }
}
} // namespace

bool parse_entry_json(const json &j, MediaInfo &out)
{
    try {
        if (j.contains("owner_id"))
            out.owner_id = j.at("owner_id").get<int>();
        if (j.contains("avid"))
            out.avid = j.at("avid").get<int>();
        out.cover = JsonStringToWString(j, "cover");
        out.title = JsonStringToWString(j, "title");
        return true;
    }
    catch (const std::exception &e) {
        std::cerr << "parse_entry_json error: " << e.what() << std::endl;
        return false;
    }
}

bool parse_index_json(const json &j, MediaInfo &out)
{
    try {
        if (j.contains("video") && j["video"].is_array()) {
            for (const auto &v : j["video"]) {
                EntryItem it;
                if (v.contains("md5"))
                    it.md5 = Tools::utf8_to_wstring(v.at("md5").get<std::string>());
                if (v.contains("base_url"))
                    it.base_url = Tools::utf8_to_wstring(v.at("base_url").get<std::string>());
                if (v.contains("id"))
                    it.id = v.at("id").get<int>();
                out.videos.push_back(std::move(it));
            }
        }

        if (j.contains("audio") && j["audio"].is_array()) {
            for (const auto &a : j["audio"]) {
                EntryItem it;
                if (a.contains("md5"))
                    it.md5 = Tools::utf8_to_wstring(a.at("md5").get<std::string>());
                if (a.contains("base_url"))
                    it.base_url = Tools::utf8_to_wstring(a.at("base_url").get<std::string>());
                if (a.contains("id"))
                    it.id = a.at("id").get<int>();
                out.audios.push_back(std::move(it));
            }
        }

        return true;
    }
    catch (const std::exception &e) {
        std::cerr << "parse_index_json error: " << e.what() << std::endl;
        return false;
    }
}

int write_mediainfo_to_avformat(AVFormatContext *fmt, const MediaInfo &info)
{
    if (!fmt)
        return AVERROR(EINVAL);

    SetMetadataWString(&fmt->metadata, "title", info.title);
    SetMetadataWString(&fmt->metadata, "cover", info.cover);

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d", info.owner_id);
        av_dict_set(&fmt->metadata, "owner_id", buf, 0);
        snprintf(buf, sizeof(buf), "%d", info.avid);
        av_dict_set(&fmt->metadata, "avid", buf, 0);
    }

    for (size_t i = 0; i < info.videos.size(); ++i) {
        const EntryItem &it = info.videos[i];
        char key[128];

        if (!it.md5.empty()) {
            snprintf(key, sizeof(key), "video.%zu.md5", i);
            SetMetadataWString(&fmt->metadata, key, it.md5);
        }
        if (!it.base_url.empty()) {
            snprintf(key, sizeof(key), "video.%zu.base_url", i);
            SetMetadataWString(&fmt->metadata, key, it.base_url);
        }

        snprintf(key, sizeof(key), "video.%zu.id", i);
        {
            char val[32];
            snprintf(val, sizeof(val), "%d", it.id);
            av_dict_set(&fmt->metadata, key, val, 0);
        }
    }

    for (size_t i = 0; i < info.audios.size(); ++i) {
        const EntryItem &it = info.audios[i];
        char key[128];

        if (!it.md5.empty()) {
            snprintf(key, sizeof(key), "audio.%zu.md5", i);
            SetMetadataWString(&fmt->metadata, key, it.md5);
        }
        if (!it.base_url.empty()) {
            snprintf(key, sizeof(key), "audio.%zu.base_url", i);
            SetMetadataWString(&fmt->metadata, key, it.base_url);
        }

        snprintf(key, sizeof(key), "audio.%zu.id", i);
        {
            char val[32];
            snprintf(val, sizeof(val), "%d", it.id);
            av_dict_set(&fmt->metadata, key, val, 0);
        }
    }

    return 0;
}

int read_mediainfo_from_avformat(const AVFormatContext *fmt, MediaInfo &out)
{
    if (!fmt) {
        return AVERROR(EINVAL);
    }

    out = {};
    out.title = GetMetadataWString(fmt->metadata, "title");
    out.cover = GetMetadataWString(fmt->metadata, "cover");
    GetMetadataInt(fmt->metadata, "owner_id", out.owner_id);
    GetMetadataInt(fmt->metadata, "avid", out.avid);

    std::map<size_t, EntryItem> videoEntries;
    std::map<size_t, EntryItem> audioEntries;

    const AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_get(fmt->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)) != nullptr) {
        size_t index = 0;
        std::string field;

        if (ParseIndexedMetadataKey(entry->key, "video.", index, field)) {
            ApplyEntryField(videoEntries[index], field, entry->value);
            continue;
        }

        if (ParseIndexedMetadataKey(entry->key, "audio.", index, field)) {
            ApplyEntryField(audioEntries[index], field, entry->value);
        }
    }

    out.videos.reserve(videoEntries.size());
    for (auto &kv : videoEntries) {
        out.videos.push_back(std::move(kv.second));
    }

    out.audios.reserve(audioEntries.size());
    for (auto &kv : audioEntries) {
        out.audios.push_back(std::move(kv.second));
    }

    return 0;
}

void print_avformat_metadata(const AVFormatContext *fmt)
{
    if (!fmt) {
        std::cout << "[fmt metadata] fmt == nullptr" << std::endl;
        return;
    }

    const int count = av_dict_count(fmt->metadata);
    std::cout << "[fmt metadata] count = " << count << std::endl;
    if (!fmt->metadata || count == 0) {
        return;
    }

    const AVDictionaryEntry *entry = nullptr;
    while ((entry = av_dict_get(fmt->metadata, "", entry, AV_DICT_IGNORE_SUFFIX)) != nullptr) {
        std::cout << entry->key << " = " << (entry->value ? entry->value : "") << std::endl;
    }
}

bool load_json_file(const std::wstring &path, json &out)
{
    std::ifstream ifs{fs::path(path)};
    if (!ifs.is_open())
        return false;

    try {
        ifs >> out;
        return true;
    }
    catch (...) {
        return false;
    }
}
