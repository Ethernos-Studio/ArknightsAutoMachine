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
// @file memory_pool.cpp
// @author dhjs0000
// @brief 定长内存池实现
// ==========================================================================

#include "aam/core/memory_pool.hpp"

#include <algorithm>
#include <cstring>

namespace aam::core {

// ==========================================================================
// FixedMemoryPool 实现
// ==========================================================================

FixedMemoryPool::FixedMemoryPool(const MemoryPoolConfig& config)
    : block_size_(std::max(config.block_size, sizeof(BlockHeader)))
    , max_blocks_(config.max_blocks)
    , alignment_(config.alignment)
    , allow_growth_(config.allow_growth)
    , track_allocations_(config.track_allocations) {
    // 确保块大小至少为 BlockHeader 大小
    block_size_ = std::max(block_size_, sizeof(BlockHeader));

    // 对齐块大小
    if (alignment_ > 0) {
        block_size_ = (block_size_ + alignment_ - 1) & ~(alignment_ - 1);
    }

    // 预分配初始块
    if (config.initial_blocks > 0) {
        [[maybe_unused]] auto result = grow(config.initial_blocks);
    }
}

FixedMemoryPool::~FixedMemoryPool() {
    // 释放所有内存块
    MemoryChunk* chunk = chunks_;
    while (chunk) {
        MemoryChunk* next = chunk->next;
        // 使用对应的对齐内存释放函数
        #ifdef _WIN32
            _aligned_free(chunk->memory);
        #else
            free(chunk->memory);
        #endif
        delete chunk;
        chunk = next;
    }
}

FixedMemoryPool::FixedMemoryPool(FixedMemoryPool&& other) noexcept
    : block_size_(other.block_size_)
    , max_blocks_(other.max_blocks_)
    , alignment_(other.alignment_)
    , allow_growth_(other.allow_growth_)
    , track_allocations_(other.track_allocations_)
    , free_list_(other.free_list_.load(std::memory_order_relaxed))
    , chunks_(other.chunks_)
    , total_blocks_(other.total_blocks_.load(std::memory_order_relaxed))
    , free_blocks_(other.free_blocks_.load(std::memory_order_relaxed))
    , peak_used_(other.peak_used_.load(std::memory_order_relaxed))
    , allocation_count_(other.allocation_count_.load(std::memory_order_relaxed))
    , deallocation_count_(other.deallocation_count_.load(std::memory_order_relaxed))
    , growth_count_(other.growth_count_.load(std::memory_order_relaxed)) {
    other.free_list_.store(nullptr, std::memory_order_relaxed);
    other.chunks_ = nullptr;
    other.total_blocks_.store(0, std::memory_order_relaxed);
    other.free_blocks_.store(0, std::memory_order_relaxed);
}

FixedMemoryPool& FixedMemoryPool::operator=(FixedMemoryPool&& other) noexcept {
    if (this != &other) {
        // 释放当前资源
        MemoryChunk* chunk = chunks_;
        while (chunk) {
            MemoryChunk* next = chunk->next;
            #ifdef _WIN32
                _aligned_free(chunk->memory);
            #else
                free(chunk->memory);
            #endif
            delete chunk;
            chunk = next;
        }

        // 移动资源
        block_size_ = other.block_size_;
        max_blocks_ = other.max_blocks_;
        alignment_ = other.alignment_;
        allow_growth_ = other.allow_growth_;
        track_allocations_ = other.track_allocations_;
        free_list_.store(other.free_list_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        chunks_ = other.chunks_;
        total_blocks_.store(other.total_blocks_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        free_blocks_.store(other.free_blocks_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        peak_used_.store(other.peak_used_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        allocation_count_.store(other.allocation_count_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        deallocation_count_.store(other.deallocation_count_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        growth_count_.store(other.growth_count_.load(std::memory_order_relaxed), std::memory_order_relaxed);

        // 清空源对象
        other.free_list_.store(nullptr, std::memory_order_relaxed);
        other.chunks_ = nullptr;
        other.total_blocks_.store(0, std::memory_order_relaxed);
        other.free_blocks_.store(0, std::memory_order_relaxed);
    }
    return *this;
}

void* FixedMemoryPool::allocate() {
    // 尝试从空闲链表获取
    BlockHeader* head = free_list_.load(std::memory_order_relaxed);

    while (head != nullptr) {
        BlockHeader* next = head->next.load(std::memory_order_relaxed);

        // CAS 操作：尝试将空闲链表头设置为 next
        if (free_list_.compare_exchange_weak(head, next,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed)) {
            // 成功获取块
            free_blocks_.fetch_sub(1, std::memory_order_relaxed);

            if (track_allocations_) {
                allocation_count_.fetch_add(1, std::memory_order_relaxed);

                const std::size_t used = total_blocks_.load(std::memory_order_relaxed) -
                                        free_blocks_.load(std::memory_order_relaxed);
                std::size_t current_peak = peak_used_.load(std::memory_order_relaxed);
                while (used > current_peak &&
                       !peak_used_.compare_exchange_weak(current_peak, used,
                                                         std::memory_order_relaxed,
                                                         std::memory_order_relaxed)) {
                    // 重试
                }
            }

            // 清零内存（安全考虑）
            std::memset(head, 0, block_size_);

            return head;
        }
        // CAS 失败，重试
    }

    // 空闲链表为空，尝试增长
    if (allow_growth_ && try_grow()) {
        return allocate();  // 递归调用，应该能获取到新块
    }

    return nullptr;  // 分配失败
}

void* FixedMemoryPool::allocate_uninitialized() {
    // 与 allocate 相同，但不清零内存
    BlockHeader* head = free_list_.load(std::memory_order_relaxed);

    while (head != nullptr) {
        BlockHeader* next = head->next.load(std::memory_order_relaxed);

        if (free_list_.compare_exchange_weak(head, next,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed)) {
            free_blocks_.fetch_sub(1, std::memory_order_relaxed);

            if (track_allocations_) {
                allocation_count_.fetch_add(1, std::memory_order_relaxed);

                const std::size_t used = total_blocks_.load(std::memory_order_relaxed) -
                                        free_blocks_.load(std::memory_order_relaxed);
                std::size_t current_peak = peak_used_.load(std::memory_order_relaxed);
                while (used > current_peak &&
                       !peak_used_.compare_exchange_weak(current_peak, used,
                                                         std::memory_order_relaxed,
                                                         std::memory_order_relaxed)) {
                }
            }

            return head;
        }
    }

    if (allow_growth_ && try_grow()) {
        return allocate_uninitialized();
    }

    return nullptr;
}

void FixedMemoryPool::deallocate(void* ptr) {
    if (!ptr) return;

    // 验证指针属于此池
    if (!contains(ptr)) {
        // 指针不属于此池，可能是错误调用
        return;
    }

    BlockHeader* block = static_cast<BlockHeader*>(ptr);

    // 将块添加到空闲链表头部
    BlockHeader* head = free_list_.load(std::memory_order_relaxed);
    do {
        block->next.store(head, std::memory_order_relaxed);
    } while (!free_list_.compare_exchange_weak(head, block,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));

    free_blocks_.fetch_add(1, std::memory_order_relaxed);

    if (track_allocations_) {
        deallocation_count_.fetch_add(1, std::memory_order_relaxed);
    }
}

bool FixedMemoryPool::contains(const void* ptr) const noexcept {
    const std::byte* byte_ptr = static_cast<const std::byte*>(ptr);

    MemoryChunk* chunk = chunks_;
    while (chunk) {
        const std::byte* start = chunk->memory;
        const std::byte* end = chunk->memory + (chunk->block_count * block_size_);

        if (byte_ptr >= start && byte_ptr < end) {
            // 检查对齐
            const std::ptrdiff_t offset = byte_ptr - start;
            return (offset % block_size_) == 0;
        }

        chunk = chunk->next;
    }

    return false;
}

MemoryPoolStats FixedMemoryPool::stats() const noexcept {
    MemoryPoolStats s;
    s.total_blocks = total_blocks_.load(std::memory_order_relaxed);
    s.free_blocks = free_blocks_.load(std::memory_order_relaxed);
    s.used_blocks = s.total_blocks - s.free_blocks;
    s.peak_used = peak_used_.load(std::memory_order_relaxed);
    s.allocation_count = allocation_count_.load(std::memory_order_relaxed);
    s.deallocation_count = deallocation_count_.load(std::memory_order_relaxed);
    s.growth_count = growth_count_.load(std::memory_order_relaxed);
    return s;
}

std::size_t FixedMemoryPool::grow(std::size_t additional_blocks) {
    const std::size_t current_total = total_blocks_.load(std::memory_order_relaxed);

    if (current_total + additional_blocks > max_blocks_) {
        additional_blocks = max_blocks_ - current_total;
    }

    if (additional_blocks == 0) {
        return 0;
    }

    // 分配内存 - 使用 _aligned_malloc / posix_memalign
    const std::size_t memory_size = additional_blocks * block_size_;
    void* memory = nullptr;

    #ifdef _WIN32
        memory = _aligned_malloc(memory_size, alignment_);
    #else
        if (posix_memalign(&memory, alignment_, memory_size) != 0) {
            memory = nullptr;
        }
    #endif

    if (!memory) {
        return 0;
    }
    std::byte* byte_memory = static_cast<std::byte*>(memory);

    // 初始化块并添加到空闲链表
    for (std::size_t i = 0; i < additional_blocks; ++i) {
        BlockHeader* block = reinterpret_cast<BlockHeader*>(byte_memory + (i * block_size_));
        block->next.store(nullptr, std::memory_order_relaxed);

        // 添加到空闲链表
        BlockHeader* head = free_list_.load(std::memory_order_relaxed);
        do {
            block->next.store(head, std::memory_order_relaxed);
        } while (!free_list_.compare_exchange_weak(head, block,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed));
    }

    // 添加到内存块链表
    add_chunk(byte_memory, additional_blocks);

    // 更新统计
    total_blocks_.fetch_add(additional_blocks, std::memory_order_relaxed);
    free_blocks_.fetch_add(additional_blocks, std::memory_order_relaxed);
    growth_count_.fetch_add(1, std::memory_order_relaxed);

    return additional_blocks;
}

void FixedMemoryPool::reset() {
    // 重建空闲链表
    MemoryChunk* chunk = chunks_;
    free_list_.store(nullptr, std::memory_order_relaxed);
    std::size_t total = 0;

    while (chunk) {
        for (std::size_t i = 0; i < chunk->block_count; ++i) {
            BlockHeader* block = reinterpret_cast<BlockHeader*>(chunk->memory + (i * block_size_));
            block->next.store(nullptr, std::memory_order_relaxed);

            BlockHeader* head = free_list_.load(std::memory_order_relaxed);
            do {
                block->next.store(head, std::memory_order_relaxed);
            } while (!free_list_.compare_exchange_weak(head, block,
                                                        std::memory_order_release,
                                                        std::memory_order_relaxed));
        }
        total += chunk->block_count;
        chunk = chunk->next;
    }

    free_blocks_.store(total, std::memory_order_relaxed);
    allocation_count_.store(0, std::memory_order_relaxed);
    deallocation_count_.store(0, std::memory_order_relaxed);
}

bool FixedMemoryPool::try_grow() {
    // 默认增长策略：翻倍，但不超过剩余空间
    const std::size_t current_total = total_blocks_.load(std::memory_order_relaxed);
    std::size_t grow_size = std::max(current_total, static_cast<std::size_t>(1));

    if (current_total + grow_size > max_blocks_) {
        grow_size = max_blocks_ - current_total;
    }

    if (grow_size == 0) {
        return false;
    }

    return grow(grow_size) > 0;
}

void FixedMemoryPool::add_chunk(std::byte* memory, std::size_t block_count) {
    MemoryChunk* chunk = new MemoryChunk();
    chunk->memory = memory;
    chunk->block_count = block_count;
    chunk->next = chunks_;
    chunks_ = chunk;
}

// ==========================================================================
// FrameBufferPool 实现
// ==========================================================================

FrameBufferPool::FrameBufferPool(std::size_t buffer_size, std::size_t initial_count, std::size_t max_count)
    : buffer_size_(buffer_size)
    , pool_(MemoryPoolConfig{
          .block_size = buffer_size,
          .initial_blocks = initial_count,
          .max_blocks = max_count,
          .alignment = 64,  // 缓存行对齐
          .allow_growth = true,
          .track_allocations = true
      }) {}

FrameBufferPool::~FrameBufferPool() = default;

std::optional<FrameBufferPool::FrameBuffer> FrameBufferPool::acquire() {
    void* mem = pool_.allocate_uninitialized();
    if (!mem) {
        return std::nullopt;
    }

    FrameBufferPool::FrameBuffer buffer;
    buffer.data = static_cast<std::byte*>(mem);
    buffer.size = buffer_size_;
    buffer.used = 0;
    buffer.sequence = sequence_.fetch_add(1, std::memory_order_relaxed);
    buffer.acquire_time = Clock::now();

    return buffer;
}

void FrameBufferPool::release(FrameBufferPool::FrameBuffer& buffer) {
    if (buffer.data) {
        pool_.deallocate(buffer.data);
        buffer.data = nullptr;
        buffer.size = 0;
        buffer.used = 0;
    }
}

} // namespace aam::core
