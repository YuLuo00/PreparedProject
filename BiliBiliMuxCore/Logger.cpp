#include "Logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> Logger::s_loggers;

static std::vector<spdlog::sink_ptr> g_sinks;

void Logger::Init()
{
    if (!g_sinks.empty())
        return;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/app.log", 1024 * 1024 * 5, 3);

    g_sinks = { console_sink, file_sink };
}

std::shared_ptr<spdlog::logger> Logger::Get(const std::string& group)
{
    if (g_sinks.empty())
        Init();

    auto it = s_loggers.find(group);
    if (it != s_loggers.end())
        return it->second;

    // 创建新的 group logger
    auto logger = std::make_shared<spdlog::logger>(group, g_sinks.begin(), g_sinks.end());

    // 格式：yyyy::mm::dd hh::mm::ss [level][group] 内容
    logger->set_pattern("%Y::%m::%d %H::%M::%S [%P][%t] [%^%l%$][%n] %v");

    logger->set_level(spdlog::level::trace); // 默认全开
    logger->flush_on(spdlog::level::err);

    spdlog::register_logger(logger);

    s_loggers[group] = logger;
    return logger;
}

void Logger::SetLevel(const std::string& group, spdlog::level::level_enum level)
{
    auto logger = Get(group);
    logger->set_level(level);
}