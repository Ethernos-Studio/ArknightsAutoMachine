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
// @file capture_test.cpp
// @brief 捕获测试命令实现
// @version 0.2.0-alpha.2
// ==========================================================================

#include "capture_test.hpp"

#include "../../core/src/l0_sensing/adb_capture.hpp"
#include "../utils/frame_saver.hpp"
#include "../utils/statistics.hpp"
#ifdef AAM_PLATFORM_WINDOWS
#    include "../../core/src/l0_sensing/win32_capture.hpp"
#endif
#ifdef AAM_MAAFW_ENABLED
#    include "../../core/src/l0_sensing/maa_adapter.hpp"
#endif

#include <aam/l0/capture_backend.hpp>
#include <iostream>
#include <thread>

#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <opencv2/opencv.hpp>

namespace aam::tools
{

namespace
{
/**
 * @brief 像素格式转换
 */
l0::PixelFormat ToL0PixelFormat(PixelFormatType type)
{
    switch (type) {
        case PixelFormatType::RGBA:
            return l0::PixelFormat::RGBA32;
        case PixelFormatType::BGRA:
            return l0::PixelFormat::BGRA32;
        case PixelFormatType::RGB:
            return l0::PixelFormat::RGB24;
        case PixelFormatType::Gray:
            return l0::PixelFormat::Unknown;
    }
    return l0::PixelFormat::RGBA32;
}

/**
 * @brief 创建捕获后端
 */
std::unique_ptr<l0::ICaptureBackend> CreateBackend(BackendType type)
{
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
#ifdef AAM_MAAFW_ENABLED
            return std::make_unique<l0::MaaCaptureBackend>();
#else
            std::cerr << "MAA backend not enabled. Build with -DAAM_MAAFW_ENABLED=ON\n";
            return nullptr;
#endif
        case BackendType::Auto:
            // 自动选择：优先尝试 ADB，然后 Win32
            return std::make_unique<l0::AdbCaptureBackend>();
    }
    return nullptr;
}

/**
 * @brief 自动选择后端
 */
std::unique_ptr<l0::ICaptureBackend> AutoSelectBackend(const std::string& target_id)
{
    // 首先尝试 ADB
    auto adb_result = l0::AdbCaptureBackend::IsDeviceAvailable(target_id);
    if (adb_result && *adb_result) {
        fmt::print(fg(fmt::color::green), "Auto-selected backend: ADB\n");
        return std::make_unique<l0::AdbCaptureBackend>();
    }

#ifdef AAM_PLATFORM_WINDOWS
    // 尝试 Win32
    auto win_result = l0::Win32CaptureBackend::FindWindowByTitle(target_id);
    if (win_result) {
        fmt::print(fg(fmt::color::green), "Auto-selected backend: Win32\n");
        return std::make_unique<l0::Win32CaptureBackend>();
    }
#endif

#ifdef AAM_MAAFW_ENABLED
    // 尝试 MAA（检查是否是 IP:端口格式）
    if (target_id.find(':') != std::string::npos) {
        fmt::print(fg(fmt::color::green), "Auto-selected backend: MAA\n");
        return std::make_unique<l0::MaaCaptureBackend>();
    }
#endif

    return nullptr;
}
}  // namespace

