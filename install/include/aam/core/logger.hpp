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
// @file logger.hpp
// @author dhjs0000
// @brief spdlog 日志系统封装头文件
// ==========================================================================
// 版本: v0.2.0-alpha.1
// 功能: 提供统一的日志接口，支持控制台、文件、异步输出
// 依赖: spdlog, fmt
// ==========================================================================

#ifndef AAM_CORE_LOGGER_HPP
#define AAM_CORE_LOGGER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace aam::core
{

// ==========================================================================
// 日志级别枚举
// ==========================================================================

/**
 * @brief 日志级别
 * @details 与 spdlog 级别对应，但提供更简洁的命名
 */
enum class LogLevel : std::uint8_t
{
    Trace   = 0,  ///< 跟踪信息（最详细）
    Debug   = 1,  ///< 调试信息
    Info    = 2,  ///< 普通信息
    Warning = 3,  ///< 警告信息
    Error   = 4,  ///< 错误信息
    Fatal   = 5,  ///< 致命错误
    Off     = 6,  ///< 关闭日志
};

// ==========================================================================
// 日志轮转策略
// ==========================================================================

/**
 * @brief 日志文件轮转策略
 */
enum class LogRotationPolicy : std::uint8_t
{
    None,       ///< 不轮转
    SizeBased,  ///< 基于文件大小轮转
    Daily,      ///< 按天轮转
};

// ==========================================================================
// 日志配置结构
// ==========================================================================

/**
 * @brief 日志配置参数
 */
struct LoggerConfig
{
    // 日志级别
    LogLevel level{LogLevel::Info};           ///< 日志级别
    LogLevel flush_level{LogLevel::Warning};  ///< 自动刷新级别

    // 控制台输出
    bool enable_console{true};  ///< 启用控制台输出
    bool use_color{true};       ///< 使用彩色输出

    // 文件输出
    bool        enable_file{false};  ///< 启用文件输出
    std::string file_path;           ///< 日志文件路径

    // 轮转配置
    LogRotationPolicy rotation_policy{LogRotationPolicy::None};
    std::size_t       max_file_size{10 * 1024 * 1024};  ///< 单个文件最大大小（默认 10MB）
    std::size_t       max_files{5};                     ///< 最大保留文件数
    int               rotation_hour{0};                 ///< 轮转小时（Daily 模式）
    int               rotation_minute{0};               ///< 轮转分钟（Daily 模式）

    // 异步配置
    bool async_mode{false};  ///< 异步模式

    // 格式配置
    std::string pattern;  ///< 自定义格式模式（空则使用默认）
};

// ==========================================================================
// 日志器类
// ==========================================================================

/**
 * @brief 日志器封装类
 * @details 提供类型安全的日志接口，封装 spdlog 功能
 */
class Logger
{
public:
    /**
     * @brief 默认构造函数
     * @note 构造空日志器，所有操作无效果
     */
    Logger() = default;

    /**
     * @brief 从 spdlog logger 构造
     * @param logger spdlog 日志器
     */
    explicit Logger(std::shared_ptr<spdlog::logger> logger);

    /**
     * @brief 析构函数
     */
    ~Logger() = default;

    // 拷贝和移动
    Logger(const Logger&)            = default;
    Logger& operator=(const Logger&) = default;
    Logger(Logger&&)                 = default;
    Logger& operator=(Logger&&)      = default;

    /**
     * @brief 检查日志器是否有效
     * @return true 如果有效
     */
    [[nodiscard]] explicit operator bool() const noexcept
    {
        return logger_ != nullptr;
    }

    /**
     * @brief 记录日志
     * @param level 日志级别
     * @param message 日志消息
     */
    void log(LogLevel level, std::string_view message);

    /**
     * @brief 记录跟踪日志
     * @param message 日志消息
     */
    void trace(std::string_view message);

    /**
     * @brief 记录调试日志
     * @param message 日志消息
     */
    void debug(std::string_view message);

    /**
     * @brief 记录信息日志
     * @param message 日志消息
     */
    void info(std::string_view message);

    /**
     * @brief 记录警告日志
     * @param message 日志消息
     */
    void warning(std::string_view message);

    /**
     * @brief 记录错误日志
     * @param message 日志消息
     */
    void error(std::string_view message);

    /**
     * @brief 记录致命错误日志
     * @param message 日志消息
     */
    void fatal(std::string_view message);

    /**
     * @brief 设置日志级别
     * @param level 日志级别
     */
    void set_level(LogLevel level);

    /**
     * @brief 获取当前日志级别
     * @return 日志级别
     */
    [[nodiscard]] LogLevel get_level() const noexcept;

    /**
     * @brief 刷新日志缓冲区
     */
    void flush();

    /**
     * @brief 设置日志格式
     * @param pattern 格式模式字符串
     */
    void set_pattern(std::string_view pattern);

    /**
     * @brief 检查是否应该记录指定级别
     * @param level 日志级别
     * @return true 如果应该记录
     */
    [[nodiscard]] bool should_log(LogLevel level) const noexcept;

    /**
     * @brief 获取内部 spdlog logger
     * @return spdlog logger 指针
     */
    [[nodiscard]] std::shared_ptr<spdlog::logger> native() const
    {
        return logger_;
    }

    /**
     * @brief 格式化日志（模板版本）
     * @tparam Args 参数类型
     * @param level 日志级别
     * @param fmt 格式字符串
     * @param args 参数
     */
    template <typename... Args>
    void log_format(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args)
    {
        if (!logger_ || !should_log(level))
            return;

        std::string message = fmt::format(fmt, std::forward<Args>(args)...);
        log(level, message);
    }

    /**
     * @brief 格式化跟踪日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 参数
     */
    template <typename... Args>
    void trace_fmt(fmt::format_string<Args...> fmt, Args&&... args)
    {
        if (!logger_ || !should_log(LogLevel::Trace))
            return;
        logger_->trace(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 格式化调试日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 参数
     */
    template <typename... Args>
    void debug_fmt(fmt::format_string<Args...> fmt, Args&&... args)
    {
        if (!logger_ || !should_log(LogLevel::Debug))
            return;
        logger_->debug(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 格式化信息日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 参数
     */
    template <typename... Args>
    void info_fmt(fmt::format_string<Args...> fmt, Args&&... args)
    {
        if (!logger_ || !should_log(LogLevel::Info))
            return;
        logger_->info(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 格式化警告日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 参数
     */
    template <typename... Args>
    void warning_fmt(fmt::format_string<Args...> fmt, Args&&... args)
    {
        if (!logger_ || !should_log(LogLevel::Warning))
            return;
        logger_->warn(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 格式化错误日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 参数
     */
    template <typename... Args>
    void error_fmt(fmt::format_string<Args...> fmt, Args&&... args)
    {
        if (!logger_ || !should_log(LogLevel::Error))
            return;
        logger_->error(fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 格式化致命错误日志
     * @tparam Args 参数类型
     * @param fmt 格式字符串
     * @param args 参数
     */
    template <typename... Args>
    void fatal_fmt(fmt::format_string<Args...> fmt, Args&&... args)
    {
        if (!logger_ || !should_log(LogLevel::Fatal))
            return;
        logger_->critical(fmt, std::forward<Args>(args)...);
    }

private:
    std::shared_ptr<spdlog::logger> logger_;
};

// ==========================================================================
// 日志管理器
// ==========================================================================

/**
 * @brief 日志管理器
 * @details 管理所有日志器的生命周期和配置
 */
class LoggerManager
{
public:
    /**
     * @brief 初始化日志系统
     * @param config 日志配置
     * @note 应在程序启动时调用一次
     */
    static void initialize(const LoggerConfig& config = LoggerConfig{});

    /**
     * @brief 关闭日志系统
     * @note 应在程序退出前调用，确保所有日志写入
     */
    static void shutdown();

    /**
     * @brief 获取日志器
     * @param name 日志器名称
     * @return 日志器对象
     */
    [[nodiscard]] static Logger get_logger(const std::string& name);

    /**
     * @brief 创建新日志器
     * @param name 日志器名称
     * @param config 日志配置
     * @return 日志器对象
     */
    [[nodiscard]] static Logger create_logger(const std::string& name, const LoggerConfig& config);

    /**
     * @brief 检查日志器是否存在
     * @param name 日志器名称
     * @return true 如果存在
     */
    [[nodiscard]] static bool has_logger(const std::string& name);

    /**
     * @brief 移除日志器
     * @param name 日志器名称
     */
    static void remove_logger(const std::string& name);

    /**
     * @brief 设置默认日志级别
     * @param level 日志级别
     */
    static void set_default_level(LogLevel level);

    /**
     * @brief 刷新所有日志器
     */
    static void flush_all();

private:
    LoggerManager()  = delete;
    ~LoggerManager() = delete;
};

// ==========================================================================
// 便捷日志函数
// ==========================================================================

/**
 * @brief 全局日志便捷函数命名空间
 */
namespace log
{

/**
 * @brief 初始化日志系统
 * @param config 日志配置
 */
void initialize(const LoggerConfig& config = LoggerConfig{});

/**
 * @brief 关闭日志系统
 */
void shutdown();

/**
 * @brief 获取默认日志器
 * @return 默认日志器
 */
[[nodiscard]] Logger& default_logger();

/**
 * @brief 设置默认日志器
 * @param logger 日志器
 */
void set_default_logger(const Logger& logger);

/**
 * @brief 记录跟踪日志
 * @param message 日志消息
 */
void trace(std::string_view message);

/**
 * @brief 记录调试日志
 * @param message 日志消息
 */
void debug(std::string_view message);

/**
 * @brief 记录信息日志
 * @param message 日志消息
 */
void info(std::string_view message);

/**
 * @brief 记录警告日志
 * @param message 日志消息
 */
void warning(std::string_view message);

/**
 * @brief 记录错误日志
 * @param message 日志消息
 */
void error(std::string_view message);

/**
 * @brief 记录致命错误日志
 * @param message 日志消息
 */
void fatal(std::string_view message);

/**
 * @brief 刷新日志缓冲区
 */
void flush();

/**
 * @brief 格式化跟踪日志
 * @tparam Args 参数类型
 * @param fmt 格式字符串
 * @param args 参数
 */
template <typename... Args>
void trace_fmt(fmt::format_string<Args...> fmt, Args&&... args)
{
    default_logger().trace_fmt(fmt, std::forward<Args>(args)...);
}

/**
 * @brief 格式化调试日志
 * @tparam Args 参数类型
 * @param fmt 格式字符串
 * @param args 参数
 */
template <typename... Args>
void debug_fmt(fmt::format_string<Args...> fmt, Args&&... args)
{
    default_logger().debug_fmt(fmt, std::forward<Args>(args)...);
}

/**
 * @brief 格式化信息日志
 * @tparam Args 参数类型
 * @param fmt 格式字符串
 * @param args 参数
 */
template <typename... Args>
void info_fmt(fmt::format_string<Args...> fmt, Args&&... args)
{
    default_logger().info_fmt(fmt, std::forward<Args>(args)...);
}

/**
 * @brief 格式化警告日志
 * @tparam Args 参数类型
 * @param fmt 格式字符串
 * @param args 参数
 */
template <typename... Args>
void warning_fmt(fmt::format_string<Args...> fmt, Args&&... args)
{
    default_logger().warning_fmt(fmt, std::forward<Args>(args)...);
}

/**
 * @brief 格式化错误日志
 * @tparam Args 参数类型
 * @param fmt 格式字符串
 * @param args 参数
 */
template <typename... Args>
void error_fmt(fmt::format_string<Args...> fmt, Args&&... args)
{
    default_logger().error_fmt(fmt, std::forward<Args>(args)...);
}

/**
 * @brief 格式化致命错误日志
 * @tparam Args 参数类型
 * @param fmt 格式字符串
 * @param args 参数
 */
template <typename... Args>
void fatal_fmt(fmt::format_string<Args...> fmt, Args&&... args)
{
    default_logger().fatal_fmt(fmt, std::forward<Args>(args)...);
}

}  // namespace log

// ==========================================================================
// 宏定义（可选，用于快速日志记录）
// ==========================================================================

/**
 * @brief 条件日志宏
 * @param level 日志级别
 * @param ... 格式参数
 */
#define AAM_LOG(level, ...)                                                                        \
    do {                                                                                           \
        auto& logger = aam::core::log::default_logger();                                           \
        if (logger.should_log(level)) {                                                            \
            logger.log_format(level, __VA_ARGS__);                                                 \
        }                                                                                          \
    } while (0)

/**
 * @brief 跟踪日志宏
 */
#define AAM_LOG_TRACE(...) AAM_LOG(aam::core::LogLevel::Trace, __VA_ARGS__)

/**
 * @brief 调试日志宏
 */
#define AAM_LOG_DEBUG(...) AAM_LOG(aam::core::LogLevel::Debug, __VA_ARGS__)

/**
 * @brief 信息日志宏
 */
#define AAM_LOG_INFO(...) AAM_LOG(aam::core::LogLevel::Info, __VA_ARGS__)

/**
 * @brief 警告日志宏
 */
#define AAM_LOG_WARNING(...) AAM_LOG(aam::core::LogLevel::Warning, __VA_ARGS__)

/**
 * @brief 错误日志宏
 */
#define AAM_LOG_ERROR(...) AAM_LOG(aam::core::LogLevel::Error, __VA_ARGS__)

/**
 * @brief 致命错误日志宏
 */
#define AAM_LOG_FATAL(...) AAM_LOG(aam::core::LogLevel::Fatal, __VA_ARGS__)

/**
 * @brief 条件日志宏（仅在条件为真时记录）
 */
#define AAM_LOG_IF(condition, level, ...)                                                          \
    do {                                                                                           \
        if (condition) {                                                                           \
            AAM_LOG(level, __VA_ARGS__);                                                           \
        }                                                                                          \
    } while (0)

}  // namespace aam::core

#endif  // AAM_CORE_LOGGER_HPP
