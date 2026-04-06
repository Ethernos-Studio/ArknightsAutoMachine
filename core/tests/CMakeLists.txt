# =============================================================================
# Copyright (C) 2026 Ethernos Studio
# This file is part of Arknights Auto Machine (AAM).
#
# AAM is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# AAM is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with AAM. If not, see <https://www.gnu.org/licenses/>.
# =============================================================================
# @file CMakeLists.txt
# @brief Core module test suite configuration
# @version 0.1.0-alpha.3
# =============================================================================

# 查找 GTest
find_package(GTest CONFIG REQUIRED)

# 测试可执行文件
set(CORE_TEST_SOURCES
    test_main.cpp
    test_memory_pool.cpp
    test_timer.cpp
    test_frame_buffer.cpp
)

# 创建测试可执行文件
add_executable(aam_core_tests ${CORE_TEST_SOURCES})

# 链接依赖
target_link_libraries(aam_core_tests
    PRIVATE
        aam_core
        GTest::gtest
        GTest::gtest_main
)

# 包含目录
target_include_directories(aam_core_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
)

# 添加测试
add_test(NAME CoreTests COMMAND aam_core_tests)

# 设置测试属性
set_target_properties(aam_core_tests PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
