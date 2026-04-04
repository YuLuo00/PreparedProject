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

int main()
{
    GlobalInit();
    LOG_INFO(LogGroup::DEFAULT, "--------------------------------------run begin-----------------------------------");

    fs::path root = R"(C:\Users\Administrator\Desktop\bili_zip_1)";
    auto matches = BiliCache::CollectBiliFoldersStructured(root);
    Match aimMatch;
    std::vector<MediaInfo> infos;
    for (const auto &m : matches) {
        std::string title = BiliCache::GetTitle(m);
        std::wstring titleWstr = Tools::utf8_to_wstring(title);
        LOGINFO("{}", LOGUTF8(title));
        if (titleWstr == LR"(夏日再见∪人见人爱小海豚~)") {
            aimMatch = m;
            //break;
        }
        for (const MediaSubdir &sub : m.media_subdirs) {
            MediaInfo mediaInfo;
            loadFile2MediaInfo(m.entry_json_path.generic_wstring(), mediaInfo);
            loadFile2MediaInfo(sub.index.generic_wstring(), mediaInfo);
            infos.push_back(mediaInfo);
        }
        
    }

    LOGINFO("Found {} matching folders.", matches.size());

    ////AvApiWrapper::_AvformatAllocOutputContext2()

    //return 0;
    //


    MediaMux mux;
    //av_log_set_callback(my_ffmpeg_log_callback);
    //av_log_set_level(AV_LOG_VERBOSE); // 或 AV_LOG_DEBUG

    
    const std::set<std::string> files = {
        R"(C:\Users\Administrator\Desktop\bili_zip_1\00\285915854\1\120\audio.m4s)",
        R"(C:\Users\Administrator\Desktop\bili_zip_1\00\285915854\1\120\video.m4s)",

        //R"(C:\Users\Administrator\Desktop\bili_zip_1\type1-1\c_469842584\120\audio.m4s)",
        //R"(C:\Users\Administrator\Desktop\bili_zip_1\type1-1\c_469842584\120\video.m4s)",
    };


    mux.Open(files);
    write_mediainfo_to_avformat(mux.m_outCtx, infos[0]);
    mux.mux(false);
    print_avformat_metadata(mux.m_outCtx);
    mux.Close();

    
    AVFormatContext *ctx = nullptr;
    ctx = mux.m_outCtx;
    ////int i = av_dict_count(mux.m_outCtx->);

    std::multimap<AVFormatContext *, AVStream *> ii = MediaMux::GetInputStreams("result.mp4");
    ctx = ii.begin()->first;
    MediaInfo media;
    read_mediainfo_from_avformat(ctx, media);
    print_avformat_metadata(ctx);
    return -1;
    return 0;
}
