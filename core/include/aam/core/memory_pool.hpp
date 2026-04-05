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
// @file memory_pool.hpp
// @author AAM Development Team
// @brief 定长内存池实现
// ==========================================================================
// 版本: v0.2.0-alpha.1
// 功能: 提供固定大小对象的快速分配与释放，避免堆碎片化
// 依赖: C++23, atomic, memory_order
// 算法: 基于空闲链表（Free List）的内存池，O(1) 分配/释放
// ==========================================================================

#ifndef AAM_CORE_MEMORY_POOL_HPP
#define AAM_CORE_MEMORY_POOL_HPP

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324)  // 禁用"结构被填充"警告
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "aam/core/timer.hpp"

namespace aam::core {

// ==========================================================================
// 内存池配置
// ==========================================================================

/**
 * @brief 内存池配置参数
 */
struct MemoryPoolConfig {
    std::size_t block_size{64};                 ///< 块大小（字节）
    std::size_t initial_blocks{1024};           ///< 初始块数量
    std::size_t max_blocks{65536};              ///< 最大块数量
    std::size_t alignment{alignof(std::max_align_t)}; ///< 内存对齐
    bool allow_growth{true};                    ///< 允许自动增长
    bool track_allocations{false};              ///< 跟踪分配统计
};

// ==========================================================================
// 内存池统计信息
// ==========================================================================

/**
 * @brief 内存池统计信息
 */
struct MemoryPoolStats {
    std::size_t total_blocks{0};                ///< 总块数
    std::size_t free_blocks{0};                 ///< 空闲块数
    std::size_t used_blocks{0};                 ///< 已用块数
    std::size_t peak_used{0};                   ///< 峰值使用量
    std::size_t allocation_count{0};            ///< 分配次数
    std::size_t deallocation_count{0};          ///< 释放次数
    std::size_t growth_count{0};                ///< 增长次数

    /**
     * @brief 计算使用率
     * @return 使用率（0.0 - 1.0）
     */
    [[nodiscard]] double utilization() const noexcept {
        return total_blocks > 0 ? static_cast<double>(used_blocks) / total_blocks : 0.0;
    }

    /**
     * @brief 计算碎片率
     * @return 碎片率（0.0 - 1.0）
     */
    [[nodiscard]] double fragmentation() const noexcept {
        return total_blocks > 0 ? static_cast<double>(free_blocks) / total_blocks : 0.0;
    }
};

// ==========================================================================
// 定长内存池
// ==========================================================================

/**
 * @brief 定长内存池
 * @details 提供固定大小内存块的快速分配与释放
 * @thread_safety 线程安全，支持多线程并发分配/释放
 */
class FixedMemoryPool {
public:
    /**
     * @brief 构造函数
     * @param config 内存池配置
     */
    explicit FixedMemoryPool(const MemoryPoolConfig& config);

    /**
     * @brief 析构函数
     */
    ~FixedMemoryPool();

    // 禁用拷贝
    FixedMemoryPool(const FixedMemoryPool&) = delete;
    FixedMemoryPool& operator=(const FixedMemoryPool&) = delete;

    // 允许移动
    FixedMemoryPool(FixedMemoryPool&& other) noexcept;
    FixedMemoryPool& operator=(FixedMemoryPool&& other) noexcept;

    /**
     * @brief 分配内存块
     * @return 内存块指针，失败返回 nullptr
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    [[nodiscard]] void* allocate();

    /**
     * @brief 分配内存块（不初始化）
     * @return 内存块指针，失败返回 nullptr
     */
    [[nodiscard]] void* allocate_uninitialized();

    /**
     * @brief 释放内存块
     * @param ptr 内存块指针
     * @complexity O(1)
     * @thread_safety 线程安全
     */
    void deallocate(void* ptr);

    /**
     * @brief 检查指针是否属于此内存池
     * @param ptr 内存指针
     * @return true 如果属于此池
     */
    [[nodiscard]] bool contains(const void* ptr) const noexcept;

    /**
     * @brief 获取块大小
     * @return 块大小（字节）
     */
    [[nodiscard]] std::size_t block_size() const noexcept {
        return block_size_;
    }

