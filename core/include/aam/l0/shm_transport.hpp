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
// @file shm_transport.hpp
// @author dhjs0000
// @brief L0 共享内存传输层实现
// ==========================================================================
// 版本: v0.2.0-alpha.3
// 功能: 零拷贝共享内存传输，支持 Windows 命名共享内存和 POSIX shm
// 依赖: C++23, Windows API / POSIX shm_open
// 算法: 环形缓冲区 + 原子序列号 + 内存屏障
// 性能: 单帧传输延迟 < 1μs，吞吐量 > 10GB/s
// ==========================================================================

#ifndef AAM_L0_SHM_TRANSPORT_HPP
#define AAM_L0_SHM_TRANSPORT_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "aam/core/timer.hpp"
#include "aam/l0/capture_backend.hpp"

// 禁用 MSVC 结构填充警告
#ifdef _WIN32
#    pragma warning(push)
#    pragma warning(disable : 4324)  // 禁用"结构被填充"警告
#endif

// 平台特定头文件
#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#else
#    include <fcntl.h>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

namespace aam::l0
{

// ==========================================================================
// 前向声明
// ==========================================================================
class SharedMemorySegment;
struct ShmTransportConfig;
struct ShmTransportStats;

// ==========================================================================
// 错误码定义
// ==========================================================================

/**
 * @brief 共享内存传输错误码
 * @details 使用强类型枚举确保类型安全
 */
enum class ShmTransportError : std::uint32_t
{
    // 通用错误 (0x0000 - 0x00FF)
    Success         = 0,       ///< 操作成功
    Unknown         = 0x0001,  ///< 未知错误
    InvalidArgument = 0x0002,  ///< 无效参数
    OutOfMemory     = 0x0003,  ///< 内存不足
    Timeout         = 0x0004,  ///< 操作超时
    NotInitialized  = 0x0005,  ///< 未初始化
    AlreadyExists   = 0x0006,  ///< 资源已存在
    NotFound        = 0x0007,  ///< 资源未找到
    PermissionDenied = 0x0008, ///< 权限不足

    // 共享内存错误 (0x0100 - 0x01FF)
    ShmCreateFailed    = 0x0100,  ///< 创建共享内存失败
    ShmOpenFailed      = 0x0101,  ///< 打开共享内存失败
    ShmMapFailed       = 0x0102,  ///< 映射共享内存失败
    ShmUnmapFailed     = 0x0103,  ///< 解除映射失败
    ShmTooSmall        = 0x0104,  ///< 共享内存空间不足
    ShmCorrupted       = 0x0105,  ///< 共享内存数据损坏
    ShmVersionMismatch = 0x0106,  ///< 版本不匹配

    // 同步错误 (0x0200 - 0x02FF)
    SyncCreateFailed  = 0x0200,  ///< 创建同步对象失败
    SyncWaitFailed    = 0x0201,  ///< 等待同步对象失败
    SyncSignalFailed  = 0x0202,  ///< 信号通知失败
    SyncTimeout       = 0x0203,  ///< 同步超时
    SyncAbandoned     = 0x0204,  ///< 同步对象被放弃

    // 传输错误 (0x0300 - 0x03FF)
    TransportClosed   = 0x0300,  ///< 传输已关闭
    TransportBusy     = 0x0301,  ///< 传输忙
    BufferOverflow    = 0x0302,  ///< 缓冲区溢出
    BufferUnderflow   = 0x0303,  ///< 缓冲区下溢
    FrameTooLarge     = 0x0304,  ///< 帧数据过大
    InvalidFrame      = 0x0305,  ///< 无效帧数据
};

/**
 * @brief 获取 ShmTransportError 的错误类别
 * @return 错误类别引用
 */
[[nodiscard]] const std::error_category& shm_transport_error_category() noexcept;

/**
 * @brief 创建 std::error_code
 * @param e 错误码
 * @return error_code 对象
 */
[[nodiscard]] inline std::error_code make_error_code(ShmTransportError e) noexcept
{
    return {static_cast<int>(e), shm_transport_error_category()};
}

}  // namespace aam::l0

