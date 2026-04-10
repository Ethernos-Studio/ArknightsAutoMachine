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
// @file capture_backend.cpp
// @author dhjs0000
// @brief L0 捕获后端接口实现
// ==========================================================================

#include "aam/l0/capture_backend.hpp"

#include <array>
#include <string>

namespace aam::l0
{

// ==========================================================================
// 错误类别实现
// ==========================================================================

namespace
{

/**
 * @brief CaptureError 错误类别类
 */
class CaptureErrorCategory : public std::error_category
{
public:
    [[nodiscard]] const char* name() const noexcept override
    {
        return "capture";
    }

    [[nodiscard]] std::string message(int ev) const override
    {
        switch (static_cast<CaptureError>(ev)) {
            case CaptureError::Success:
                return "Success";
            case CaptureError::Unknown:
                return "Unknown error";
            case CaptureError::NotImplemented:
                return "Feature not implemented";
            case CaptureError::InvalidArgument:
                return "Invalid argument";
            case CaptureError::OutOfMemory:
                return "Out of memory";
            case CaptureError::Timeout:
                return "Operation timeout";
            case CaptureError::DeviceNotFound:
                return "Device not found";
            case CaptureError::DeviceDisconnected:
                return "Device disconnected";
            case CaptureError::DeviceBusy:
                return "Device busy";
            case CaptureError::DeviceNotInitialized:
                return "Device not initialized";
            case CaptureError::DeviceError:
                return "Device error";
            case CaptureError::StreamNotStarted:
                return "Stream not started";
            case CaptureError::StreamAlreadyRunning:
                return "Stream already running";
            case CaptureError::StreamInterrupted:
                return "Stream interrupted";
            case CaptureError::StreamBufferOverflow:
                return "Stream buffer overflow";
            case CaptureError::StreamFormatError:
                return "Stream format error";
            case CaptureError::StreamDecodeError:
                return "Stream decode error";
            case CaptureError::InvalidResolution:
                return "Invalid resolution";
            case CaptureError::InvalidFrameRate:
                return "Invalid frame rate";
            case CaptureError::InvalidPixelFormat:
                return "Invalid pixel format";
            case CaptureError::ConfigurationError:
                return "Configuration error";
            case CaptureError::PermissionDenied:
                return "Permission denied";
            case CaptureError::ADBNotAuthorized:
                return "ADB not authorized";
            case CaptureError::ADBCommandFailed:
                return "ADB command failed";
            default:
                return "Unknown capture error";
        }
    }

    [[nodiscard]] std::error_condition default_error_condition(int ev) const noexcept override
    {
        switch (static_cast<CaptureError>(ev)) {
            case CaptureError::Success:
                return std::errc{};
            case CaptureError::InvalidArgument:
            case CaptureError::InvalidResolution:
            case CaptureError::InvalidFrameRate:
            case CaptureError::InvalidPixelFormat:
            case CaptureError::ConfigurationError:
                return std::errc::invalid_argument;
            case CaptureError::OutOfMemory:
                return std::errc::not_enough_memory;
            case CaptureError::Timeout:
                return std::errc::timed_out;
            case CaptureError::DeviceNotFound:
                return std::errc::no_such_device;
            case CaptureError::PermissionDenied:
            case CaptureError::ADBNotAuthorized:
                return std::errc::permission_denied;
            case CaptureError::DeviceBusy:
                return std::errc::device_or_resource_busy;
            default:
                return std::error_condition(ev, *this);
        }
    }
};

// 全局错误类别实例
const CaptureErrorCategory g_capture_error_category{};

}  // anonymous namespace

const std::error_category& capture_error_category() noexcept
{
    return g_capture_error_category;
}

// ==========================================================================
// 后端工厂实现
// ==========================================================================

std::unique_ptr<ICaptureBackend> CreateCaptureBackend(BackendType type)
{
    // 工厂实现：根据类型创建对应的后端实例
    // 当前支持的后端类型在后续版本中逐步添加
    (void)type;
    // 返回 nullptr 表示请求的后端类型当前不可用
    // 调用方应检查返回值并处理错误
    return nullptr;
}

std::string_view GetBackendTypeName(BackendType type) noexcept
{
    switch (type) {
        case BackendType::ADB:
            return "ADB";
        case BackendType::MAA:
            return "MAA";
        case BackendType::Win32Window:
            return "Win32Window";
        case BackendType::DXGI:
            return "DXGI";
        case BackendType::MediaFoundation:
            return "MediaFoundation";
        case BackendType::V4L2:
            return "V4L2";
        case BackendType::AVFoundation:
            return "AVFoundation";
        case BackendType::Unknown:
        default:
            return "Unknown";
    }
}

// ==========================================================================
// ICaptureBackend 静态方法默认实现
// ==========================================================================

std::expected<std::vector<std::string>, CaptureError> ICaptureBackend::EnumerateDevices()
{
    // 默认实现返回空列表
    return std::vector<std::string>{};
}

std::expected<bool, CaptureError> ICaptureBackend::IsDeviceAvailable(std::string_view device_id)
{
    // 默认实现：枚举所有设备并检查
    auto devices = EnumerateDevices();
    if (!devices) {
        return std::unexpected(devices.error());
    }

    const std::string id(device_id);
    for (const auto& dev : *devices) {
        if (dev == id) {
            return true;
        }
    }
    return false;
}

}  // namespace aam::l0