    /**
     * @brief 获取总块数
     * @return 总块数
     */
    [[nodiscard]] std::size_t total_blocks() const noexcept {
        return total_blocks_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取空闲块数
     * @return 空闲块数
     */
    [[nodiscard]] std::size_t free_blocks() const noexcept {
        return free_blocks_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 获取已用块数
     * @return 已用块数
     */
    [[nodiscard]] std::size_t used_blocks() const noexcept {
        return total_blocks() - free_blocks();
    }

    /**
     * @brief 获取统计信息
     * @return 统计信息快照
     */
    [[nodiscard]] MemoryPoolStats stats() const noexcept;

    /**
     * @brief 手动增长池大小
     * @param additional_blocks 要添加的块数
     * @return 实际添加的块数
     */
    [[nodiscard]] std::size_t grow(std::size_t additional_blocks);

    /**
     * @brief 重置内存池（释放所有分配）
     * @warning 仅在确保无未释放分配时调用
     */
    void reset();

private:
    /**
     * @brief 内存块头部（空闲链表节点）
     */
    struct BlockHeader {
        std::atomic<BlockHeader*> next{nullptr};  ///< 下一个空闲块
    };

    /**
     * @brief 内存块结构
     */
    struct alignas(64) MemoryChunk {
        std::byte* memory{nullptr};               ///< 内存起始地址
        std::size_t block_count{0};               ///< 块数量
        MemoryChunk* next{nullptr};               ///< 下一个内存块
    };

    // 私有方法
    [[nodiscard]] bool try_grow();
    void add_chunk(std::byte* memory, std::size_t block_count);

    // 配置
    std::size_t block_size_;
    std::size_t max_blocks_;
    std::size_t alignment_;
    bool allow_growth_;
    bool track_allocations_;

    // 空闲链表头（原子指针）
    alignas(64) std::atomic<BlockHeader*> free_list_{nullptr};

    // 内存块链表
    MemoryChunk* chunks_{nullptr};

    // 统计信息（原子计数器）
    alignas(64) std::atomic<std::size_t> total_blocks_{0};
    alignas(64) std::atomic<std::size_t> free_blocks_{0};
    alignas(64) std::atomic<std::size_t> peak_used_{0};
    alignas(64) std::atomic<std::size_t> allocation_count_{0};
    alignas(64) std::atomic<std::size_t> deallocation_count_{0};
    alignas(64) std::atomic<std::size_t> growth_count_{0};
};

// ==========================================================================
// 对象池模板
// ==========================================================================

/**
 * @brief 对象池
 * @tparam T 对象类型
 * @details 基于 FixedMemoryPool 的对象级分配器，支持构造/析构
 * @thread_safety 线程安全
 */
template <typename T>
class ObjectPool {
    static_assert(std::is_nothrow_destructible_v<T>, "T must have noexcept destructor");

public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    /**
     * @brief 构造函数
     * @param initial_count 初始对象数量
     * @param max_count 最大对象数量
     */
    explicit ObjectPool(std::size_t initial_count = 1024, std::size_t max_count = 65536)
        : pool_(MemoryPoolConfig{
              .block_size = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*),
              .initial_blocks = initial_count,
              .max_blocks = max_count,
              .alignment = alignof(T),
              .allow_growth = true,
              .track_allocations = true
          }) {}

    /**
     * @brief 构造对象
     * @tparam Args 构造参数类型
     * @param args 构造参数
     * @return 对象指针，失败返回 nullptr
     */
    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) {
        void* mem = pool_.allocate_uninitialized();
        if (!mem) {
            return nullptr;
        }
        try {
            return new (mem) T(std::forward<Args>(args)...);
        } catch (...) {
            pool_.deallocate(mem);
            return nullptr;
        }
    }

    /**
     * @brief 销毁对象
     * @param ptr 对象指针
     */
    void destroy(T* ptr) {
        if (ptr) {
            ptr->~T();
            pool_.deallocate(ptr);
        }
    }

    /**
     * @brief 获取统计信息
     * @return 内存池统计信息
     */
    [[nodiscard]] MemoryPoolStats stats() const noexcept {
        return pool_.stats();
    }

    /**
     * @brief 创建智能指针对象
     * @tparam Args 构造参数类型
     * @param args 构造参数
     * @return std::unique_ptr 管理的对象
     */
    template <typename... Args>
    [[nodiscard]] std::unique_ptr<T, std::function<void(T*)>> make_unique(Args&&... args) {
        T* ptr = construct(std::forward<Args>(args)...);
        return {ptr, [this](T* p) { destroy(p); }};
    }

