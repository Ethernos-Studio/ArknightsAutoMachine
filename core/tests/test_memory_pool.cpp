// =============================================================================
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
// =============================================================================
// @file test_memory_pool.cpp
// @brief Unit tests for memory pool
// @version 0.1.0-alpha.3
// =============================================================================

#include <aam/core/memory_pool.hpp>
#include <gtest/gtest.h>

using namespace aam::core;

/**
 * @brief Test fixture for MemoryPool tests
 */
class MemoryPoolTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        MemoryPoolConfig config;
        config.block_size     = 256;
        config.initial_blocks = 1024;
        config.max_blocks     = 65536;
        config.allow_growth   = true;
        pool_                 = std::make_unique<FixedMemoryPool>(config);
    }

    void TearDown() override
    {
        pool_.reset();
    }

    std::unique_ptr<FixedMemoryPool> pool_;
};

/**
 * @brief Test basic allocation and deallocation
 */
TEST_F(MemoryPoolTest, BasicAllocation)
{
    void* ptr = pool_->allocate();
    ASSERT_NE(ptr, nullptr);

    // Write to memory
    std::memset(ptr, 0xAB, 256);

    // Verify data
    auto* bytes = static_cast<std::uint8_t*>(ptr);
    for (size_t i = 0; i < 256; ++i) {
        EXPECT_EQ(bytes[i], 0xAB);
    }

    pool_->deallocate(ptr);
}

/**
 * @brief Test multiple allocations
 */
TEST_F(MemoryPoolTest, MultipleAllocations)
{
    constexpr size_t num_allocations = 100;

    std::vector<void*> ptrs;
    ptrs.reserve(num_allocations);

    // Allocate
    for (size_t i = 0; i < num_allocations; ++i) {
        void* ptr = pool_->allocate();
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    // Deallocate
    for (void* ptr : ptrs) {
        pool_->deallocate(ptr);
    }
}

/**
 * @brief Test pool statistics
 */
TEST_F(MemoryPoolTest, PoolStatistics)
{
    auto initial_free = pool_->free_blocks();
    EXPECT_GT(initial_free, 0);

    void* ptr = pool_->allocate();
    ASSERT_NE(ptr, nullptr);

    EXPECT_EQ(pool_->used_blocks(), 1);
    EXPECT_EQ(pool_->free_blocks(), initial_free - 1);

    pool_->deallocate(ptr);

    EXPECT_EQ(pool_->used_blocks(), 0);
    EXPECT_EQ(pool_->free_blocks(), initial_free);
}

/**
 * @brief Test block size
 */
TEST_F(MemoryPoolTest, BlockSize)
{
    EXPECT_EQ(pool_->block_size(), 256);
}
