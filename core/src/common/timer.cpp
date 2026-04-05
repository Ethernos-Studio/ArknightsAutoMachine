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
// @file timer.cpp
// @author dhjs0000
// @brief 高精度计时器实现
// ==========================================================================

#include "aam/core/timer.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <system_error>

namespace aam::core
{

// ==========================================================================
// LatencyHistogram 实现
// ==========================================================================

LatencyHistogram::LatencyHistogram(std::size_t bucket_count, std::int64_t max_latency_ns)
    : bucket_count_(bucket_count),
      max_latency_ns_(max_latency_ns),
      log_max_latency_(std::log10(static_cast<double>(max_latency_ns))),
      buckets_(bucket_count)
{
    for (auto& bucket : buckets_) {
        bucket.store(0, std::memory_order_relaxed);
    }
}

void LatencyHistogram::record(Duration latency)
{
    record_ns(latency.count());
}

void LatencyHistogram::record_ns(std::int64_t latency_ns)
{
    // 更新统计信息
    total_count_.fetch_add(1, std::memory_order_relaxed);
    sum_latency_.fetch_add(latency_ns, std::memory_order_relaxed);

    // 更新最小/最大值
    std::int64_t current_min = min_latency_.load(std::memory_order_relaxed);
    while (latency_ns < current_min
           && !min_latency_.compare_exchange_weak(
               current_min, latency_ns, std::memory_order_relaxed, std::memory_order_relaxed)) {
        // 重试
    }

    std::int64_t current_max = max_latency_.load(std::memory_order_relaxed);
    while (latency_ns > current_max
           && !max_latency_.compare_exchange_weak(
               current_max, latency_ns, std::memory_order_relaxed, std::memory_order_relaxed)) {
        // 重试
    }

    // 计算桶索引（对数分布）
    std::size_t bucket_idx = 0;
    if (latency_ns > 0) {
        // 使用对数分布：0-1us, 1-10us, 10-100us, ...
        // 使用缓存的 log_max_latency_ 避免重复计算
        double log_val = std::log10(static_cast<double>(latency_ns));
        bucket_idx     = static_cast<std::size_t>((log_val / log_max_latency_) * bucket_count_);
        bucket_idx     = std::min(bucket_idx, bucket_count_ - 1);
    }

    buckets_[bucket_idx].fetch_add(1, std::memory_order_relaxed);
}

Duration LatencyHistogram::get_percentile(double percentile) const
{
    if (percentile < 0.0 || percentile > 1.0) {
        return Duration::zero();
    }

    const std::uint64_t total = total_count_.load(std::memory_order_relaxed);
    if (total == 0) {
        return Duration::zero();
    }

    const std::uint64_t target_count = static_cast<std::uint64_t>(percentile * total);
    std::uint64_t       cumulative   = 0;

    for (std::size_t i = 0; i < bucket_count_; ++i) {
        cumulative += buckets_[i].load(std::memory_order_relaxed);
        if (cumulative >= target_count) {
            // 计算该桶的上界
            const double       bucket_ratio = static_cast<double>(i + 1) / bucket_count_;
            const std::int64_t latency      = static_cast<std::int64_t>(
                std::pow(static_cast<double>(max_latency_ns_), bucket_ratio));
            return Duration(latency);
        }
    }

    return Duration(max_latency_ns_);
}

Duration LatencyHistogram::avg_latency() const noexcept
{
    const std::uint64_t count = total_count_.load(std::memory_order_relaxed);
    if (count == 0) {
        return Duration::zero();
    }

    const std::int64_t sum = sum_latency_.load(std::memory_order_relaxed);
    return Duration(sum / static_cast<std::int64_t>(count));
}

void LatencyHistogram::reset()
{
    total_count_.store(0, std::memory_order_relaxed);
    sum_latency_.store(0, std::memory_order_relaxed);
    min_latency_.store(std::numeric_limits<std::int64_t>::max(), std::memory_order_relaxed);
    max_latency_.store(0, std::memory_order_relaxed);

    for (auto& bucket : buckets_) {
        bucket.store(0, std::memory_order_relaxed);
    }
}

std::string LatencyHistogram::export_csv() const
{
    std::ostringstream oss;
    oss << "BucketIndex,LatencyLowerNs,LatencyUpperNs,Count\n";

    for (std::size_t i = 0; i < bucket_count_; ++i) {
        const double lower_ratio = static_cast<double>(i) / bucket_count_;
        const double upper_ratio = static_cast<double>(i + 1) / bucket_count_;

        const std::int64_t lower_ns =
            static_cast<std::int64_t>(std::pow(static_cast<double>(max_latency_ns_), lower_ratio));
        const std::int64_t upper_ns =
            static_cast<std::int64_t>(std::pow(static_cast<double>(max_latency_ns_), upper_ratio));

        const std::uint64_t count = buckets_[i].load(std::memory_order_relaxed);

        oss << i << "," << lower_ns << "," << upper_ns << "," << count << "\n";
    }

    return oss.str();
}

// ==========================================================================
// FrameRateCalculator 实现
// ==========================================================================

FrameRateCalculator::FrameRateCalculator(std::size_t window_size)
    : window_size_(window_size),
      timestamps_(window_size)
{
}

void FrameRateCalculator::record_frame(Timestamp timestamp)
{
    timestamps_[index_] = timestamp;
    index_              = (index_ + 1) % window_size_;
    if (count_ < window_size_) {
        ++count_;
    }
}

double FrameRateCalculator::get_fps() const noexcept
{
    if (count_ < 2) {
        return 0.0;
    }

    // 计算时间窗口
    std::size_t oldest_idx = (index_ + window_size_ - count_) % window_size_;
    std::size_t newest_idx = (index_ + window_size_ - 1) % window_size_;

    const auto   duration = timestamps_[newest_idx] - timestamps_[oldest_idx];
    const double seconds  = std::chrono::duration<double>(duration).count();

    if (seconds <= 0.0) {
        return 0.0;
    }

    return static_cast<double>(count_ - 1) / seconds;
}

double FrameRateCalculator::get_frame_interval_ms() const noexcept
{
    const double fps = get_fps();
    if (fps <= 0.0) {
        return 0.0;
    }
    return 1000.0 / fps;
}

void FrameRateCalculator::reset()
{
    count_ = 0;
    index_ = 0;
    std::fill(timestamps_.begin(), timestamps_.end(), Timestamp{});
}

// ==========================================================================
// 时间格式化工具实现
// ==========================================================================

std::string format_duration(Duration duration, int precision)
{
    const auto ns = duration.count();

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);

