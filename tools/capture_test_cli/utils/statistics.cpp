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
// @file statistics.cpp
// @brief 捕获统计工具实现
// @version 0.2.0-alpha.2
// ==========================================================================

#include "statistics.hpp"

#include <algorithm>
#include <numeric>

namespace aam::tools {

CaptureStatistics::CaptureStatistics() {
    Reset();
}

void CaptureStatistics::AddFrame([[maybe_unused]] const l0::FrameMetadata& metadata, size_t data_size) {
    auto now = std::chrono::steady_clock::now();
    
    if (total_frames_ == 0) {
        start_time_ = now;
        last_frame_time_ = now;
        min_fps_ = 0.0;
        max_fps_ = 0.0;
    } else {
        // 计算瞬时 FPS
        auto delta_us = std::chrono::duration_cast<std::chrono::microseconds>(
            now - last_frame_time_).count();
        
        if (delta_us > 0) {
            current_fps_ = 1000000.0 / delta_us;
            
            // 更新最小/最大 FPS
            if (min_fps_ == 0.0 || current_fps_ < min_fps_) {
                min_fps_ = current_fps_;
            }
            if (current_fps_ > max_fps_) {
                max_fps_ = current_fps_;
            }
            
            // 添加到历史记录
            fps_history_.push_back(current_fps_);
            if (fps_history_.size() > kFpsHistorySize) {
                fps_history_.erase(fps_history_.begin());
            }
        }
    }
    
    last_frame_time_ = now;
    total_frames_++;
    total_bytes_ += data_size;
}

[[nodiscard]] double CaptureStatistics::GetCurrentFps() const {
    if (fps_history_.empty()) {
        return 0.0;
    }
    
    // 返回移动平均值
    double sum = std::accumulate(fps_history_.begin(), fps_history_.end(), 0.0);
    return sum / fps_history_.size();
}

[[nodiscard]] CaptureStats CaptureStatistics::GetStats() const {
    CaptureStats stats;
    
    stats.total_frames = total_frames_;
    
    auto duration = std::chrono::steady_clock::now() - start_time_;
    stats.duration_sec = std::chrono::duration<double>(duration).count();
    
    if (stats.duration_sec > 0) {
        stats.avg_fps = total_frames_ / stats.duration_sec;
    }
    
    stats.min_fps = min_fps_;
    stats.max_fps = max_fps_;
    stats.total_mb = total_bytes_ / (1024.0 * 1024.0);
    
    return stats;
}

void CaptureStatistics::Reset() {
    start_time_ = std::chrono::steady_clock::now();
    last_frame_time_ = start_time_;
    total_frames_ = 0;
    total_bytes_ = 0;
    min_fps_ = 0.0;
    max_fps_ = 0.0;
    current_fps_ = 0.0;
    fps_history_.clear();
}

} // namespace aam::tools
