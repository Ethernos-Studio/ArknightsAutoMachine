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
// @file adb_capture.hpp
// @author dhjs0000
// @brief ADB H264 屏幕捕获后端实现
// ==========================================================================
// 版本: v0.2.0-alpha.2
// 功能: 通过 ADB shell screenrecord 实现 H264 视频流捕获
// 依赖: C++23, FFmpeg (libavcodec/libavformat), Windows Process API
// 算法: 管道读取 → H264 NAL 解析 → FFmpeg 解码 → RGB24 输出
// 性能: P99 延迟 < 20ms @ 1920x1080@60fps
// ==========================================================================

#ifndef AAM_L0_ADB_CAPTURE_HPP
#define AAM_L0_ADB_CAPTURE_HPP

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "aam/core/logger.hpp"
#include "aam/core/memory_pool.hpp"
#include "aam/l0/capture_backend.hpp"
#include "aam/l0/frame_buffer.hpp"

// Windows 特定头文件
#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#endif

// OpenCV 用于 H264 解码
#include <opencv2/opencv.hpp>

namespace aam::l0
{

// ==========================================================================
// 前向声明
// ==========================================================================
class AdbProcessManager;
class FFmpegH264Decoder;

// ==========================================================================
// ADB 捕获后端实现
// ==========================================================================

/**
 * @brief ADB H264 屏幕捕获后端
 * @details 使用 ADB shell screenrecord 命令捕获 Android 设备屏幕
 *          支持 H264 编码流直接传输，通过 OpenCV 解码为 RGB24
 * @thread_safety 线程安全，支持多线程并发访问
 */
class AdbCaptureBackend final : public ICaptureBackend
{
public:
    // ==================================================================
    // 构造与析构
    // ==================================================================

    /**
     * @brief 构造函数
     * @complexity O(1)，初始化成员变量
     */
    AdbCaptureBackend();

    /**
     * @brief 析构函数
     * @complexity O(n)，停止捕获并释放资源
     * @note 自动调用 Shutdown() 确保资源释放
     */
    ~AdbCaptureBackend() override;

    // 禁用拷贝
    AdbCaptureBackend(const AdbCaptureBackend&)            = delete;
    AdbCaptureBackend& operator=(const AdbCaptureBackend&) = delete;

    // 允许移动
    AdbCaptureBackend(AdbCaptureBackend&&)            = delete;
    AdbCaptureBackend& operator=(AdbCaptureBackend&&) = delete;

    // ==================================================================
    // 生命周期管理 (ICaptureBackend 接口实现)
    // ==================================================================

    /**
     * @brief 初始化 ADB 捕获后端
     * @param config 捕获配置，包含目标设备ID、分辨率等
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)，验证配置并准备资源
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result Initialize(const CaptureConfig& config) override;

    /**
     * @brief 反初始化，释放所有资源
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)，停止后台线程并关闭管道
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result Shutdown() override;

    /**
     * @brief 检查后端是否已初始化
     * @return true 如果已初始化
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] bool IsInitialized() const noexcept override;

    // ==================================================================
    // 捕获控制 (ICaptureBackend 接口实现)
    // ==================================================================

    /**
     * @brief 启动 ADB 屏幕捕获会话
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)，启动后台捕获线程
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result StartCapture() override;

    /**
     * @brief 停止 ADB 屏幕捕获会话
     * @return 成功返回 void，失败返回错误码
     * @complexity O(n)，等待后台线程安全退出
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result StopCapture() override;

    /**
     * @brief 检查捕获是否正在运行
     * @return true 如果正在捕获
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] bool IsCapturing() const noexcept override;

    // ==================================================================
    // 帧获取 (ICaptureBackend 接口实现)
    // ==================================================================

    /**
     * @brief 获取最新帧（阻塞模式）
     * @param timeout 最大等待时间
     * @return 成功返回帧元数据和数据，失败返回错误码
     * @complexity O(1)，无锁队列操作
     * @thread_safety 线程安全
     */
    [[nodiscard]] std::expected<std::pair<FrameMetadata, std::vector<std::byte>>, CaptureError>
    GetFrame(core::Duration timeout) override;

    /**
     * @brief 尝试获取最新帧（非阻塞模式）
     * @return 成功返回帧元数据和数据，队列为空返回 std::nullopt，失败返回错误码
     * @complexity O(1)，无锁队列操作
     * @thread_safety 线程安全
     */
    [[nodiscard]] std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>,
                                CaptureError>
    TryGetFrame() override;

    /**
     * @brief 获取最新帧（回调模式）
     * @param timeout 最大等待时间
     * @param callback 帧数据回调函数
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result GetFrameWithCallback(core::Duration timeout,
                                              FrameCallback  callback) override;

    // ==================================================================
    // 配置管理 (ICaptureBackend 接口实现)
    // ==================================================================

    /**
     * @brief 获取当前配置
     * @return 当前配置副本
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] CaptureConfig GetConfig() const override;

    /**
     * @brief 动态更新配置
     * @param config 新配置
     * @return 成功返回 void，失败返回错误码
     * @note 分辨率变更需要重启捕获会话
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result UpdateConfig(const CaptureConfig& config) override;

    // ==================================================================
    // 查询接口 (ICaptureBackend 接口实现)
    // ==================================================================

    /**
     * @brief 获取捕获统计信息
     * @return 统计信息快照
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] CaptureStats GetStats() const override;

    /**
     * @brief 获取后端名称
     * @return "ADB"
     * @complexity O(1)
     */
    [[nodiscard]] std::string_view GetBackendName() const noexcept override;

