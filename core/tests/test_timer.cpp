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
// @file test_timer.cpp
// @brief Unit tests for high-resolution timer
// @version 0.1.0-alpha.3
// =============================================================================

#include <aam/core/timer.hpp>
#include <gtest/gtest.h>
#include <thread>

using namespace aam::core;

/**
 * @brief Test fixture for Timer tests
 */
class TimerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        timer_ = std::make_unique<Timer>();
    }

    void TearDown() override
    {
        timer_.reset();
    }

    std::unique_ptr<Timer> timer_;
};

/**
 * @brief Test timer basic functionality
 */
TEST_F(TimerTest, BasicTiming)
{
    timer_->start();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto elapsed_ms = timer_->elapsed_ms();
    auto elapsed_us = timer_->elapsed_us();

    // Should be at least 50ms (with some tolerance)
    EXPECT_GE(elapsed_ms, 45);
    EXPECT_GE(elapsed_us, 45000);
}

/**
 * @brief Test timer reset
 */
TEST_F(TimerTest, TimerReset)
{
    timer_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    timer_->reset();
    auto elapsed = timer_->elapsed_ms();

    // After reset, should be close to 0
    EXPECT_LT(elapsed, 5);
}

/**
 * @brief Test timer stop
 */
TEST_F(TimerTest, TimerStop)
{
    timer_->start();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    [[maybe_unused]] auto stopped  = timer_->stop();
    auto                  elapsed1 = timer_->elapsed_ms();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto elapsed2 = timer_->elapsed_ms();

    // Elapsed time should not increase after stop
    EXPECT_EQ(elapsed1, elapsed2);
}
