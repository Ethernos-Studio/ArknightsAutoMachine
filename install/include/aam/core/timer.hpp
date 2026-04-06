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
// @file timer.hpp
// @author dhjs0000
// @brief 高精度计时器与时间管理工具
// ==========================================================================
// 版本: v0.2.0-alpha.1
// 功能: 提供纳秒级精度的时间戳、计时器和性能分析工具
// 依赖: C++23, chrono, Windows QueryPerformanceCounter / POSIX clock_gettime
// ==========================================================================

#ifndef AAM_CORE_TIMER_HPP
#define AAM_CORE_TIMER_HPP

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <ratio>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
#    include <time.h>
#endif

namespace aam::core
{

// ==========================================================================
// 时间类型定义
// ==========================================================================

/**
 * @brief 高精度时钟类型
 * @details 使用 steady_clock 保证单调递增，避免系统时间调整影响
 */
using Clock = std::chrono::steady_clock;

/**
 * @brief 时间戳类型
 * @details 纳秒精度的绝对时间点
 */
using Timestamp = Clock::time_point;

/**
 * @brief 时长类型
 * @details 纳秒精度的相对时间间隔
 */
using Duration = Clock::duration;

/**
 * @brief 纳秒类型别名
 */
using Nanoseconds = std::chrono::nanoseconds;

/**
 * @brief 微秒类型别名
 */
using Microseconds = std::chrono::microseconds;

/**
 * @brief 毫秒类型别名
 */
using Milliseconds = std::chrono::milliseconds;

/**
 * @brief 秒类型别名
 */
using Seconds = std::chrono::seconds;

// ==========================================================================
// 平台特定高精度计时器
// ==========================================================================

#ifdef _WIN32

/**
 * @brief Windows 高精度查询计数器
 * @details 使用 QueryPerformanceCounter 获取最高精度时间戳
 */
class HighResolutionCounter
{
public:
    /**
     * @brief 获取计数器频率
     * @return 每秒计数次数
     */
    [[nodiscard]] static std::int64_t frequency() noexcept
    {
        std::int64_t freq = 0;
        QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&freq));
        return freq;
    }

    /**
     * @brief 获取当前计数器值
     * @return 当前计数
     */
    [[nodiscard]] static std::int64_t now() noexcept
    {
        std::int64_t count = 0;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&count));
        return count;
    }

    /**
     * @brief 将计数转换为纳秒
     * @param count 计数器值
     * @return 纳秒数
     */
    [[nodiscard]] static std::int64_t to_nanoseconds(std::int64_t count) noexcept
    {
        static const std::int64_t freq = frequency();
        return (count * 1'000'000'000) / freq;
    }

    /**
     * @brief 将纳秒转换为计数
     * @param ns 纳秒数
     * @return 计数器值
     */
    [[nodiscard]] static std::int64_t from_nanoseconds(std::int64_t ns) noexcept
    {
        static const std::int64_t freq = frequency();
        return (ns * freq) / 1'000'000'000;
    }
};

#else  // POSIX

/**
 * @brief POSIX 高精度时钟
 * @details 使用 clock_gettime(CLOCK_MONOTONIC) 获取纳秒级时间戳
 */
class HighResolutionCounter
{
public:
    /**
     * @brief 获取当前时间（纳秒）
     * @return 纳秒时间戳
     */
    [[nodiscard]] static std::int64_t now() noexcept
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
    }

    /**
     * @brief 将计数转换为纳秒（POSIX 下直接返回）
     * @param count 纳秒值
     * @return 纳秒数
     */
    [[nodiscard]] static std::int64_t to_nanoseconds(std::int64_t count) noexcept
    {
        return count;
    }

    /**
     * @brief 将纳秒转换为计数（POSIX 下直接返回）
     * @param ns 纳秒数
     * @return 纳秒值
     */
    [[nodiscard]] static std::int64_t from_nanoseconds(std::int64_t ns) noexcept
    {
        return ns;
    }
};

