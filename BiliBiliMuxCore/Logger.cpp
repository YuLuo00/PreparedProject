#include "Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> Logger::s_loggers;

static std::vector<spdlog::sink_ptr> g_sinks;

void Logger::Init(const Options &options)
{
    if (!g_sinks.empty()) {
        return;
    }

    if (options.enable_console) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(options.console_level);
        console_sink->set_pattern("[%P][%t] [%^%l%$][%n] %v");
        g_sinks.push_back(console_sink);
    }

    if (options.enable_file) {
        const std::filesystem::path logPath(options.file_path);
        if (logPath.has_parent_path()) {
            std::filesystem::create_directories(logPath.parent_path());
        }

        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            options.file_path, 1024 * 1024 * 5, 3);
        file_sink->set_level(options.file_level);
        file_sink->set_pattern("%Y-%m-%d %H:%M:%S.%e [%P][%t] [%l][%n] [%s:%#] %v");
        g_sinks.push_back(file_sink);
    }

    if (g_sinks.empty()) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::info);
        console_sink->set_pattern("[%P][%t] [%^%l%$][%n] %v");
        g_sinks.push_back(console_sink);
    }
}

std::shared_ptr<spdlog::logger> Logger::Get(const std::string& group)
{
    if (g_sinks.empty()) {
        Init();
    }

    auto it = s_loggers.find(group);
    if (it != s_loggers.end()) {
        return it->second;
    }

    auto logger = std::make_shared<spdlog::logger>(group, g_sinks.begin(), g_sinks.end());
    logger->set_level(spdlog::level::trace);
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

spdlog::level::level_enum Logger::ParseLevel(const std::string &text,
                                             spdlog::level::level_enum fallback)
{
    std::string level = text;
    std::transform(level.begin(), level.end(), level.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    const auto parsed = spdlog::level::from_str(level);
    if (parsed == spdlog::level::off && level != "off") {
        return fallback;
    }
    return parsed;
}
