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
// @file capture_backend.hpp
// @author AAM Development Team
// @brief L0 捕获后端接口定义
// ==========================================================================
// 版本: v0.2.0-alpha.1
// 功能: 定义帧捕获后端的抽象接口，支持多种捕获源
// 依赖: C++23, expected, chrono
// ==========================================================================

#ifndef AAM_L0_CAPTURE_BACKEND_HPP
#define AAM_L0_CAPTURE_BACKEND_HPP

#include <chrono>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "aam/core/timer.hpp"

namespace aam::l0 {

// ==========================================================================
// 前向声明
// ==========================================================================
struct FrameMetadata;
struct CaptureConfig;
struct CaptureStats;

// ==========================================================================
// 错误码定义
// ==========================================================================

/**
 * @brief 捕获操作错误码
 * @details 使用强类型枚举确保类型安全，支持 std::error_code 转换
 */
enum class CaptureError : std::uint32_t {
    // 通用错误 (0x0000 - 0x00FF)
    Success = 0,                    ///< 操作成功
    Unknown = 0x0001,               ///< 未知错误
    NotImplemented = 0x0002,        ///< 功能未实现
    InvalidArgument = 0x0003,       ///< 无效参数
    OutOfMemory = 0x0004,           ///< 内存不足
    Timeout = 0x0005,               ///< 操作超时

    // 设备错误 (0x0100 - 0x01FF)
    DeviceNotFound = 0x0100,        ///< 设备未找到
    DeviceDisconnected = 0x0101,    ///< 设备断开连接
    DeviceBusy = 0x0102,            ///< 设备被占用
    DeviceNotInitialized = 0x0103,  ///< 设备未初始化
    DeviceError = 0x0104,           ///< 设备通用错误

    // 流错误 (0x0200 - 0x02FF)
    StreamNotStarted = 0x0200,      ///< 流未启动
    StreamAlreadyRunning = 0x0201,  ///< 流已在运行
    StreamInterrupted = 0x0202,     ///< 流被中断
    StreamBufferOverflow = 0x0203,  ///< 缓冲区溢出
    StreamFormatError = 0x0204,     ///< 流格式错误
    StreamDecodeError = 0x0205,     ///< 解码错误

    // 配置错误 (0x0300 - 0x03FF)
    InvalidResolution = 0x0300,     ///< 无效分辨率
    InvalidFrameRate = 0x0301,      ///< 无效帧率
    InvalidPixelFormat = 0x0302,    ///< 无效像素格式
    ConfigurationError = 0x0303,    ///< 配置错误

    // 权限错误 (0x0400 - 0x04FF)
    PermissionDenied = 0x0400,      ///< 权限不足
    ADBNotAuthorized = 0x0401,      ///< ADB 未授权
    ADBCommandFailed = 0x0402,      ///< ADB 命令失败
};

/**
 * @brief 获取 CaptureError 的错误类别
 * @return 错误类别引用
 */
[[nodiscard]] const std::error_category& capture_error_category() noexcept;

/**
 * @brief 创建 std::error_code
 * @param e 错误码
 * @return error_code 对象
 */
[[nodiscard]] inline std::error_code make_error_code(CaptureError e) noexcept {
    return {static_cast<int>(e), capture_error_category()};
}

} // namespace aam::l0

// ==========================================================================
// std::error_code 特化
// ==========================================================================
template <>
struct std::is_error_code_enum<aam::l0::CaptureError> : std::true_type {};

namespace aam::l0 {

// ==========================================================================
// 像素格式枚举
// ==========================================================================

/**
 * @brief 支持的像素格式
 * @details 定义帧数据的内存布局格式
 */
enum class PixelFormat : std::uint8_t {
    Unknown = 0,                    ///< 未知格式
    RGB24,                          ///< RGB888，24位
    BGR24,                          ///< BGR888，24位
    RGBA32,                         ///< RGBA8888，32位
    BGRA32,                         ///< BGRA8888，32位
    NV12,                           ///< YUV420 NV12，半平面
    NV21,                           ///< YUV420 NV21，半平面
    I420,                           ///< YUV420 I420，三平面
    YUY2,                           ///< YUV422 YUY2
    H264,                           ///< H.264 编码流
    H265,                           ///< H.265/HEVC 编码流
};

/**
 * @brief 获取像素格式的每像素字节数
 * @param format 像素格式
 * @return 每像素字节数，编码格式返回0
 */
[[nodiscard]] constexpr std::size_t get_pixel_format_bpp(PixelFormat format) noexcept {
    switch (format) {
        case PixelFormat::RGB24:
        case PixelFormat::BGR24:
            return 3;
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32:
            return 4;
        case PixelFormat::NV12:
        case PixelFormat::NV21:
        case PixelFormat::I420:
            return 1;  // YUV 格式需要特殊计算
        case PixelFormat::YUY2:
            return 2;
        default:
            return 0;
    }
}

/**
 * @brief 检查像素格式是否为压缩格式
 * @param format 像素格式
 * @return true 如果是压缩格式(H264/H265)
 */
[[nodiscard]] constexpr bool is_compressed_format(PixelFormat format) noexcept {
    return format == PixelFormat::H264 || format == PixelFormat::H265;
}

// ==========================================================================
// 帧元数据结构
// ==========================================================================

/**
 * @brief 帧元数据
 * @details 描述一帧图像的属性和时间戳信息
 */
struct FrameMetadata {
    // 图像尺寸
    std::uint32_t width{0};                     ///< 图像宽度（像素）
    std::uint32_t height{0};                    ///< 图像高度（像素）
    std::uint32_t stride{0};                    ///< 行步长（字节）

