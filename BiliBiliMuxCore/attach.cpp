#include "attach.h"

// 解析 entry.json（包含 owner_id, avid, cover, title）
bool parse_entry_json(const json &j, MediaInfo &out)
{
    try {
        if (j.contains("owner_id"))
            out.owner_id = j.at("owner_id").get<int>();
        if (j.contains("avid"))
            out.avid = j.at("avid").get<int>();
        if (j.contains("cover"))
            out.cover = j.at("cover").get<std::string>();
        if (j.contains("title"))
            out.title = j.at("title").get<std::string>();
        // page_data 里也可能有部分信息（可选）
        return true;
    }
    catch (const std::exception &e) {
        std::cerr << "parse_entry_json error: " << e.what() << std::endl;
        return false;
    }
}

// 解析 index.json（包含 video/audio 数组）
bool parse_index_json(const json &j, MediaInfo &out)
{
    try {
        if (j.contains("video") && j["video"].is_array()) {
            for (const auto &v : j["video"]) {
                EntryItem it;
                if (v.contains("md5"))
                    it.md5 = v.at("md5").get<std::string>();
                if (v.contains("base_url"))
                    it.base_url = v.at("base_url").get<std::string>();
                if (v.contains("id"))
                    it.id = v.at("id").get<int>();
                out.videos.push_back(std::move(it));
            }
        }
        if (j.contains("audio") && j["audio"].is_array()) {
            for (const auto &a : j["audio"]) {
                EntryItem it;
                if (a.contains("md5"))
                    it.md5 = a.at("md5").get<std::string>();
                if (a.contains("base_url"))
                    it.base_url = a.at("base_url").get<std::string>();
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

// 把 MediaInfo 写入 AVFormatContext 的 metadata，并为每个条目创建 AVStream 并写入流级 metadata
// 注意：创建流时我们至少设置 codecpar->codec_type，避免某些 muxer 在写入时丢弃“空流”。
// 假设已包含 nlohmann::json, ffmpeg headers, MediaInfo/EntryItem 定义与 parse 函数
// 只展示修改后的 write_mediainfo_to_avformat 函数

int write_mediainfo_to_avformat(AVFormatContext *fmt, const MediaInfo &info)
{
    if (!fmt)
        return AVERROR(EINVAL);

    // 全局 metadata：基础字段
    if (!info.title.empty())
        av_dict_set(&fmt->metadata, "title", info.title.c_str(), 0);
    if (!info.cover.empty())
        av_dict_set(&fmt->metadata, "cover", info.cover.c_str(), 0);

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d", info.owner_id);
        av_dict_set(&fmt->metadata, "owner_id", buf, 0);
        snprintf(buf, sizeof(buf), "%d", info.avid);
        av_dict_set(&fmt->metadata, "avid", buf, 0);
    }

    // 把 video 列表写成带索引的键：video.N.md5, video.N.base_url, video.N.id
    for (size_t i = 0; i < info.videos.size(); ++i) {
        const EntryItem &it = info.videos[i];
        char key[128];

        if (!it.md5.empty()) {
            snprintf(key, sizeof(key), "video.%zu.md5", i);
            av_dict_set(&fmt->metadata, key, it.md5.c_str(), 0);
        }
        if (!it.base_url.empty()) {
            snprintf(key, sizeof(key), "video.%zu.base_url", i);
            av_dict_set(&fmt->metadata, key, it.base_url.c_str(), 0);
        }
        snprintf(key, sizeof(key), "video.%zu.id", i);
        {
            char val[32];
            snprintf(val, sizeof(val), "%d", it.id);
            av_dict_set(&fmt->metadata, key, val, 0);
        }
    }

    // 把 audio 列表写成带索引的键：audio.N.md5, audio.N.base_url, audio.N.id
    for (size_t i = 0; i < info.audios.size(); ++i) {
        const EntryItem &it = info.audios[i];
        char key[128];

        if (!it.md5.empty()) {
            snprintf(key, sizeof(key), "audio.%zu.md5", i);
            av_dict_set(&fmt->metadata, key, it.md5.c_str(), 0);
        }
        if (!it.base_url.empty()) {
            snprintf(key, sizeof(key), "audio.%zu.base_url", i);
            av_dict_set(&fmt->metadata, key, it.base_url.c_str(), 0);
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

// 辅助：从文件加载 json
// 辅助：从文件加载 json
bool load_json_file(const std::wstring &path, json &out)
{
    std::ifstream ifs(path);
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


//// 示例主流程（演示如何使用）
//int main_example(const char *entry_path, const char *index_path, const char *out_filename)
//{
//    json jentry, jindex;
//    //if (!load_json_file(entry_path, jentry)) {
//    //    std::cerr << "failed load entry.json\n";
//    //    return -1;
//    //}
//    //if (!load_json_file(index_path, jindex)) {
//    //    std::cerr << "failed load index.json\n";
//    //    return -1;
//    //}
//
//    MediaInfo info;
//    if (!parse_entry_json(jentry, info))
//        return -1;
//    if (!parse_index_json(jindex, info))
//        return -1;
//
//    avformat_network_init();
//
//    AVFormatContext *oc = nullptr;
//    avformat_alloc_output_context2(&oc, nullptr, nullptr, out_filename);
//    if (!oc) {
//        std::cerr << "failed alloc output context\n";
//        return -1;
//    }
//
//    // 把 metadata 写入上下文并创建流
//    if (write_mediainfo_to_avformat(oc, info) < 0) {
//        avformat_free_context(oc);
//        return -1;
//    }
//
//    // 打开输出（如果需要真正写文件）
//    if (!(oc->oformat->flags & AVFMT_NOFILE)) {
//        if (avio_open(&oc->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
//            std::cerr << "failed open output file\n";
//            avformat_free_context(oc);
//            return -1;
//        }
//    }
//
//    // 写头（注意：有些 muxer 可能会丢弃没有 packet 的流）
//    if (avformat_write_header(oc, nullptr) < 0) {
//        std::cerr << "write header failed\n";
//    }
//
//    // 如果你需要确保流不会被丢弃，可以写入占位 packet（见之前讨论）
//    av_write_trailer(oc);
//
//    if (!(oc->oformat->flags & AVFMT_NOFILE))
//        avio_closep(&oc->pb);
//    avformat_free_context(oc);
//    avformat_network_deinit();
//    return 0;
//}