#endif  // _WIN32

// ==========================================================================
// 自旋等待优化
// ==========================================================================

/**
 * @brief CPU 自旋等待提示
 * @details 使用 PAUSE 指令（x86）或 YIELD 指令（ARM）减少功耗
 * @note 适用于短时间的忙等待循环
 */
inline void spin_wait_hint() noexcept
{
#ifdef _WIN32
    YieldProcessor();
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    __asm__ volatile("yield" ::: "memory");
#else
    // 默认：短暂睡眠
    struct timespec ts = {0, 1};
    nanosleep(&ts, nullptr);
#endif
}

/**
 * @brief 指数退避自旋等待
 * @param iteration 当前迭代次数
 * @details 随着迭代次数增加，等待时间指数增长
 */
inline void spin_wait_backoff(std::uint32_t iteration) noexcept
{
    // 指数退避：最多自旋 2^12 = 4096 次
    const std::uint32_t count = 1u << std::min(iteration, 12u);
    for (std::uint32_t i = 0; i < count; ++i) {
        spin_wait_hint();
    }
}

// ==========================================================================
// 高精度计时器类
// ==========================================================================

/**
 * @brief 高精度计时器
 * @details 提供开始、停止、暂停、继续等功能，支持多次计时
 */
class Timer
{
public:
    /**
     * @brief 默认构造函数
     */
    Timer() = default;

    /**
     * @brief 析构函数
     */
    ~Timer() = default;

    // 禁用拷贝
    Timer(const Timer&)            = delete;
    Timer& operator=(const Timer&) = delete;

    // 允许移动
    Timer(Timer&&)            = default;
    Timer& operator=(Timer&&) = default;

    /**
     * @brief 开始计时
     * @complexity O(1)
     */
    void start() noexcept
    {
        start_time_ = Clock::now();
        running_    = true;
        paused_     = false;
    }

    /**
     * @brief 停止计时
     * @return 本次计时时长
     * @complexity O(1)
     */
    [[nodiscard]] Duration stop() noexcept
    {
        if (!running_) {
            return Duration::zero();
        }

        const auto     end_time = Clock::now();
        const Duration elapsed  = end_time - start_time_;

        if (!paused_) {
            total_elapsed_ += elapsed;
        }

        running_ = false;
        paused_  = false;
        lap_count_++;

        return elapsed;
    }

    /**
     * @brief 暂停计时
     * @complexity O(1)
     */
    void pause() noexcept
    {
        if (running_ && !paused_) {
            pause_time_ = Clock::now();
            paused_     = true;
        }
    }

    /**
     * @brief 继续计时
     * @complexity O(1)
     */
    void resume() noexcept
    {
        if (running_ && paused_) {
            const auto now  = Clock::now();
            total_elapsed_ += pause_time_ - start_time_;
            start_time_     = now - (pause_time_ - start_time_);
            paused_         = false;
        }
    }

    /**
     * @brief 重置计时器
     * @complexity O(1)
     */
    void reset() noexcept
    {
        running_       = false;
        paused_        = false;
        total_elapsed_ = Duration::zero();
        lap_count_     = 0;
    }

    /**
     * @brief 获取当前经过时间（不停止计时器）
     * @return 已计时时长
     * @complexity O(1)
     */
    [[nodiscard]] Duration elapsed() const noexcept
    {
        if (!running_) {
            return total_elapsed_;
        }

        if (paused_) {
            return total_elapsed_ + (pause_time_ - start_time_);
        }

        return total_elapsed_ + (Clock::now() - start_time_);
    }

    /**
     * @brief 获取当前经过时间（毫秒）
     * @return 毫秒数
     */
    [[nodiscard]] double elapsed_ms() const noexcept
    {
        return std::chrono::duration<double, std::milli>(elapsed()).count();
    }

    /**
     * @brief 获取当前经过时间（微秒）
     * @return 微秒数
     */
    [[nodiscard]] double elapsed_us() const noexcept
    {
        return std::chrono::duration<double, std::micro>(elapsed()).count();
    }