    // 格式信息
    PixelFormat pixel_format{PixelFormat::Unknown};  ///< 像素格式

    // 时间戳（高精度）
    core::Timestamp capture_timestamp;          ///< 捕获时间戳（硬件/驱动层）
    core::Timestamp process_timestamp;          ///< 处理时间戳（接收时刻）

    // 帧序列信息
    std::uint64_t frame_number{0};              ///< 帧序号（单调递增）
    std::uint64_t sequence_id{0};               ///< 序列ID（用于检测丢帧）

    // 数据大小
    std::size_t data_size{0};                   ///< 实际数据大小（字节）

    /**
     * @brief 计算帧数据理论大小
     * @return 理论数据大小（字节）
     */
    [[nodiscard]] constexpr std::size_t calculate_buffer_size() const noexcept {
        if (is_compressed_format(pixel_format)) {
            return data_size;  // 压缩格式使用实际大小
        }
        const std::size_t bpp = get_pixel_format_bpp(pixel_format);
        if (pixel_format == PixelFormat::NV12 || pixel_format == PixelFormat::NV21) {
            // NV12/NV21: Y平面 + UV平面
            return (width * height) + (width * height / 2);
        }
        if (pixel_format == PixelFormat::I420) {
            // I420: Y平面 + U平面 + V平面
            return (width * height * 3) / 2;
        }
        return width * height * bpp;
    }

    /**
     * @brief 验证元数据有效性
     * @return true 如果元数据有效
     */
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return width > 0 && height > 0 &&
               pixel_format != PixelFormat::Unknown &&
               data_size > 0;
    }

    /**
     * @brief 计算捕获延迟
     * @return 从捕获到处理的延迟
     */
    [[nodiscard]] core::Duration get_latency() const noexcept {
        return process_timestamp - capture_timestamp;
    }
};

// ==========================================================================
// 捕获配置结构
// ==========================================================================

/**
 * @brief 捕获配置参数
 * @details 定义捕获会话的配置选项
 */
struct CaptureConfig {
    // 目标设备/窗口标识
    std::string target_id;                      ///< 目标标识（设备序列号/窗口句柄等）

    // 分辨率配置
    std::uint32_t target_width{1920};           ///< 目标宽度
    std::uint32_t target_height{1080};          ///< 目标高度

    // 帧率配置
    std::uint32_t target_fps{60};               ///< 目标帧率

    // 格式配置
    PixelFormat preferred_format{PixelFormat::RGB24};  ///< 首选像素格式
    bool allow_format_conversion{true};         ///< 允许格式转换

    // 缓冲区配置
    std::size_t buffer_queue_size{3};           ///< 帧缓冲队列大小
    std::size_t max_frame_size{1920 * 1080 * 4}; ///< 最大帧大小（用于内存分配）

    // 超时配置
    core::Duration capture_timeout{std::chrono::milliseconds(5000)};  ///< 捕获超时
    core::Duration frame_wait_timeout{std::chrono::milliseconds(100)}; ///< 等待帧超时

    // 性能配置
    bool enable_hardware_acceleration{true};    ///< 启用硬件加速
    bool use_zero_copy{true};                   ///< 使用零拷贝传输
    std::size_t memory_pool_size{64 * 1024 * 1024}; ///< 内存池大小（64MB）

    // 回调配置
    using FrameCallback = std::function<void(const FrameMetadata&, std::span<const std::byte>)>;
    FrameCallback on_frame_received;            ///< 帧接收回调（可选）

    /**
     * @brief 验证配置有效性
     * @return 错误码或 void
     */
    [[nodiscard]] std::expected<void, CaptureError> validate() const noexcept {
        if (target_width == 0 || target_height == 0) {
            return std::unexpected(CaptureError::InvalidResolution);
        }
        if (target_fps == 0 || target_fps > 240) {
            return std::unexpected(CaptureError::InvalidFrameRate);
        }
        if (buffer_queue_size == 0 || buffer_queue_size > 64) {
            return std::unexpected(CaptureError::InvalidArgument);
        }
        return {};
    }
};