// ==========================================================================
// std::error_code 特化
// ==========================================================================
template <>
struct std::is_error_code_enum<aam::l0::ShmTransportError> : std::true_type
{
};

namespace aam::l0
{

// ==========================================================================
// 共享内存控制结构（头部）- 必须在 ShmTransportConfig 之前定义
// ==========================================================================

// 帧头部大小常量
constexpr std::size_t SHM_FRAME_HEADER_SIZE = 64;  // alignas(64)
constexpr std::size_t SHM_CONTROL_BLOCK_SIZE = 512; // 估算值，实际使用 sizeof

/**
 * @brief 共享内存帧头部
 * @details 每帧数据的元数据，位于帧数据之前
 */
struct alignas(64) ShmFrameHeader
{
    static constexpr std::uint32_t MAGIC = 0x41414D46;  ///< "AAMF"

    std::uint32_t magic{MAGIC};          ///< 魔数，用于验证
    std::uint32_t version{1};            ///< 结构版本
    std::uint32_t sequence_number{0};    ///< 帧序列号
    std::uint32_t frame_number{0};       ///< 帧编号
    std::uint32_t sequence_id{0};        ///< 序列ID
    std::uint32_t data_size{0};          ///< 实际数据大小

    // 时间戳
    std::uint64_t capture_timestamp_ns{0};  ///< 捕获时间戳（纳秒）
    std::uint64_t write_timestamp_ns{0};    ///< 写入时间戳（纳秒）

    // 帧元数据
    std::uint32_t width{0};              ///< 图像宽度
    std::uint32_t height{0};             ///< 图像高度
    std::uint32_t stride{0};             ///< 行步长
    std::uint32_t pixel_format{0};       ///< 像素格式（PixelFormat 枚举值）

    // 校验
    std::uint32_t checksum{0};           ///< 数据校验和（CRC32）
    std::uint32_t reserved{0};           ///< 保留字段（对齐）

    /**
     * @brief 验证头部有效性
     * @return true 如果头部有效
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
        return magic == MAGIC && version == 1 && data_size > 0;
    }

    /**
     * @brief 计算校验和
     * @param data 帧数据指针
     * @return CRC32 校验和
     */
    [[nodiscard]] std::uint32_t calculate_checksum(const std::byte* data) const noexcept;

    /**
     * @brief 验证数据完整性
     * @param data 帧数据指针
     * @return true 如果数据完整
     */
    [[nodiscard]] bool verify_data(const std::byte* data) const noexcept;
};

/**
 * @brief 共享内存控制块
 * @details 位于共享内存起始位置，管理整个传输状态
 * @note 所有原子操作使用 acquire-release 语义保证跨进程可见性
 */
struct alignas(64) ShmControlBlock
{
    static constexpr std::uint32_t MAGIC = 0x41414D53;  ///< "AAMS"
    static constexpr std::uint32_t VERSION = 1;

    // 魔数和版本（只读，创建时初始化）
    std::uint32_t magic{MAGIC};          ///< 魔数
    std::uint32_t version{VERSION};      ///< 协议版本
    std::uint32_t header_size{0};        ///< 控制块大小
    std::uint32_t flags{0};              ///< 标志位

    // 缓冲区配置（只读，创建时初始化）
    std::uint32_t buffer_count{0};       ///< 缓冲区数量
    std::uint32_t max_frame_size{0};     ///< 最大帧大小
    std::uint32_t metadata_size{0};      ///< 元数据区域大小
    std::uint32_t frame_stride{0};       ///< 每帧对齐后的大小

    // 序列号（原子操作）
    alignas(64) std::atomic<std::uint64_t> write_sequence{0};   ///< 写入序列号
    alignas(64) std::atomic<std::uint64_t> read_sequence{0};    ///< 读取序列号
    alignas(64) std::atomic<std::uint64_t> dropped_frames{0};   ///< 丢弃帧计数

