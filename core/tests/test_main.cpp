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
// @file test_main.cpp
// @brief Entry point for core module tests
// @version 0.1.0-alpha.3
// =============================================================================

#include <aam/core/logger.hpp>
#include <gtest/gtest.h>

/**
 * @brief Global test environment for AAM tests
 */
class AAMTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        // Initialize logger for tests
        aam::core::LoggerConfig config;
        config.level          = aam::core::LogLevel::Warning;
        config.enable_console = true;
        aam::core::LoggerManager::initialize(config);
    }

    void TearDown() override
    {
        // Cleanup
        aam::core::LoggerManager::shutdown();
    }
};

/**
 * @brief Main entry point for tests
 */
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new AAMTestEnvironment);
    return RUN_ALL_TESTS();
}
