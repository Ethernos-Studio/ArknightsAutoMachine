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
// @file list_devices.cpp
// @brief 设备列表命令实现
// @version 0.2.0-alpha.2
// ==========================================================================

#include "list_devices.hpp"

#include "../../core/src/l0_sensing/adb_capture.hpp"
#ifdef AAM_PLATFORM_WINDOWS
#    include "../../core/src/l0_sensing/win32_capture.hpp"
#endif

#include <cstring>
#include <iostream>
#include <vector>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

// MaaFramework 头文件
#ifdef AAM_MAAFW_ENABLED
#    include <MaaFramework/MaaAPI.h>
#    include <MaaToolkit/MaaToolkitAPI.h>
#endif

namespace aam::tools
{

// 引入 l0 命名空间
using namespace aam::l0;

#ifdef AAM_MAAFW_ENABLED
/**
 * @brief 格式化 ADB 截图方法为可读字符串
 */
std::string FormatAdbScreencapMethod(MaaAdbScreencapMethod methods)
{
    if (methods == MaaAdbScreencapMethod_None) {
        return "None";
    }

    std::vector<std::string> method_names;
    if (methods & MaaAdbScreencapMethod_RawByNetcat) {
        method_names.push_back("RawByNetcat");
    }
    if (methods & MaaAdbScreencapMethod_RawWithGzip) {
        method_names.push_back("RawWithGzip");
    }
    if (methods & MaaAdbScreencapMethod_Encode) {
        method_names.push_back("Encode");
    }
    if (methods & MaaAdbScreencapMethod_EncodeToFileAndPull) {
        method_names.push_back("EncodeToFile");
    }
    if (methods & MaaAdbScreencapMethod_MinicapDirect) {
        method_names.push_back("MinicapDirect");
    }
    if (methods & MaaAdbScreencapMethod_MinicapStream) {
        method_names.push_back("MinicapStream");
    }

    return fmt::format("{}", fmt::join(method_names, ", "));
}

/**
 * @brief 格式化 ADB 输入方法为可读字符串
 */
std::string FormatAdbInputMethod(MaaAdbInputMethod methods)
{
    if (methods == MaaAdbInputMethod_None) {
        return "None";
    }

    std::vector<std::string> method_names;
    if (methods & MaaAdbInputMethod_AdbShell) {
        method_names.push_back("AdbShell");
    }
    if (methods & MaaAdbInputMethod_MinitouchAndAdbKey) {
        method_names.push_back("MinitouchAndAdbKey");
    }
    if (methods & MaaAdbInputMethod_Maatouch) {
        method_names.push_back("Maatouch");
    }

    return fmt::format("{}", fmt::join(method_names, ", "));
}

/**
 * @brief 使用 MaaToolkit 枚举 ADB 设备
 */
void ListMaaAdbDevices(bool verbose)
{
    std::cout << fmt::format(fg(fmt::color::yellow), "[MAA ADB Devices]\n");

    auto* device_list = MaaToolkitAdbDeviceListCreate();
    if (!device_list) {
        std::cout << fmt::format(fg(fmt::color::red), "  Error: Failed to create device list\n");
        return;
    }

    if (!MaaToolkitAdbDeviceFind(device_list)) {
        std::cout << fmt::format(fg(fmt::color::gray), "  No MAA ADB devices found\n");
        MaaToolkitAdbDeviceListDestroy(device_list);
        return;
    }

    MaaSize device_count = MaaToolkitAdbDeviceListSize(device_list);
    if (device_count == 0) {
        std::cout << fmt::format(fg(fmt::color::gray), "  No MAA ADB devices found\n");
        MaaToolkitAdbDeviceListDestroy(device_list);
        return;
    }

    for (MaaSize i = 0; i < device_count; ++i) {
        const auto* device = MaaToolkitAdbDeviceListAt(device_list, i);
        if (!device)
            continue;

        const char* name     = MaaToolkitAdbDeviceGetName(device);
        const char* adb_path = MaaToolkitAdbDeviceGetAdbPath(device);
        const char* address  = MaaToolkitAdbDeviceGetAddress(device);

        std::cout << fmt::format("  [{}] {}\n", i, name ? name : "Unknown");
        std::cout << fmt::format("    Address: {}\n", address ? address : "N/A");

        if (verbose) {
            std::cout << fmt::format("    ADB Path: {}\n", adb_path ? adb_path : "N/A");

            MaaAdbScreencapMethod screencap_methods =
                MaaToolkitAdbDeviceGetScreencapMethods(device);
            std::cout << fmt::format("    Screencap: {}\n",
                                     FormatAdbScreencapMethod(screencap_methods));

            MaaAdbInputMethod input_methods = MaaToolkitAdbDeviceGetInputMethods(device);
            std::cout << fmt::format("    Input: {}\n", FormatAdbInputMethod(input_methods));

            const char* config = MaaToolkitAdbDeviceGetConfig(device);
            if (config && strlen(config) > 0) {
                std::cout << fmt::format("    Config: {}\n", config);
            }
        }
    }

    MaaToolkitAdbDeviceListDestroy(device_list);
    std::cout << "\n";
}

/**
 * @brief 使用 MaaToolkit 枚举桌面窗口
 */
void ListMaaDesktopWindows(bool verbose)
{
    std::cout << fmt::format(fg(fmt::color::yellow), "[MAA Desktop Windows]\n");

    auto* window_list = MaaToolkitDesktopWindowListCreate();
    if (!window_list) {
        std::cout << fmt::format(fg(fmt::color::red), "  Error: Failed to create window list\n");
        return;
    }

    if (!MaaToolkitDesktopWindowFindAll(window_list)) {
        std::cout << fmt::format(fg(fmt::color::gray), "  No MAA desktop windows found\n");
        MaaToolkitDesktopWindowListDestroy(window_list);
        return;
    }

    MaaSize window_count = MaaToolkitDesktopWindowListSize(window_list);
    if (window_count == 0) {
        std::cout << fmt::format(fg(fmt::color::gray), "  No MAA desktop windows found\n");
        MaaToolkitDesktopWindowListDestroy(window_list);
        return;
    }

    int valid_count = 0;
    for (MaaSize i = 0; i < window_count; ++i) {
        const auto* window = MaaToolkitDesktopWindowListAt(window_list, i);
        if (!window)
            continue;

        const char* window_name = MaaToolkitDesktopWindowGetWindowName(window);
        const char* class_name  = MaaToolkitDesktopWindowGetClassName(window);
        void*       handle      = MaaToolkitDesktopWindowGetHandle(window);

        // 过滤掉无效窗口
        if (!window_name || strlen(window_name) == 0) {
            continue;
        }

        std::cout << fmt::format("  [{}] {}\n", valid_count++, window_name);

        if (verbose) {
            std::cout << fmt::format("    Class: {}\n", class_name ? class_name : "N/A");
            std::cout << fmt::format("    Handle: {}\n", handle);
        }
    }

    if (valid_count == 0) {
        std::cout << fmt::format(fg(fmt::color::gray), "  No valid desktop windows found\n");
    }

    MaaToolkitDesktopWindowListDestroy(window_list);
    std::cout << "\n";
}
#endif  // AAM_MAAFW_ENABLED

int ExecuteListDevices(bool verbose)
{
    std::cout << fmt::format(fg(fmt::color::cyan), "=== AAM Capture Backend Device List ===\n\n");

    // ADB 设备列表（原生实现）
    std::cout << fmt::format(fg(fmt::color::yellow), "[Native ADB Devices]\n");
    auto adb_devices = AdbCaptureBackend::EnumerateDevices();
    if (adb_devices && !adb_devices->empty()) {
        for (const auto& device : *adb_devices) {
            std::cout << fmt::format("  - {}\n", device);

            if (verbose) {
                // 获取设备详细信息
                auto detail_result =
                    AdbCaptureBackend::ExecuteAdbCommand(device, "shell getprop ro.product.model");
                if (detail_result) {
                    std::cout << fmt::format("    Model: {}\n",
                                             detail_result->empty() ? "Unknown" : *detail_result);
                }

                auto android_version = AdbCaptureBackend::ExecuteAdbCommand(
                    device, "shell getprop ro.build.version.release");
                if (android_version) {
                    std::cout << fmt::format("    Android: {}\n",
                                             android_version->empty() ? "Unknown"
                                                                      : *android_version);
                }
            }
        }
    }
    else {
        std::cout << fmt::format(fg(fmt::color::gray), "  No ADB devices found\n");
        if (!adb_devices) {
            std::cout << fmt::format(fg(fmt::color::red),
                                     "  Error: Failed to enumerate ADB devices\n");
        }
    }
    std::cout << "\n";

#ifdef AAM_PLATFORM_WINDOWS
    // Win32 窗口列表（原生实现）
    std::cout << fmt::format(fg(fmt::color::yellow), "[Native Win32 Windows]\n");
    auto windows = Win32CaptureBackend::EnumerateWindows();
    if (windows && !windows->empty()) {
        int count = 0;
        for (const auto& window : *windows) {
            // 只显示可见且足够大的窗口
            if (window.is_visible && !window.is_minimized && window.width >= 320
                && window.height >= 240) {
                std::cout << fmt::format("  [{}] {} ({}x{})\n",
                                         count++,
                                         window.title.empty() ? "[No Title]" : window.title,
                                         window.width,
                                         window.height);

                if (verbose) {
                    std::cout << fmt::format("    Class: {}\n", window.class_name);
                    std::cout << fmt::format("    Process: {}\n", window.process_name);
                    std::cout << fmt::format("    HWND: {}\n", fmt::ptr(window.hwnd));
                }
            }
        }

        if (count == 0) {
            std::cout << fmt::format(fg(fmt::color::gray), "  No suitable windows found\n");
        }
    }
    else {
        std::cout << fmt::format(fg(fmt::color::gray), "  No windows found\n");
    }
    std::cout << "\n";
#endif

    // MaaFramework 设备列表
#ifdef AAM_MAAFW_ENABLED
    std::cout << fmt::format(fg(fmt::color::yellow), "[MAA Framework Devices]\n");

    // 初始化 MaaToolkit
    std::cout << fmt::format("  Initializing MaaToolkit...\n");

    // 使用当前工作目录作为用户路径，空 JSON 作为默认配置
    std::string user_path      = ".";
    std::string default_config = "{}";

    if (MaaToolkitConfigInitOption(user_path.c_str(), default_config.c_str())) {
        std::cout << fmt::format("  MaaToolkit initialized successfully\n");
        ListMaaAdbDevices(verbose);
        ListMaaDesktopWindows(verbose);
        // Note: MaaToolkit 没有提供 Uninit 函数
    }
    else {
        std::cout << fmt::format(fg(fmt::color::red), "  Error: Failed to initialize MaaToolkit\n");
    }
#else
    std::cout << fmt::format(fg(fmt::color::yellow), "[MAA Framework Devices]\n");
    std::cout << fmt::format(fg(fmt::color::gray), "  MAA Framework support is not enabled\n");
    std::cout << fmt::format("  Build with -DAAM_MAAFW_ENABLED=ON to enable MAA support\n");
#endif

    std::cout << "\n";
    std::cout << fmt::format(fg(fmt::color::green), "=== End of Device List ===\n");

    return 0;
}

}  // namespace aam::tools