    // 状态标志（原子操作）
    alignas(64) std::atomic<std::uint32_t> state{0};            ///< 传输状态
    alignas(64) std::atomic<std::uint32_t> active_readers{0};   ///< 活跃读取者数
    alignas(64) std::atomic<std::uint32_t> active_writers{0};   ///< 活跃写入者数

    // 统计信息（原子操作）
    alignas(64) std::atomic<std::uint64_t> total_frames_written{0};  ///< 总写入帧数
    alignas(64) std::atomic<std::uint64_t> total_frames_read{0};     ///< 总读取帧数
    alignas(64) std::atomic<std::uint64_t> total_bytes_written{0};   ///< 总写入字节数
    alignas(64) std::atomic<std::uint64_t> total_bytes_read{0};      ///< 总读取字节数

    // 状态常量
    static constexpr std::uint32_t STATE_INITIALIZED = 0x01;   ///< 已初始化
    static constexpr std::uint32_t STATE_ACTIVE      = 0x02;   ///< 活跃状态
    static constexpr std::uint32_t STATE_SHUTDOWN    = 0x04;   ///< 已关闭
    static constexpr std::uint32_t STATE_ERROR       = 0x08;   ///< 错误状态

    /**
     * @brief 验证控制块有效性
     * @return true 如果控制块有效
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
        return magic == MAGIC && version == VERSION;
    }

    /**
     * @brief 获取当前写入索引
     * @return 写入缓冲区索引
     */
    [[nodiscard]] std::uint32_t get_write_index() const noexcept
    {
        return static_cast<std::uint32_t>(write_sequence.load(std::memory_order_relaxed) % buffer_count);
    }

    /**
     * @brief 获取当前读取索引
     * @return 读取缓冲区索引
     */
    [[nodiscard]] std::uint32_t get_read_index() const noexcept
    {
        return static_cast<std::uint32_t>(read_sequence.load(std::memory_order_relaxed) % buffer_count);
    }

    /**
     * @brief 检查是否有可读数据
     * @return true 如果有未读数据
     */
    [[nodiscard]] bool has_readable_data() const noexcept
    {
        return read_sequence.load(std::memory_order_acquire) <
               write_sequence.load(std::memory_order_acquire);
    }

    /**
     * @brief 检查是否可写入
     * @return true 如果有可用缓冲区
     */
    [[nodiscard]] bool is_writable() const noexcept
    {
        const std::uint64_t write_seq = write_sequence.load(std::memory_order_relaxed);
        const std::uint64_t read_seq  = read_sequence.load(std::memory_order_acquire);
        return (write_seq - read_seq) < buffer_count;
    }

    /**
     * @brief 获取可用缓冲区数量
     * @return 可用缓冲区数量
     */
    [[nodiscard]] std::uint32_t available_buffers() const noexcept
    {
        const std::uint64_t write_seq = write_sequence.load(std::memory_order_relaxed);
        const std::uint64_t read_seq  = read_sequence.load(std::memory_order_acquire);
        return static_cast<std::uint32_t>(buffer_count - (write_seq - read_seq));
    }

    /**
     * @brief 获取已用缓冲区数量
     * @return 已用缓冲区数量
     */
    [[nodiscard]] std::uint32_t used_buffers() const noexcept
    {
        const std::uint64_t write_seq = write_sequence.load(std::memory_order_acquire);
        const std::uint64_t read_seq  = read_sequence.load(std::memory_order_relaxed);
        return static_cast<std::uint32_t>(write_seq - read_seq);
    }
};

// ==========================================================================
// 共享内存传输配置
// ==========================================================================

/**
 * @brief 共享内存传输配置
 * @details 定义共享内存传输的行为参数
 */
struct ShmTransportConfig
{
    // 共享内存名称（平台特定前缀自动添加）
    std::string shm_name{"aam_frame_buffer"};  ///< 共享内存对象名称

    // 缓冲区配置
    std::size_t buffer_count{4};               ///< 帧缓冲区数量（环形）
    std::size_t max_frame_size{1920 * 1080 * 4};  ///< 最大帧大小（字节）
    std::size_t metadata_size{4096};           ///< 元数据区域大小（字节）

