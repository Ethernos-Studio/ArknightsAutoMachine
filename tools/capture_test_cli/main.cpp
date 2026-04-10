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
// @file main.cpp
// @brief 捕获测试 CLI 入口点
// @version 0.2.0-alpha.2
// ==========================================================================

#include <iostream>
#include <stdexcept>

#include <fmt/color.h>
#include <fmt/format.h>

#include "cli_parser.hpp"
#include "commands/benchmark.hpp"
#include "commands/capture_test.hpp"
#include "commands/compare.hpp"
#include "commands/list_devices.hpp"

/**
 * @brief 主函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return int 返回码
 */
int main(int argc, char* argv[])
{
    try {
        // 解析命令行参数
        aam::tools::CliParser parser(argc, argv);
        auto                  args = parser.Parse();

        // 显示帮助
        if (args.show_help) {
            std::cout << aam::tools::CliParser::GetHelpString() << std::endl;
            return 0;
        }

        // 显示版本
        if (args.show_version) {
            std::cout << aam::tools::CliParser::GetVersionString() << std::endl;
            return 0;
        }

        // 执行对应命令
        switch (args.mode) {
            case aam::tools::TestMode::ListDevices:
                return aam::tools::ExecuteListDevices(args.capture_config.verbose);

            case aam::tools::TestMode::Capture:
                return aam::tools::ExecuteCaptureTest(args.capture_config);

            case aam::tools::TestMode::Benchmark:
                return aam::tools::ExecuteBenchmark(args.benchmark_config);

            case aam::tools::TestMode::Compare:
                return aam::tools::ExecuteCompare(args.compare_config);

            case aam::tools::TestMode::Help:
            default:
                std::cout << aam::tools::CliParser::GetHelpString() << std::endl;
                return 0;
        }
    }
    catch (const std::exception& e) {
        fmt::print(fg(fmt::color::red), "Error: {}\n", e.what());
        std::cerr << "\nUse -h or --help for usage information.\n";
        return 1;
    }

    return 0;
}
