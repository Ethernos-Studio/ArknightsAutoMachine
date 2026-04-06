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
// @file win32_capture.hpp
// @brief Win32 窗口捕获后端实现
// @version 0.2.0-alpha.2
// ==========================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "aam/core/logger.hpp"
#include "aam/core/memory_pool.hpp"
#include "aam/l0/capture_backend.hpp"

// Windows API 前向声明，避免在头文件中包含 windows.h
#ifndef _WINDOWS_
struct HWND__;
typedef HWND__* HWND;
struct HBITMAP__;
typedef HBITMAP__* HBITMAP;
struct HDC__;
typedef HDC__* HDC;
using DWORD = unsigned long;
using UINT  = unsigned int;
using LONG  = long;
using BOOL  = int;
using BYTE  = unsigned char;
using RECT  = struct _RECT;
#else
#    include <windows.h>
#endif

namespace aam::l0
{

// 前向声明
class WindowsGraphicsCapture;
class BitBltCapture;

/**
 * @brief Win32 窗口捕获后端实现
 *
 * 使用 Windows GDI API 捕获指定窗口的屏幕内容。
 * 作为 ADB 和 MAA 捕获失败时的后备方案。
 *
 * @note 仅支持 Windows 平台
 * @note 捕获性能受窗口可见性和遮挡影响
 * @note 支持后台窗口捕获（使用 PrintWindow API）
 */
class Win32CaptureBackend final : public ICaptureBackend
{
public:
    // ==================================================================
    // 构造函数与析构函数
    // ==================================================================

    /**
     * @brief 构造函数
     */
    Win32CaptureBackend();

    /**
     * @brief 析构函数
     *
     * 确保资源正确释放，即使发生异常
     */
    ~Win32CaptureBackend() override;

    // 禁止拷贝和移动
    Win32CaptureBackend(const Win32CaptureBackend&)            = delete;
    Win32CaptureBackend& operator=(const Win32CaptureBackend&) = delete;
    Win32CaptureBackend(Win32CaptureBackend&&)                 = delete;
    Win32CaptureBackend& operator=(Win32CaptureBackend&&)      = delete;

    // ==================================================================
    // ICaptureBackend 接口实现
    // ==================================================================

    /**
     * @brief 初始化捕获后端
     * @param config 捕获配置参数
     * @return Result 操作结果，失败时包含错误信息
     *
     * @note 配置中的 target_id 可以是窗口标题或窗口句柄
     * @note 如果找不到窗口，会尝试使用进程名查找
     */
    [[nodiscard]] Result Initialize(const CaptureConfig& config) override;

    /**
     * @brief 关闭捕获后端并释放资源
     * @return Result 操作结果
     */
    [[nodiscard]] Result Shutdown() override;

    /**
     * @brief 检查后端是否已初始化
     * @return bool 初始化状态
     */
    [[nodiscard]] bool IsInitialized() const noexcept override;

    /**
     * @brief 开始捕获
     * @return Result 操作结果
     *
     * @note 启动后台捕获线程
     */
    [[nodiscard]] Result StartCapture() override;

    /**
     * @brief 停止捕获
     * @return Result 操作结果
     */
    [[nodiscard]] Result StopCapture() override;

    /**
     * @brief 检查是否正在捕获
     * @return bool 捕获状态
     */
    [[nodiscard]] bool IsCapturing() const noexcept override;

    /**
     * @brief 获取一帧图像（阻塞式）
     * @param timeout 超时时间
     * @return std::expected<std::pair<FrameMetadata, std::vector<std::byte>>, CaptureError>
     *         成功时返回帧元数据和像素数据，失败时返回错误码
     */
    [[nodiscard]] std::expected<std::pair<FrameMetadata, std::vector<std::byte>>, CaptureError>
    GetFrame(core::Duration timeout) override;

    /**
     * @brief 尝试获取一帧图像（非阻塞式）
     * @return std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>,
     * CaptureError> 成功时返回帧数据（如果有），失败时返回错误码
     */
    [[nodiscard]] std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>,
                                CaptureError>
    TryGetFrame() override;

    /**
     * @brief 使用回调函数获取帧
     * @param timeout 超时时间
     * @param callback 帧回调函数
     * @return Result 操作结果
     */
    [[nodiscard]] Result GetFrameWithCallback(core::Duration timeout,
                                              FrameCallback  callback) override;

