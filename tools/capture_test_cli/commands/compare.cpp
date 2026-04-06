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
// @file compare.cpp
// @brief 后端对比测试命令实现
// @version 0.2.0-alpha.2
// ==========================================================================

#include "compare.hpp"

#include "../utils/frame_saver.hpp"
#include "../utils/statistics.hpp"
#include "../../core/src/l0_sensing/adb_capture.hpp"
#ifdef AAM_PLATFORM_WINDOWS
#include "../../core/src/l0_sensing/win32_capture.hpp"
#endif

#include <aam/l0/capture_backend.hpp>

#include <fmt/format.h>
#include <fmt/color.h>
#include <iostream>
#include <opencv2/opencv.hpp>

namespace aam::tools {

namespace {
    /**
     * @brief 后端对比结果
     */
    struct CompareResult {
        std::string backend_name;
        uint64_t frames_captured{0};
        double avg_fps{0.0};
        double consistency{0.0};  // FPS 稳定性 (0-1)
        cv::Mat sample_frame;
        bool success{false};
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
            default:
                return nullptr;
        }
    }
    
    /**
     * @brief 运行单个后端对比测试
     */
    CompareResult RunCompareTest(BackendType type, const std::string& target_id,
                                 uint32_t duration_ms, bool save_frames,
                                 const std::string& output_dir) {
        CompareResult result;
        result.backend_name = std::string(BackendTypeToString(type));
        
        auto backend = CreateBackend(type);
        if (!backend) {
            return result;
        }
        
        // 配置
        l0::CaptureConfig config;
        config.target_id = target_id;
        config.preferred_format = l0::PixelFormat::RGBA32;
        config.target_fps = 30;
        config.buffer_queue_size = 30;
        config.max_frame_size = 1920 * 1080 * 4;
        
        // 初始化
        if (!backend->Initialize(config)) {
            return result;
        }
        
        // 开始捕获
        if (!backend->StartCapture()) {
            [[maybe_unused]] auto _ = backend->Shutdown();
            return result;
        }
        
        // 收集帧
        CaptureStatistics stats;
        std::vector<double> fps_samples;
        cv::Mat last_frame;
        
        auto start_time = std::chrono::steady_clock::now();
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
                
                // 记录 FPS 样本
                fps_samples.push_back(stats.GetCurrentFps());
                
                // 保存最后一帧
                cv::Mat frame(metadata.height, metadata.width, CV_8UC4,
                             const_cast<std::byte*>(pixel_data.data()));
                cv::cvtColor(frame, last_frame, cv::COLOR_RGBA2BGR);
            }
        }
        
        // 停止
        [[maybe_unused]] auto _stop_result = backend->StopCapture();
        [[maybe_unused]] auto _shutdown_result = backend->Shutdown();
        
        // 计算结果
        auto final_stats = stats.GetStats();
        result.frames_captured = final_stats.total_frames;
        result.avg_fps = final_stats.avg_fps;
        result.sample_frame = last_frame;
        result.success = true;
        
        // 计算稳定性（FPS 标准差的倒数）
        if (fps_samples.size() > 1) {
            double sum = 0.0;
            for (double fps : fps_samples) {
                sum += fps;
            }
            double mean = sum / fps_samples.size();
            
            double variance = 0.0;
            for (double fps : fps_samples) {
                variance += (fps - mean) * (fps - mean);
            }
            variance /= fps_samples.size();
            
            double std_dev = std::sqrt(variance);
            result.consistency = 1.0 / (1.0 + std_dev / mean);  // 归一化到 0-1
        }
        
        // 保存样本帧
        if (save_frames && !last_frame.empty() && !output_dir.empty()) {
            std::filesystem::path dir(output_dir);
            std::filesystem::create_directories(dir);
            std::string filename = fmt::format("compare_{}.png", result.backend_name);
            cv::imwrite((dir / filename).string(), last_frame);
        }
        
        return result;
    }
    
    /**
     * @brief 计算图像相似度（简化版 MSE）
     */
    double CalculateSimilarity(const cv::Mat& img1, const cv::Mat& img2) {
        if (img1.empty() || img2.empty() || 
            img1.size() != img2.size()) {
            return 0.0;
        }
        
        cv::Mat diff;
        cv::absdiff(img1, img2, diff);
        diff.convertTo(diff, CV_32F);
        
        double mse = cv::mean(diff.mul(diff))[0];
        double similarity = 1.0 / (1.0 + mse / 10000.0);
        
        return similarity;
    }
}