// ==========================================================================
// 捕获统计信息
// ==========================================================================

/**
 * @brief 捕获统计信息
 * @details 实时统计捕获性能指标
 */
struct CaptureStats {
    // 帧计数
    std::uint64_t frames_captured{0};           ///< 已捕获帧数
    std::uint64_t frames_dropped{0};            ///< 丢弃帧数
    std::uint64_t frames_decoded{0};            ///< 解码帧数

    // 错误计数
    std::uint64_t errors_count{0};              ///< 错误次数
    std::uint64_t disconnections{0};            ///< 断开连接次数

    // 性能指标（纳秒）
    core::Duration min_latency{core::Duration::max()};  ///< 最小延迟
    core::Duration max_latency{core::Duration::zero()}; ///< 最大延迟
    core::Duration avg_latency{core::Duration::zero()}; ///< 平均延迟

    // 帧率计算
    double current_fps{0.0};                    ///< 当前帧率
    double target_fps{60.0};                    ///< 目标帧率

    // 带宽（字节/秒）
    std::uint64_t bytes_per_second{0};          ///< 当前带宽

    // 时间戳
    core::Timestamp session_start;              ///< 会话开始时间
    core::Timestamp last_frame_time;            ///< 最后一帧时间

    /**
     * @brief 计算丢帧率
     * @return 丢帧率（0.0 - 1.0）
     */
    [[nodiscard]] double get_drop_rate() const noexcept {
        const std::uint64_t total = frames_captured + frames_dropped;
        return total > 0 ? static_cast<double>(frames_dropped) / total : 0.0;
    }

    /**
     * @brief 计算运行时长
     * @return 运行时长
     */
    [[nodiscard]] core::Duration get_session_duration() const noexcept {
        return core::Clock::now() - session_start;
    }

    /**
     * @brief 更新延迟统计
     * @param latency 新延迟值
     */
    void update_latency(core::Duration latency) noexcept {
        min_latency = std::min(min_latency, latency);
        max_latency = std::max(max_latency, latency);
        // 指数移动平均
        const double alpha = 0.1;
        const double new_latency_ns = static_cast<double>(latency.count());
        const double old_avg_ns = static_cast<double>(avg_latency.count());
        avg_latency = core::Duration(static_cast<core::Duration::rep>(
            alpha * new_latency_ns + (1.0 - alpha) * old_avg_ns));
    }

    /**
     * @brief 重置统计信息
     */
    void reset() noexcept {
        *this = CaptureStats{};
        session_start = core::Clock::now();
    }
};

// ==========================================================================
// 捕获后端接口
// ==========================================================================

/**
 * @brief 帧捕获后端接口
 * @details 抽象基类定义所有捕获后端的通用接口
 * @note 实现类必须保证线程安全
 */
class ICaptureBackend {
public:
    // ======================================================================
    // 类型别名
    // ======================================================================
    using FrameData = std::span<const std::byte>;
    using Result = std::expected<void, CaptureError>;

    // ======================================================================
    // 虚析构
    // ======================================================================
    virtual ~ICaptureBackend() = default;

    // ======================================================================
    // 生命周期管理
    // ======================================================================

    /**
     * @brief 初始化捕获后端
     * @param config 捕获配置
     * @return 成功返回 void，失败返回错误码
     * @note 必须在 StartCapture 之前调用
     * @complexity O(1)，可能涉及设备初始化
     */
    [[nodiscard]] virtual Result Initialize(const CaptureConfig& config) = 0;

    /**
     * @brief 反初始化，释放资源
     * @return 成功返回 void，失败返回错误码
     * @note 会自动停止捕获会话
     * @complexity O(1)
     */
    [[nodiscard]] virtual Result Shutdown() = 0;

    /**
     * @brief 检查后端是否已初始化
     * @return true 如果已初始化
     */
    [[nodiscard]] virtual bool IsInitialized() const noexcept = 0;

    // ======================================================================
    // 捕获控制
    // ======================================================================

    /**
     * @brief 启动捕获会话
     * @return 成功返回 void，失败返回错误码
     * @note 会启动后台捕获线程
     * @complexity O(1)，启动后台线程
     */
    [[nodiscard]] virtual Result StartCapture() = 0;

    /**
     * @brief 停止捕获会话
     * @return 成功返回 void，失败返回错误码
     * @note 会等待后台线程安全退出
     * @complexity O(n)，n为等待帧处理完成的时间
     */
    [[nodiscard]] virtual Result StopCapture() = 0;

    /**
     * @brief 检查捕获是否正在运行
     * @return true 如果正在捕获
     */
    [[nodiscard]] virtual bool IsCapturing() const noexcept = 0;