    /**
     * @brief 获取当前配置
     * @return CaptureConfig 配置副本
     */
    [[nodiscard]] CaptureConfig GetConfig() const override;

    /**
     * @brief 更新配置
     * @param config 新配置
     * @return Result 操作结果
     *
     * @note 部分配置更改可能需要重启捕获才能生效
     */
    [[nodiscard]] Result UpdateConfig(const CaptureConfig& config) override;

    /**
     * @brief 获取捕获统计信息
     * @return CaptureStats 统计信息副本
     */
    [[nodiscard]] CaptureStats GetStats() const override;

    /**
     * @brief 获取后端名称
     * @return std::string_view 后端名称
     */
    [[nodiscard]] std::string_view GetBackendName() const noexcept override;

    /**
     * @brief 获取后端版本
     * @return std::string_view 版本字符串
     */
    [[nodiscard]] std::string_view GetBackendVersion() const noexcept override;

    /**
     * @brief 检查是否支持指定像素格式
     * @param format 像素格式
     * @return bool 是否支持
     */
    [[nodiscard]] bool SupportsPixelFormat(PixelFormat format) const noexcept override;

    /**
     * @brief 获取支持的像素格式列表
     * @return std::vector<PixelFormat> 支持的格式列表
     */
    [[nodiscard]] std::vector<PixelFormat> GetSupportedPixelFormats() const override;

    /**
     * @brief 枚举可用设备
     * @return std::expected<std::vector<std::string>, CaptureError> 设备ID列表
     */
    [[nodiscard]] static std::expected<std::vector<std::string>, CaptureError> EnumerateDevices();

    /**
     * @brief 检查设备是否可用
     * @param device_id 设备ID
     * @return std::expected<bool, CaptureError> 是否可用
     */
    [[nodiscard]] static std::expected<bool, CaptureError>
    IsDeviceAvailable(std::string_view device_id);

    // ==================================================================
    // Win32 特定类型定义
    // ==================================================================

    /**
     * @brief 窗口信息结构
     */
    struct WindowInfo
    {
        std::string id;                   ///< 窗口句柄ID（十六进制字符串）
        std::string title;                ///< 窗口标题
        std::string class_name;           ///< 窗口类名
        std::string process_name;         ///< 进程名
        int         x{0};                 ///< 窗口X坐标
        int         y{0};                 ///< 窗口Y坐标
        int         width{0};             ///< 窗口宽度
        int         height{0};            ///< 窗口高度
        int         client_width{0};      ///< 客户区宽度
        int         client_height{0};     ///< 客户区高度
        bool        is_visible{false};    ///< 是否可见
        bool        is_minimized{false};  ///< 是否最小化
        HWND        hwnd{nullptr};        ///< 窗口句柄
    };

    // ==================================================================
    // Win32 特定功能
    // ==================================================================

    /**
     * @brief 枚举所有可见窗口
     * @param title_filter 标题过滤字符串（可选）
     * @param exact_match 是否精确匹配标题
     * @return std::expected<std::vector<WindowInfo>, CaptureError> 窗口信息列表
     */
    [[nodiscard]] static std::expected<std::vector<WindowInfo>, CaptureError>
    EnumerateWindows(std::string_view title_filter = "", bool exact_match = false);

    /**
     * @brief 获取窗口信息
     * @param window_id 窗口ID（十六进制句柄字符串）
     * @return std::expected<WindowInfo, CaptureError> 窗口信息
     */
    [[nodiscard]] static std::expected<WindowInfo, CaptureError>
    GetWindowInfo(std::string_view window_id);

    /**
     * @brief 通过窗口标题查找窗口
     * @param title 窗口标题（支持部分匹配）
     * @param exact_match 是否精确匹配
     * @return std::expected<WindowInfo, CaptureError> 窗口信息
     */
    [[nodiscard]] static std::expected<WindowInfo, CaptureError>
    FindWindowByTitle(std::string_view title, bool exact_match = false);

    /**
     * @brief 获取前台窗口ID
     * @return std::expected<std::string, CaptureError> 窗口ID（十六进制句柄字符串）
     */
    [[nodiscard]] static std::expected<std::string, CaptureError> GetForegroundWindowId();

