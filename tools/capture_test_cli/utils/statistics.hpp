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
// @file statistics.hpp
// @brief 捕获统计工具
// @version 0.2.0-alpha.2
// ==========================================================================

#pragma once

#include "aam/l0/capture_backend.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aam::tools {

/**
 * @brief 捕获统计信息
 */
struct CaptureStats {
    uint64_t total_frames{0};       ///< 总帧数
    double duration_sec{0.0};       ///< 持续时间（秒）
    double avg_fps{0.0};            ///< 平均 FPS
    double min_fps{0.0};            ///< 最小 FPS
    double max_fps{0.0};            ///< 最大 FPS
    double total_mb{0.0};           ///< 总数据量（MB）
};

/**
 * @brief 捕获统计收集器
 */
class CaptureStatistics {
public:
    /**
     * @brief 构造函数
     */
    CaptureStatistics();
    
    /**
     * @brief 添加帧数据
     * @param metadata 帧元数据
     * @param data_size 数据大小（字节）
     */
    void AddFrame(const l0::FrameMetadata& metadata, size_t data_size);
    
    /**
     * @brief 获取当前 FPS
     * @return double 当前 FPS
     */
    [[nodiscard]] double GetCurrentFps() const;
    
    /**
     * @brief 获取完整统计信息
     * @return CaptureStats 统计信息
     */
    [[nodiscard]] CaptureStats GetStats() const;
    
    /**
     * @brief 重置统计
     */
    void Reset();

private:
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_frame_time_;
    
    uint64_t total_frames_{0};
    uint64_t total_bytes_{0};
    
    double min_fps_{0.0};
    double max_fps_{0.0};
    double current_fps_{0.0};
    
    // 用于计算移动平均
    std::vector<double> fps_history_;
    static constexpr size_t kFpsHistorySize = 30;
};

} // namespace aam::tools
