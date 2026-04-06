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
// @file benchmark.cpp
// @brief 性能基准测试命令实现
// @version 0.2.0-alpha.2
// ==========================================================================

#include "benchmark.hpp"

#include "../utils/statistics.hpp"
#include "../../core/src/l0_sensing/adb_capture.hpp"
#ifdef AAM_PLATFORM_WINDOWS
#include "../../core/src/l0_sensing/win32_capture.hpp"
#endif

#include <aam/l0/capture_backend.hpp>

#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/os.h>
#include <iostream>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>

namespace aam::tools {

namespace {
    /**
     * @brief 后端性能结果
     */
    struct BackendResult {
        std::string backend_name;
        uint64_t total_frames{0};
        double duration_sec{0.0};
        double avg_fps{0.0};
        double min_fps{0.0};
        double max_fps{0.0};
        double avg_latency_ms{0.0};
        double total_mb{0.0};
        bool success{false};
        std::string error_message;
    };
    
    /**
     * @brief 创建捕获后端
     */
    std::unique_ptr<l0::ICaptureBackend> CreateBackend(BackendType type) {
        switch (type) {
            case BackendType::Adb:
                return std::make_unique<l0::AdbCaptureBackend>();
            case BackendType::Win32:
#ifdef AAM_PLATFORM_WINDOWS
                return std::make_unique<l0::Win32CaptureBackend>();
#else
                std::cerr << "Win32 backend only available on Windows\n";
                return nullptr;
#endif
            case BackendType::Maa:
                return nullptr;  // MAA 未实现
            default:
                return nullptr;
        }
    }
    
    /**
     * @brief 运行单个后端基准测试
     */
    BackendResult RunSingleBenchmark(BackendType type, const std::string& target_id,
                                     uint32_t duration_ms, bool verbose) {
        BackendResult result;
        result.backend_name = std::string(BackendTypeToString(type));
        
        auto backend = CreateBackend(type);
        if (!backend) {
            result.error_message = "Failed to create backend";
            return result;
        }
        
        if (verbose) {
            fmt::print("Testing {} backend...\n", result.backend_name);
        }
        
        // 配置
        l0::CaptureConfig config;
        config.target_id = target_id;
        config.preferred_format = l0::PixelFormat::RGBA32;
        config.target_fps = 60;
        config.buffer_queue_size = 60;
        config.max_frame_size = 1920 * 1080 * 4;
        
        // 初始化
        auto init_result = backend->Initialize(config);
        if (!init_result) {
            result.error_message = fmt::format("Init failed: {}", 
                                               static_cast<int>(init_result.error()));
            return result;
        }
        
        // 开始捕获
        auto start_result = backend->StartCapture();
        if (!start_result) {
            result.error_message = fmt::format("Start failed: {}",
                                               static_cast<int>(start_result.error()));
            [[maybe_unused]] auto _ = backend->Shutdown();
            return result;
        }
        
        // 收集帧
        CaptureStatistics stats;
        auto start_time = std::chrono::steady_clock::now();
        uint64_t frame_count = 0;
        uint64_t total_bytes = 0;
        
        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            
            if (elapsed >= static_cast<int64_t>(duration_ms)) {
                break;
            }
            
            auto frame_result = backend->TryGetFrame();
            if (frame_result && *frame_result) {
                auto& [metadata, pixel_data] = **frame_result;
                stats.AddFrame(metadata, pixel_data.size());
                frame_count++;
                total_bytes += pixel_data.size();
            }
        }
        
        // 停止
        [[maybe_unused]] auto _stop = backend->StopCapture();
        [[maybe_unused]] auto _shutdown = backend->Shutdown();
        
        // 收集结果
        auto final_stats = stats.GetStats();
        result.total_frames = final_stats.total_frames;
        result.duration_sec = final_stats.duration_sec;
        result.avg_fps = final_stats.avg_fps;
        result.min_fps = final_stats.min_fps;
        result.max_fps = final_stats.max_fps;
        result.total_mb = final_stats.total_mb;
        result.success = true;
        