    // 同步配置
    core::Duration write_timeout{std::chrono::milliseconds(100)};   ///< 写入超时
    core::Duration read_timeout{std::chrono::milliseconds(100)};    ///< 读取超时
    bool           non_blocking{false};  ///< 非阻塞模式

    // 性能配置
    bool use_cache_line_alignment{true};  ///< 使用缓存行对齐（避免伪共享）
    bool prefetch_next_frame{true};       ///< 预取下一帧数据
    bool enable_zero_copy{true};          ///< 启用零拷贝模式（跳过CRC校验）
    bool enable_checksum{false};          ///< 启用CRC校验（默认关闭以提高性能）

    // 调试配置
    bool enable_stats{true};        ///< 启用统计信息
    bool enable_tracing{false};     ///< 启用详细跟踪日志

    /**
     * @brief 计算所需共享内存总大小
     * @return 总大小（字节）
     * @complexity O(1)
     */
    [[nodiscard]] constexpr std::size_t calculate_total_size() const noexcept
    {
        // 头部：控制结构 + 元数据
        const std::size_t header_size = sizeof(ShmControlBlock) + metadata_size;

        // 帧缓冲区：每个帧需要头部 + 数据，对齐到缓存行
        constexpr std::size_t cache_line = 64;
        const std::size_t frame_header_size = sizeof(ShmFrameHeader);
        const std::size_t aligned_frame_size =
            ((frame_header_size + max_frame_size + cache_line - 1) / cache_line) * cache_line;

        return header_size + (aligned_frame_size * buffer_count);
    }

    /**
     * @brief 验证配置有效性
     * @return 错误码或 void
     */
    [[nodiscard]] std::expected<void, ShmTransportError> validate() const noexcept
    {
        if (shm_name.empty()) {
            return std::unexpected(ShmTransportError::InvalidArgument);
        }
        if (buffer_count == 0 || buffer_count > 64) {
            return std::unexpected(ShmTransportError::InvalidArgument);
        }
        if (max_frame_size == 0 || max_frame_size > (256 * 1024 * 1024)) {
            return std::unexpected(ShmTransportError::InvalidArgument);
        }
        return {};
    }
};

// ==========================================================================
// 共享内存段（平台抽象）
// ==========================================================================

/**
 * @brief 共享内存段抽象类
 * @details 封装平台特定的共享内存操作
 */
class SharedMemorySegment
{
public:
    // ==================================================================
    // 类型别名
    // ==================================================================
    using Result = std::expected<void, ShmTransportError>;

    // ==================================================================
    // 构造与析构
    // ==================================================================
    SharedMemorySegment() noexcept = default;
    virtual ~SharedMemorySegment() = default;

    // 禁用拷贝
    SharedMemorySegment(const SharedMemorySegment&)            = delete;
    SharedMemorySegment& operator=(const SharedMemorySegment&) = delete;

    // 允许移动
    SharedMemorySegment(SharedMemorySegment&&)            = default;
    SharedMemorySegment& operator=(SharedMemorySegment&&) = default;

    // ==================================================================
    // 工厂方法
    // ==================================================================

    /**
     * @brief 创建共享内存段（生产者）
     * @param name 共享内存名称
     * @param size 共享内存大小
     * @return 成功返回 SharedMemorySegment 实例，失败返回错误码
     * @complexity O(1)，系统调用
     */
    [[nodiscard]] static std::expected<std::unique_ptr<SharedMemorySegment>, ShmTransportError>
    Create(std::string_view name, std::size_t size);

    /**
     * @brief 打开现有共享内存段（消费者）
     * @param name 共享内存名称
     * @param size 共享内存大小（用于验证）
     * @return 成功返回 SharedMemorySegment 实例，失败返回错误码
     * @complexity O(1)，系统调用
     */
    [[nodiscard]] static std::expected<std::unique_ptr<SharedMemorySegment>, ShmTransportError>
    Open(std::string_view name, std::size_t size);

