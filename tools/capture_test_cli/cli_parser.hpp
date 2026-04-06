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
// @file cli_parser.hpp
// @brief 命令行参数解析器
// @version 0.2.0-alpha.2
// ==========================================================================

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace aam::tools
{

/**
 * @brief 捕获后端类型
 */
enum class BackendType
{
    Adb,    ///< ADB H264 捕获
    Maa,    ///< MaaFramework 桥接
    Win32,  ///< Win32 窗口捕获
    Auto    ///< 自动选择
};

/**
 * @brief 像素格式类型
 */
enum class PixelFormatType
{
    RGBA,
    BGRA,
    RGB,
    Gray
};

/**
 * @brief 测试模式
 */
enum class TestMode
{
    ListDevices,  ///< 列出可用设备
    Capture,      ///< 捕获测试
    Benchmark,    ///< 性能基准测试
    Compare,      ///< 后端对比测试
    Help          ///< 显示帮助
};

/**
 * @brief 捕获测试配置
 */
struct CaptureTestConfig
{
    BackendType                backend{BackendType::Auto};     ///< 捕获后端类型
    std::string                target_id;                      ///< 目标设备/窗口 ID
    PixelFormatType            format{PixelFormatType::RGBA};  ///< 输出像素格式
    uint32_t                   fps{30};                        ///< 目标帧率
    uint32_t                   duration_ms{10000};             ///< 测试持续时间（毫秒）
    std::optional<std::string> output_dir;                     ///< 输出目录
    bool                       save_frames{false};             ///< 是否保存帧
    uint32_t                   save_interval{30};              ///< 保存间隔（帧数）
    bool                       show_preview{false};            ///< 是否显示预览窗口
    bool                       verbose{false};                 ///< 详细输出
};

/**
 * @brief 基准测试配置
 */
struct BenchmarkConfig
{
    std::vector<BackendType>   backends;            ///< 要测试的后端列表
    std::string                target_id;           ///< 目标设备/窗口 ID
    uint32_t                   duration_ms{30000};  ///< 每个后端的测试时长
    std::optional<std::string> output_file;         ///< 结果输出文件
    bool                       verbose{false};      ///< 详细输出
};

/**
 * @brief 对比测试配置
 */
struct CompareConfig
{
    std::vector<BackendType>   backends;            ///< 要对比的后端列表
    std::string                target_id;           ///< 目标设备/窗口 ID
    uint32_t                   duration_ms{10000};  ///< 测试持续时间
    bool                       save_frames{false};  ///< 保存对比帧
    std::optional<std::string> output_dir;          ///< 输出目录
    bool                       verbose{false};      ///< 详细输出
};

/**
 * @brief 解析后的命令行参数
 */
struct ParsedArgs
{
    TestMode          mode{TestMode::Help};  ///< 测试模式
    CaptureTestConfig capture_config;        ///< 捕获测试配置
    BenchmarkConfig   benchmark_config;      ///< 基准测试配置
    CompareConfig     compare_config;        ///< 对比测试配置
    bool              show_help{false};      ///< 显示帮助
    bool              show_version{false};   ///< 显示版本
};

/**
 * @brief 命令行参数解析器
 */
class CliParser
{
public:
    /**
     * @brief 构造函数
     * @param argc 参数个数
     * @param argv 参数数组
     */
    CliParser(int argc, char* argv[]);

    /**
     * @brief 解析命令行参数
     * @return ParsedArgs 解析结果
     * @throws std::runtime_error 解析失败时抛出
     */
    [[nodiscard]] ParsedArgs Parse() const;

    /**
     * @brief 获取帮助信息字符串
     * @return std::string 帮助信息
     */
    [[nodiscard]] static std::string GetHelpString();

    /**
     * @brief 获取版本信息字符串
     * @return std::string 版本信息
     */
    [[nodiscard]] static std::string GetVersionString();

private:
    int    argc_;
    char** argv_;
};

/**
 * @brief 将字符串转换为后端类型
 * @param str 输入字符串
 * @return BackendType 后端类型
 * @throws std::invalid_argument 无效字符串时抛出
 */
[[nodiscard]] BackendType StringToBackendType(std::string_view str);

/**
 * @brief 将后端类型转换为字符串
 * @param type 后端类型
 * @return std::string_view 字符串表示
 */
[[nodiscard]] std::string_view BackendTypeToString(BackendType type);

/**
 * @brief 将字符串转换为像素格式类型
 * @param str 输入字符串
 * @return PixelFormatType 像素格式类型
 * @throws std::invalid_argument 无效字符串时抛出
 */
[[nodiscard]] PixelFormatType StringToPixelFormat(std::string_view str);

/**
 * @brief 将像素格式类型转换为字符串
 * @param format 像素格式类型
 * @return std::string_view 字符串表示
 */
[[nodiscard]] std::string_view PixelFormatToString(PixelFormatType format);

}  // namespace aam::tools
