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
# @brief Examples configuration
# @version 0.1.0-alpha.3
# =============================================================================

# 示例可执行文件
add_executable(aam_example_basic
    basic_example.cpp
)

target_link_libraries(aam_example_basic
    PRIVATE
        aam_core
)

target_include_directories(aam_example_basic
    PRIVATE
        ${CMAKE_SOURCE_DIR}/core/include
)

set_target_properties(aam_example_basic PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
