// ==========================================================================
// Copyright (C) 2026 Ethernos Studio
// This file is part of Arknights Auto Machine (AAM).
//
// AAM is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// AAM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with AAM. If not, see <https://www.gnu.org/licenses/>.
// ==========================================================================
// @file logger.cpp
// @author dhjs0000
// @brief spdlog 日志系统封装实现
// ==========================================================================
// 版本: v0.2.0-alpha.1
// 功能: 提供统一的日志接口，支持控制台、文件、异步输出
// 依赖: spdlog, fmt
// ==========================================================================

#include "aam/core/logger.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <mutex>

namespace aam::core {

// ==========================================================================
// 内部实现
// ==========================================================================

namespace detail {

    // 全局日志管理器互斥锁
    std::mutex g_logger_mutex;

    // 默认日志配置
    LoggerConfig g_default_config;

    // 全局日志器注册表
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> g_loggers;

    // 异步线程池
    std::shared_ptr<spdlog::details::thread_pool> g_thread_pool;

    // 默认异步队列大小
    constexpr std::size_t kDefaultAsyncQueueSize = 8192;

    // 默认异步线程数
    constexpr std::size_t kDefaultAsyncThreads = 1;

    /**
     * @brief 转换日志级别
     * @param level AAM 日志级别
     * @return spdlog 日志级别
     */
    [[nodiscard]] spdlog::level::level_enum to_spdlog_level(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Trace:
                return spdlog::level::trace;
            case LogLevel::Debug:
                return spdlog::level::debug;
            case LogLevel::Info:
                return spdlog::level::info;
            case LogLevel::Warning:
                return spdlog::level::warn;
            case LogLevel::Error:
                return spdlog::level::err;
            case LogLevel::Fatal:
                return spdlog::level::critical;
            case LogLevel::Off:
                return spdlog::level::off;
            default:
                return spdlog::level::info;
        }
    }

    /**
     * @brief 从字符串解析日志级别
     * @param str 级别字符串
     * @return 日志级别
     */
    [[nodiscard]] LogLevel parse_log_level(std::string_view str) noexcept {
        if (str == "trace" || str == "TRACE") return LogLevel::Trace;
        if (str == "debug" || str == "DEBUG") return LogLevel::Debug;
        if (str == "info" || str == "INFO") return LogLevel::Info;
        if (str == "warning" || str == "WARN" || str == "WARNING") return LogLevel::Warning;
        if (str == "error" || str == "ERROR") return LogLevel::Error;
        if (str == "fatal" || str == "FATAL" || str == "critical" || str == "CRITICAL") {
            return LogLevel::Fatal;
        }
        if (str == "off" || str == "OFF") return LogLevel::Off;
        return LogLevel::Info;
    }

    /**
     * @brief 获取环境变量中的日志级别
     * @return 日志级别，未设置返回 Info
     */
    [[nodiscard]] LogLevel get_env_log_level() noexcept {
        const char* env_level = std::getenv("AAM_LOG_LEVEL");
        if (env_level) {
            return parse_log_level(env_level);
        }
        return LogLevel::Info;
    }

    /**
     * @brief 确保日志目录存在
     * @param filepath 文件路径
     */
    void ensure_log_directory(const std::string& filepath) {
        std::filesystem::path path(filepath);
        std::filesystem::path dir = path.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    }

    /**
     * @brief 创建控制台输出目标
     * @param use_color 是否使用彩色输出
     * @return 输出目标
     */
    [[nodiscard]] std::shared_ptr<spdlog::sinks::sink> create_console_sink(bool use_color) {
        if (use_color) {
            return std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        }
        return std::make_shared<spdlog::sinks::stdout_sink_mt>();
    }