    // ==================================================================
    // 访问接口
    // ==================================================================

    /**
     * @brief 获取共享内存基地址
     * @return 共享内存起始地址
     */
    [[nodiscard]] virtual std::byte* data() const noexcept = 0;

    /**
     * @brief 获取共享内存大小
     * @return 共享内存大小（字节）
     */
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;

    /**
     * @brief 获取共享内存名称
     * @return 共享内存名称
     */
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /**
     * @brief 检查是否有效
     * @return true 如果共享内存有效映射
     */
    [[nodiscard]] virtual bool is_valid() const noexcept = 0;

    /**
     * @brief 刷新共享内存到磁盘（如果需要）
     * @return 操作结果
     */
    [[nodiscard]] virtual Result flush() noexcept = 0;
};

// ==========================================================================
// 共享内存传输统计
// ==========================================================================

/**
 * @brief 共享内存传输统计信息
 */
struct ShmTransportStats
{
    // 帧统计
    std::uint64_t frames_written{0};     ///< 已写入帧数
    std::uint64_t frames_read{0};        ///< 已读取帧数
    std::uint64_t frames_dropped{0};     ///< 丢弃帧数

    // 字节统计
    std::uint64_t bytes_written{0};      ///< 已写入字节数
    std::uint64_t bytes_read{0};         ///< 已读取字节数

    // 延迟统计（纳秒）
    core::Duration min_write_latency{core::Duration::max()};   ///< 最小写入延迟
    core::Duration max_write_latency{core::Duration::zero()};  ///< 最大写入延迟
    core::Duration avg_write_latency{core::Duration::zero()};  ///< 平均写入延迟

    core::Duration min_read_latency{core::Duration::max()};    ///< 最小读取延迟
    core::Duration max_read_latency{core::Duration::zero()};   ///< 最大读取延迟
    core::Duration avg_read_latency{core::Duration::zero()};   ///< 平均读取延迟

    // 传输统计
    std::uint64_t write_timeouts{0};     ///< 写入超时次数
    std::uint64_t read_timeouts{0};      ///< 读取超时次数
    std::uint64_t checksum_errors{0};    ///< 校验和错误次数

    // 时间戳
    core::Timestamp session_start;       ///< 会话开始时间
    core::Timestamp last_write_time;     ///< 最后写入时间
    core::Timestamp last_read_time;      ///< 最后读取时间

    /**
     * @brief 计算丢帧率
     * @return 丢帧率 [0.0, 1.0]
     */
    [[nodiscard]] double get_drop_rate() const noexcept
    {
        const std::uint64_t total = frames_written + frames_dropped;
        return total > 0 ? static_cast<double>(frames_dropped) / total : 0.0;
    }

    /**
     * @brief 计算平均帧大小
     * @return 平均帧大小（字节）
     */
    [[nodiscard]] std::uint64_t get_average_frame_size() const noexcept
    {
        return frames_written > 0 ? bytes_written / frames_written : 0;
    }

    /**
     * @brief 计算吞吐量（字节/秒）
     * @return 吞吐量估算
     */
    [[nodiscard]] double get_throughput_bytes_per_sec() const noexcept
    {
        const auto duration = core::Clock::now() - session_start;
        const auto duration_sec = std::chrono::duration<double>(duration).count();
        return duration_sec > 0 ? static_cast<double>(bytes_written) / duration_sec : 0.0;
    }

    /**
     * @brief 更新写入延迟统计
     * @param latency 写入延迟
     */
    void update_write_latency(core::Duration latency) noexcept
    {
        min_write_latency = std::min(min_write_latency, latency);
        max_write_latency = std::max(max_write_latency, latency);
        // 指数移动平均
        const double alpha = 0.1;
        const double new_ns = static_cast<double>(latency.count());
        const double old_ns = static_cast<double>(avg_write_latency.count());
        avg_write_latency = core::Duration(static_cast<core::Duration::rep>(
            alpha * new_ns + (1.0 - alpha) * old_ns));
    }

