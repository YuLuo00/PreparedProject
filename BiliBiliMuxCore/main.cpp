#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavformat/avio.h"
    #include "libavutil/avutil.h"
}

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/opt.h>
    #include <libavutil/samplefmt.h>
    #include <libswresample/swresample.h>
}

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <string>
#include <fstream>
#include <iosfwd>
#include <sstream>
#include <iostream>
#include <functional>  // std::reference_wrapper
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace std;

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>

#include <tbb/concurrent_queue.h>
#include <tbb/flow_graph.h>
using namespace tbb::flow;

#include "attach.h"
#include "AvApiWrapper.h"
#include "BiliCache.h"
#include "common.h"
#include "ffmpegMsg.h"
#include "Logger.h"
#include "MediaMux.h"
#include "ReadNode.h"
#include "tools.h"


#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cstring>



std::mutex ffmpeg_log_mutex;

static spdlog::level::level_enum FFmpegLevelToSpd(int level)
{
    if (level <= AV_LOG_PANIC)
        return spdlog::level::critical;
    if (level <= AV_LOG_FATAL)
        return spdlog::level::critical;
    if (level <= AV_LOG_ERROR)
        return spdlog::level::err;
    if (level <= AV_LOG_WARNING)
        return spdlog::level::warn;
    if (level <= AV_LOG_INFO)
        return spdlog::level::info;
    if (level <= AV_LOG_VERBOSE)
        return spdlog::level::debug;
    return spdlog::level::trace;
}

void my_ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    std::lock_guard<std::mutex> lock(ffmpeg_log_mutex);

    // ❗注意：FFmpeg 是 level 越小越严重
    // 如果你只想要 warning 及以上：<std::string>
    if (level > AV_LOG_WARNING) {
        return;
    }

    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, vl);

    // 去掉末尾换行
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    auto logger = Logger::Get("ffmpeg");
    auto spd_level = FFmpegLevelToSpd(level);

    // 用 spdlog 的 level 控制输出
    if (logger->should_log(spd_level)) {
        logger->log(spd_level, "{}", buf);
    }

    // ❗是否保留 FFmpeg 默认输出（二选一）
    // 一般建议关掉，否则会重复打印
    // av_log_default_callback(ptr, level, fmt, vl);
}
void GlobalInit()
{
    Logger::Init();
    LOG_INFO("mux", "start muxing {}", 123);
    LOG_DEBUG("decode", "frame pts={}", 456);
    Logger::SetLevel("mux", spdlog::level::info);
    Logger::SetLevel("decode", spdlog::level::err);
    Logger::SetLevel("ffmpeg", spdlog::level::debug);

    LOG_DEBUG("mux", "不会打印");  // 被过滤
    LOG_ERROR("decode", "会打印"); // ✔
    av_log_set_callback(my_ffmpeg_log_callback);
}

int mainPipeline(const MediaSubdir &sub)
{
    const Match *match = sub.match;
    if (match == nullptr) {
        LOGINFO("sub.match 为空，无法提取 mediainfo");
        return -1;
    }
    if (match->entry_json_path.empty()) {
        LOGINFO("match->entry_json_path 为空，无法提取 mediainfo");
        return -2;
    }
    if (sub.audio.empty() || sub.video.empty() || sub.index.empty()) {
        LOGINFO("MediaSubdir 缺少 audio/video/index，无法混流");
        return -2;
    }

    if (match->media_subdirs.size() > 1) {
        LOGINFO("发现 {} 个媒体子目录，当前使用第一个: {}",
                match->media_subdirs.size(),
                LOGUTF8(sub.dir.filename().wstring()));
    }

    MediaInfo mediaInfo;
    loadFile2MediaInfo(match->entry_json_path.generic_wstring(), mediaInfo);
    loadFile2MediaInfo(sub.index.generic_wstring(), mediaInfo);

    std::wstring title = mediaInfo.title;
    if (title.empty()) {
        title = match->dir.filename().wstring();
    }
    if (title.empty()) {
        title = L"output";
    }

    const std::wstring safeTitle = Tools::sanitize_windows_filename(title);
    const fs::path outputDir = match->dir.empty() ? fs::current_path() : match->dir;
    const fs::path outputPath = outputDir / (safeTitle + L".mp4");
    const std::set<std::string> files = {
        Tools::wstring_to_utf8(sub.audio.wstring()),
        Tools::wstring_to_utf8(sub.video.wstring()),
    };

    LOGINFO("开始混流: {}", LOGUTF8(title));
    LOGINFO("输出文件: {}", LOGUTF8(outputPath.filename().wstring()));

    MediaMux mux;
    std::wstring outputPathU8 = outputPath.wstring();
    std::string opU8 = Tools::wstring_to_utf8(outputPathU8);
    int ret = mux.Open(files, opU8);
    if (ret != 0) {
        return ret;
    }

    ret = write_mediainfo_to_avformat(mux.m_outCtx, mediaInfo);
    if (ret != 0) {
        LOGINFO("写入 metadata 失败: {}", ret);
        mux.Close();
        return ret;
    }

    ret = mux.mux();
    if (ret != 0) {
        LOGINFO("混流失败: {}", ret);
        return ret;
    }

    LOGINFO("混流完成: {}", LOGUTF8(outputPath.filename().wstring()));
    return 0;
}

int main()
{
    GlobalInit();
    LOG_INFO(LogGroup::DEFAULT, "--------------------------------------run begin-----------------------------------");

    fs::path root = R"(C:\Users\Administrator\Desktop\bili_zip_1)";
    auto matches = BiliCache::CollectBiliFoldersStructured(root);
    MediaSubdir aimSub;
    for (const auto &m : matches) {
        std::string title = BiliCache::GetTitle(m);
        std::wstring titleWstr = Tools::utf8_to_wstring(title);
        LOGINFO("{}", LOGUTF8(title));
        if (titleWstr == LR"(夏日再见∪人见人爱小海豚~)") {
            if (!m.media_subdirs.empty()) {
                aimSub = m.media_subdirs.front();
            }
            //break;
        }
    }

    LOGINFO("Found {} matching folders.", matches.size());

    if (aimSub.match == nullptr) {
        LOGINFO("没有找到目标 MediaSubdir");
        return -1;
    }

    return mainPipeline(aimSub);
}
