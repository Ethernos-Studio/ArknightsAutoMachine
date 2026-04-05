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
// @file frame_buffer.hpp
// @author AAM Development Team
// @brief L0 无锁帧缓冲区实现
// ==========================================================================

#pragma once

#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "aam/core/timer.hpp"

namespace aam::l0 {

// ==========================================================================
// 前向声明
// ==========================================================================

/**
 * @brief 缓冲区策略枚举
 * @details 定义当缓冲区满时的处理策略
 */
enum class BufferPolicy : std::uint8_t {
    DropOldest,     ///< 丢弃最旧的帧
    DropNewest,     ///< 丢弃最新的帧（即拒绝写入）
    Block,          ///< 阻塞等待（仅适用于阻塞式缓冲区）
    Overwrite,      ///< 直接覆盖（可能导致读取到不完整帧）
};

/**
 * @brief 缓冲区统计信息
 */
struct BufferStats {
    std::uint64_t total_pushed = 0;      ///< 总推送帧数
    std::uint64_t total_popped = 0;      ///< 总弹出帧数
    std::uint64_t dropped_frames = 0;    ///< 丢弃帧数
    std::uint64_t overflow_count = 0;    ///< 溢出次数
    std::uint64_t current_size = 0;      ///< 当前大小
    std::uint64_t capacity = 0;          ///< 容量

    /**
     * @brief 计算丢帧率
     * @return 丢帧率 [0.0, 1.0]
     */
    [[nodiscard]] double drop_rate() const noexcept {
        const std::uint64_t total = total_pushed + dropped_frames;
        return total > 0 ? static_cast<double>(dropped_frames) / total : 0.0;
    }

    /**
     * @brief 计算填充率
     * @return 填充率 [0.0, 1.0]
     */
    [[nodiscard]] double fill_rate() const noexcept {
        return capacity > 0 ? static_cast<double>(current_size) / capacity : 0.0;
    }
};

// ==========================================================================
// 无锁帧缓冲区（固定容量）
// ==========================================================================

/**
 * @brief 固定容量无锁帧缓冲区
 * @tparam T 存储元素类型，必须满足：
 *           - 可移动构造
 *           - 析构不抛异常
 * @tparam Capacity 缓冲区容量，必须是2的幂（用于位运算优化）
 * @details 基于序列号的环形缓冲区实现，支持单生产者-单消费者无锁并发
 *          使用原子操作保证内存序，支持 acquire-release 语义
 * @note 容量必须是2的幂，以便使用位掩码代替取模运算
 * @thread_safety 单生产者-单消费者线程安全
 */
template <typename T, std::size_t Capacity>
class LockFreeFrameBuffer {
    // 确保容量是2的幂
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
    static_assert(Capacity > 0, "Capacity must be greater than 0");
    static_assert(std::is_nothrow_destructible_v<T>, "T must have noexcept destructor");
    static_assert(std::is_move_constructible_v<T>, "T must be move constructible");

public:
    // ======================================================================
    // 类型别名
    // ======================================================================
    using value_type = T;
    using size_type = std::size_t;
    static constexpr size_type capacity = Capacity;
    static constexpr size_type mask = Capacity - 1;  ///< 位掩码，用于快速取模

    // ======================================================================
    // 构造与析构
    // ======================================================================

    /**
     * @brief 默认构造函数
     * @complexity O(Capacity)，预分配存储空间
     */
    LockFreeFrameBuffer()
        : buffer_(allocate_buffer())
        , write_seq_(0)
        , read_seq_(0)
        , policy_(BufferPolicy::DropOldest) {}

    /**
     * @brief 带策略的构造函数
     * @param policy 缓冲区满时的处理策略
     */
    explicit LockFreeFrameBuffer(BufferPolicy policy)
        : buffer_(allocate_buffer())
        , write_seq_(0)
        , read_seq_(0)
        , policy_(policy) {}

    /**
     * @brief 析构函数
     * @complexity O(n)，析构所有未弹出元素
     */
    ~LockFreeFrameBuffer() {
        // 析构所有未读元素
        clear();
        // 释放缓冲区内存
        deallocate_buffer(buffer_);
    }

