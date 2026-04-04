#pragma once

// requires: nlohmann/json.hpp, libavformat (ffmpeg)
// compile example (linux):
// g++ -std=c++17 -I/path/to/nlohmann -o app app.cpp `pkg-config --cflags --libs libavformat libavcodec libavutil`

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <filesystem>
namespace  fs = std::filesystem;

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
}

using json = nlohmann::json;

#include "tools.h"

struct EntryItem
{
    std::string md5;
    std::string base_url;
    int id = 0;
};

struct MediaInfo
{
    int owner_id = 0;
    int avid = 0;
    std::string cover;
    std::string title;
    std::vector<EntryItem> videos;
    std::vector<EntryItem> audios;
};

// 解析 entry.json（包含 owner_id, avid, cover, title）
bool parse_entry_json(const json &j, MediaInfo &out);

// 解析 index.json（包含 video/audio 数组）
bool parse_index_json(const json &j, MediaInfo &out);

// 把 MediaInfo 写入 AVFormatContext 的 metadata，并为每个条目创建 AVStream 并写入流级 metadata
// 注意：创建流时我们至少设置 codecpar->codec_type，避免某些 muxer 在写入时丢弃“空流”。
int write_mediainfo_to_avformat(AVFormatContext *fmt, const MediaInfo &info);

// 辅助：从文件加载 json
// 辅助：从文件加载 json
bool load_json_file(const std::wstring &path, json &out);

inline void loadFile2MediaInfo(const std::wstring &path, MediaInfo &out) {
    std::wstring fileName = fs::path(path).filename().generic_wstring();
    json js;
    load_json_file(path, js);
    if (Tools::iequals_wstring(fileName, L"index.json")) {
        parse_index_json(js, out);
    }
    if (Tools::iequals_wstring(fileName, L"entry.json")) {
        parse_entry_json(js, out);
    }
}