    /**
     * @brief 获取当前经过时间（纳秒）
     * @return 纳秒数
     */
    [[nodiscard]] std::int64_t elapsed_ns() const noexcept
    {
        return elapsed().count();
    }

    /**
     * @brief 检查计时器是否正在运行
     * @return true 如果正在运行
     */
    [[nodiscard]] bool is_running() const noexcept
    {
        return running_;
    }

    /**
     * @brief 检查计时器是否已暂停
     * @return true 如果已暂停
     */
    [[nodiscard]] bool is_paused() const noexcept
    {
        return paused_;
    }

    /**
     * @brief 获取计时圈数
     * @return 计时次数
     */
    [[nodiscard]] std::uint64_t lap_count() const noexcept
    {
        return lap_count_;
    }

private:
    Timestamp     start_time_;
    Timestamp     pause_time_;
    Duration      total_elapsed_{Duration::zero()};
    std::uint64_t lap_count_{0};
    bool          running_{false};
    bool          paused_{false};
};

// ==========================================================================
// 作用域计时器（RAII）
// ==========================================================================

/**
 * @brief 作用域计时器
 * @details 在构造时开始计时，析构时自动记录时长
 * @tparam Callback 回调函数类型，接收 Duration 参数
 */
template <typename Callback = std::function<void(Duration)>>
class ScopeTimer
{
public:
    /**
     * @brief 构造函数
     * @param callback 析构时调用的回调函数
     */
    explicit ScopeTimer(Callback callback)
        : callback_(std::move(callback)),
          start_time_(Clock::now())
    {
    }

    /**
     * @brief 析构函数
     * @details 自动调用回调函数传递计时时长
     */
    ~ScopeTimer()
    {
        const auto elapsed = Clock::now() - start_time_;
        callback_(elapsed);
    }

    // 禁用拷贝和移动
    ScopeTimer(const ScopeTimer&)            = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;
    ScopeTimer(ScopeTimer&&)                 = delete;
    ScopeTimer& operator=(ScopeTimer&&)      = delete;

private:
    Callback  callback_;
    Timestamp start_time_;
};

/**
 * @brief 创建作用域计时器的辅助函数
 * @tparam Callback 回调类型
 * @param callback 回调函数
 * @return ScopeTimer 对象
 */
template <typename Callback>
[[nodiscard]] auto make_scope_timer(Callback&& callback)
{
    return ScopeTimer<std::decay_t<Callback>>(std::forward<Callback>(callback));
}

// ==========================================================================
// 延迟直方图（性能分析）
// ==========================================================================

/**
 * @brief 延迟直方图
 * @details 用于收集和分析延迟分布，支持 P50/P95/P99/P99.9 计算
 */