    if (ns < 1000) {
        oss << ns << " ns";
    }
    else if (ns < 1'000'000) {
        oss << (ns / 1000.0) << " us";
    }
    else if (ns < 1'000'000'000) {
        oss << (ns / 1'000'000.0) << " ms";
    }
    else {
        oss << (ns / 1'000'000'000.0) << " s";
    }

    return oss.str();
}

std::string format_duration_compact(Duration duration)
{
    const auto ns = duration.count();

    if (ns < 1000) {
        return std::to_string(ns) + "ns";
    }
    else if (ns < 1'000'000) {
        return std::to_string(ns / 1000) + "us";
    }
    else if (ns < 1'000'000'000) {
        return std::to_string(ns / 1'000'000) + "ms";
    }
    else {
        return std::to_string(ns / 1'000'000'000) + "s";
    }
}

std::optional<Duration> parse_duration(std::string_view str)
{
    // 移除空白字符
    while (!str.empty() && std::isspace(str.front())) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(str.back())) {
        str.remove_suffix(1);
    }

    if (str.empty()) {
        return std::nullopt;
    }

    // 解析数值部分
    double value   = 0.0;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);

    if (ec != std::errc()) {
        return std::nullopt;
    }

    // 解析单位
    std::string_view unit(ptr, str.size() - (ptr - str.data()));
    while (!unit.empty() && std::isspace(unit.front())) {
        unit.remove_prefix(1);
    }

    if (unit == "ns" || unit == "nanosecond" || unit == "nanoseconds") {
        return Duration(static_cast<std::int64_t>(value));
    }
    else if (unit == "us" || unit == "microsecond" || unit == "microseconds" || unit == "μs") {
        return std::chrono::duration_cast<Duration>(Microseconds(static_cast<std::int64_t>(value)));
    }
    else if (unit == "ms" || unit == "millisecond" || unit == "milliseconds") {
        return std::chrono::duration_cast<Duration>(Milliseconds(static_cast<std::int64_t>(value)));
    }
    else if (unit == "s" || unit == "second" || unit == "seconds") {
        return std::chrono::duration_cast<Duration>(Seconds(static_cast<std::int64_t>(value)));
    }
    else if (unit == "m" || unit == "minute" || unit == "minutes") {
        return std::chrono::duration_cast<Duration>(
            std::chrono::minutes(static_cast<std::int64_t>(value)));
    }
    else if (unit == "h" || unit == "hour" || unit == "hours") {
        return std::chrono::duration_cast<Duration>(
            std::chrono::hours(static_cast<std::int64_t>(value)));
    }

    // 默认单位为毫秒
    return std::chrono::duration_cast<Duration>(Milliseconds(static_cast<std::int64_t>(value)));
}

}  // namespace aam::core