    // 禁用拷贝
    LockFreeFrameBuffer(const LockFreeFrameBuffer&) = delete;
    LockFreeFrameBuffer& operator=(const LockFreeFrameBuffer&) = delete;

    // 支持移动
    LockFreeFrameBuffer(LockFreeFrameBuffer&& other) noexcept
        : buffer_(other.buffer_)
        , write_seq_(other.write_seq_.load(std::memory_order_relaxed))
        , read_seq_(other.read_seq_.load(std::memory_order_relaxed))
        , policy_(other.policy_)
        , total_pushed_(other.total_pushed_.load(std::memory_order_relaxed))
        , total_popped_(other.total_popped_.load(std::memory_order_relaxed))
        , dropped_frames_(other.dropped_frames_.load(std::memory_order_relaxed))
        , overflow_count_(other.overflow_count_.load(std::memory_order_relaxed)) {
        other.buffer_ = nullptr;
        other.write_seq_.store(0, std::memory_order_relaxed);
        other.read_seq_.store(0, std::memory_order_relaxed);
    }

    LockFreeFrameBuffer& operator=(LockFreeFrameBuffer&& other) noexcept {
        if (this != &other) {
            // 清空当前内容
            clear();

            // 释放当前缓冲区
            deallocate_buffer(buffer_);

            // 转移所有权
            buffer_ = other.buffer_;
            other.buffer_ = nullptr;

            write_seq_.store(other.write_seq_.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
            read_seq_.store(other.read_seq_.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
            policy_ = other.policy_;
            total_pushed_.store(other.total_pushed_.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
            total_popped_.store(other.total_popped_.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
            dropped_frames_.store(other.dropped_frames_.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            overflow_count_.store(other.overflow_count_.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);

            other.write_seq_.store(0, std::memory_order_relaxed);
            other.read_seq_.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    // ======================================================================
    // 核心操作接口
    // ======================================================================

    /**
     * @brief 推入元素（移动语义）
     * @param value 要推入的元素（将被移动）
     * @return true 推入成功
     * @return false 缓冲区已满（根据策略处理）
     * @complexity O(1)
     * @thread_safety 单生产者安全
     */
    [[nodiscard]] bool push(T&& value) {
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_acquire);

        // 检查是否已满
        if (write_seq - read_seq >= Capacity) {
            return handle_overflow(std::move(value));
        }

        // 写入数据 - 使用 placement new 在预分配内存中构造对象
        const size_type idx = static_cast<size_type>(write_seq & mask);
        T* slot = get_slot(idx);
        new (slot) T(std::move(value));

        // 发布写入（release 语义保证之前的写入对消费者可见）
        write_seq_.store(write_seq + 1, std::memory_order_release);
        total_pushed_.fetch_add(1, std::memory_order_relaxed);

        return true;
    }

    /**
     * @brief 推入元素（拷贝语义）
     * @param value 要推入的元素
     * @return true 推入成功
     * @return false 缓冲区已满
     * @complexity O(1)
     */
    [[nodiscard]] bool push(const T& value) {
        T copy = value;  // 拷贝
        return push(std::move(copy));  // 移动入缓冲区
    }

    /**
     * @brief 原地构造元素
     * @tparam Args 构造参数类型
     * @param args 构造参数
     * @return true 构造成功
     * @return false 缓冲区已满
     * @complexity O(1)
     * @thread_safety 单生产者安全
     */
    template <typename... Args>
    [[nodiscard]] bool emplace(Args&&... args) {
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_acquire);

        if (write_seq - read_seq >= Capacity) {
            // 对于 emplace，无法直接处理溢出，返回 false
            overflow_count_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        const size_type idx = static_cast<size_type>(write_seq & mask);
        T* slot = get_slot(idx);
        new (slot) T(std::forward<Args>(args)...);

        write_seq_.store(write_seq + 1, std::memory_order_release);
        total_pushed_.fetch_add(1, std::memory_order_relaxed);

        return true;
    }

    /**
     * @brief 弹出元素
     * @return std::optional<T> 弹出的元素，空表示缓冲区为空
     * @complexity O(1)
     * @thread_safety 单消费者安全
     */
    [[nodiscard]] std::optional<T> pop() {
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_acquire);

        // 检查是否为空
        if (read_seq >= write_seq) {
            return std::nullopt;
        }

        // 读取数据
        const size_type idx = static_cast<size_type>(read_seq & mask);
        T* slot = get_slot(idx);
        T value(std::move(*slot));
        slot->~T();  // 显式析构

        // 发布读取
        read_seq_.store(read_seq + 1, std::memory_order_release);
        total_popped_.fetch_add(1, std::memory_order_relaxed);

        return value;
    }

    /**
     * @brief 带超时的弹出操作
     * @param timeout 最大等待时间
     * @return std::optional<T> 弹出的元素，超时返回空
     * @complexity O(1) 平均，可能等待
     * @note 当前实现使用忙等待，非最优但简单可靠
     */
    [[nodiscard]] std::optional<T> pop_wait(core::Duration timeout) {
        const auto deadline = core::Clock::now() + timeout;

        while (core::Clock::now() < deadline) {
            if (auto result = pop()) {
                return result;
            }
            // 短暂自旋后让出 CPU
            std::this_thread::yield();
        }

        return std::nullopt;
    }

    /**
     * @brief 查看队首元素（不弹出）
     * @return T* 指向队首元素的指针，空表示缓冲区为空
     * @complexity O(1)
     * @warning 返回的指针仅在下次 pop 前有效
     * @thread_safety 单消费者安全
     */
    [[nodiscard]] T* peek() {
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_acquire);

        if (read_seq >= write_seq) {
            return nullptr;
        }

        const size_type idx = static_cast<size_type>(read_seq & mask);
        return get_slot(idx);
    }

    [[nodiscard]] const T* peek() const {
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_acquire);

        if (read_seq >= write_seq) {
            return nullptr;
        }

        const size_type idx = static_cast<size_type>(read_seq & mask);
        return get_slot(idx);
    }

    // ======================================================================
    // 状态查询
    // ======================================================================

    /**
     * @brief 获取当前大小
     * @return size_type 当前元素数量
     * @complexity O(1)
     * @note 返回值是近似值，多线程环境下可能不准确
     */
    [[nodiscard]] size_type size() const noexcept {
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
        return static_cast<size_type>(write_seq - read_seq);
    }

    /**
     * @brief 检查是否为空
     * @return true 缓冲区为空
     */
    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    /**
     * @brief 检查是否已满
     * @return true 缓冲区已满
     */
    [[nodiscard]] bool full() const noexcept {
        return size() >= Capacity;
    }

    /**
     * @brief 获取容量
     * @return size_type 缓冲区容量
     */
    [[nodiscard]] static constexpr size_type get_capacity() noexcept {
        return Capacity;
    }

    /**
     * @brief 清空缓冲区
     * @complexity O(n)，析构所有元素
     * @thread_safety 非线程安全，调用者需确保无并发访问
     */
    void clear() noexcept {
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);

        // 析构所有未读元素
        for (std::uint64_t seq = read_seq; seq < write_seq; ++seq) {
            const size_type idx = static_cast<size_type>(seq & mask);
            T* slot = get_slot(idx);
            slot->~T();
        }

        read_seq_.store(write_seq, std::memory_order_relaxed);
    }

    /**
     * @brief 获取统计信息
     * @return BufferStats 当前统计信息
     */
    [[nodiscard]] BufferStats stats() const noexcept {
        BufferStats s;
        s.total_pushed = total_pushed_.load(std::memory_order_relaxed);
        s.total_popped = total_popped_.load(std::memory_order_relaxed);
        s.dropped_frames = dropped_frames_.load(std::memory_order_relaxed);
        s.overflow_count = overflow_count_.load(std::memory_order_relaxed);
        s.current_size = size();
        s.capacity = Capacity;
        return s;
    }

    /**
     * @brief 重置统计信息
     */
    void reset_stats() noexcept {
        total_pushed_.store(0, std::memory_order_relaxed);
        total_popped_.store(0, std::memory_order_relaxed);
        dropped_frames_.store(0, std::memory_order_relaxed);
        overflow_count_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief 获取当前策略
     * @return BufferPolicy 当前策略
     */
    [[nodiscard]] BufferPolicy policy() const noexcept {
        return policy_;
    }

    /**
     * @brief 设置策略
     * @param policy 新策略
     */
    void set_policy(BufferPolicy policy) noexcept {
        policy_ = policy;
    }

private:
    // ======================================================================
    // 辅助方法
    // ======================================================================

    /**
     * @brief 分配对齐的缓冲区内存
     * @return T* 指向分配内存的指针
     */
    [[nodiscard]] static T* allocate_buffer() {
        // 使用 aligned_alloc 分配对齐内存
        constexpr std::size_t alignment = alignof(T);
        constexpr std::size_t size = Capacity * sizeof(T);

        void* ptr = nullptr;
        #ifdef _WIN32
            ptr = _aligned_malloc(size, alignment);
            if (!ptr) throw std::bad_alloc();
        #else
            if (posix_memalign(&ptr, alignment, size) != 0) {
                throw std::bad_alloc();
            }
        #endif

        return static_cast<T*>(ptr);
    }

    /**
     * @brief 释放缓冲区内存
     * @param ptr 要释放的指针
     */
    static void deallocate_buffer(T* ptr) {
        if (ptr) {
            #ifdef _WIN32
                _aligned_free(ptr);
            #else
                free(ptr);
            #endif
        }
    }

    /**
     * @brief 获取指定索引的槽位指针
     * @param idx 索引
     * @return T* 槽位指针
     */
    [[nodiscard]] T* get_slot(size_type idx) noexcept {
        return buffer_ + idx;
    }

    [[nodiscard]] const T* get_slot(size_type idx) const noexcept {
        return buffer_ + idx;
    }

    /**
     * @brief 处理缓冲区溢出
     * @param value 要写入的值
     * @return true 处理成功
     * @return false 无法处理（缓冲区已满且策略为 DropNewest）
     */
    [[nodiscard]] bool handle_overflow(T&& value) {
        overflow_count_.fetch_add(1, std::memory_order_relaxed);

        switch (policy_) {
            case BufferPolicy::DropOldest: {
                // 丢弃最旧的帧
                const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
                const size_type idx = static_cast<size_type>(read_seq & mask);
                T* slot = get_slot(idx);
                slot->~T();

                // 推进读指针
                read_seq_.store(read_seq + 1, std::memory_order_release);
                dropped_frames_.fetch_add(1, std::memory_order_relaxed);

                // 写入新帧
                const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
                const size_type write_idx = static_cast<size_type>(write_seq & mask);
                T* write_slot = get_slot(write_idx);
                new (write_slot) T(std::move(value));
                write_seq_.store(write_seq + 1, std::memory_order_release);
                total_pushed_.fetch_add(1, std::memory_order_relaxed);

                return true;
            }

            case BufferPolicy::DropNewest:
                // 丢弃新帧
                dropped_frames_.fetch_add(1, std::memory_order_relaxed);
                return false;

            case BufferPolicy::Overwrite: {
                // 直接覆盖，不析构旧元素
                const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
                const size_type idx = static_cast<size_type>(write_seq & mask);
                T* slot = get_slot(idx);
                // 先析构旧对象
                slot->~T();
                // 构造新对象
                new (slot) T(std::move(value));
                write_seq_.store(write_seq + 1, std::memory_order_release);
                total_pushed_.fetch_add(1, std::memory_order_relaxed);
                return true;
            }

            case BufferPolicy::Block:
                // 无锁缓冲区不支持阻塞
                dropped_frames_.fetch_add(1, std::memory_order_relaxed);
                return false;
        }

        return false;
    }

private:
    // ======================================================================
    // 成员变量
    // ======================================================================

    // 使用原始指针存储缓冲区，手动管理内存
    T* buffer_;

    // 序列号（原子操作）
    alignas(64) std::atomic<std::uint64_t> write_seq_;  ///< 写序列号
    alignas(64) std::atomic<std::uint64_t> read_seq_;   ///< 读序列号

    // 策略
    BufferPolicy policy_;

    // 统计信息（原子计数器）
    alignas(64) std::atomic<std::uint64_t> total_pushed_{0};
    alignas(64) std::atomic<std::uint64_t> total_popped_{0};
    alignas(64) std::atomic<std::uint64_t> dropped_frames_{0};
    alignas(64) std::atomic<std::uint64_t> overflow_count_{0};
};

// ==========================================================================
// 动态容量帧缓冲区（运行时确定容量）
// ==========================================================================

/**
 * @brief 计算下一个2的幂
 * @param n 输入值
 * @return std::size_t 大于等于n的最小2的幂
 */
[[nodiscard]] inline std::size_t next_power_of_2(std::size_t n) {
    if (n == 0) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(std::size_t) == 8) {
        n |= n >> 32;
    }
    return n + 1;
}

/**
 * @brief 动态容量无锁帧缓冲区
 * @tparam T 存储元素类型
 * @details 支持运行时确定容量，但容量仍必须是2的幂
 * @note 性能略低于固定容量版本，需要堆分配
 */
template <typename T>
class DynamicFrameBuffer {
    static_assert(std::is_nothrow_destructible_v<T>, "T must have noexcept destructor");
    static_assert(std::is_move_constructible_v<T>, "T must be move constructible");

public:
    using value_type = T;
    using size_type = std::size_t;

    /**
     * @brief 构造函数
     * @param capacity 缓冲区容量（将向上取整到2的幂）
     */
    explicit DynamicFrameBuffer(size_type capacity)
        : capacity_(next_power_of_2(capacity))
        , mask_(capacity_ - 1)
        , buffer_(allocate_buffer(capacity_))
        , write_seq_(0)
        , read_seq_(0)
        , policy_(BufferPolicy::DropOldest) {}

    explicit DynamicFrameBuffer(size_type capacity, BufferPolicy policy)
        : capacity_(next_power_of_2(capacity))
        , mask_(capacity_ - 1)
        , buffer_(allocate_buffer(capacity_))
        , write_seq_(0)
        , read_seq_(0)
        , policy_(policy) {}

    ~DynamicFrameBuffer() {
        clear();
        deallocate_buffer(buffer_, capacity_);
    }

    // 禁用拷贝
    DynamicFrameBuffer(const DynamicFrameBuffer&) = delete;
    DynamicFrameBuffer& operator=(const DynamicFrameBuffer&) = delete;

    // 支持移动
    DynamicFrameBuffer(DynamicFrameBuffer&& other) noexcept
        : capacity_(other.capacity_)
        , mask_(other.mask_)
        , buffer_(other.buffer_)
        , write_seq_(other.write_seq_.load(std::memory_order_relaxed))
        , read_seq_(other.read_seq_.load(std::memory_order_relaxed))
        , policy_(other.policy_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
        other.mask_ = 0;
        other.write_seq_.store(0, std::memory_order_relaxed);
        other.read_seq_.store(0, std::memory_order_relaxed);
    }

    DynamicFrameBuffer& operator=(DynamicFrameBuffer&& other) noexcept {
        if (this != &other) {
            clear();
            deallocate_buffer(buffer_, capacity_);

            capacity_ = other.capacity_;
            mask_ = other.mask_;
            buffer_ = other.buffer_;
            write_seq_.store(other.write_seq_.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
            read_seq_.store(other.read_seq_.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
            policy_ = other.policy_;

            other.buffer_ = nullptr;
            other.capacity_ = 0;
            other.mask_ = 0;
            other.write_seq_.store(0, std::memory_order_relaxed);
            other.read_seq_.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    // 核心操作（与 LockFreeFrameBuffer 相同接口）
    [[nodiscard]] bool push(T&& value) {
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_acquire);

        if (write_seq - read_seq >= capacity_) {
            return handle_overflow(std::move(value));
        }

        const size_type idx = static_cast<size_type>(write_seq & mask_);
        T* slot = get_slot(idx);
        new (slot) T(std::move(value));

        write_seq_.store(write_seq + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool push(const T& value) {
        T copy = value;
        return push(std::move(copy));
    }

    template <typename... Args>
    [[nodiscard]] bool emplace(Args&&... args) {
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_acquire);

        if (write_seq - read_seq >= capacity_) {
            return false;
        }

        const size_type idx = static_cast<size_type>(write_seq & mask_);
        T* slot = get_slot(idx);
        new (slot) T(std::forward<Args>(args)...);

        write_seq_.store(write_seq + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<T> pop() {
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_acquire);

        if (read_seq >= write_seq) {
            return std::nullopt;
        }

        const size_type idx = static_cast<size_type>(read_seq & mask_);
        T* slot = get_slot(idx);
        T value(std::move(*slot));
        slot->~T();

        read_seq_.store(read_seq + 1, std::memory_order_release);
        return value;
    }

    [[nodiscard]] size_type size() const noexcept {
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
        return static_cast<size_type>(write_seq - read_seq);
    }

    [[nodiscard]] bool empty() const noexcept {
        return size() == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return size() >= capacity_;
    }

    [[nodiscard]] size_type capacity() const noexcept {
        return capacity_;
    }

    void clear() noexcept {
        const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
        const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);

        for (std::uint64_t seq = read_seq; seq < write_seq; ++seq) {
            const size_type idx = static_cast<size_type>(seq & mask_);
            T* slot = get_slot(idx);
            slot->~T();
        }

        read_seq_.store(write_seq, std::memory_order_relaxed);
    }

private:
    [[nodiscard]] T* allocate_buffer(size_type capacity) {
        constexpr std::size_t alignment = alignof(T);
        const std::size_t size = capacity * sizeof(T);

        void* ptr = nullptr;
        #ifdef _WIN32
            ptr = _aligned_malloc(size, alignment);
            if (!ptr) throw std::bad_alloc();
        #else
            if (posix_memalign(&ptr, alignment, size) != 0) {
                throw std::bad_alloc();
            }
        #endif

        return static_cast<T*>(ptr);
    }

    void deallocate_buffer(T* ptr, size_type) {
        if (ptr) {
            #ifdef _WIN32
                _aligned_free(ptr);
            #else
                free(ptr);
            #endif
        }
    }

    [[nodiscard]] T* get_slot(size_type idx) noexcept {
        return buffer_ + idx;
    }

    [[nodiscard]] bool handle_overflow(T&& value) {
        switch (policy_) {
            case BufferPolicy::DropOldest: {
                const std::uint64_t read_seq = read_seq_.load(std::memory_order_relaxed);
                const size_type idx = static_cast<size_type>(read_seq & mask_);
                T* slot = get_slot(idx);
                slot->~T();

                read_seq_.store(read_seq + 1, std::memory_order_release);

                const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
                const size_type write_idx = static_cast<size_type>(write_seq & mask_);
                T* write_slot = get_slot(write_idx);
                new (write_slot) T(std::move(value));
                write_seq_.store(write_seq + 1, std::memory_order_release);

                return true;
            }

            case BufferPolicy::DropNewest:
                return false;

            case BufferPolicy::Overwrite: {
                const std::uint64_t write_seq = write_seq_.load(std::memory_order_relaxed);
                const size_type idx = static_cast<size_type>(write_seq & mask_);
                T* slot = get_slot(idx);
                slot->~T();
                new (slot) T(std::move(value));
                write_seq_.store(write_seq + 1, std::memory_order_release);
                return true;
            }

            case BufferPolicy::Block:
                return false;
        }
        return false;
    }

private:
    size_type capacity_;
    size_type mask_;
    T* buffer_;

    alignas(64) std::atomic<std::uint64_t> write_seq_;
    alignas(64) std::atomic<std::uint64_t> read_seq_;

    BufferPolicy policy_;
};

} // namespace aam::l0
