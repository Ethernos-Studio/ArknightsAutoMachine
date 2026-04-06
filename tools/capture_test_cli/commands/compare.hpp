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
// @file compare.hpp
// @brief 后端对比测试命令
// @version 0.2.0-alpha.2
// ==========================================================================

#pragma once

#include "../cli_parser.hpp"

namespace aam::tools {

/**
 * @brief 执行后端对比测试命令
 * @param config 对比测试配置
 * @return int 返回码，0 表示成功
 */
int ExecuteCompare(const CompareConfig& config);

} // namespace aam::tools