int ExecuteCaptureTest(const CaptureTestConfig& config)
{
    fmt::print(fg(fmt::color::cyan), "=== AAM Capture Test ===\n\n");

    // 创建后端
    std::unique_ptr<l0::ICaptureBackend> backend;

    if (config.backend == BackendType::Auto) {
        backend = AutoSelectBackend(config.target_id);
        if (!backend) {
            fmt::print(fg(fmt::color::red),
                       "Error: Could not auto-select backend for target: {}\n",
                       config.target_id);
            return 1;
        }
    }
    else {
        backend = CreateBackend(config.backend);
        if (!backend) {
            fmt::print(fg(fmt::color::red), "Error: Failed to create backend\n");
            return 1;
        }
    }

    fmt::print("Backend: {} v{}\n", backend->GetBackendName(), backend->GetBackendVersion());
    fmt::print("Target: {}\n", config.target_id);
    fmt::print("Format: {}\n", PixelFormatToString(config.format));
    fmt::print("Target FPS: {}\n", config.fps);
    fmt::print("Duration: {} ms\n", config.duration_ms);
    fmt::print("\n");

    // 配置捕获参数
    l0::CaptureConfig capture_config;
    capture_config.target_id         = config.target_id;
    capture_config.preferred_format  = ToL0PixelFormat(config.format);
    capture_config.target_fps        = config.fps;
    capture_config.buffer_queue_size = 60;
    capture_config.max_frame_size    = 3840 * 2160 * 4;  // 4K RGBA

    // 初始化后端
    fmt::print("Initializing backend...\n");
    auto init_result = backend->Initialize(capture_config);
    if (!init_result) {
        fmt::print(fg(fmt::color::red),
                   "Error: Failed to initialize backend (error: {})\n",
                   static_cast<int>(init_result.error()));
        return 1;
    }

    // 创建输出目录
    std::unique_ptr<FrameSaver> frame_saver;
    if (config.save_frames && config.output_dir) {
        frame_saver = std::make_unique<FrameSaver>(*config.output_dir);
        if (!frame_saver->IsValid()) {
            fmt::print(fg(fmt::color::yellow), "Warning: Failed to create output directory\n");
            frame_saver.reset();
        }
    }

    // 创建预览窗口
    cv::Mat preview_frame;
    if (config.show_preview) {
        cv::namedWindow("AAM Capture Preview", cv::WINDOW_NORMAL);
    }

    // 统计信息
    CaptureStatistics stats;

    // 开始捕获
    fmt::print("Starting capture...\n");
    auto start_result = backend->StartCapture();
    if (!start_result) {
        fmt::print(fg(fmt::color::red),
                   "Error: Failed to start capture (error: {})\n",
                   static_cast<int>(start_result.error()));
        [[maybe_unused]] auto _ = backend->Shutdown();
        return 1;
    }

    fmt::print(fg(fmt::color::green), "Capture started!\n\n");

    // 捕获循环
    auto     start_time  = std::chrono::steady_clock::now();
    uint64_t frame_count = 0;
    uint64_t saved_count = 0;

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start_time)
                           .count();

        if (elapsed >= static_cast<int64_t>(config.duration_ms)) {
            break;
        }

        // 获取帧
        auto frame_result = backend->GetFrame(std::chrono::seconds(1));
        if (!frame_result) {
            if (frame_result.error() == l0::CaptureError::Timeout) {
                fmt::print(fg(fmt::color::yellow), "Timeout waiting for frame\n");
                continue;
            }
            fmt::print(fg(fmt::color::red),
                       "Error getting frame: {}\n",
                       static_cast<int>(frame_result.error()));
            break;
        }

        auto& [metadata, pixel_data] = *frame_result;
        frame_count++;

        // 更新统计
        stats.AddFrame(metadata, pixel_data.size());

        // 保存帧
        if (frame_saver && frame_count % config.save_interval == 0) {
            if (frame_saver->SaveFrame(metadata, pixel_data, PixelFormatToString(config.format))) {
                saved_count++;
            }
        }

        // 显示预览
        if (config.show_preview) {
            // 将像素数据转换为 OpenCV Mat
            int channels = (config.format == PixelFormatType::RGB) ? 3 : 4;
            int cv_type  = (channels == 3) ? CV_8UC3 : CV_8UC4;

            cv::Mat frame(metadata.height,
                          metadata.width,
                          cv_type,
                          const_cast<std::byte*>(pixel_data.data()));

            // 转换颜色格式以便显示
            if (config.format == PixelFormatType::RGBA) {
                cv::cvtColor(frame, preview_frame, cv::COLOR_RGBA2BGR);
            }
            else if (config.format == PixelFormatType::BGRA) {
                cv::cvtColor(frame, preview_frame, cv::COLOR_BGRA2BGR);
            }
            else if (config.format == PixelFormatType::RGB) {
                cv::cvtColor(frame, preview_frame, cv::COLOR_RGB2BGR);
            }
            else {
                preview_frame = frame.clone();
            }

            // 添加信息叠加
            std::string info =
                fmt::format("Frame: {} | FPS: {:.1f}", frame_count, stats.GetCurrentFps());
            cv::putText(preview_frame,
                        info,
                        cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.7,
                        cv::Scalar(0, 255, 0),
                        2);

            cv::imshow("AAM Capture Preview", preview_frame);

            // 检查按键（ESC 退出）
            if (cv::waitKey(1) == 27) {
                fmt::print("User interrupted\n");
                break;
            }
        }

        // 打印进度
        if (config.verbose && frame_count % 30 == 0) {
            fmt::print("Progress: {} ms / {} ms | Frames: {} | FPS: {:.1f}\n",
                       elapsed,
                       config.duration_ms,
                       frame_count,
                       stats.GetCurrentFps());
        }
    }

    // 停止捕获
    fmt::print("\nStopping capture...\n");
    [[maybe_unused]] auto _stop     = backend->StopCapture();
    [[maybe_unused]] auto _shutdown = backend->Shutdown();

    if (config.show_preview) {
        cv::destroyWindow("AAM Capture Preview");
    }

    // 打印统计信息
    auto final_stats = stats.GetStats();

    fmt::print(fg(fmt::color::cyan), "\n=== Capture Statistics ===\n");
    fmt::print("Total frames: {}\n", final_stats.total_frames);
    fmt::print("Total duration: {:.2f} s\n", final_stats.duration_sec);
    fmt::print("Average FPS: {:.2f}\n", final_stats.avg_fps);
    fmt::print("Min FPS: {:.2f}\n", final_stats.min_fps);
    fmt::print("Max FPS: {:.2f}\n", final_stats.max_fps);
    fmt::print("Frames saved: {}\n", saved_count);
    fmt::print("Total data: {:.2f} MB\n", final_stats.total_mb);

    fmt::print(fg(fmt::color::green), "\nCapture test completed!\n");

    return 0;
}

}  // namespace aam::tools