    /**
     * @brief 创建共享指针对象
     * @tparam Args 构造参数类型
     * @param args 构造参数
     * @return std::shared_ptr 管理的对象
     */
    template <typename... Args>
    [[nodiscard]] std::shared_ptr<T> make_shared(Args&&... args) {
        T* ptr = construct(std::forward<Args>(args)...);
        return std::shared_ptr<T>(ptr, [this](T* p) { destroy(p); });
    }

private:
    FixedMemoryPool pool_;
};

// ==========================================================================
// 帧缓冲区专用内存池
// ==========================================================================

/**
 * @brief 帧缓冲区内存池
 * @details 针对帧数据分配优化的内存池，支持预分配和复用
 */
class FrameBufferPool {
public:
    /**
     * @brief 帧缓冲区描述
     */
    struct FrameBuffer {
        std::byte* data{nullptr};               ///< 数据指针
        std::size_t size{0};                    ///< 缓冲区大小
        std::size_t used{0};                    ///< 实际使用大小
        std::uint64_t sequence{0};              ///< 序列号
        Timestamp acquire_time;                 ///< 获取时间

        /**
         * @brief 获取可用空间
         * @return 可用字节数
         */
        [[nodiscard]] std::size_t available() const noexcept {
            return size > used ? size - used : 0;
        }
    };

    /**
     * @brief 构造函数
     * @param buffer_size 每个缓冲区大小
     * @param initial_count 初始缓冲区数量
     * @param max_count 最大缓冲区数量
     */
    FrameBufferPool(
        std::size_t buffer_size,
        std::size_t initial_count = 8,
        std::size_t max_count = 64);

    /**
     * @brief 析构函数
     */
    ~FrameBufferPool();

    // 禁用拷贝
    FrameBufferPool(const FrameBufferPool&) = delete;
    FrameBufferPool& operator=(const FrameBufferPool&) = delete;

    // 允许移动
    FrameBufferPool(FrameBufferPool&&) = default;
    FrameBufferPool& operator=(FrameBufferPool&&) = default;

    /**
     * @brief 获取缓冲区
     * @return 帧缓冲区，失败返回空 optional
     */
    [[nodiscard]] std::optional<FrameBuffer> acquire();

    /**
     * @brief 释放缓冲区
     * @param buffer 要释放的缓冲区
     */
    void release(FrameBuffer& buffer);

    /**
     * @brief 获取缓冲区大小
     * @return 缓冲区大小
     */
    [[nodiscard]] std::size_t buffer_size() const noexcept {
        return buffer_size_;
    }

    /**
     * @brief 获取统计信息
     * @return 内存池统计信息
     */
    [[nodiscard]] MemoryPoolStats stats() const noexcept {
        return pool_.stats();
    }

private:
    std::size_t buffer_size_;
    FixedMemoryPool pool_;
    std::atomic<std::uint64_t> sequence_{0};
};

// ==========================================================================
// 内存池分配器（STL 兼容）
// ==========================================================================

/**
 * @brief 内存池分配器
 * @tparam T 元素类型
 * @details STL 兼容的分配器，可用于 std::vector 等容器
 */
template <typename T>
class PoolAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind {
        using other = PoolAllocator<U>;
    };

    /**
     * @brief 构造函数
     * @param pool 内存池指针
     */
    explicit PoolAllocator(FixedMemoryPool* pool) : pool_(pool) {}

    /**
     * @brief 从其他类型构造
     * @tparam U 其他类型
     * @param other 其他分配器
     */
    template <typename U>
    explicit PoolAllocator(const PoolAllocator<U>& other) : pool_(other.pool_) {}

    /**
     * @brief 分配内存
     * @param n 元素数量
     * @return 内存指针
     */
    [[nodiscard]] T* allocate(std::size_t n) {
        if (n != 1) {
            // 内存池只支持单元素分配
            return static_cast<T*>(::operator new(n * sizeof(T)));
        }
        return static_cast<T*>(pool_->allocate());
    }

    /**
     * @brief 释放内存
     * @param ptr 内存指针
     * @param n 元素数量
     */
    void deallocate(T* ptr, std::size_t n) {
        if (n != 1) {
            ::operator delete(ptr);
            return;
        }
        pool_->deallocate(ptr);
    }

    /**
     * @brief 构造对象
     * @tparam Args 构造参数类型
     * @param p 内存位置
     * @param args 构造参数
     */
    template <typename... Args>
    void construct(T* p, Args&&... args) {
        new (p) T(std::forward<Args>(args)...);
    }

    /**
     * @brief 销毁对象
     * @param p 对象指针
     */
    void destroy(T* p) {
        p->~T();
    }

    /**
     * @brief 比较相等
     */
    bool operator==(const PoolAllocator& other) const noexcept {
        return pool_ == other.pool_;
    }

    bool operator!=(const PoolAllocator& other) const noexcept {
        return !(*this == other);
    }

private:
    template <typename U>
    friend class PoolAllocator;

    FixedMemoryPool* pool_;
};

} // namespace aam::core

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // AAM_CORE_MEMORY_POOL_HPP