int ExecuteCompare(const CompareConfig& config) {
    fmt::print(fg(fmt::color::cyan), 
               "=== AAM Backend Comparison ===\n\n");
    fmt::print("Target: {}\n", config.target_id);
    fmt::print("Duration: {} ms\n", config.duration_ms);
    fmt::print("\n");
    
    std::vector<CompareResult> results;
    
    // 运行每个后端的测试
    for (auto backend_type : config.backends) {
        if (backend_type == BackendType::Auto || backend_type == BackendType::Maa) {
            continue;
        }
        
        fmt::print("Testing {} backend...\n", BackendTypeToString(backend_type));
        
        std::string output_dir;
        if (config.output_dir) {
            output_dir = *config.output_dir;
        }
        
        auto result = RunCompareTest(backend_type, config.target_id,
                                     config.duration_ms, config.save_frames,
                                     output_dir);
        results.push_back(result);
    }
    
    // 打印结果
    fmt::print(fg(fmt::color::cyan), "\n=== Comparison Results ===\n\n");
    
    fmt::print("{:<12} {:>10} {:>10} {:>12} {:>10}\n",
               "Backend", "Frames", "Avg FPS", "Consistency", "Status");
    fmt::print("{:-<12}-{:->10}-{:->10}-{:->12}-{:->10}\n",
               "", "", "", "", "");
    
    for (const auto& r : results) {
        if (r.success) {
            fmt::print(fg(fmt::color::green),
                       "{:<12} {:>10} {:>10.2f} {:>11.1f}% {:>10}\n",
                       r.backend_name, r.frames_captured, r.avg_fps,
                       r.consistency * 100, "OK");
        } else {
            fmt::print(fg(fmt::color::red),
                       "{:<12} {:>10} {:>10} {:>12} {:>10}\n",
                       r.backend_name, "-", "-", "-", "FAILED");
        }
    }
    
    // 帧质量对比
    if (results.size() >= 2 && config.save_frames) {
        fmt::print(fg(fmt::color::cyan), "\n=== Frame Quality Comparison ===\n\n");
        
        for (size_t i = 0; i < results.size(); ++i) {
            for (size_t j = i + 1; j < results.size(); ++j) {
                if (results[i].success && results[j].success &&
                    !results[i].sample_frame.empty() && !results[j].sample_frame.empty()) {
                    
                    double similarity = CalculateSimilarity(results[i].sample_frame,
                                                             results[j].sample_frame);
                    fmt::print("{} vs {}: {:.1f}% similar\n",
                               results[i].backend_name, results[j].backend_name,
                               similarity * 100);
                }
            }
        }
    }
    
    // 推荐
    fmt::print(fg(fmt::color::cyan), "\n=== Recommendation ===\n\n");
    
    const CompareResult* best = nullptr;
    double best_score = 0.0;
    
    for (const auto& r : results) {
        if (r.success) {
            // 综合评分：FPS * 稳定性
            double score = r.avg_fps * r.consistency;
            if (score > best_score) {
                best_score = score;
                best = &r;
            }
        }
    }
    
    if (best) {
        fmt::print(fg(fmt::color::green),
                   "Recommended backend: {}\n", best->backend_name);
        fmt::print("  - Average FPS: {:.2f}\n", best->avg_fps);
        fmt::print("  - Consistency: {:.1f}%\n", best->consistency * 100);
        fmt::print("  - Total frames: {}\n", best->frames_captured);
    } else {
        fmt::print(fg(fmt::color::red),
                   "No backend succeeded in the comparison.\n");
    }
    
    return 0;
}

} // namespace aam::tools