    /**
     * @brief 获取后端版本
     * @return 版本字符串
     * @complexity O(1)
     */
    [[nodiscard]] std::string_view GetBackendVersion() const noexcept override;

    /**
     * @brief 检查是否支持特定像素格式
     * @param format 像素格式
     * @return true 如果支持
     * @complexity O(1)
     */
    [[nodiscard]] bool SupportsPixelFormat(PixelFormat format) const noexcept override;

    /**
     * @brief 获取支持的像素格式列表
     * @return 支持的格式列表
     * @complexity O(1)
     */
    [[nodiscard]] std::vector<PixelFormat> GetSupportedPixelFormats() const override;

    // ==================================================================
    // 静态设备管理接口
    // ==================================================================

    /**
     * @brief 枚举可用 ADB 设备
     * @return 设备ID列表（格式：IP:port 或 serial）
     * @complexity O(n)，n为已连接设备数
     * @thread_safety 线程安全
     */
    [[nodiscard]] static std::expected<std::vector<std::string>, CaptureError> EnumerateDevices();

    /**
     * @brief 检查 ADB 设备是否可用
     * @param device_id 设备ID
     * @return true 如果设备可用且已授权
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] static std::expected<bool, CaptureError>
    IsDeviceAvailable(std::string_view device_id);

    /**
     * @brief 执行 ADB 命令
     * @param device_id 目标设备ID（空字符串表示无设备特定）
     * @param command ADB 命令
     * @param timeout 命令超时时间
     * @return 命令输出或错误码
     * @complexity O(n)，n为命令执行时间
     * @thread_safety 线程安全
     */
    [[nodiscard]] static std::expected<std::string, CaptureError>
    ExecuteAdbCommand(std::string_view device_id,
                      std::string_view command,
                      core::Duration   timeout = std::chrono::seconds(30));

private:
    // ==================================================================
    // 内部类型定义
    // ==================================================================

    /**
     * @brief 帧缓冲区元素类型
     */
    struct FrameBufferElement
    {
        FrameMetadata          metadata;
        std::vector<std::byte> data;
        core::Timestamp        enqueue_time;

        FrameBufferElement() = default;

        FrameBufferElement(FrameMetadata m, std::vector<std::byte> d)
            : metadata(std::move(m)),
              data(std::move(d)),
              enqueue_time(core::Clock::now())
        {
        }
    };

    /**
     * @brief 捕获状态枚举
     */
    enum class State : std::uint8_t
    {
        Uninitialized = 0,
        Initialized   = 1,
        Starting      = 2,
        Capturing     = 3,
        Stopping      = 4,
        Error         = 5,
    };

    // ==================================================================
    // 内部方法
    // ==================================================================

    /**
     * @brief 后台捕获线程主函数
     * @complexity O(∞)，持续运行直到停止信号
     */
    void CaptureThreadFunc();

    /**
     * @brief 启动 ADB screenrecord 进程
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)
     */
    [[nodiscard]] Result StartAdbProcess();

    /**
     * @brief 停止 ADB screenrecord 进程
     * @complexity O(1)
     */
    void StopAdbProcess();

    /**
     * @brief 从管道读取 H264 数据并解码
     * @complexity O(n)，n为读取的数据量
     */
    void ReadAndDecodeLoop();

    /**
     * @brief 解码 H264 NAL 单元
     * @param nal_data NAL 单元数据
     * @param nal_size NAL 单元大小
     * @return 成功返回解码后的帧，失败返回错误码
     * @complexity O(n)，n为NAL单元大小
     */
    [[nodiscard]] std::expected<FrameBufferElement, CaptureError>
    DecodeH264Frame(const std::uint8_t* nal_data, std::size_t nal_size);

    /**
     * @brief 推送帧到输出队列
     * @param frame 帧数据
     * @complexity O(1)
     */
    void PushFrame(FrameBufferElement&& frame);

    /**
     * @brief 更新统计信息
     * @param latency 帧延迟
     * @complexity O(1)
     */
    void UpdateStats(core::Duration latency);

    /**
     * @brief 查找 ADB 可执行文件路径
     * @return ADB 路径或空字符串
     * @complexity O(1)
     */
    [[nodiscard]] static std::string FindAdbExecutable();

    // ==================================================================
    // 成员变量
    // ==================================================================

    // 配置和状态（受 state_mutex_ 保护）
    mutable std::mutex state_mutex_;
    CaptureConfig      config_;
    std::atomic<State> state_{State::Uninitialized};

    // 捕获线程
    std::thread capture_thread_;

    // 输出帧队列（生产者-消费者模式）
    mutable std::mutex             queue_mutex_;
    std::condition_variable        queue_cv_;
    std::queue<FrameBufferElement> frame_queue_;
    std::size_t                    max_queue_size_ = 3;

    // ADB 进程管理
    std::unique_ptr<AdbProcessManager> adb_process_;

    // H264 解码器
    std::unique_ptr<FFmpegH264Decoder> decoder_;

    // 内存池（用于帧数据分配）
    std::unique_ptr<core::FixedMemoryPool> memory_pool_;

    // 统计信息（原子操作）
    mutable std::mutex         stats_mutex_;
    CaptureStats               stats_;
    std::atomic<std::uint64_t> frame_counter_{0};

    // 日志器
    core::Logger logger_;

    // ADB 可执行路径
    std::string adb_path_;

    // 停止信号
    std::atomic<bool> stop_requested_{false};

    // 连续错误计数
    int consecutive_errors_ = 0;
};

}  // namespace aam::l0

#endif  // AAM_L0_ADB_CAPTURE_HPP