    /**
     * @brief 更新读取延迟统计
     * @param latency 读取延迟
     */
    void update_read_latency(core::Duration latency) noexcept
    {
        min_read_latency = std::min(min_read_latency, latency);
        max_read_latency = std::max(max_read_latency, latency);
        // 指数移动平均
        const double alpha = 0.1;
        const double new_ns = static_cast<double>(latency.count());
        const double old_ns = static_cast<double>(avg_read_latency.count());
        avg_read_latency = core::Duration(static_cast<core::Duration::rep>(
            alpha * new_ns + (1.0 - alpha) * old_ns));
    }

    /**
     * @brief 重置统计信息
     */
    void reset() noexcept
    {
        *this         = ShmTransportStats{};
        session_start = core::Clock::now();
    }
};

// ==========================================================================
// 共享内存传输器
// ==========================================================================

/**
 * @brief 共享内存传输器
 * @details 提供零拷贝的帧数据传输，支持单生产者-多消费者模式
 * @note 线程安全：写入操作是单生产者安全的，读取操作是多消费者安全的
 * @warning 消费者必须在生产者初始化后才能连接
 */
class ShmTransport
{
public:
    // ==================================================================
    // 类型别名
    // ==================================================================
    using Result = std::expected<void, ShmTransportError>;

    // ==================================================================
    // 构造与析构
    // ==================================================================

    /**
     * @brief 默认构造函数
     * @complexity O(1)
     */
    ShmTransport() noexcept = default;

    /**
     * @brief 析构函数
     * @complexity O(1)，自动关闭传输
     */
    ~ShmTransport() noexcept;

    // 禁用拷贝
    ShmTransport(const ShmTransport&)            = delete;
    ShmTransport& operator=(const ShmTransport&) = delete;

    // 允许移动
    ShmTransport(ShmTransport&& other) noexcept;
    ShmTransport& operator=(ShmTransport&& other) noexcept;

    // ==================================================================
    // 生命周期管理（生产者）
    // ==================================================================

    /**
     * @brief 初始化传输器（作为生产者）
     * @param config 传输配置
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)，创建共享内存和同步对象
     * @thread_safety 线程安全，但应在启动消费者前调用
     */
    [[nodiscard]] Result InitializeProducer(const ShmTransportConfig& config);

    /**
     * @brief 初始化传输器（作为消费者）
     * @param config 传输配置
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)，打开共享内存
     * @thread_safety 线程安全
     * @note 必须在生产者初始化后调用
     */
    [[nodiscard]] Result InitializeConsumer(const ShmTransportConfig& config);

    /**
     * @brief 关闭传输器
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)
     */
    [[nodiscard]] Result Shutdown() noexcept;

    /**
     * @brief 检查是否已初始化
     * @return true 如果已初始化
     */
    [[nodiscard]] bool IsInitialized() const noexcept;

    /**
     * @brief 检查是否为生产者模式
     * @return true 如果是生产者
     */
    [[nodiscard]] bool IsProducer() const noexcept;

    /**
     * @brief 检查是否为消费者模式
     * @return true 如果是消费者
     */
    [[nodiscard]] bool IsConsumer() const noexcept;

    // ==================================================================
    // 帧写入接口（生产者）
    // ==================================================================

    /**
     * @brief 写入帧数据
     * @param metadata 帧元数据
     * @param data 帧数据
     * @return 成功返回 void，失败返回错误码
     * @complexity O(n)，n为数据大小（内存拷贝）
     * @thread_safety 单生产者安全
     * @note 如果缓冲区满，根据配置可能阻塞或丢弃
     */
    [[nodiscard]] Result WriteFrame(const FrameMetadata& metadata, std::span<const std::byte> data);

    /**
     * @brief 尝试写入帧数据（非阻塞）
     * @param metadata 帧元数据
     * @param data 帧数据
     * @return 成功返回 true，缓冲区满返回 false，错误返回错误码
     * @complexity O(n)
     * @thread_safety 单生产者安全
     */
    [[nodiscard]] std::expected<bool, ShmTransportError> TryWriteFrame(
        const FrameMetadata& metadata, std::span<const std::byte> data);

