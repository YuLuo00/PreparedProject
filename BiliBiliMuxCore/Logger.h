#pragma once


#include <memory>
#include <unordered_map>
#include <string>
#include <spdlog/spdlog.h>

class Logger
{
public:
    static void Init();

    // 获取指定 group 的 logger
    static std::shared_ptr<spdlog::logger> Get(const std::string& group);

    // 设置某个 group 的日志级别
    static void SetLevel(const std::string& group, spdlog::level::level_enum level);

private:
    static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> s_loggers;
};

namespace LogGroup
{
    constexpr const char *DEFAULT = "DEFAULT";
    constexpr const char *MUX = "mux";
    constexpr const char* DECODE = "decode";
    constexpr const char* IO = "io";
}


// 初始化
#define LOG_INIT() Logger::Init()
// 自动获取 logger
#define LOG_TRACE(group, ...) Logger::Get(group)->trace(__VA_ARGS__)
#define LOG_DEBUG(group, ...) Logger::Get(group)->debug(__VA_ARGS__)
#define LOG_INFO(group, ...)  Logger::Get(group)->info(__VA_ARGS__)
#define LOG_WARN(group, ...)  Logger::Get(group)->warn(__VA_ARGS__)
#define LOG_ERROR(group, ...) Logger::Get(group)->error(__VA_ARGS__)
#define LOG_CRITICAL(group, ...) Logger::Get(group)->critical(__VA_ARGS__)