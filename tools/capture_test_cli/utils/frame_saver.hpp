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
// @file frame_saver.hpp
// @brief 帧保存工具
// @version 0.2.0-alpha.2
// ==========================================================================

#pragma once

#include "aam/l0/capture_backend.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aam::tools {

/**
 * @brief 帧保存器
 */
class FrameSaver {
public:
    /**
     * @brief 构造函数
     * @param output_dir 输出目录路径
     */
    explicit FrameSaver(const std::string& output_dir);
    
    /**
     * @brief 检查保存器是否有效
     * @return bool 是否有效
     */
    [[nodiscard]] bool IsValid() const noexcept;
    
    /**
     * @brief 保存帧到文件
     * @param metadata 帧元数据
     * @param pixel_data 像素数据
     * @param format 像素格式字符串
     * @return bool 是否成功
     */
    [[nodiscard]] bool SaveFrame(const l0::FrameMetadata& metadata,
                                 const std::vector<std::byte>& pixel_data,
                                 std::string_view format);
    
    /**
     * @brief 获取保存的帧数量
     * @return uint64_t 帧数量
     */
    [[nodiscard]] uint64_t GetSavedCount() const noexcept;
    
    /**
     * @brief 获取输出目录路径
     * @return std::string 路径
     */
    [[nodiscard]] std::string GetOutputDirectory() const;

private:
    std::filesystem::path output_dir_;
    uint64_t saved_count_{0};
    bool valid_{false};
};

} // namespace aam::tools
