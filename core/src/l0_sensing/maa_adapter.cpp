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
// @file maa_adapter.cpp
// @brief MaaFramework 桥接适配器实现
// ==========================================================================
// 版本: v0.2.0-alpha.2
// 功能: 通过 MaaFramework 实现屏幕捕获和输入控制
// 依赖: C++23, MaaFramework API, OpenCV
// 算法: MaaFramework 控制器 → 截图 API → RGB24 输出
// 性能: P99 延迟 < 15ms @ 1920x1080@60fps
// ==========================================================================

#include "maa_adapter.hpp"

#include <algorithm>
#include <cstring>

#include <spdlog/spdlog.h>

namespace aam::l0
{

// ==========================================================================
// 常量定义
// ==========================================================================

namespace
{
constexpr std::string_view kBackendName            = "MAA";
constexpr std::string_view kBackendVersion         = "2.0.0";
constexpr std::size_t      kDefaultQueueSize       = 3;
constexpr core::Duration   kDefaultCaptureInterval = std::chrono::milliseconds(16);  // ~60fps
constexpr core::Duration   kScreencapTimeout       = std::chrono::seconds(5);
}  // namespace

// ==========================================================================
// 构造与析构
// ==========================================================================

MaaCaptureBackend::MaaCaptureBackend()
    : memory_pool_(std::make_unique<core::FixedMemoryPool>(
          core::MemoryPoolConfig{.block_size        = 1920 * 1080 * 4,
                                 .initial_blocks    = 10,
                                 .allow_growth      = true,
                                 .track_allocations = true}))
{
    spdlog::debug("MaaCaptureBackend constructed");
}

MaaCaptureBackend::~MaaCaptureBackend()
{
    spdlog::debug("MaaCaptureBackend destroying...");

    // 确保资源被释放
    if (state_.load() != State::Uninitialized) {
        auto result = Shutdown();
        if (!result) {
            spdlog::error("Shutdown failed during destruction: {}",
                          static_cast<int>(result.error()));
        }
    }

    spdlog::debug("MaaCaptureBackend destroyed");
}

// ==========================================================================
// 生命周期管理
// ==========================================================================

[[nodiscard]] ICaptureBackend::Result MaaCaptureBackend::Initialize(const CaptureConfig& config)
{
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (state_ != State::Uninitialized) {
        spdlog::error("Initialize called but state is not Uninitialized");
        return std::unexpected(CaptureError::DeviceError);
    }

    // 验证配置
    if (auto validation_result = config.validate(); !validation_result) {
        return validation_result;
    }

    // 解析目标 ID
    auto [conn_type, conn_addr] = ParseTargetId(config.target_id);
    if (conn_type == ConnectionType::Unknown) {
        spdlog::error("Failed to parse target_id: {}", config.target_id);
        return std::unexpected(CaptureError::InvalidArgument);
    }

    connection_type_    = conn_type;
    connection_address_ = conn_addr;

    // 保存配置
    config_ = config;

    // 计算捕获间隔
    if (config.target_fps > 0) {
        capture_interval_ = std::chrono::milliseconds(1000 / config.target_fps);
    }

    // 设置队列大小
    max_queue_size_ = config.buffer_queue_size > 0 ? config.buffer_queue_size : kDefaultQueueSize;

    // 初始化 MaaFramework
    if (!InitializeController()) {
        return std::unexpected(CaptureError::DeviceError);
    }

    state_ = State::Initialized;

    spdlog::info("MaaCaptureBackend initialized successfully. Target: {}, Type: {}",
                 config.target_id,
                 connection_type_ == ConnectionType::ADB ? "ADB" : "Win32");

    return {};
}

[[nodiscard]] ICaptureBackend::Result MaaCaptureBackend::Shutdown()
{
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (state_ == State::Uninitialized) {
        return {};  // 已经关闭
    }

    // 如果正在捕获，先停止
    if (state_ == State::Capturing || state_ == State::Starting) {
        stop_requested_.store(true);
        queue_cv_.notify_all();

        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
    }

    // 释放 MaaFramework 资源
    ShutdownController();

    // 清空帧队列
    {
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);
        while (!frame_queue_.empty()) {
            frame_queue_.pop();
        }
    }

