#pragma once
#include <cstdarg>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <thread>

extern "C"
{
#include <libavutil/log.h>
}

// =======================
// 全局静态数据
// =======================
class FFmpegLogScope
{
public:
    // thread_id -> buffer
    static inline std::map<std::thread::id, std::string> log_map;
    static inline std::mutex map_mutex;
    static inline std::map<std::thread::id, int> most_hight_level_map;
    static inline std::mutex level_mutex;

    // 是否正在捕获
    static inline thread_local bool capturing = false;

    // 保存原始回调
    static inline void (*default_callback)(void *, int, const char *, va_list) = nullptr;

public:
    FFmpegLogScope();

    ~FFmpegLogScope();

    // 获取当前线程的日志
    static std::string get_log()
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        return log_map[std::this_thread::get_id()];
    }

    static int get_level()
    {
        std::lock_guard<std::mutex> lock(level_mutex);
        return most_hight_level_map[std::this_thread::get_id()];
    }

private:
    // FFmpeg 日志回调
    static void ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl);
};