    /**
     * @brief 检查窗口是否有效
     * @param window_id 窗口ID（十六进制句柄字符串）
     * @return bool 是否有效
     */
    [[nodiscard]] static bool IsWindowValid(std::string_view window_id);

    /**
     * @brief 检查硬件捕获是否可用
     * @return bool 是否可用
     */
    [[nodiscard]] bool IsHardwareCaptureAvailable() const noexcept;

    /**
     * @brief 检查是否使用硬件捕获
     * @return bool 是否使用硬件捕获
     */
    [[nodiscard]] bool UseHardwareCapture() const noexcept;

    /**
     * @brief 设置是否使用硬件捕获
     * @param use_hardware true 表示使用硬件捕获
     */
    void SetUseHardwareCapture(bool use_hardware);

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
     * @brief 捕获线程主函数
     */
    void CaptureThreadFunc();

    /**
     * @brief 将 BGRA 数据转换为 RGB24
     * @param bgra_data BGRA 源数据
     * @return std::vector<std::uint8_t> RGB24 数据
     */
    [[nodiscard]] std::vector<std::uint8_t>
    ConvertBGRAtoRGB24(const std::vector<std::uint8_t>& bgra_data);

    /**
     * @brief 创建帧
     * @param rgb_data RGB 数据
     * @return std::optional<FrameBufferElement> 帧元素
     */
    [[nodiscard]] std::optional<FrameBufferElement>
    CreateFrame(std::vector<std::uint8_t>&& rgb_data);

    /**
     * @brief 推送帧到队列
     * @param frame 帧元素
     */
    void PushFrame(FrameBufferElement&& frame);

    /**
     * @brief 更新统计信息
     * @param latency 延迟
     */
    void UpdateStats(core::Duration latency);

    /**
     * @brief 枚举窗口内部实现
     * @param title_filter 标题过滤字符串
     * @param exact_match 是否精确匹配
     * @return std::vector<WindowInfo> 窗口信息列表
     */
    [[nodiscard]] static std::vector<WindowInfo>
    EnumerateWindowsInternal(const std::string& title_filter, bool exact_match);

    /**
     * @brief 获取窗口信息内部实现
     * @param hwnd 窗口句柄
     * @return WindowInfo 窗口信息
     */
    [[nodiscard]] static WindowInfo GetWindowInfoInternal(HWND hwnd);

    // ==================================================================
    // 成员变量
    // ==================================================================

    // 状态管理
    std::atomic<State> state_{State::Uninitialized};  ///< 当前状态
    mutable std::mutex state_mutex_;                  ///< 状态互斥锁

    // 配置
    CaptureConfig      config_;        ///< 捕获配置
    mutable std::mutex config_mutex_;  ///< 配置互斥锁

    // 窗口相关
    HWND target_window_{nullptr};  ///< 目标窗口句柄

    // 捕获实现
    std::unique_ptr<WindowsGraphicsCapture> wgc_capture_;     ///< WGC 捕获器
    std::unique_ptr<BitBltCapture>          bitblt_capture_;  ///< BitBlt 捕获器
    bool                                    use_wgc_{false};  ///< 是否使用 WGC

    // 捕获线程
    std::thread       capture_thread_;         ///< 捕获线程
    std::atomic<bool> stop_requested_{false};  ///< 停止请求标志

    // 帧队列
    std::queue<FrameBufferElement> frame_queue_;         ///< 帧队列
    mutable std::mutex             queue_mutex_;         ///< 队列互斥锁
    std::condition_variable        queue_cv_;            ///< 队列条件变量
    size_t                         max_queue_size_{10};  ///< 最大队列大小

    // 内存池
    std::unique_ptr<core::FixedMemoryPool> memory_pool_;  ///< 内存池

    // 统计信息
    CaptureStats       stats_;        ///< 统计信息
    mutable std::mutex stats_mutex_;  ///< 统计信息互斥锁

    // 日志
    core::Logger logger_;  ///< 日志记录器

    // 帧计数器
    std::atomic<std::uint64_t> frame_counter_{0};  ///< 帧计数器

    // 连续错误计数
    std::atomic<std::uint64_t> consecutive_errors_{0};  ///< 连续错误计数
};

}  // namespace aam::l0