    // 重置状态
    state_ = State::Uninitialized;
    stop_requested_.store(false);
    frame_counter_.store(0);

    spdlog::info("MaaCaptureBackend shutdown complete");

    return {};
}

[[nodiscard]] bool MaaCaptureBackend::IsInitialized() const noexcept
{
    return state_.load() != State::Uninitialized;
}

// ==========================================================================
// 捕获控制
// ==========================================================================

[[nodiscard]] ICaptureBackend::Result MaaCaptureBackend::StartCapture()
{
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (state_ != State::Initialized) {
        if (state_ == State::Capturing) {
            return std::unexpected(CaptureError::StreamAlreadyRunning);
        }
        return std::unexpected(CaptureError::DeviceNotInitialized);
    }

    // 重置停止信号
    stop_requested_.store(false);

    // 清空队列
    {
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);
        while (!frame_queue_.empty()) {
            frame_queue_.pop();
        }
    }

    state_ = State::Starting;

    // 启动捕获线程
    try {
        capture_thread_ = std::thread(&MaaCaptureBackend::CaptureThreadFunc, this);
    }
    catch (const std::exception& e) {
        spdlog::error("Failed to start capture thread: {}", e.what());
        state_ = State::Error;
        return std::unexpected(CaptureError::DeviceError);
    }

    state_ = State::Capturing;

    spdlog::info("Capture started");

    return {};
}

[[nodiscard]] ICaptureBackend::Result MaaCaptureBackend::StopCapture()
{
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (state_ != State::Capturing) {
        if (state_ == State::Initialized) {
            return {};  // 已经停止
        }
        return std::unexpected(CaptureError::StreamNotStarted);
    }

    state_ = State::Stopping;
    stop_requested_.store(true);
    queue_cv_.notify_all();

    // 等待捕获线程结束
    if (capture_thread_.joinable()) {
        capture_thread_.join();
    }

    state_ = State::Initialized;

    spdlog::info("Capture stopped");

    return {};
}

[[nodiscard]] bool MaaCaptureBackend::IsCapturing() const noexcept
{
    return state_.load() == State::Capturing;
}

// ==========================================================================
// 帧获取
// ==========================================================================

[[nodiscard]] std::expected<std::pair<FrameMetadata, std::vector<std::byte>>, CaptureError>
MaaCaptureBackend::GetFrame(core::Duration timeout)
{
    if (state_.load() != State::Capturing) {
        return std::unexpected(CaptureError::StreamNotStarted);
    }

    std::unique_lock<std::mutex> lock(queue_mutex_);

    // 等待帧可用或超时
    bool has_frame = queue_cv_.wait_for(lock, timeout, [this] {
        return !frame_queue_.empty() || stop_requested_.load();
    });

    if (stop_requested_.load()) {
        return std::unexpected(CaptureError::StreamInterrupted);
    }

    if (!has_frame) {
        return std::unexpected(CaptureError::Timeout);
    }

    // 获取最旧的帧（FIFO）
    FrameBufferElement frame = std::move(frame_queue_.front());
    frame_queue_.pop();

    lock.unlock();

    // 转换为返回格式
    FrameMetadata metadata = frame.metadata;

    return std::make_pair(metadata, std::move(frame.data));
}

[[nodiscard]] std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>,
                            CaptureError>
