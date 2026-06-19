#ifndef LOGGER_H
#define LOGGER_H

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <vector>
#include <memory>
#include <utility>
#include <iostream>

class Logger {
public:
    static void init() {
        try {
            // 1. 创建控制台输出目标
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);
            console_sink->set_pattern("%^[%T] [%l]%$ %v");

            // 2. 创建文件输出目标
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("NarakaMapTool.log", true);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %T] [%l] %v");

            // 3. 组合 Sink
            std::vector<spdlog::sink_ptr> sinks;
            sinks.push_back(console_sink);
            sinks.push_back(file_sink);

            auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());

            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::trace);
            spdlog::flush_on(spdlog::level::info);

            spdlog::info("Logger initialized successfully.");
        }
        catch (const spdlog::spdlog_ex& ex) {
            std::cout << "Log initialization failed: " << ex.what() << std::endl;
        }
    }

    // 常用静态辅助函数
    template<typename... Args>
    static void info(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        spdlog::info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        spdlog::warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        spdlog::error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void debug(spdlog::format_string_t<Args...> fmt, Args &&...args) {
        spdlog::debug(fmt, std::forward<Args>(args)...);
    }
};

#endif // LOGGER_H