    // ======================================================================
    // 帧获取
    // ======================================================================

    /**
     * @brief 获取最新帧（阻塞模式）
     * @param timeout 最大等待时间
     * @return 成功返回帧元数据和数据，失败返回错误码
     * @note 如果队列为空，会阻塞直到有帧或超时
     * @complexity O(1)，无锁队列操作
     */
    [[nodiscard]] virtual std::expected<std::pair<FrameMetadata, std::vector<std::byte>>, CaptureError>
        GetFrame(core::Duration timeout) = 0;

    /**
     * @brief 尝试获取最新帧（非阻塞模式）
     * @return 成功返回帧元数据和数据，队列为空返回 std::nullopt，失败返回错误码
     * @complexity O(1)，无锁队列操作
     */
    [[nodiscard]] virtual std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>, CaptureError>
        TryGetFrame() = 0;

    /**
     * @brief 获取最新帧（指针模式，零拷贝）
     * @param timeout 最大等待时间
     * @return 成功返回帧元数据和数据指针，失败返回错误码
     * @note 数据指针仅在回调执行期间有效
     * @complexity O(1)
     */
    using FrameCallback = std::function<void(const FrameMetadata&, FrameData)>;
    [[nodiscard]] virtual Result GetFrameWithCallback(core::Duration timeout, FrameCallback callback) = 0;

    // ======================================================================
    // 配置管理
    // ======================================================================

    /**
     * @brief 获取当前配置
     * @return 当前配置副本
     */
    [[nodiscard]] virtual CaptureConfig GetConfig() const = 0;

    /**
     * @brief 动态更新配置（部分参数支持热更新）
     * @param config 新配置
     * @return 成功返回 void，失败返回错误码
     * @note 某些参数（如分辨率）可能需要重启捕获
     * @complexity O(1)
     */
    [[nodiscard]] virtual Result UpdateConfig(const CaptureConfig& config) = 0;

    // ======================================================================
    // 查询接口
    // ======================================================================

    /**
     * @brief 获取捕获统计信息
     * @return 统计信息快照
     */
    [[nodiscard]] virtual CaptureStats GetStats() const = 0;

    /**
     * @brief 获取后端名称
     * @return 后端标识名称
     */
    [[nodiscard]] virtual std::string_view GetBackendName() const noexcept = 0;

    /**
     * @brief 获取后端版本
     * @return 版本字符串
     */
    [[nodiscard]] virtual std::string_view GetBackendVersion() const noexcept = 0;

    /**
     * @brief 检查是否支持特定像素格式
     * @param format 像素格式
     * @return true 如果支持
     */
    [[nodiscard]] virtual bool SupportsPixelFormat(PixelFormat format) const noexcept = 0;

    /**
     * @brief 获取支持的像素格式列表
     * @return 支持的格式列表
     */
    [[nodiscard]] virtual std::vector<PixelFormat> GetSupportedPixelFormats() const = 0;

    // ======================================================================
    // 设备管理（静态接口）
    // ======================================================================

    /**
     * @brief 枚举可用设备
     * @return 设备ID列表
     * @note 静态方法，可在未初始化时调用
     */
    [[nodiscard]] static std::expected<std::vector<std::string>, CaptureError>
        EnumerateDevices();

    /**
     * @brief 检查设备是否可用
     * @param device_id 设备ID
     * @return true 如果设备可用
     */
    [[nodiscard]] static std::expected<bool, CaptureError>
        IsDeviceAvailable(std::string_view device_id);
};

// ==========================================================================
// 捕获后端工厂
// ==========================================================================

/**
 * @brief 捕获后端类型枚举
 */
enum class BackendType : std::uint8_t {
    Unknown = 0,
    ADB,                            ///< Android Debug Bridge 捕获
    MAA,                            ///< MaaFramework 桥接
    Win32Window,                    ///< Win32 窗口捕获
    DXGI,                           ///< DXGI 桌面复制
    MediaFoundation,                ///< Windows Media Foundation
    V4L2,                           ///< Linux Video4Linux2
    AVFoundation,                   ///< macOS AVFoundation
};

/**
 * @brief 创建捕获后端实例
 * @param type 后端类型
 * @return 后端实例的智能指针
 * @throws std::bad_alloc 如果内存分配失败
 */
[[nodiscard]] std::unique_ptr<ICaptureBackend> CreateCaptureBackend(BackendType type);

/**
 * @brief 获取后端类型名称
 * @param type 后端类型
 * @return 类型名称字符串
 */
[[nodiscard]] std::string_view GetBackendTypeName(BackendType type) noexcept;

} // namespace aam::l0

#endif // AAM_L0_CAPTURE_BACKEND_HPP
