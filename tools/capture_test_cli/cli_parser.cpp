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
// @file cli_parser.cpp
// @brief 命令行参数解析器实现
// @version 0.2.0-alpha.2
// ==========================================================================

#include "cli_parser.hpp"

#include <algorithm>
#include <cstdlib>
#include <cxxopts.hpp>
#include <fmt/format.h>
#include <stdexcept>
#include <string_view>

namespace aam::tools {

namespace {
    constexpr std::string_view kVersion = "0.2.0-alpha.2";
    constexpr std::string_view kAppName = "aam-capture-test";
}

CliParser::CliParser(int argc, char* argv[])
    : argc_(argc), argv_(argv) {}

[[nodiscard]] ParsedArgs CliParser::Parse() const {
    ParsedArgs result;
    
    cxxopts::Options options(std::string(kAppName), 
                             "AAM Capture Backend Test CLI");
    
    // 全局选项
    options.add_options("Global")
        ("h,help", "Show help message")
        ("v,version", "Show version information")
        ("verbose", "Enable verbose output", cxxopts::value<bool>()->default_value("false"));
    
    // 子命令：list
    options.add_options("List")
        ("l,list", "List available capture devices/windows");
    
    // 子命令：capture
    options.add_options("Capture")
        ("c,capture", "Run capture test")
        ("b,backend", "Capture backend (adb|maa|win32|auto)", 
         cxxopts::value<std::string>()->default_value("auto"))
        ("t,target", "Target device/window ID", 
         cxxopts::value<std::string>())
        ("f,format", "Output pixel format (rgba|bgra|rgb|gray)", 
         cxxopts::value<std::string>()->default_value("rgba"))
        ("fps", "Target FPS", cxxopts::value<uint32_t>()->default_value("30"))
        ("d,duration", "Test duration in milliseconds", 
         cxxopts::value<uint32_t>()->default_value("10000"))
        ("o,output", "Output directory for saved frames", 
         cxxopts::value<std::string>())
        ("save-frames", "Save captured frames to disk", 
         cxxopts::value<bool>()->default_value("false"))
        ("save-interval", "Frame save interval", 
         cxxopts::value<uint32_t>()->default_value("30"))
        ("preview", "Show preview window", 
         cxxopts::value<bool>()->default_value("false"));
    
    // 子命令：benchmark
    options.add_options("Benchmark")
        ("benchmark", "Run performance benchmark")
        ("backends", "Backends to benchmark (comma-separated: adb,maa,win32)", 
         cxxopts::value<std::string>()->default_value("adb,maa,win32"))
        ("benchmark-duration", "Duration per backend in milliseconds", 
         cxxopts::value<uint32_t>()->default_value("30000"))
        ("result-file", "Benchmark result output file", 
         cxxopts::value<std::string>());
    
    // 子命令：compare
    options.add_options("Compare")
        ("compare", "Compare multiple backends")
        ("compare-backends", "Backends to compare (comma-separated)", 
         cxxopts::value<std::string>()->default_value("adb,maa,win32"))
        ("compare-duration", "Comparison duration in milliseconds", 
         cxxopts::value<uint32_t>()->default_value("10000"))
        ("compare-save", "Save comparison frames", 
         cxxopts::value<bool>()->default_value("false"))
        ("compare-output", "Comparison output directory", 
         cxxopts::value<std::string>());
    
    options.positional_help("<command> [options]");
    options.show_positional_help();
    
    // 添加位置参数（子命令）
    options.parse_positional({"command"});
    options.add_options("Positional")
        ("command", "Command to execute (list, capture, benchmark, compare)", cxxopts::value<std::string>());
    
    try {
        auto parsed = options.parse(argc_, argv_);
        
        // 处理全局选项
        if (parsed.count("help")) {
            result.show_help = true;
            return result;
        }
        
        if (parsed.count("version")) {
            result.show_version = true;
            return result;
        }
        
        // 确定子命令（从位置参数或选项）
        std::string command;
        if (parsed.count("command")) {
            command = parsed["command"].as<std::string>();
        }
        
        if (command == "list" || parsed.count("list")) {
            result.mode = TestMode::ListDevices;
        } else if (command == "capture" || parsed.count("capture")) {
            result.mode = TestMode::Capture;
            
            // 解析捕获配置
            auto& config = result.capture_config;
            config.backend = StringToBackendType(parsed["backend"].as<std::string>());
            
            if (parsed.count("target")) {
                config.target_id = parsed["target"].as<std::string>();
            } else {
                throw std::runtime_error("Target ID is required for capture test");
            }
            
            config.format = StringToPixelFormat(parsed["format"].as<std::string>());
            config.fps = parsed["fps"].as<uint32_t>();
            config.duration_ms = parsed["duration"].as<uint32_t>();
            config.save_frames = parsed["save-frames"].as<bool>();
            config.save_interval = parsed["save-interval"].as<uint32_t>();
            config.show_preview = parsed["preview"].as<bool>();
            config.verbose = parsed["verbose"].as<bool>();
            
            if (parsed.count("output")) {
                config.output_dir = parsed["output"].as<std::string>();
            }
        } else if (command == "benchmark" || parsed.count("benchmark")) {
            result.mode = TestMode::Benchmark;
            
            auto& config = result.benchmark_config;
            
            // 解析后端列表
            std::string backends_str = parsed["backends"].as<std::string>();
            size_t pos = 0;
            while ((pos = backends_str.find(',')) != std::string::npos) {
                std::string backend = backends_str.substr(0, pos);
                backends_str.erase(0, pos + 1);
                config.backends.push_back(StringToBackendType(backend));
            }
            if (!backends_str.empty()) {
                config.backends.push_back(StringToBackendType(backends_str));
            }
            
            if (parsed.count("target")) {
                config.target_id = parsed["target"].as<std::string>();
            } else {
                throw std::runtime_error("Target ID is required for benchmark");
            }
            
            config.duration_ms = parsed["benchmark-duration"].as<uint32_t>();
            config.verbose = parsed["verbose"].as<bool>();
            
            if (parsed.count("result-file")) {
                config.output_file = parsed["result-file"].as<std::string>();
            }
        } else if (command == "compare" || parsed.count("compare")) {
            result.mode = TestMode::Compare;
            
            auto& config = result.compare_config;
            
            // 解析后端列表
            std::string backends_str = parsed["compare-backends"].as<std::string>();
            size_t pos = 0;
            while ((pos = backends_str.find(',')) != std::string::npos) {
                std::string backend = backends_str.substr(0, pos);
                backends_str.erase(0, pos + 1);
                config.backends.push_back(StringToBackendType(backend));
            }
            if (!backends_str.empty()) {
                config.backends.push_back(StringToBackendType(backends_str));
            }
            
            if (parsed.count("target")) {
                config.target_id = parsed["target"].as<std::string>();
            } else {
                throw std::runtime_error("Target ID is required for comparison");
            }
            
            config.duration_ms = parsed["compare-duration"].as<uint32_t>();
            config.save_frames = parsed["compare-save"].as<bool>();
            config.verbose = parsed["verbose"].as<bool>();
            
            if (parsed.count("compare-output")) {
                config.output_dir = parsed["compare-output"].as<std::string>();
            }
        } else {
            result.show_help = true;
        }
        
    } catch (const cxxopts::exceptions::exception& e) {
        throw std::runtime_error(fmt::format("Argument parsing error: {}", e.what()));
    }
    
    return result;
}

[[nodiscard]] std::string CliParser::GetHelpString() {
    return R"(AAM Capture Backend Test CLI - Version )" + std::string(kVersion) + R"(

USAGE:
    aam-capture-test <command> [options]

COMMANDS:
    list                    List available capture devices/windows
    capture                 Run capture test with specified backend
    benchmark               Run performance benchmark across backends
    compare                 Compare multiple backends side-by-side

GLOBAL OPTIONS:
    -h, --help              Show this help message
    -v, --version           Show version information
    --verbose               Enable verbose output

CAPTURE OPTIONS:
    -b, --backend <type>    Backend type: adb, maa, win32, auto (default: auto)
    -t, --target <id>       Target device/window ID
    -f, --format <fmt>      Pixel format: rgba, bgra, rgb, gray (default: rgba)
    --fps <n>               Target FPS (default: 30)
    -d, --duration <ms>     Test duration in milliseconds (default: 10000)
    -o, --output <dir>      Output directory for saved frames
    --save-frames           Save captured frames to disk
    --save-interval <n>     Frame save interval (default: 30)
    --preview               Show preview window

BENCHMARK OPTIONS:
    --backends <list>       Comma-separated backend list (default: adb,maa,win32)
    --benchmark-duration <ms> Duration per backend (default: 30000)
    --result-file <file>    Save results to file

COMPARE OPTIONS:
    --compare-backends <list>   Backends to compare (default: adb,maa,win32)
    --compare-duration <ms>     Comparison duration (default: 10000)
    --compare-save              Save comparison frames
    --compare-output <dir>      Comparison output directory

EXAMPLES:
    # List all available devices
    aam-capture-test list

    # Capture from ADB device
    aam-capture-test capture -b adb -t emulator-5554 --save-frames -o ./frames

    # Benchmark all backends
    aam-capture-test benchmark -t "My Window" --backends adb,win32

    # Compare ADB and Win32 backends
    aam-capture-test compare -t emulator-5554 --compare-backends adb,win32
)";
}

