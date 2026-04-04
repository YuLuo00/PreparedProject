#include "ffmpegMsg.h"

FFmpegLogScope::FFmpegLogScope()
{
    std::lock_guard<std::mutex> lock(map_mutex);

    // 保存默认回调（只保存一次）
    if (!default_callback) {
        default_callback = av_log_default_callback;
    }

    // 清空当前线程的 buffer
    log_map[std::this_thread::get_id()].clear();
    most_hight_level_map[std::this_thread::get_id()] = AV_LOG_TRACE;

    // 开始捕获
    capturing = true;

    // 设置我们的回调
    av_log_set_callback(ffmpeg_log_callback);
}

FFmpegLogScope::~FFmpegLogScope()
{
    // 停止捕获
    capturing = false;

    // 恢复默认回调
    av_log_set_callback(default_callback);
}

// FFmpeg 日志回调
void FFmpegLogScope::ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    // 先调用默认回调，让 FFmpeg 正常打印
    if (default_callback) {
        default_callback(ptr, level, fmt, vl);
    }

    // 如果当前线程没有开启捕获，则不记录
    if (!capturing)
        return;

    // 格式化日志
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, vl);

    // 写入 map
    {
        std::lock_guard<std::mutex> lock(map_mutex);
        log_map[std::this_thread::get_id()] += buf;
    }
    {
        std::lock_guard<std::mutex> lock(level_mutex);
        most_hight_level_map[std::this_thread::get_id()] =
            std::min(most_hight_level_map[std::this_thread::get_id()], level);
    }
}
