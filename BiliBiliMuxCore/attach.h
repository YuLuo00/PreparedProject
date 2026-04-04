#pragma once

// requires: nlohmann/json.hpp, libavformat (ffmpeg)
// compile example (linux):
// g++ -std=c++17 -I/path/to/nlohmann -o app app.cpp `pkg-config --cflags --libs libavformat libavcodec libavutil`

#include <fstream>
#include <cstdint>
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
    std::wstring md5;
    std::wstring base_url;
    int id = 0;
};

struct MediaInfo
{
    int owner_id = 0;
    int avid = 0;
    std::wstring bvid;
    int64_t cid = 0;
    int page = 0;
    std::wstring cover;
    std::wstring title;
    std::wstring download_subtitle;
    std::wstring download_title;
    std::vector<EntryItem> videos;
    std::vector<EntryItem> audios;
};

// 解析 entry.json（包含 owner_id, avid, bvid, cover, title, page_data）
bool parse_entry_json(const json &j, MediaInfo &out);

// 解析 index.json（包含 video/audio 数组）
bool parse_index_json(const json &j, MediaInfo &out);

// 把 MediaInfo 写入 AVFormatContext 的 metadata，并为每个条目创建 AVStream 并写入流级 metadata
// 注意：创建流时我们至少设置 codecpar->codec_type，避免某些 muxer 在写入时丢弃“空流”。
int write_mediainfo_to_avformat(AVFormatContext *fmt, const MediaInfo &info);

// 从 AVFormatContext 的 metadata 读取 MediaInfo。
// 读取格式与 write_mediainfo_to_avformat 写入的 key 保持一致。
int read_mediainfo_from_avformat(const AVFormatContext *fmt, MediaInfo &out);

// 打印 AVFormatContext 自身的文件级 metadata（不包含 stream metadata）。
void print_avformat_metadata(const AVFormatContext *fmt);

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