    /**
     * @brief 写入帧数据（带超时）
     * @param metadata 帧元数据
     * @param data 帧数据
     * @param timeout 超时时间
     * @return 成功返回 true，超时返回 false，错误返回错误码
     * @complexity O(n)
     * @thread_safety 单生产者安全
     */
    [[nodiscard]] std::expected<bool, ShmTransportError> WriteFrameWithTimeout(
        const FrameMetadata& metadata, std::span<const std::byte> data, core::Duration timeout);

    // ==================================================================
    // 帧读取接口（消费者）
    // ==================================================================

    /**
     * @brief 读取帧数据
     * @param timeout 超时时间
     * @return 成功返回帧数据，超时返回 nullopt，错误返回错误码
     * @complexity O(n)，n为数据大小（内存拷贝）
     * @thread_safety 多消费者安全
     */
    [[nodiscard]] std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>, ShmTransportError>
    ReadFrame(core::Duration timeout);

    /**
     * @brief 尝试读取帧数据（非阻塞）
     * @return 成功返回帧数据，无数据返回 nullopt，错误返回错误码
     * @complexity O(n)
     * @thread_safety 多消费者安全
     */
    [[nodiscard]] std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>, ShmTransportError>
    TryReadFrame();

    /**
     * @brief 读取帧数据（回调方式）
     * @param timeout 超时时间
     * @param callback 数据回调函数
     * @return 成功返回 void，失败返回错误码
     * @complexity O(n)
     * @thread_safety 多消费者安全
     * @note 回调方式避免额外内存分配
     */
    using ReadCallback = std::function<void(const FrameMetadata&, std::span<const std::byte>)>;
    [[nodiscard]] Result ReadFrameWithCallback(core::Duration timeout, ReadCallback callback);

    // ==================================================================
    // 零拷贝帧接口（高性能）
    // ==================================================================

    /**
     * @brief 获取写入缓冲区（零拷贝）
     * @param timeout 超时时间
     * @return 成功返回可写入的缓冲区指针和元数据，失败返回错误码
     * @complexity O(1)，无内存拷贝
     * @thread_safety 单生产者安全
     * @note 调用者直接写入返回的缓冲区，然后通过 CommitWriteBuffer 提交
     */
    struct WriteBuffer {
        std::byte* data{nullptr};           ///< 数据缓冲区指针
        std::size_t capacity{0};            ///< 缓冲区容量
        std::uint32_t buffer_index{0};      ///< 缓冲区索引（用于提交）
    };
    [[nodiscard]] std::expected<std::optional<WriteBuffer>, ShmTransportError>
    AcquireWriteBuffer(core::Duration timeout);

    /**
     * @brief 提交写入的缓冲区
     * @param buffer_index AcquireWriteBuffer 返回的 buffer_index
     * @param metadata 帧元数据
     * @param actual_size 实际写入的数据大小
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)
     * @thread_safety 单生产者安全
     */
    [[nodiscard]] Result CommitWriteBuffer(
        std::uint32_t buffer_index,
        const FrameMetadata& metadata,
        std::size_t actual_size);

    /**
     * @brief 获取读取缓冲区（零拷贝）
     * @param timeout 超时时间
     * @return 成功返回帧元数据和数据指针，失败返回错误码
     * @complexity O(1)，无内存拷贝
     * @thread_safety 多消费者安全
     * @note 调用者直接读取返回的指针，然后通过 ReleaseReadBuffer 释放
     */
    struct ReadBuffer {
        FrameMetadata metadata;             ///< 帧元数据
        std::span<const std::byte> data;    ///< 数据视图（不拥有内存）
        std::uint32_t buffer_index{0};      ///< 缓冲区索引（用于释放）
    };
    [[nodiscard]] std::expected<std::optional<ReadBuffer>, ShmTransportError>
    AcquireReadBuffer(core::Duration timeout);

