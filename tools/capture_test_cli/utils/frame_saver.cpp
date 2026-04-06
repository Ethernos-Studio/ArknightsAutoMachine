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
// @file frame_saver.cpp
// @brief 帧保存工具实现
// @version 0.2.0-alpha.2
// ==========================================================================

#include "frame_saver.hpp"

#include <fstream>

#include <fmt/format.h>
#include <opencv2/opencv.hpp>

namespace aam::tools
{

FrameSaver::FrameSaver(const std::string& output_dir)
{
    output_dir_ = std::filesystem::path(output_dir);

    try {
        // 创建输出目录
        std::filesystem::create_directories(output_dir_);
        valid_ = std::filesystem::exists(output_dir_) && std::filesystem::is_directory(output_dir_);
    }
    catch (const std::exception&) {
        valid_ = false;
    }
}

[[nodiscard]] bool FrameSaver::IsValid() const noexcept
{
    return valid_;
}

[[nodiscard]] bool FrameSaver::SaveFrame(const l0::FrameMetadata&      metadata,
                                         const std::vector<std::byte>& pixel_data,
                                         std::string_view              format)
{
    if (!valid_) {
        return false;
    }

    // 确定 OpenCV 格式
    int cv_type;
    int channels;

    if (format == "rgba" || format == "bgra") {
        cv_type  = CV_8UC4;
        channels = 4;
    }
    else if (format == "rgb" || format == "bgr") {
        cv_type  = CV_8UC3;
        channels = 3;
    }
    else if (format == "gray") {
        cv_type  = CV_8UC1;
        channels = 1;
    }
    else {
        // 默认 RGBA
        cv_type  = CV_8UC4;
        channels = 4;
    }

    // 创建 OpenCV Mat
    cv::Mat frame(
        metadata.height, metadata.width, cv_type, const_cast<std::byte*>(pixel_data.data()));

    // 转换为 BGR 以便保存为 PNG/JPEG
    cv::Mat save_frame;
    if (format == "rgba") {
        cv::cvtColor(frame, save_frame, cv::COLOR_RGBA2BGR);
    }
    else if (format == "bgra") {
        cv::cvtColor(frame, save_frame, cv::COLOR_BGRA2BGR);
    }
    else if (format == "rgb") {
        cv::cvtColor(frame, save_frame, cv::COLOR_RGB2BGR);
    }
    else {
        save_frame = frame.clone();
    }

    // 生成文件名
    std::string filename =
        fmt::format("frame_{:06d}_{}x{}.png", saved_count_, metadata.width, metadata.height);
    std::filesystem::path filepath = output_dir_ / filename;

    // 保存图像
    bool success = cv::imwrite(filepath.string(), save_frame);

    if (success) {
        saved_count_++;
    }

    return success;
}

[[nodiscard]] uint64_t FrameSaver::GetSavedCount() const noexcept
{
    return saved_count_;
}

[[nodiscard]] std::string FrameSaver::GetOutputDirectory() const
{
    return output_dir_.string();
}

}  // namespace aam::tools