    /**
     * @brief 创建文件输出目标
     * @param config 日志配置
     * @return 输出目标
     */
    [[nodiscard]] std::shared_ptr<spdlog::sinks::sink> create_file_sink(const LoggerConfig& config) {
        ensure_log_directory(config.file_path);

        switch (config.rotation_policy) {
            case LogRotationPolicy::SizeBased: {
                return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    config.file_path,
                    config.max_file_size,
                    config.max_files);
            }
            case LogRotationPolicy::Daily: {
                return std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                    config.file_path,
                    config.rotation_hour,
                    config.rotation_minute);
            }
            case LogRotationPolicy::None:
            default: {
                return std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.file_path, true);
            }
        }
    }

    /**
     * @brief 初始化异步线程池
     */
    void init_async_thread_pool() {
        if (!g_thread_pool) {
            g_thread_pool = std::make_shared<spdlog::details::thread_pool>(
                kDefaultAsyncQueueSize, kDefaultAsyncThreads);
        }
    }

    /**
     * @brief 创建日志器
     * @param name 日志器名称
     * @param config 日志配置
     * @return spdlog 日志器
     */
    [[nodiscard]] std::shared_ptr<spdlog::logger> create_logger(
        const std::string& name,
        const LoggerConfig& config) {

        std::vector<spdlog::sink_ptr> sinks;

        // 控制台输出
        if (config.enable_console) {
            sinks.push_back(create_console_sink(config.use_color));
        }

        // 文件输出
        if (config.enable_file && !config.file_path.empty()) {
            sinks.push_back(create_file_sink(config));
        }

        // 如果没有配置任何输出，使用 null sink
        if (sinks.empty()) {
            sinks.push_back(std::make_shared<spdlog::sinks::null_sink_mt>());
        }

        std::shared_ptr<spdlog::logger> logger;

        // 异步或同步日志器
        if (config.async_mode) {
            init_async_thread_pool();
            logger = std::make_shared<spdlog::async_logger>(
                name, sinks.begin(), sinks.end(), g_thread_pool, spdlog::async_overflow_policy::block);
        } else {
            logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        }

        // 设置日志级别
        logger->set_level(to_spdlog_level(config.level));

        // 设置日志格式
        if (!config.pattern.empty()) {
            logger->set_pattern(config.pattern);
        }

        // 设置刷新级别
        logger->flush_on(to_spdlog_level(config.flush_level));

        return logger;
    }

} // namespace detail

// ==========================================================================
// Logger 类实现
// ==========================================================================

Logger::Logger(std::shared_ptr<spdlog::logger> logger) : logger_(std::move(logger)) {}

void Logger::log(LogLevel level, std::string_view message) {
    if (!logger_) return;

    switch (level) {
        case LogLevel::Trace:
            logger_->trace("{}", message);
            break;
        case LogLevel::Debug:
            logger_->debug("{}", message);
            break;
        case LogLevel::Info:
            logger_->info("{}", message);
            break;
        case LogLevel::Warning:
            logger_->warn("{}", message);
            break;
        case LogLevel::Error:
            logger_->error("{}", message);
            break;
        case LogLevel::Fatal:
            logger_->critical("{}", message);
            break;
        default:
            break;
    }
}

void Logger::trace(std::string_view message) {
    if (logger_) logger_->trace("{}", message);
}

void Logger::debug(std::string_view message) {
    if (logger_) logger_->debug("{}", message);
}

void Logger::info(std::string_view message) {
    if (logger_) logger_->info("{}", message);
}

void Logger::warning(std::string_view message) {
    if (logger_) logger_->warn("{}", message);
}

void Logger::error(std::string_view message) {
    if (logger_) logger_->error("{}", message);
}

void Logger::fatal(std::string_view message) {
    if (logger_) logger_->critical("{}", message);
}

void Logger::set_level(LogLevel level) {
    if (logger_) {
        logger_->set_level(detail::to_spdlog_level(level));
    }
}

LogLevel Logger::get_level() const noexcept {
    if (!logger_) return LogLevel::Off;

    switch (logger_->level()) {
        case spdlog::level::trace:
            return LogLevel::Trace;
        case spdlog::level::debug:
            return LogLevel::Debug;
        case spdlog::level::info:
            return LogLevel::Info;
        case spdlog::level::warn:
            return LogLevel::Warning;
        case spdlog::level::err:
            return LogLevel::Error;
        case spdlog::level::critical:
            return LogLevel::Fatal;
        case spdlog::level::off:
            return LogLevel::Off;
        default:
            return LogLevel::Info;
    }
}

void Logger::flush() {
    if (logger_) {
        logger_->flush();
    }
}