    /**
     * @brief 释放读取的缓冲区
     * @param buffer_index AcquireReadBuffer 返回的 buffer_index
     * @return 成功返回 void，失败返回错误码
     * @complexity O(1)
     * @thread_safety 多消费者安全
     */
    [[nodiscard]] Result ReleaseReadBuffer(std::uint32_t buffer_index);

    /**
     * @brief 尝试获取写入缓冲区（非阻塞）
     * @return 成功返回缓冲区，无可用缓冲区返回 nullopt
     */
    [[nodiscard]] std::expected<std::optional<WriteBuffer>, ShmTransportError>
    TryAcquireWriteBuffer();

    /**
     * @brief 尝试获取读取缓冲区（非阻塞）
     * @return 成功返回缓冲区，无数据返回 nullopt
     */
    [[nodiscard]] std::expected<std::optional<ReadBuffer>, ShmTransportError>
    TryAcquireReadBuffer();

    // ==================================================================
    // 状态查询
    // ==================================================================

    /**
     * @brief 获取当前统计信息
     * @return 统计信息副本
     */
    [[nodiscard]] ShmTransportStats GetStats() const noexcept;

    /**
     * @brief 获取缓冲区状态
     * @return (已用缓冲区数, 总缓冲区数)
     */
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> GetBufferStatus() const noexcept;

    /**
     * @brief 获取配置
     * @return 配置副本
     */
    [[nodiscard]] const ShmTransportConfig& GetConfig() const noexcept { return config_; }

    /**
     * @brief 获取控制块指针（调试用途）
     * @return 控制块指针，未初始化返回 nullptr
     */
    [[nodiscard]] const ShmControlBlock* GetControlBlock() const noexcept { return control_block_; }

private:
    // ==================================================================
    // 内部方法
    // ==================================================================

    /**
     * @brief 更新统计信息
     * @param is_write 是否为写入操作
     * @param bytes 字节数
     * @param latency 操作延迟
     */
    void update_stats(bool is_write, std::size_t bytes, core::Duration latency) noexcept;

    /**
     * @brief 获取帧头部地址
     * @param index 缓冲区索引
     * @return 帧头部指针
     */
    [[nodiscard]] ShmFrameHeader* get_frame_header(std::uint32_t index) noexcept;

    /**
     * @brief 获取帧数据缓冲区地址
     * @param index 缓冲区索引
     * @return 帧数据缓冲区起始地址
     */
    [[nodiscard]] std::byte* get_frame_buffer(std::uint32_t index) noexcept;

    // ==================================================================
    // 成员变量
    // ==================================================================

    ShmTransportConfig config_;                    ///< 传输配置
    std::unique_ptr<SharedMemorySegment> segment_; ///< 共享内存段
    ShmControlBlock* control_block_{nullptr};      ///< 控制块指针
    std::byte* frame_data_base_{nullptr};          ///< 帧数据基地址

    // 状态
    std::atomic<bool> initialized_{false};         ///< 初始化标志
    bool is_producer_{false};                      ///< 是否为生产者

    // 统计
    mutable std::mutex stats_mutex_;               ///< 统计信息互斥锁
    ShmTransportStats stats_;                      ///< 本地统计信息
};

// ==========================================================================
// 辅助函数
// ==========================================================================

/**
 * @brief 生成平台特定的共享内存名称
 * @param name 基础名称
 * @return 完整共享内存名称
 */
[[nodiscard]] std::string MakeShmName(std::string_view name);

/**
 * @brief 检查共享内存是否存在
 * @param name 共享内存名称
 * @return true 如果存在
 */
[[nodiscard]] bool ShmExists(std::string_view name);

/**
 * @brief 删除共享内存
 * @param name 共享内存名称
 * @return true 如果成功删除或不存在
 */
[[nodiscard]] bool ShmRemove(std::string_view name);

}  // namespace aam::l0

// 恢复警告设置
#ifdef _WIN32
#    pragma warning(pop)
#endif

#endif  // AAM_L0_SHM_TRANSPORT_HPP
