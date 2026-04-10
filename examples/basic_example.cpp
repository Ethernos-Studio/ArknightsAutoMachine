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
// @file basic_example.cpp
// @brief Basic usage example for AAM core library
// @version 0.1.0-alpha.3
// =============================================================================

#include <aam/core/logger.hpp>
#include <aam/core/timer.hpp>

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>

/**
 * @brief Basic example demonstrating AAM core functionality
 */
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    std::cout << "Arknights Auto Machine (AAM) - Basic Example\n";
    std::cout << "============================================\n\n";

    // Initialize logger
    aam::core::LoggerConfig config;
    config.level = aam::core::LogLevel::Info;
    config.enable_console = true;
    aam::core::LoggerManager::initialize(config);

    auto logger = aam::core::LoggerManager::get_logger("basic_example");
    logger.info("Starting AAM basic example");

    // Example: Timer usage
    {
        aam::core::Timer timer;
        timer.start();

        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto elapsed = timer.elapsed_ms();
        logger.info_fmt("Timer example: {} ms elapsed", elapsed);
    }

    logger.info("Basic example completed successfully");

    // Shutdown logger
    aam::core::LoggerManager::shutdown();

    return 0;
}