void Logger::set_pattern(std::string_view pattern) {
    if (logger_) {
        logger_->set_pattern(std::string(pattern));
    }
}

bool Logger::should_log(LogLevel level) const noexcept {
    if (!logger_) return false;
    return logger_->should_log(detail::to_spdlog_level(level));
}

// ==========================================================================
// LoggerManager 实现
// ==========================================================================

void LoggerManager::initialize(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(detail::g_logger_mutex);

    detail::g_default_config = config;

    // 从环境变量覆盖日志级别
    LogLevel env_level = detail::get_env_log_level();
    if (env_level != LogLevel::Info) {
        detail::g_default_config.level = env_level;
    }

    // 创建默认日志器
    auto default_logger = detail::create_logger("default", detail::g_default_config);
    spdlog::set_default_logger(default_logger);
    spdlog::set_level(detail::to_spdlog_level(detail::g_default_config.level));

    // 注册到全局注册表
    detail::g_loggers["default"] = default_logger;
}

void LoggerManager::shutdown() {
    std::lock_guard<std::mutex> lock(detail::g_logger_mutex);

    // 刷新所有日志器
    spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) {
        logger->flush();
    });

    // 清空注册表
    detail::g_loggers.clear();

    // 释放线程池
    detail::g_thread_pool.reset();

    // 关闭 spdlog
    spdlog::shutdown();
}

Logger LoggerManager::get_logger(const std::string& name) {
    std::lock_guard<std::mutex> lock(detail::g_logger_mutex);

    auto it = detail::g_loggers.find(name);
    if (it != detail::g_loggers.end()) {
        return Logger(it->second);
    }

    // 创建新日志器
    auto logger = detail::create_logger(name, detail::g_default_config);
    detail::g_loggers[name] = logger;

    return Logger(logger);
}

Logger LoggerManager::create_logger(const std::string& name, const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(detail::g_logger_mutex);

    auto logger = detail::create_logger(name, config);
    detail::g_loggers[name] = logger;

    return Logger(logger);
}

bool LoggerManager::has_logger(const std::string& name) {
    std::lock_guard<std::mutex> lock(detail::g_logger_mutex);
    return detail::g_loggers.find(name) != detail::g_loggers.end();
}

void LoggerManager::remove_logger(const std::string& name) {
    std::lock_guard<std::mutex> lock(detail::g_logger_mutex);

    auto it = detail::g_loggers.find(name);
    if (it != detail::g_loggers.end()) {
        it->second->flush();
        detail::g_loggers.erase(it);
    }
}

void LoggerManager::set_default_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(detail::g_logger_mutex);

    detail::g_default_config.level = level;
    spdlog::set_level(detail::to_spdlog_level(level));

    for (auto& [name, logger] : detail::g_loggers) {
        logger->set_level(detail::to_spdlog_level(level));
    }
}

void LoggerManager::flush_all() {
    spdlog::apply_all([](const std::shared_ptr<spdlog::logger>& logger) {
        logger->flush();
    });
}

// ==========================================================================
// 便捷函数实现
// ==========================================================================

namespace log {

    namespace {
        // 线程局部默认日志器
        thread_local Logger t_default_logger;
    }

    void initialize(const LoggerConfig& config) {
        LoggerManager::initialize(config);
        t_default_logger = LoggerManager::get_logger("default");
    }

    void shutdown() {
        LoggerManager::shutdown();
        t_default_logger = Logger();
    }

    Logger& default_logger() {
        if (!t_default_logger) {
            t_default_logger = LoggerManager::get_logger("default");
        }
        return t_default_logger;
    }

    void set_default_logger(const Logger& logger) {
        t_default_logger = logger;
    }

    void trace(std::string_view message) {
        default_logger().trace(message);
    }

    void debug(std::string_view message) {
        default_logger().debug(message);
    }

    void info(std::string_view message) {
        default_logger().info(message);
    }

    void warning(std::string_view message) {
        default_logger().warning(message);
    }

    void error(std::string_view message) {
        default_logger().error(message);
    }

    void fatal(std::string_view message) {
        default_logger().fatal(message);
    }

    void flush() {
        default_logger().flush();
    }

} // namespace log

} // namespace aam::core
