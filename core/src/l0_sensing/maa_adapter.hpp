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
// @file maa_adapter.hpp
// @author dhjs0000
// @brief MaaFramework 桥接适配器实现
// ==========================================================================
// 版本: v0.2.0-alpha.2
// 功能: 通过 MaaFramework 实现屏幕捕获和输入控制
// 依赖: C++23, MaaFramework API, OpenCV
// 算法: MaaFramework 实例 → 截图回调 → RGB24 输出
// 性能: P99 延迟 < 15ms @ 1920x1080@60fps
// ==========================================================================

#ifndef AAM_L0_MAA_ADAPTER_HPP
#define AAM_L0_MAA_ADAPTER_HPP

#include "aam/l0/capture_backend.hpp"
#include "aam/l0/frame_buffer.hpp"
#include "aam/core/memory_pool.hpp"
#include "aam/core/logger.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

// MaaFramework C API
#include <MaaFramework/MaaAPI.h>
#include <MaaToolkit/MaaToolkitAPI.h>

// OpenCV 用于图像处理
#include <opencv2/opencv.hpp>

namespace aam::l0
{

// ==========================================================================
// MaaFramework 桥接适配器
// ==========================================================================

/**
 * @brief MaaFramework 捕获后端
 * @details 使用 MaaFramework 的截图功能实现屏幕捕获
 *          支持 Android 设备（通过 ADB）和 Windows 窗口
 * @thread_safety 线程安全，支持多线程并发访问
 */
class MaaCaptureBackend final : public ICaptureBackend
{
public:
    // ==================================================================
    // 构造与析构
    // ==================================================================

    /**
     * @brief 构造函数
     * @complexity O(1)，初始化成员变量
     */
    MaaCaptureBackend();

    /**
     * @brief 析构函数
     * @complexity O(n)，停止捕获并释放资源
     * @note 自动调用 Shutdown() 确保资源释放
     */
    ~MaaCaptureBackend() override;

    // 禁用拷贝
    MaaCaptureBackend(const MaaCaptureBackend&)            = delete;
    MaaCaptureBackend& operator=(const MaaCaptureBackend&) = delete;

    // 允许移动
    MaaCaptureBackend(MaaCaptureBackend&&)            = delete;
    MaaCaptureBackend& operator=(MaaCaptureBackend&&) = delete;

    // ==================================================================
    // 生命周期管理 (ICaptureBackend 接口实现)
    // ==================================================================

    /**
     * @brief 初始化 MaaFramework 捕获后端
     * @param config 捕获配置，包含目标设备ID、连接地址等
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)，验证配置并初始化 MaaFramework
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result Initialize(const CaptureConfig& config) override;

    /**
     * @brief 反初始化，释放所有资源
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)，停止后台线程并释放 MaaFramework 实例
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
     * @brief 启动 MaaFramework 屏幕捕获会话
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)，启动后台捕获线程
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result StartCapture() override;

    /**
     * @brief 停止 MaaFramework 屏幕捕获会话
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
    [[nodiscard]] std::expected<std::pair<FrameMetadata, std::vector<std::byte>>,
                                CaptureError>
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
     * @return "MAA"
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
     * @brief 枚举可用设备
     * @return 设备ID列表
     * @complexity O(n)，n为已连接设备数
     * @thread_safety 线程安全
     */
    [[nodiscard]] static std::expected<std::vector<std::string>, CaptureError>
    EnumerateDevices();

    /**
     * @brief 检查设备是否可用
     * @param device_id 设备ID
     * @return true 如果设备可用
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] static std::expected<bool, CaptureError>
    IsDeviceAvailable(std::string_view device_id);

    // ==================================================================
    // MaaFramework 特定接口
    // ==================================================================

    /**
     * @brief 执行 MaaFramework 任务
     * @param task_name 任务名称
     * @param task_params 任务参数（JSON 格式）
     * @return 任务 ID 或错误码
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] std::expected<MaaTaskId, CaptureError>
    ExecuteTask(std::string_view task_name, std::string_view task_params);

    /**
     * @brief 停止任务
     * @param task_id 任务 ID
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] Result StopTask(MaaTaskId task_id);

    /**
     * @brief 检查任务是否完成
     * @param task_id 任务 ID
     * @return true 如果任务已完成
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] bool IsTaskCompleted(MaaTaskId task_id) const;

private:
    // ==================================================================
    // 内部类型定义
    // ==================================================================

    /**
     * @brief 帧缓冲区元素类型
     */
    struct FrameBufferElement
    {
        FrameMetadata              metadata;
        std::vector<std::byte>     data;
        core::Timestamp            enqueue_time;

        FrameBufferElement() = default;
        FrameBufferElement(FrameMetadata m, std::vector<std::byte> d)
            : metadata(std::move(m)), data(std::move(d)), enqueue_time(core::Clock::now())
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

    /**
     * @brief MaaFramework 连接类型
     */
    enum class ConnectionType : std::uint8_t
    {
        ADB,           ///< Android Debug Bridge
        Win32,         ///< Windows Win32 API
        Unknown = 255,
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
     * @brief 初始化 MaaFramework 控制器
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)
     */
    [[nodiscard]] Result InitializeController();

    /**
     * @brief 反初始化 MaaFramework 控制器
     * @complexity O(1)
     */
    void ShutdownController();

    /**
     * @brief 执行截图
     * @return 成功返回帧数据，失败返回错误码
     * @complexity O(n)，n为图像大小
     */
    [[nodiscard]] std::expected<FrameBufferElement, CaptureError> CaptureScreenshot();

    /**
     * @brief 处理截图数据
     * @param image_data OpenCV 图像数据
     * @return 帧缓冲区元素
     * @complexity O(n)，n为图像大小
     */
    [[nodiscard]] FrameBufferElement ProcessScreenshot(const cv::Mat& image_data);

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
     * @brief 解析目标 ID 获取连接信息
     * @param target_id 目标设备ID
     * @return 连接类型和地址
     * @complexity O(1)
     */
    [[nodiscard]] std::pair<ConnectionType, std::string>
    ParseTargetId(std::string_view target_id);

    // ==================================================================
    // 成员变量
    // ==================================================================

    // 配置和状态（受 state_mutex_ 保护）
    mutable std::mutex state_mutex_;
    CaptureConfig      config_;
    std::atomic<State> state_{State::Uninitialized};

    // 连接信息
    ConnectionType connection_type_ = ConnectionType::Unknown;
    std::string    connection_address_;

    // 捕获线程
    std::thread capture_thread_;

    // 输出帧队列（生产者-消费者模式）
    mutable std::mutex              queue_mutex_;
    std::condition_variable         queue_cv_;
    std::queue<FrameBufferElement>  frame_queue_;
    std::size_t                     max_queue_size_ = 3;

    // MaaFramework 实例 (v2.0+ API)
    MaaTasker*    maa_instance_    = nullptr;
    MaaController*  maa_controller_  = nullptr;
    MaaResource*    maa_resource_    = nullptr;

    // 内存池（用于帧数据分配）
    std::unique_ptr<core::FixedMemoryPool> memory_pool_;

    // 统计信息（原子操作）
    mutable std::mutex stats_mutex_;
    CaptureStats       stats_;
    std::atomic<std::uint64_t> frame_counter_{0};

    // 日志器
    core::Logger logger_;

    // 停止信号
    std::atomic<bool> stop_requested_{false};

    // 捕获间隔（基于目标帧率）
    core::Duration capture_interval_{std::chrono::milliseconds(16)};  // ~60fps
};

}  // namespace aam::l0

#endif  // AAM_L0_MAA_ADAPTER_HPP