class LatencyHistogram
{
public:
    /**
     * @brief 构造函数
     * @param bucket_count 桶数量（默认 100）
     * @param max_latency_ns 最大延迟（纳秒，默认 1秒）
     */
    explicit LatencyHistogram(std::size_t  bucket_count   = 100,
                              std::int64_t max_latency_ns = 1'000'000'000);

    /**
     * @brief 记录延迟值
     * @param latency 延迟时长
     * @complexity O(1)
     */
    void record(Duration latency);

    /**
     * @brief 记录延迟值（纳秒）
     * @param latency_ns 延迟（纳秒）
     */
    void record_ns(std::int64_t latency_ns);

    /**
     * @brief 获取样本数量
     * @return 记录的总样本数
     */
    [[nodiscard]] std::uint64_t sample_count() const noexcept
    {
        return total_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取指定百分位数的延迟
     * @param percentile 百分位数（0.0 - 1.0）
     * @return 对应延迟值
     */
    [[nodiscard]] Duration get_percentile(double percentile) const;

    /**
     * @brief 获取 P50 延迟
     * @return 中位数延迟
     */
    [[nodiscard]] Duration p50() const
    {
        return get_percentile(0.50);
    }

    /**
     * @brief 获取 P95 延迟
     * @return 95% 延迟
     */
    [[nodiscard]] Duration p95() const
    {
        return get_percentile(0.95);
    }

    /**
     * @brief 获取 P99 延迟
     * @return 99% 延迟
     */
    [[nodiscard]] Duration p99() const
    {
        return get_percentile(0.99);
    }

    /**
     * @brief 获取 P99.9 延迟
     * @return 99.9% 延迟
     */
    [[nodiscard]] Duration p999() const
    {
        return get_percentile(0.999);
    }

    /**
     * @brief 获取最小延迟
     * @return 最小延迟
     */
    [[nodiscard]] Duration min_latency() const noexcept
    {
        return Duration(min_latency_.load(std::memory_order_relaxed));
    }

    /**
     * @brief 获取最大延迟
     * @return 最大延迟
     */
    [[nodiscard]] Duration max_latency() const noexcept
    {
        return Duration(max_latency_.load(std::memory_order_relaxed));
    }

    /**
     * @brief 获取平均延迟
     * @return 平均延迟
     */
    [[nodiscard]] Duration avg_latency() const noexcept;

    /**
     * @brief 重置直方图
     */
    void reset();

    /**
     * @brief 导出 CSV 格式数据
     * @return CSV 字符串
     */
    [[nodiscard]] std::string export_csv() const;

private:
    std::size_t  bucket_count_;
    std::int64_t max_latency_ns_;
    double       log_max_latency_;  ///< 缓存的 log10(max_latency_ns_)，避免重复计算
    std::vector<std::atomic<std::uint64_t>> buckets_;
    std::atomic<std::uint64_t>              total_count_{0};
    std::atomic<std::int64_t>               sum_latency_{0};
    std::atomic<std::int64_t>               min_latency_{std::numeric_limits<std::int64_t>::max()};
    std::atomic<std::int64_t>               max_latency_{0};
};

// ==========================================================================
// 帧率计算器
// ==========================================================================

/**
 * @brief 帧率计算器
 * @details 使用滑动窗口计算平均 FPS
 */
class FrameRateCalculator
{
public:
    /**
     * @brief 构造函数
     * @param window_size 滑动窗口大小（默认 60 帧）
     */
    explicit FrameRateCalculator(std::size_t window_size = 60);

    /**
     * @brief 记录一帧
     * @param timestamp 帧时间戳（默认使用当前时间）
     */
    void record_frame(Timestamp timestamp = Clock::now());

    /**
     * @brief 获取当前 FPS
     * @return 当前帧率
     */
    [[nodiscard]] double get_fps() const noexcept;

    /**
     * @brief 获取平均帧间隔
     * @return 平均帧间隔（毫秒）
     */
    [[nodiscard]] double get_frame_interval_ms() const noexcept;

    /**
     * @brief 重置计算器
     */
    void reset();

private:
    std::size_t            window_size_;
    std::vector<Timestamp> timestamps_;
    std::size_t            index_{0};
    std::size_t            count_{0};
};

// ==========================================================================
// 时间格式化工具
// ==========================================================================

/**
 * @brief 格式化时长为人类可读字符串
 * @param duration 时长
 * @param precision 小数精度（默认 2）
 * @return 格式化字符串，如 "1.23 ms"
 */
[[nodiscard]] std::string format_duration(Duration duration, int precision = 2);

/**
 * @brief 格式化时长为紧凑字符串
 * @param duration 时长
 * @return 格式化字符串，自动选择合适单位
 */
[[nodiscard]] std::string format_duration_compact(Duration duration);

/**
 * @brief 解析时长字符串
 * @param str 时长字符串，如 "100ms", "1.5s"
 * @return 解析后的时长，失败返回 std::nullopt
 */
[[nodiscard]] std::optional<Duration> parse_duration(std::string_view str);

}  // namespace aam::core

#endif  // AAM_CORE_TIMER_HPP