[[nodiscard]] std::string CliParser::GetVersionString() {
    return fmt::format("{} version {}", kAppName, kVersion);
}

[[nodiscard]] BackendType StringToBackendType(std::string_view str) {
    std::string lower(str);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "adb") return BackendType::Adb;
    if (lower == "maa") return BackendType::Maa;
    if (lower == "win32") return BackendType::Win32;
    if (lower == "auto") return BackendType::Auto;
    
    throw std::invalid_argument(fmt::format("Unknown backend type: {}", str));
}

[[nodiscard]] std::string_view BackendTypeToString(BackendType type) {
    switch (type) {
        case BackendType::Adb: return "adb";
        case BackendType::Maa: return "maa";
        case BackendType::Win32: return "win32";
        case BackendType::Auto: return "auto";
    }
    return "unknown";
}

[[nodiscard]] PixelFormatType StringToPixelFormat(std::string_view str) {
    std::string lower(str);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "rgba") return PixelFormatType::RGBA;
    if (lower == "bgra") return PixelFormatType::BGRA;
    if (lower == "rgb") return PixelFormatType::RGB;
    if (lower == "gray" || lower == "grey") return PixelFormatType::Gray;
    
    throw std::invalid_argument(fmt::format("Unknown pixel format: {}", str));
}

[[nodiscard]] std::string_view PixelFormatToString(PixelFormatType format) {
    switch (format) {
        case PixelFormatType::RGBA: return "rgba";
        case PixelFormatType::BGRA: return "bgra";
        case PixelFormatType::RGB: return "rgb";
        case PixelFormatType::Gray: return "gray";
    }
    return "unknown";
}

} // namespace aam::tools
