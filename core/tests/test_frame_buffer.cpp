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
// @file test_frame_buffer.cpp
// @brief Unit tests for frame buffer
// @version 0.1.0-alpha.3
// =============================================================================

#include <aam/l0/frame_buffer.hpp>
#include <gtest/gtest.h>

using namespace aam::l0;

/**
 * @brief Test fixture for FrameBuffer tests
 */
class FrameBufferTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // LockFreeFrameBuffer is a template, need to specify type and capacity
        // Using int as element type and 64 as capacity (must be power of 2)
        buffer_ = std::make_unique<LockFreeFrameBuffer<int, 64>>();
    }

    void TearDown() override
    {
        buffer_.reset();
    }

    std::unique_ptr<LockFreeFrameBuffer<int, 64>> buffer_;
};

/**
 * @brief Test frame buffer initialization
 */
TEST_F(FrameBufferTest, Initialization)
{
    EXPECT_EQ(buffer_->capacity, 64);
    EXPECT_TRUE(buffer_->empty());
    EXPECT_FALSE(buffer_->full());
}

/**
 * @brief Test push and pop operations
 */
TEST_F(FrameBufferTest, PushAndPop)
{
    // Push some elements
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(buffer_->push(i));
    }

    EXPECT_EQ(buffer_->size(), 10);
    EXPECT_FALSE(buffer_->empty());

    // Pop elements
    for (int i = 0; i < 10; ++i) {
        auto result = buffer_->pop();
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), i);
    }

    EXPECT_TRUE(buffer_->empty());
}

/**
 * @brief Test buffer full condition
 */
TEST_F(FrameBufferTest, BufferFull)
{
    // Fill the buffer
    for (int i = 0; i < 64; ++i) {
        EXPECT_TRUE(buffer_->push(i));
    }

    EXPECT_TRUE(buffer_->full());
    EXPECT_EQ(buffer_->size(), 64);

    // Try to push when full (should fail or drop based on policy)
    // With default DropOldest policy, it should succeed but drop oldest
    [[maybe_unused]] bool push_result = buffer_->push(999);
    // Result depends on policy, just verify it doesn't crash
}

/**
 * @brief Test buffer statistics
 */
TEST_F(FrameBufferTest, BufferStatistics)
{
    BufferStats stats = buffer_->stats();
    EXPECT_EQ(stats.total_pushed, 0);
    EXPECT_EQ(stats.total_popped, 0);

    [[maybe_unused]] auto push1 = buffer_->push(1);
    [[maybe_unused]] auto push2 = buffer_->push(2);
    [[maybe_unused]] auto pop1  = buffer_->pop();

    stats = buffer_->stats();
    EXPECT_EQ(stats.total_pushed, 2);
    EXPECT_EQ(stats.total_popped, 1);
}

/**
 * @brief Test clear operation
 */
TEST_F(FrameBufferTest, ClearBuffer)
{
    for (int i = 0; i < 10; ++i) {
        [[maybe_unused]] auto result = buffer_->push(i);
    }

    EXPECT_EQ(buffer_->size(), 10);

    buffer_->clear();

    EXPECT_TRUE(buffer_->empty());
    EXPECT_EQ(buffer_->size(), 0);
}