        return result;
    }
    
    /**
     * @brief 打印结果表格
     */
    void PrintResultsTable(const std::vector<BackendResult>& results) {
        fmt::print(fg(fmt::color::cyan), "\n=== Benchmark Results ===\n\n");
        
        // 表头
        fmt::print("{:<12} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10}\n",
                   "Backend", "Frames", "Avg FPS", "Min FPS", "Max FPS", "Latency", "Data(MB)");
        fmt::print("{:-<12}-{:->10}-{:->10}-{:->10}-{:->10}-{:->10}-{:->10}\n",
                   "", "", "", "", "", "", "");
        
        // 数据行
        for (const auto& r : results) {
            if (r.success) {
                fmt::print(fg(fmt::color::green),
                           "{:<12} {:>10} {:>10.2f} {:>10.2f} {:>10.2f} {:>10.2f} {:>10.2f}\n",
                           r.backend_name, r.total_frames, r.avg_fps, r.min_fps, 
                           r.max_fps, r.avg_latency_ms, r.total_mb);
            } else {
                fmt::print(fg(fmt::color::red),
                           "{:<12} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10}  [{}]\n",
                           r.backend_name, "-", "-", "-", "-", "-", "-", r.error_message);
            }
        }
        
        fmt::print("\n");
    }
    
    /**
     * @brief 保存结果为 JSON
     */
    void SaveResultsToJson(const std::vector<BackendResult>& results,
                           const std::string& filename) {
        nlohmann::json j;
        auto now_time_t = std::time(nullptr);
        auto now_tm = *std::localtime(&now_time_t);
        j["timestamp"] = fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}",
                                     now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday,
                                     now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
        j["results"] = nlohmann::json::array();
        
        for (const auto& r : results) {
            nlohmann::json jr;
            jr["backend"] = r.backend_name;
            jr["success"] = r.success;
            if (r.success) {
                jr["frames"] = r.total_frames;
                jr["duration_sec"] = r.duration_sec;
                jr["avg_fps"] = r.avg_fps;
                jr["min_fps"] = r.min_fps;
                jr["max_fps"] = r.max_fps;
                jr["avg_latency_ms"] = r.avg_latency_ms;
                jr["total_mb"] = r.total_mb;
            } else {
                jr["error"] = r.error_message;
            }
            j["results"].push_back(jr);
        }
        
        std::ofstream file(filename);
        if (file) {
            file << j.dump(2);
            fmt::print(fg(fmt::color::green), "Results saved to: {}\n", filename);
        }
    }
}

int ExecuteBenchmark(const BenchmarkConfig& config) {
    fmt::print(fg(fmt::color::cyan), 
               "=== AAM Capture Backend Benchmark ===\n\n");
    fmt::print("Target: {}\n", config.target_id);
    fmt::print("Duration per backend: {} ms\n", config.duration_ms);
    fmt::print("Backends to test: {}\n", config.backends.size());
    fmt::print("\n");
    
    std::vector<BackendResult> results;
    
    for (auto backend_type : config.backends) {
        if (backend_type == BackendType::Auto) {
            continue;  // 跳过 Auto
        }
        
        auto result = RunSingleBenchmark(backend_type, config.target_id,
                                         config.duration_ms, config.verbose);
        results.push_back(result);
    }
    
    // 打印结果
    PrintResultsTable(results);
    
    // 找出最佳后端
    const BackendResult* best = nullptr;
    for (const auto& r : results) {
        if (r.success && (!best || r.avg_fps > best->avg_fps)) {
            best = &r;
        }
    }
    
    if (best) {
        fmt::print(fg(fmt::color::green), 
                   "Best performing backend: {} ({} FPS)\n",
                   best->backend_name, best->avg_fps);
    }
    
    // 保存结果
    if (config.output_file) {
        SaveResultsToJson(results, *config.output_file);
    }
    
    return 0;
}

} // namespace aam::tools