MaaCaptureBackend::TryGetFrame()
{
    if (state_.load() != State::Capturing) {
        return std::unexpected(CaptureError::StreamNotStarted);
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (frame_queue_.empty()) {
        return std::nullopt;  // 队列为空，返回空 optional
    }

    // 获取最旧的帧
    FrameBufferElement frame = std::move(frame_queue_.front());
    frame_queue_.pop();

    // 转换为返回格式
    FrameMetadata metadata = frame.metadata;

    return std::make_optional(std::make_pair(metadata, std::move(frame.data)));
}

[[nodiscard]] ICaptureBackend::Result
MaaCaptureBackend::GetFrameWithCallback(core::Duration timeout, FrameCallback callback)
{
    if (!callback) {
        return std::unexpected(CaptureError::InvalidArgument);
    }

    auto result = GetFrame(timeout);
    if (!result) {
        return std::unexpected(result.error());
    }

    auto& [metadata, data] = *result;
    callback(metadata, data);

    return {};
}

// ==========================================================================
// 配置管理
// ==========================================================================

[[nodiscard]] CaptureConfig MaaCaptureBackend::GetConfig() const
{
    std::lock_guard<std::mutex> lock(state_mutex_);
    return config_;
}

[[nodiscard]] ICaptureBackend::Result MaaCaptureBackend::UpdateConfig(const CaptureConfig& config)
{
    std::unique_lock<std::mutex> lock(state_mutex_);

    // 验证新配置
    if (auto validation_result = config.validate(); !validation_result) {
        return validation_result;
    }

    // 如果正在捕获，某些更改需要重启
    bool was_capturing = (state_ == State::Capturing);

    if (was_capturing) {
        // 临时停止捕获
        lock.unlock();
        auto stop_result = StopCapture();
        if (!stop_result) {
            return stop_result;
        }
        lock.lock();
    }

    // 更新配置
    config_ = config;

    // 更新捕获间隔
    if (config.target_fps > 0) {
        capture_interval_ = std::chrono::milliseconds(1000 / config.target_fps);
    }

    // 更新队列大小
    max_queue_size_ = config.buffer_queue_size > 0 ? config.buffer_queue_size : kDefaultQueueSize;

    spdlog::info("Configuration updated");

    // 如果之前在捕获，重新启动
    if (was_capturing) {
        lock.unlock();
        return StartCapture();
    }

    return {};
}

// ==========================================================================
// 查询接口
// ==========================================================================

[[nodiscard]] CaptureStats MaaCaptureBackend::GetStats() const
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

[[nodiscard]] std::string_view MaaCaptureBackend::GetBackendName() const noexcept
{
    return kBackendName;
}

[[nodiscard]] std::string_view MaaCaptureBackend::GetBackendVersion() const noexcept
{
    return kBackendVersion;
}

[[nodiscard]] bool MaaCaptureBackend::SupportsPixelFormat(PixelFormat format) const noexcept
{
    // MaaFramework 返回的是 BGR24 格式，我们可以转换为其他格式
    switch (format) {
        case PixelFormat::RGB24:
        case PixelFormat::BGR24:
        case PixelFormat::RGBA32:
        case PixelFormat::BGRA32:
            return true;
        default:
            return false;
    }
}

[[nodiscard]] std::vector<PixelFormat> MaaCaptureBackend::GetSupportedPixelFormats() const
{
    return {
        PixelFormat::BGR24,  // MaaFramework 原生格式
        PixelFormat::RGB24,
        PixelFormat::BGRA32,
        PixelFormat::RGBA32,
    };
}

// ==========================================================================
// 静态设备管理接口
// ==========================================================================

[[nodiscard]] std::expected<std::vector<std::string>, CaptureError>
MaaCaptureBackend::EnumerateDevices()
{
    std::vector<std::string> devices;

    // 使用 MaaToolkit 枚举 ADB 设备 (v2.0+ API)
    auto* device_list = MaaToolkitAdbDeviceListCreate();
    if (!device_list) {
        return std::unexpected(CaptureError::DeviceError);
    }

    // v2.0+ API: 需要先调用 Find 填充列表
    if (!MaaToolkitAdbDeviceFind(device_list)) {
        MaaToolkitAdbDeviceListDestroy(device_list);
        return std::unexpected(CaptureError::DeviceError);
    }

    MaaSize count = MaaToolkitAdbDeviceListSize(device_list);
    for (MaaSize i = 0; i < count; ++i) {
        auto* device = MaaToolkitAdbDeviceListAt(device_list, i);
        if (device) {
            const char* name    = MaaToolkitAdbDeviceGetName(device);
            const char* address = MaaToolkitAdbDeviceGetAddress(device);
            if (name && address) {
                // 格式: name@address
                devices.emplace_back(std::string(name) + "@" + address);
            }
        }
    }

    MaaToolkitAdbDeviceListDestroy(device_list);

    return devices;
}

[[nodiscard]] std::expected<bool, CaptureError>
MaaCaptureBackend::IsDeviceAvailable(std::string_view device_id)
{
    auto devices = EnumerateDevices();
    if (!devices) {
        return std::unexpected(devices.error());
    }

    for (const auto& device : *devices) {
        if (device.find(device_id) != std::string::npos) {
            return true;
        }
    }

    return false;
}

// ==========================================================================
// MaaFramework 特定接口
// ==========================================================================

[[nodiscard]] std::expected<MaaTaskId, CaptureError>
MaaCaptureBackend::ExecuteTask(std::string_view task_name, std::string_view task_params)
{
    if (!maa_instance_) {
        return std::unexpected(CaptureError::DeviceNotInitialized);
    }

    // 创建任务
    MaaTaskId task_id = MaaTaskerPostTask(maa_instance_, task_name.data(), task_params.data());

    if (task_id == MaaInvalidId) {
        return std::unexpected(CaptureError::DeviceError);
    }

    return task_id;
}

[[nodiscard]] ICaptureBackend::Result MaaCaptureBackend::StopTask(MaaTaskId task_id)
{
    if (!maa_instance_) {
        return std::unexpected(CaptureError::DeviceNotInitialized);
    }

    // MaaFramework 没有直接停止任务的 API，任务会在完成后自动结束
    // 这里我们只是记录日志
    spdlog::info("Task {} stop requested (no-op in MAA)", task_id);

    return {};
}

[[nodiscard]] bool MaaCaptureBackend::IsTaskCompleted(MaaTaskId task_id) const
{
    if (!maa_instance_) {
        return false;
    }

    MaaStatus status = MaaTaskerStatus(maa_instance_, task_id);
    return status == MaaStatus_Succeeded || status == MaaStatus_Failed;
}

// ==========================================================================
// 内部方法
// ==========================================================================

void MaaCaptureBackend::CaptureThreadFunc()
{
    spdlog::debug("Capture thread started");

    while (!stop_requested_.load()) {
        auto start_time = core::Clock::now();

        // 执行截图
        auto result = CaptureScreenshot();

        if (result) {
            // 推送到队列
            PushFrame(std::move(*result));

            // 更新统计
            auto latency = core::Clock::now() - start_time;
            UpdateStats(latency);
        }
        else {
            spdlog::warn("Screenshot failed: {}", static_cast<int>(result.error()));
        }

        // 计算睡眠时间以维持目标帧率
        auto elapsed    = core::Clock::now() - start_time;
        auto sleep_time = capture_interval_ - elapsed;

        if (sleep_time > core::Duration::zero()) {
            std::this_thread::sleep_for(sleep_time);
        }
    }

    spdlog::debug("Capture thread stopped");
}

[[nodiscard]] ICaptureBackend::Result MaaCaptureBackend::InitializeController()
{
    // 初始化 MaaToolkit (使用当前目录作为用户路径)
    std::string user_path      = ".";
    std::string default_config = "{}";
    if (!MaaToolkitConfigInitOption(user_path.c_str(), default_config.c_str())) {
        spdlog::error("Failed to initialize MaaToolkit");
        return std::unexpected(CaptureError::DeviceError);
    }

    // 创建控制器
    if (connection_type_ == ConnectionType::ADB) {
        // 解析连接地址 (格式: adb_path@device_address)
        size_t at_pos = connection_address_.find('@');
        if (at_pos == std::string::npos) {
            spdlog::error("Invalid ADB connection address format: {}", connection_address_);
            return std::unexpected(CaptureError::InvalidArgument);
        }

        std::string adb_path = connection_address_.substr(0, at_pos);
        std::string address  = connection_address_.substr(at_pos + 1);

        spdlog::info("Creating ADB controller with path: {}, address: {}", adb_path, address);

        // Agent 路径 - 使用 CMake 配置的绝对路径、环境变量或默认路径
        std::string agent_path;
        if (const char* env_path = std::getenv("MAA_AGENT_PATH")) {
            // 优先使用环境变量指定的路径
            agent_path = env_path;
        }
#ifdef AAM_MAAFW_DIR
        else {
            // 使用 CMake 配置的绝对路径，避免运行时工作目录问题
            agent_path = std::string(AAM_MAAFW_DIR) + "/3rdparty/MaaAgentBinary";
        }
#else
        else {
            // 回退到相对路径（仅当 CMake 未配置时）
            spdlog::warn("AAM_MAAFW_DIR not defined, falling back to relative path for agent");
            agent_path = "third_party/maafw/3rdparty/MaaAgentBinary";
        }
#endif

        // 创建 ADB 控制器
        maa_controller_ = MaaAdbControllerCreate(adb_path.c_str(),
                                                 address.c_str(),
                                                 MaaAdbScreencapMethod_Default,
                                                 MaaAdbInputMethod_Maatouch,
                                                 "{}",  // config
                                                 agent_path.c_str());

        spdlog::info("ADB controller created: {}", maa_controller_ ? "success" : "failed");
    }
    else if (connection_type_ == ConnectionType::Win32) {
        // Win32 窗口捕获
        HWND hwnd = nullptr;
        try {
            hwnd = reinterpret_cast<HWND>(std::stoull(connection_address_));
        }
        catch (...) {
            spdlog::error("Invalid Win32 HWND: {}", connection_address_);
            return std::unexpected(CaptureError::InvalidArgument);
        }

        maa_controller_ = MaaWin32ControllerCreate(hwnd,
                                                   MaaWin32ScreencapMethod_GDI,
                                                   MaaWin32InputMethod_Seize,
                                                   MaaWin32InputMethod_Seize);
    }

    if (!maa_controller_) {
        spdlog::error("Failed to create MAA controller");
        return std::unexpected(CaptureError::DeviceError);
    }

    // 创建资源 (v2.0+ API: 无参数)
    maa_resource_ = MaaResourceCreate();
    if (!maa_resource_) {
        spdlog::error("Failed to create MAA resource");
        MaaControllerDestroy(maa_controller_);
        maa_controller_ = nullptr;
        return std::unexpected(CaptureError::DeviceError);
    }

    // 创建实例 (v2.0+ API: 无参数)
    maa_instance_ = MaaTaskerCreate();
    if (!maa_instance_) {
        spdlog::error("Failed to create MAA instance");
        MaaResourceDestroy(maa_resource_);
        MaaControllerDestroy(maa_controller_);
        maa_controller_ = nullptr;
        maa_resource_   = nullptr;
        return std::unexpected(CaptureError::DeviceError);
    }

    // 绑定控制器和资源
    MaaTaskerBindResource(maa_instance_, maa_resource_);
    MaaTaskerBindController(maa_instance_, maa_controller_);

    // 连接控制器 (v2.0+ API: 异步连接 + 等待)
    MaaCtrlId conn_id = MaaControllerPostConnection(maa_controller_);
    if (conn_id == MaaInvalidId) {
        spdlog::error("Failed to post MAA controller connection");
        MaaTaskerDestroy(maa_instance_);
        MaaResourceDestroy(maa_resource_);
        MaaControllerDestroy(maa_controller_);
        maa_instance_   = nullptr;
        maa_resource_   = nullptr;
        maa_controller_ = nullptr;
        return std::unexpected(CaptureError::DeviceError);
    }

    // 等待连接完成
    MaaStatus conn_status = MaaControllerWait(maa_controller_, conn_id);
    if (conn_status != MaaStatus_Succeeded) {
        spdlog::error("MAA controller connection failed with status: {}",
                      static_cast<int>(conn_status));
        MaaTaskerDestroy(maa_instance_);
        MaaResourceDestroy(maa_resource_);
        MaaControllerDestroy(maa_controller_);
        maa_instance_   = nullptr;
        maa_resource_   = nullptr;
        maa_controller_ = nullptr;
        return std::unexpected(CaptureError::DeviceError);
    }

    spdlog::info("MAA controller initialized successfully");

    return {};
}

void MaaCaptureBackend::ShutdownController()
{
    if (maa_instance_) {
        MaaTaskerDestroy(maa_instance_);
        maa_instance_ = nullptr;
    }

    if (maa_resource_) {
        MaaResourceDestroy(maa_resource_);
        maa_resource_ = nullptr;
    }

    if (maa_controller_) {
        MaaControllerDestroy(maa_controller_);
        maa_controller_ = nullptr;
    }

    spdlog::debug("MAA controller shutdown");
}

[[nodiscard]] std::expected<MaaCaptureBackend::FrameBufferElement, CaptureError>
MaaCaptureBackend::CaptureScreenshot()
{
    if (!maa_controller_) {
        return std::unexpected(CaptureError::DeviceNotInitialized);
    }

    // 获取截图
    MaaCtrlId ctrl_id = MaaControllerPostScreencap(maa_controller_);
    if (ctrl_id == MaaInvalidId) {
        return std::unexpected(CaptureError::DeviceError);
    }

    // 等待截图完成
    MaaStatus status     = MaaStatus_Invalid;
    auto      start_time = core::Clock::now();

    while (status != MaaStatus_Succeeded && status != MaaStatus_Failed) {
        status = MaaControllerStatus(maa_controller_, ctrl_id);

        // 检查超时
        if (core::Clock::now() - start_time > kScreencapTimeout) {
            return std::unexpected(CaptureError::Timeout);
        }

        if (status != MaaStatus_Succeeded && status != MaaStatus_Failed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    if (status != MaaStatus_Succeeded) {
        return std::unexpected(CaptureError::DeviceError);
    }

    // 获取图像数据
    MaaImageBuffer* image_buffer = MaaImageBufferCreate();
    if (!image_buffer) {
        return std::unexpected(CaptureError::OutOfMemory);
    }

    if (!MaaControllerCachedImage(maa_controller_, image_buffer)) {
        MaaImageBufferDestroy(image_buffer);
        return std::unexpected(CaptureError::DeviceError);
    }

    // 处理图像数据 (v2.0+ API: 使用 GetRawData 替代 Data)
    int32_t                  width  = MaaImageBufferWidth(image_buffer);
    int32_t                  height = MaaImageBufferHeight(image_buffer);
    [[maybe_unused]] int32_t channels =
        MaaImageBufferChannels(image_buffer);  // v2.0+ API，保留供调试使用
    MaaImageRawData raw_data_ptr = MaaImageBufferGetRawData(image_buffer);
    const uint8_t*  raw_data     = static_cast<const uint8_t*>(raw_data_ptr);

    if (!raw_data || width == 0 || height == 0) {
        MaaImageBufferDestroy(image_buffer);
        return std::unexpected(CaptureError::DeviceError);
    }

    // 创建 OpenCV Mat (使用 BGR 格式，与 MaaFramework 默认输出一致)
    // 重要：cv::Mat 与 MaaImageBuffer 共享内存，raw_data 指向 MaaImageBuffer 的内部缓冲区
    // 因此 cv::Mat 的生命周期不能超过 image_buffer，必须在 MaaImageBufferDestroy 之前完成所有使用
    cv::Mat image(
        static_cast<int>(height), static_cast<int>(width), CV_8UC3, const_cast<uint8_t*>(raw_data));

    // 处理图像 - 在 image_buffer 被释放前完成所有对 image 的操作
    FrameBufferElement frame = ProcessScreenshot(image);

    // 释放 MAA 图像缓冲区 - 此后 image 中的数据指针将失效，不得再访问
    MaaImageBufferDestroy(image_buffer);

    return frame;
}

[[nodiscard]] MaaCaptureBackend::FrameBufferElement
MaaCaptureBackend::ProcessScreenshot(const cv::Mat& image_data)
{
    FrameBufferElement frame;

    // 设置元数据
    frame.metadata.capture_timestamp = core::Clock::now();
    frame.metadata.process_timestamp = frame.metadata.capture_timestamp;
    frame.metadata.width             = static_cast<uint32_t>(image_data.cols);
    frame.metadata.height            = static_cast<uint32_t>(image_data.rows);
    frame.metadata.pixel_format      = PixelFormat::BGR24;  // MaaFramework 默认返回 BGR
    frame.metadata.stride            = static_cast<uint32_t>(image_data.step);
    frame.metadata.frame_number      = frame_counter_.fetch_add(1);

    // 根据配置转换格式
    cv::Mat converted;
    switch (config_.preferred_format) {
        case PixelFormat::RGB24:
            cv::cvtColor(image_data, converted, cv::COLOR_BGR2RGB);
            frame.metadata.pixel_format = PixelFormat::RGB24;
            break;
        case PixelFormat::BGR24:
            converted                   = image_data.clone();
            frame.metadata.pixel_format = PixelFormat::BGR24;
            break;
        case PixelFormat::RGBA32:
            cv::cvtColor(image_data, converted, cv::COLOR_BGR2RGBA);
            frame.metadata.pixel_format = PixelFormat::RGBA32;
            break;
        case PixelFormat::BGRA32:
            cv::cvtColor(image_data, converted, cv::COLOR_BGR2BGRA);
            frame.metadata.pixel_format = PixelFormat::BGRA32;
            break;
        default:
            // 默认使用 BGR24
            converted                   = image_data.clone();
            frame.metadata.pixel_format = PixelFormat::BGR24;
            break;
    }

    // 复制数据到帧缓冲区
    frame.metadata.data_size = converted.total() * converted.elemSize();
    frame.data.resize(frame.metadata.data_size);
    std::memcpy(frame.data.data(), converted.data, frame.data.size());

    return frame;
}

void MaaCaptureBackend::PushFrame(FrameBufferElement&& frame)
{
    std::lock_guard<std::mutex> lock(queue_mutex_);

    // 如果队列已满，丢弃最旧的帧
    if (frame_queue_.size() >= max_queue_size_) {
        frame_queue_.pop();
    }

    frame_queue_.push(std::move(frame));
    queue_cv_.notify_one();
}

void MaaCaptureBackend::UpdateStats(core::Duration latency)
{
    std::lock_guard<std::mutex> lock(stats_mutex_);

    // 更新统计信息
    stats_.frames_captured++;

    // 更新延迟统计 (使用 core::Duration)
    if (latency < stats_.min_latency) {
        stats_.min_latency = latency;
    }
    if (latency > stats_.max_latency) {
        stats_.max_latency = latency;
    }

    // 计算平均延迟（指数移动平均）
    constexpr double alpha = 0.1;  // 平滑因子
    if (stats_.frames_captured == 1) {
        stats_.avg_latency = latency;
    }
    else {
        stats_.avg_latency =
            std::chrono::duration_cast<core::Duration>(std::chrono::duration<double, std::nano>(
                alpha * latency.count() + (1.0 - alpha) * stats_.avg_latency.count()));
    }

    // 计算当前帧率
    auto now = core::Clock::now();
    if (stats_.frames_captured > 1) {
        auto elapsed =
            std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        if (elapsed > 0) {
            stats_.current_fps = static_cast<double>(stats_.frames_captured) / elapsed;
        }
    }
}

[[nodiscard]] std::pair<MaaCaptureBackend::ConnectionType, std::string>
MaaCaptureBackend::ParseTargetId(std::string_view target_id)
{
    // 解析目标 ID 格式:
    // - adb:<adb_path>@<device_address>  (例如: adb:C:\adb\adb.exe@127.0.0.1:5555)
    // - win32:<hwnd>  (例如: win32:12345678)

    std::string target(target_id);

    if (target.starts_with("adb:")) {
        return {ConnectionType::ADB, target.substr(4)};
    }
    else if (target.starts_with("win32:")) {
        return {ConnectionType::Win32, target.substr(6)};
    }

    // 默认尝试 ADB 格式
    if (target.find('@') != std::string::npos) {
        return {ConnectionType::ADB, target};
    }

    return {ConnectionType::Unknown, ""};
}

}  // namespace aam::l0
