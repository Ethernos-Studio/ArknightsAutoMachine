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
// @file shm_transport.cpp
// @author dhjs0000
// @brief L0 共享内存传输层实现
// ==========================================================================

#include "aam/l0/shm_transport.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

// 平台特定头文件
#ifdef _WIN32
#    include <aclapi.h>
#else
#    include <cerrno>
#    include <sys/mman.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

namespace aam::l0
{

// 使用 core 命名空间
using namespace aam::core;

// ==========================================================================
// CRC32 查找表（IEEE 802.3 标准）
// ==========================================================================
namespace
{
// CRC32 查找表 - 使用 constexpr 确保编译期计算
constexpr std::array<std::uint32_t, 256> generate_crc32_table() noexcept
{
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t crc = i;
        for (std::uint32_t j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc) & 1)));
        }
        table[i] = crc;
    }
    return table;
}

static constexpr auto g_crc32_table = generate_crc32_table();

// 错误类别实现
class ShmTransportErrorCategory : public std::error_category
{
public:
    [[nodiscard]] const char* name() const noexcept override
    {
        return "shm_transport";
    }

    [[nodiscard]] std::string message(int ev) const override
    {
        switch (static_cast<ShmTransportError>(ev)) {
            case ShmTransportError::Success:
                return "Success";
            case ShmTransportError::Unknown:
                return "Unknown error";
            case ShmTransportError::InvalidArgument:
                return "Invalid argument";
            case ShmTransportError::OutOfMemory:
                return "Out of memory";
            case ShmTransportError::Timeout:
                return "Operation timeout";
            case ShmTransportError::NotInitialized:
                return "Not initialized";
            case ShmTransportError::AlreadyExists:
                return "Resource already exists";
            case ShmTransportError::NotFound:
                return "Resource not found";
            case ShmTransportError::PermissionDenied:
                return "Permission denied";
            case ShmTransportError::ShmCreateFailed:
                return "Shared memory creation failed";
            case ShmTransportError::ShmOpenFailed:
                return "Shared memory open failed";
            case ShmTransportError::ShmMapFailed:
                return "Shared memory mapping failed";
            case ShmTransportError::ShmUnmapFailed:
                return "Shared memory unmapping failed";
            case ShmTransportError::ShmTooSmall:
                return "Shared memory too small";
            case ShmTransportError::ShmCorrupted:
                return "Shared memory data corrupted";
            case ShmTransportError::ShmVersionMismatch:
                return "Shared memory version mismatch";
            case ShmTransportError::SyncCreateFailed:
                return "Synchronization object creation failed";
            case ShmTransportError::SyncWaitFailed:
                return "Synchronization wait failed";
            case ShmTransportError::SyncSignalFailed:
                return "Synchronization signal failed";
            case ShmTransportError::SyncTimeout:
                return "Synchronization timeout";
            case ShmTransportError::SyncAbandoned:
                return "Synchronization object abandoned";
            case ShmTransportError::TransportClosed:
                return "Transport closed";
            case ShmTransportError::TransportBusy:
                return "Transport busy";
            case ShmTransportError::BufferOverflow:
                return "Buffer overflow";
            case ShmTransportError::BufferUnderflow:
                return "Buffer underflow";
            case ShmTransportError::FrameTooLarge:
                return "Frame too large";
            case ShmTransportError::InvalidFrame:
                return "Invalid frame data";
            default:
                return "Unknown shared memory transport error";
        }
    }

    [[nodiscard]] std::error_condition default_error_condition(int ev) const noexcept override
    {
        switch (static_cast<ShmTransportError>(ev)) {
            case ShmTransportError::Success:
                return std::errc{};
            case ShmTransportError::InvalidArgument:
                return std::errc::invalid_argument;
            case ShmTransportError::OutOfMemory:
                return std::errc::not_enough_memory;
            case ShmTransportError::Timeout:
            case ShmTransportError::SyncTimeout:
                return std::errc::timed_out;
            case ShmTransportError::NotFound:
                return std::errc::no_such_file_or_directory;
            case ShmTransportError::AlreadyExists:
                return std::errc::file_exists;
            case ShmTransportError::PermissionDenied:
                return std::errc::permission_denied;
            default:
                return std::error_condition(ev, *this);
        }
    }
};

// 全局错误类别实例
const ShmTransportErrorCategory g_shm_transport_error_category{};

// 平台特定的共享内存名称前缀
#ifdef _WIN32
constexpr std::string_view SHM_NAME_PREFIX = "Local\\";
#else
constexpr std::string_view SHM_NAME_PREFIX = "/";
#endif

}  // anonymous namespace

// ==========================================================================
// 错误类别接口实现
// ==========================================================================

const std::error_category& shm_transport_error_category() noexcept
{
    return g_shm_transport_error_category;
}

// ==========================================================================
// ShmFrameHeader 实现
// ==========================================================================

std::uint32_t ShmFrameHeader::calculate_checksum(const std::byte* data) const noexcept
{
    if (data == nullptr || data_size == 0) {
        return 0;
    }

    std::uint32_t crc = 0xFFFFFFFF;
    for (std::uint32_t i = 0; i < data_size; ++i) {
        const std::uint8_t byte = static_cast<std::uint8_t>(data[i]);
        crc = g_crc32_table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

bool ShmFrameHeader::verify_data(const std::byte* data) const noexcept
{
    if (data == nullptr || data_size == 0) {
        return false;
    }
    return calculate_checksum(data) == checksum;
}

// ==========================================================================
// Windows 共享内存段实现
// ==========================================================================
#ifdef _WIN32

class WindowsSharedMemorySegment : public SharedMemorySegment
{
public:
    WindowsSharedMemorySegment() noexcept = default;

    ~WindowsSharedMemorySegment() override
    {
        cleanup();
    }

    WindowsSharedMemorySegment(const WindowsSharedMemorySegment&)            = delete;
    WindowsSharedMemorySegment& operator=(const WindowsSharedMemorySegment&) = delete;

    WindowsSharedMemorySegment(WindowsSharedMemorySegment&& other) noexcept
        : name_(std::move(other.name_)),
          size_(other.size_),
          handle_(other.handle_),
          data_(other.data_)
    {
        other.handle_ = nullptr;
        other.data_   = nullptr;
        other.size_   = 0;
    }

    WindowsSharedMemorySegment& operator=(WindowsSharedMemorySegment&& other) noexcept
    {
        if (this != &other) {
            cleanup();
            name_   = std::move(other.name_);
            size_   = other.size_;
            handle_ = other.handle_;
            data_   = other.data_;
            other.handle_ = nullptr;
            other.data_   = nullptr;
            other.size_   = 0;
        }
        return *this;
    }

    [[nodiscard]] static std::expected<std::unique_ptr<SharedMemorySegment>, ShmTransportError>
    Create(std::string_view name, std::size_t size)
    {
        if (name.empty() || size == 0) {
            return std::unexpected(ShmTransportError::InvalidArgument);
        }

        auto segment = std::make_unique<WindowsSharedMemorySegment>();
        segment->name_ = std::string(name);
        segment->size_ = size;

        segment->handle_ = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            static_cast<DWORD>((size >> 32) & 0xFFFFFFFF),
            static_cast<DWORD>(size & 0xFFFFFFFF),
            segment->name_.c_str()
        );

        if (segment->handle_ == nullptr) {
            return std::unexpected(ShmTransportError::ShmCreateFailed);
        }

        // 检查是否创建了新的共享内存还是打开了已存在的
        // CreateFileMappingA 在打开已存在的映射时也会返回有效句柄
        const DWORD create_error = GetLastError();
        const bool already_exists = (create_error == ERROR_ALREADY_EXISTS);

        segment->data_ = static_cast<std::byte*>(MapViewOfFile(
            segment->handle_,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            size
        ));

        if (segment->data_ == nullptr) {
            // 映射失败时关闭句柄避免泄漏
            CloseHandle(segment->handle_);
            segment->handle_ = nullptr;
            return std::unexpected(ShmTransportError::ShmMapFailed);
        }

        // 如果是已存在的映射，在映射成功后返回错误
        // 注意：必须先关闭句柄和解除映射
        if (already_exists) {
            UnmapViewOfFile(segment->data_);
            CloseHandle(segment->handle_);
            segment->data_ = nullptr;
            segment->handle_ = nullptr;
            return std::unexpected(ShmTransportError::AlreadyExists);
        }

        std::memset(segment->data_, 0, size);
        return segment;
    }

    [[nodiscard]] static std::expected<std::unique_ptr<SharedMemorySegment>, ShmTransportError>
    Open(std::string_view name, std::size_t size)
    {
        if (name.empty() || size == 0) {
            return std::unexpected(ShmTransportError::InvalidArgument);
        }

        auto segment = std::make_unique<WindowsSharedMemorySegment>();
        segment->name_ = std::string(name);
        segment->size_ = size;

        segment->handle_ = OpenFileMappingA(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            segment->name_.c_str()
        );

        if (segment->handle_ == nullptr) {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND) {
                return std::unexpected(ShmTransportError::NotFound);
            }
            return std::unexpected(ShmTransportError::ShmOpenFailed);
        }

        segment->data_ = static_cast<std::byte*>(MapViewOfFile(
            segment->handle_,
            FILE_MAP_ALL_ACCESS,
            0,
            0,
            size
        ));

        if (segment->data_ == nullptr) {
            return std::unexpected(ShmTransportError::ShmMapFailed);
        }

        return segment;
    }

    [[nodiscard]] std::byte* data() const noexcept override { return data_; }
    [[nodiscard]] std::size_t size() const noexcept override { return size_; }
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }
    [[nodiscard]] bool is_valid() const noexcept override { return handle_ != nullptr && data_ != nullptr; }

    [[nodiscard]] Result flush() noexcept override
    {
        if (data_ == nullptr) {
            return std::unexpected(ShmTransportError::NotInitialized);
        }
        if (!FlushViewOfFile(data_, size_)) {
            return std::unexpected(ShmTransportError::Unknown);
        }
        return {};
    }

private:
    void cleanup() noexcept
    {
        if (data_ != nullptr) {
            UnmapViewOfFile(data_);
            data_ = nullptr;
        }
        if (handle_ != nullptr) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }

    std::string   name_;
    std::size_t   size_{0};
    HANDLE        handle_{nullptr};
    std::byte*    data_{nullptr};
};

using PlatformSharedMemorySegment = WindowsSharedMemorySegment;

// ==========================================================================
// POSIX 共享内存段实现
// ==========================================================================
#else

class PosixSharedMemorySegment : public SharedMemorySegment
{
public:
    PosixSharedMemorySegment() noexcept = default;

    ~PosixSharedMemorySegment() override
    {
        cleanup();
    }

    PosixSharedMemorySegment(const PosixSharedMemorySegment&)            = delete;
    PosixSharedMemorySegment& operator=(const PosixSharedMemorySegment&) = delete;

    PosixSharedMemorySegment(PosixSharedMemorySegment&& other) noexcept
        : name_(std::move(other.name_)),
          size_(other.size_),
          fd_(other.fd_),
          data_(other.data_),
          created_(other.created_)
    {
        other.fd_      = -1;
        other.data_    = nullptr;
        other.size_    = 0;
        other.created_ = false;
    }

    PosixSharedMemorySegment& operator=(PosixSharedMemorySegment&& other) noexcept
    {
        if (this != &other) {
            cleanup();
            name_    = std::move(other.name_);
            size_    = other.size_;
            fd_      = other.fd_;
            data_    = other.data_;
            created_ = other.created_;
            other.fd_      = -1;
            other.data_    = nullptr;
            other.size_    = 0;
            other.created_ = false;
        }
        return *this;
    }

    [[nodiscard]] static std::expected<std::unique_ptr<SharedMemorySegment>, ShmTransportError>
    Create(std::string_view name, std::size_t size)
    {
        if (name.empty() || size == 0) {
            return std::unexpected(ShmTransportError::InvalidArgument);
        }

        auto segment = std::make_unique<PosixSharedMemorySegment>();
        segment->name_ = std::string(name);
        segment->size_ = size;
        segment->created_ = true;

        segment->fd_ = shm_open(segment->name_.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
        if (segment->fd_ == -1) {
            if (errno == EEXIST) {
                return std::unexpected(ShmTransportError::AlreadyExists);
            }
            return std::unexpected(ShmTransportError::ShmCreateFailed);
        }

        if (ftruncate(segment->fd_, static_cast<off_t>(size)) == -1) {
            shm_unlink(segment->name_.c_str());
            return std::unexpected(ShmTransportError::ShmCreateFailed);
        }

        segment->data_ = static_cast<std::byte*>(mmap(
            nullptr,
            size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            segment->fd_,
            0
        ));

        if (segment->data_ == MAP_FAILED) {
            shm_unlink(segment->name_.c_str());
            segment->data_ = nullptr;
            return std::unexpected(ShmTransportError::ShmMapFailed);
        }

        std::memset(segment->data_, 0, size);
        return segment;
    }

    [[nodiscard]] static std::expected<std::unique_ptr<SharedMemorySegment>, ShmTransportError>
    Open(std::string_view name, std::size_t size)
    {
        if (name.empty() || size == 0) {
            return std::unexpected(ShmTransportError::InvalidArgument);
        }

        auto segment = std::make_unique<PosixSharedMemorySegment>();
        segment->name_ = std::string(name);
        segment->size_ = size;
        segment->created_ = false;

        segment->fd_ = shm_open(segment->name_.c_str(), O_RDWR, 0666);
        if (segment->fd_ == -1) {
            if (errno == ENOENT) {
                return std::unexpected(ShmTransportError::NotFound);
            }
            return std::unexpected(ShmTransportError::ShmOpenFailed);
        }

        segment->data_ = static_cast<std::byte*>(mmap(
            nullptr,
            size,
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            segment->fd_,
            0
        ));

        if (segment->data_ == MAP_FAILED) {
            segment->data_ = nullptr;
            return std::unexpected(ShmTransportError::ShmMapFailed);
        }

        return segment;
    }

    [[nodiscard]] std::byte* data() const noexcept override { return data_; }
    [[nodiscard]] std::size_t size() const noexcept override { return size_; }
    [[nodiscard]] std::string_view name() const noexcept override { return name_; }
    [[nodiscard]] bool is_valid() const noexcept override { return fd_ != -1 && data_ != nullptr; }

    [[nodiscard]] Result flush() noexcept override
    {
        if (data_ == nullptr) {
            return std::unexpected(ShmTransportError::NotInitialized);
        }
        if (msync(data_, size_, MS_SYNC) == -1) {
            return std::unexpected(ShmTransportError::Unknown);
        }
        return {};
    }

private:
    void cleanup() noexcept
    {
        if (data_ != nullptr) {
            munmap(data_, size_);
            data_ = nullptr;
        }
        if (fd_ != -1) {
            close(fd_);
            fd_ = -1;
        }
        if (created_ && !name_.empty()) {
            shm_unlink(name_.c_str());
        }
    }

    std::string   name_;
    std::size_t   size_{0};
    int           fd_{-1};
    std::byte*    data_{nullptr};
    bool          created_{false};
};

using PlatformSharedMemorySegment = PosixSharedMemorySegment;

#endif  // _WIN32

// ==========================================================================
// SharedMemorySegment 工厂方法实现
// ==========================================================================

std::expected<std::unique_ptr<SharedMemorySegment>, ShmTransportError>
SharedMemorySegment::Create(std::string_view name, std::size_t size)
{
    return PlatformSharedMemorySegment::Create(name, size);
}

std::expected<std::unique_ptr<SharedMemorySegment>, ShmTransportError>
SharedMemorySegment::Open(std::string_view name, std::size_t size)
{
    return PlatformSharedMemorySegment::Open(name, size);
}

// ==========================================================================
// ShmTransport 实现
// ==========================================================================

ShmTransport::~ShmTransport() noexcept
{
    [[maybe_unused]] auto result = Shutdown();
}

ShmTransport::ShmTransport(ShmTransport&& other) noexcept
    : config_(std::move(other.config_)),
      segment_(std::move(other.segment_)),
      control_block_(other.control_block_),
      frame_data_base_(other.frame_data_base_),
      initialized_(other.initialized_.load()),
      is_producer_(other.is_producer_),
      stats_(std::move(other.stats_))
{
    other.control_block_ = nullptr;
    other.frame_data_base_ = nullptr;
    other.initialized_ = false;
    other.is_producer_ = false;
}

ShmTransport& ShmTransport::operator=(ShmTransport&& other) noexcept
{
    if (this != &other) {
        [[maybe_unused]] auto _ = Shutdown();
        config_ = std::move(other.config_);
        segment_ = std::move(other.segment_);
        control_block_ = other.control_block_;
        frame_data_base_ = other.frame_data_base_;
        initialized_ = other.initialized_.load();
        is_producer_ = other.is_producer_;
        stats_ = std::move(other.stats_);
        other.control_block_ = nullptr;
        other.frame_data_base_ = nullptr;
        other.initialized_ = false;
        other.is_producer_ = false;
    }
    return *this;
}

ShmTransport::Result ShmTransport::InitializeProducer(const ShmTransportConfig& config)
{
    if (initialized_.load(std::memory_order_acquire)) {
        return std::unexpected(ShmTransportError::AlreadyExists);
    }

    auto validation = config.validate();
    if (!validation) {
        return validation;
    }

    const std::string full_name = MakeShmName(config.shm_name);
    const std::size_t total_size = config.calculate_total_size();

    auto segment_result = SharedMemorySegment::Create(full_name, total_size);
    if (!segment_result) {
        return std::unexpected(segment_result.error());
    }

    segment_ = std::move(*segment_result);
    config_ = config;
    is_producer_ = true;

    control_block_ = reinterpret_cast<ShmControlBlock*>(segment_->data());
    frame_data_base_ = segment_->data() + sizeof(ShmControlBlock) + config.metadata_size;

    const std::size_t frame_header_size = sizeof(ShmFrameHeader);
    const std::size_t aligned_frame_size =
        ((frame_header_size + config.max_frame_size + 63) / 64) * 64;

    control_block_->magic = ShmControlBlock::MAGIC;
    control_block_->version = ShmControlBlock::VERSION;
    control_block_->header_size = static_cast<std::uint32_t>(sizeof(ShmControlBlock) + config.metadata_size);
    control_block_->flags = 0;
    control_block_->buffer_count = static_cast<std::uint32_t>(config.buffer_count);
    control_block_->max_frame_size = static_cast<std::uint32_t>(config.max_frame_size);
    control_block_->metadata_size = static_cast<std::uint32_t>(config.metadata_size);
    control_block_->frame_stride = static_cast<std::uint32_t>(aligned_frame_size & 0xFFFFFFFFULL);

    control_block_->write_sequence.store(0, std::memory_order_relaxed);
    control_block_->read_sequence.store(0, std::memory_order_relaxed);
    control_block_->dropped_frames.store(0, std::memory_order_relaxed);
    control_block_->state.store(ShmControlBlock::STATE_INITIALIZED, std::memory_order_relaxed);
    control_block_->active_readers.store(0, std::memory_order_relaxed);
    control_block_->active_writers.store(1, std::memory_order_relaxed);
    control_block_->total_frames_written.store(0, std::memory_order_relaxed);
    control_block_->total_frames_read.store(0, std::memory_order_relaxed);
    control_block_->total_bytes_written.store(0, std::memory_order_relaxed);
    control_block_->total_bytes_read.store(0, std::memory_order_relaxed);

    stats_.reset();
    initialized_.store(true, std::memory_order_release);

    return {};
}

ShmTransport::Result ShmTransport::InitializeConsumer(const ShmTransportConfig& config)
{
    if (initialized_.load(std::memory_order_acquire)) {
        return std::unexpected(ShmTransportError::AlreadyExists);
    }

    auto validation = config.validate();
    if (!validation) {
        return validation;
    }

    const std::string full_name = MakeShmName(config.shm_name);
    const std::size_t total_size = config.calculate_total_size();

    auto segment_result = SharedMemorySegment::Open(full_name, total_size);
    if (!segment_result) {
        return std::unexpected(segment_result.error());
    }

    segment_ = std::move(*segment_result);
    config_ = config;
    is_producer_ = false;

    control_block_ = reinterpret_cast<ShmControlBlock*>(segment_->data());
    frame_data_base_ = segment_->data() + sizeof(ShmControlBlock) + config.metadata_size;

    if (!control_block_->is_valid()) {
        segment_.reset();
        control_block_ = nullptr;
        frame_data_base_ = nullptr;
        return std::unexpected(ShmTransportError::ShmCorrupted);
    }

    control_block_->active_readers.fetch_add(1, std::memory_order_relaxed);

    stats_.reset();
    initialized_.store(true, std::memory_order_release);

    return {};
}

ShmTransport::Result ShmTransport::Shutdown() noexcept
{
    if (!initialized_.load(std::memory_order_acquire)) {
        return {};
    }

    if (control_block_ != nullptr && !is_producer_) {
        control_block_->active_readers.fetch_sub(1, std::memory_order_relaxed);
    }

    segment_.reset();
    control_block_ = nullptr;
    frame_data_base_ = nullptr;
    initialized_.store(false, std::memory_order_release);
    is_producer_ = false;

    return {};
}

bool ShmTransport::IsInitialized() const noexcept
{
    return initialized_.load(std::memory_order_acquire);
}

bool ShmTransport::IsProducer() const noexcept
{
    return initialized_.load(std::memory_order_acquire) && is_producer_;
}

bool ShmTransport::IsConsumer() const noexcept
{
    return initialized_.load(std::memory_order_acquire) && !is_producer_;
}

ShmTransport::Result ShmTransport::WriteFrame(const FrameMetadata& metadata,
                                               std::span<const std::byte> data)
{
    // 前置校验：检查空数据
    if (data.empty()) {
        return std::unexpected(ShmTransportError::InvalidFrame);
    }

    // 如果启用零拷贝模式，使用零拷贝 API
    if (config_.enable_zero_copy) {
        auto buffer_result = AcquireWriteBuffer(config_.write_timeout);
        if (!buffer_result) {
            return std::unexpected(buffer_result.error());
        }
        if (!buffer_result->has_value()) {
            return std::unexpected(ShmTransportError::Timeout);
        }

        auto& buffer = buffer_result->value();
        if (data.size() > buffer.capacity) {
            return std::unexpected(ShmTransportError::FrameTooLarge);
        }

        // 直接复制到共享内存缓冲区
        std::memcpy(buffer.data, data.data(), data.size());

        return CommitWriteBuffer(buffer.buffer_index, metadata, data.size());
    }

    // 非零拷贝模式：使用传统路径
    auto result = WriteFrameWithTimeout(metadata, data, config_.write_timeout);
    if (!result) {
        return std::unexpected(result.error());
    }
    return {};
}

std::expected<bool, ShmTransportError> ShmTransport::TryWriteFrame(
    const FrameMetadata& metadata, std::span<const std::byte> data)
{
    if (!initialized_.load(std::memory_order_acquire) || !is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    if (!metadata.is_valid()) {
        return std::unexpected(ShmTransportError::InvalidFrame);
    }

    if (data.size() > config_.max_frame_size) {
        return std::unexpected(ShmTransportError::FrameTooLarge);
    }

    if (!control_block_->is_writable()) {
        return false;
    }

    const auto start_time = core::Clock::now();
    const std::uint32_t write_index = control_block_->get_write_index();

    ShmFrameHeader* header = get_frame_header(write_index);
    std::byte* frame_data = get_frame_buffer(write_index) + sizeof(ShmFrameHeader);

    header->magic = ShmFrameHeader::MAGIC;
    header->version = 1;
    header->sequence_number = static_cast<std::uint32_t>(
        control_block_->write_sequence.load(std::memory_order_relaxed));
    header->frame_number = static_cast<std::uint32_t>(metadata.frame_number);
    header->sequence_id = static_cast<std::uint32_t>(metadata.sequence_id);
    header->data_size = static_cast<std::uint32_t>(data.size());
    header->capture_timestamp_ns = static_cast<std::uint64_t>(
        metadata.capture_timestamp.time_since_epoch().count());
    header->write_timestamp_ns = static_cast<std::uint64_t>(
        core::Clock::now().time_since_epoch().count());
    header->width = metadata.width;
    header->height = metadata.height;
    header->stride = metadata.stride;
    header->pixel_format = static_cast<std::uint32_t>(metadata.pixel_format);

    std::memcpy(frame_data, data.data(), data.size());

    // 仅在启用校验和时计算CRC
    if (config_.enable_checksum) {
        header->checksum = header->calculate_checksum(frame_data);
    } else {
        header->checksum = 0;
    }

    control_block_->write_sequence.fetch_add(1, std::memory_order_release);
    control_block_->total_frames_written.fetch_add(1, std::memory_order_relaxed);
    control_block_->total_bytes_written.fetch_add(data.size(), std::memory_order_relaxed);

    const auto latency = core::Clock::now() - start_time;
    update_stats(true, data.size(), latency);

    return true;
}

std::expected<bool, ShmTransportError> ShmTransport::WriteFrameWithTimeout(
    const FrameMetadata& metadata, std::span<const std::byte> data, core::Duration timeout)
{
    if (!initialized_.load(std::memory_order_acquire) || !is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    const auto deadline = core::Clock::now() + timeout;

    while (core::Clock::now() < deadline) {
        auto result = TryWriteFrame(metadata, data);
        if (!result) {
            return result;
        }
        if (*result) {
            return true;
        }
        std::this_thread::yield();
    }

    control_block_->dropped_frames.fetch_add(1, std::memory_order_relaxed);
    stats_.write_timeouts++;
    return false;
}

std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>, ShmTransportError>
ShmTransport::ReadFrame(core::Duration timeout)
{
    if (!initialized_.load(std::memory_order_acquire) || is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    // 如果启用零拷贝模式，使用零拷贝 API 读取
    if (config_.enable_zero_copy) {
        auto buffer_result = AcquireReadBuffer(timeout);
        if (!buffer_result) {
            return std::unexpected(buffer_result.error());
        }
        if (!buffer_result->has_value()) {
            stats_.read_timeouts++;
            return std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>{};
        }

        const auto& buffer = buffer_result->value();

        // 复制数据到本地缓冲区
        std::vector<std::byte> data(buffer.data.size());
        std::memcpy(data.data(), buffer.data.data(), buffer.data.size());

        FrameMetadata metadata = buffer.metadata;

        // 释放读取缓冲区
        auto release_result = ReleaseReadBuffer(buffer.buffer_index);
        if (!release_result.has_value()) {
            return std::unexpected(release_result.error());
        }

        return std::make_optional(std::make_pair(std::move(metadata), std::move(data)));
    }

    // 非零拷贝模式：使用传统路径
    const auto deadline = core::Clock::now() + timeout;

    while (core::Clock::now() < deadline) {
        auto result = TryReadFrame();
        if (!result) {
            return result;
        }
        if (result->has_value()) {
            return result;
        }
        std::this_thread::yield();
    }

    stats_.read_timeouts++;
    return std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>{};
}

std::expected<std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>, ShmTransportError>
ShmTransport::TryReadFrame()
{
    if (!initialized_.load(std::memory_order_acquire) || is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    if (!control_block_->has_readable_data()) {
        return std::optional<std::pair<FrameMetadata, std::vector<std::byte>>>{};
    }

    const auto start_time = core::Clock::now();
    const std::uint32_t read_index = control_block_->get_read_index();

    ShmFrameHeader* header = get_frame_header(read_index);
    std::byte* frame_data = get_frame_buffer(read_index) + sizeof(ShmFrameHeader);

    if (!header->is_valid()) {
        return std::unexpected(ShmTransportError::ShmCorrupted);
    }

    // 仅在启用校验和时验证数据完整性
    if (config_.enable_checksum && !header->verify_data(frame_data)) {
        stats_.checksum_errors++;
        control_block_->read_sequence.fetch_add(1, std::memory_order_release);
        return std::unexpected(ShmTransportError::ShmCorrupted);
    }

    FrameMetadata metadata;
    metadata.width = header->width;
    metadata.height = header->height;
    metadata.stride = header->stride;
    metadata.pixel_format = static_cast<PixelFormat>(header->pixel_format);
    metadata.capture_timestamp = core::Timestamp(std::chrono::nanoseconds(header->capture_timestamp_ns));
    metadata.process_timestamp = core::Clock::now();
    metadata.frame_number = header->frame_number;
    metadata.sequence_id = header->sequence_id;
    metadata.data_size = header->data_size;

    std::vector<std::byte> data(header->data_size);
    std::memcpy(data.data(), frame_data, header->data_size);

    control_block_->read_sequence.fetch_add(1, std::memory_order_release);
    control_block_->total_frames_read.fetch_add(1, std::memory_order_relaxed);
    control_block_->total_bytes_read.fetch_add(header->data_size, std::memory_order_relaxed);

    const auto latency = core::Clock::now() - start_time;
    update_stats(false, header->data_size, latency);

    return std::make_optional(std::make_pair(std::move(metadata), std::move(data)));
}

ShmTransport::Result ShmTransport::ReadFrameWithCallback(core::Duration timeout, ReadCallback callback)
{
    if (!callback) {
        return std::unexpected(ShmTransportError::InvalidArgument);
    }

    auto result = ReadFrame(timeout);
    if (!result) {
        return std::unexpected(result.error());
    }

    if (result->has_value()) {
        const auto& [metadata, data] = result->value();
        callback(metadata, std::span<const std::byte>(data.data(), data.size()));
    }

    return {};
}

ShmTransportStats ShmTransport::GetStats() const noexcept
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ShmTransportStats result = stats_;  // 使用显式拷贝构造函数

    if (control_block_ != nullptr) {
        result.frames_dropped = control_block_->dropped_frames.load(std::memory_order_relaxed);
    }
    return result;
}

std::pair<std::uint32_t, std::uint32_t> ShmTransport::GetBufferStatus() const noexcept
{
    if (!initialized_.load(std::memory_order_acquire) || control_block_ == nullptr) {
        return {0, 0};
    }
    return {control_block_->used_buffers(), control_block_->buffer_count};
}

void ShmTransport::update_stats(bool is_write, std::size_t bytes, core::Duration latency) noexcept
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (is_write) {
        stats_.frames_written++;
        stats_.bytes_written += bytes;
        stats_.update_write_latency(latency);
        stats_.last_write_time = core::Clock::now();
    } else {
        stats_.frames_read++;
        stats_.bytes_read += bytes;
        stats_.update_read_latency(latency);
        stats_.last_read_time = core::Clock::now();
    }
}

ShmFrameHeader* ShmTransport::get_frame_header(std::uint32_t index) noexcept
{
    return reinterpret_cast<ShmFrameHeader*>(get_frame_buffer(index));
}

std::byte* ShmTransport::get_frame_buffer(std::uint32_t index) noexcept
{
    if (control_block_ == nullptr || segment_ == nullptr) {
        return nullptr;
    }
    const std::size_t header_size = sizeof(ShmControlBlock) + config_.metadata_size;
    const std::size_t offset = header_size + (index * control_block_->frame_stride);
    return segment_->data() + offset;
}

// ==========================================================================
// 辅助函数实现
// ==========================================================================

std::string MakeShmName(std::string_view name)
{
#ifdef _WIN32
    return std::string("Local\\") + std::string(name);
#else
    return std::string("/") + std::string(name);
#endif
}

bool ShmExists(std::string_view name)
{
#ifdef _WIN32
    const std::string full_name = MakeShmName(name);
    HANDLE handle = OpenFileMappingA(FILE_MAP_READ, FALSE, full_name.c_str());
    if (handle != nullptr) {
        CloseHandle(handle);
        return true;
    }
    return false;
#else
    const std::string full_name = MakeShmName(name);
    int fd = shm_open(full_name.c_str(), O_RDONLY, 0666);
    if (fd != -1) {
        close(fd);
        return true;
    }
    return false;
#endif
}

bool ShmRemove([[maybe_unused]] std::string_view name)
{
#ifdef _WIN32
    // Windows: 共享内存在最后一个句柄关闭后自动删除
    (void)name;
    return true;
#else
    const std::string full_name = MakeShmName(name);
    return shm_unlink(full_name.c_str()) == 0 || errno == ENOENT;
#endif
}


// ==========================================================================
// 零拷贝API实现
// ==========================================================================

std::expected<std::optional<ShmTransport::WriteBuffer>, ShmTransportError>
ShmTransport::AcquireWriteBuffer(core::Duration timeout)
{
    if (!initialized_.load(std::memory_order_acquire) || !is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    const auto deadline = core::Clock::now() + timeout;

    while (core::Clock::now() < deadline) {
        auto result = TryAcquireWriteBuffer();
        if (!result) {
            return result;
        }
        if (result->has_value()) {
            return result;
        }
        std::this_thread::yield();
    }

    stats_.write_timeouts++;
    return std::optional<WriteBuffer>{};
}

std::expected<std::optional<ShmTransport::WriteBuffer>, ShmTransportError>
ShmTransport::TryAcquireWriteBuffer()
{
    if (!initialized_.load(std::memory_order_acquire) || !is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    if (!control_block_->is_writable()) {
        return std::optional<WriteBuffer>{};
    }

    const std::uint32_t write_index = control_block_->get_write_index();
    std::byte* frame_data = get_frame_buffer(write_index) + sizeof(ShmFrameHeader);

    WriteBuffer buffer;
    buffer.data = frame_data;
    buffer.capacity = config_.max_frame_size;
    buffer.buffer_index = write_index;

    return std::make_optional(buffer);
}

ShmTransport::Result ShmTransport::CommitWriteBuffer(
    std::uint32_t buffer_index,
    const FrameMetadata& metadata,
    std::size_t actual_size)
{
    if (!initialized_.load(std::memory_order_acquire) || !is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    if (actual_size > config_.max_frame_size) {
        return std::unexpected(ShmTransportError::FrameTooLarge);
    }

    const auto start_time = core::Clock::now();
    ShmFrameHeader* header = get_frame_header(buffer_index);
    std::byte* frame_data = get_frame_buffer(buffer_index) + sizeof(ShmFrameHeader);

    // 填充帧头部（零拷贝模式下跳过CRC计算）
    header->magic = ShmFrameHeader::MAGIC;
    header->version = 1;
    header->sequence_number = static_cast<std::uint32_t>(
        control_block_->write_sequence.load(std::memory_order_relaxed));
    header->frame_number = static_cast<std::uint32_t>(metadata.frame_number);
    header->sequence_id = static_cast<std::uint32_t>(metadata.sequence_id);
    header->data_size = static_cast<std::uint32_t>(actual_size);
    header->capture_timestamp_ns = static_cast<std::uint64_t>(
        metadata.capture_timestamp.time_since_epoch().count());
    header->write_timestamp_ns = static_cast<std::uint64_t>(
        core::Clock::now().time_since_epoch().count());
    header->width = metadata.width;
    header->height = metadata.height;
    header->stride = metadata.stride;
    header->pixel_format = static_cast<std::uint32_t>(metadata.pixel_format);

    // 仅在启用校验和时计算CRC
    if (config_.enable_checksum) {
        header->checksum = header->calculate_checksum(frame_data);
    } else {
        header->checksum = 0;
    }

    // 发布内存屏障，确保数据在序列号更新前可见
    std::atomic_thread_fence(std::memory_order_release);

    control_block_->write_sequence.fetch_add(1, std::memory_order_release);
    control_block_->total_frames_written.fetch_add(1, std::memory_order_relaxed);
    control_block_->total_bytes_written.fetch_add(actual_size, std::memory_order_relaxed);

    const auto latency = core::Clock::now() - start_time;
    update_stats(true, actual_size, latency);

    return {};
}

std::expected<std::optional<ShmTransport::ReadBuffer>, ShmTransportError>
ShmTransport::AcquireReadBuffer(core::Duration timeout)
{
    if (!initialized_.load(std::memory_order_acquire) || is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    const auto deadline = core::Clock::now() + timeout;

    while (core::Clock::now() < deadline) {
        auto result = TryAcquireReadBuffer();
        if (!result) {
            return result;
        }
        if (result->has_value()) {
            return result;
        }
        std::this_thread::yield();
    }

    stats_.read_timeouts++;
    return std::optional<ReadBuffer>{};
}

std::expected<std::optional<ShmTransport::ReadBuffer>, ShmTransportError>
ShmTransport::TryAcquireReadBuffer()
{
    if (!initialized_.load(std::memory_order_acquire) || is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    if (!control_block_->has_readable_data()) {
        return std::optional<ReadBuffer>{};
    }

    const std::uint32_t read_index = control_block_->get_read_index();
    ShmFrameHeader* header = get_frame_header(read_index);
    std::byte* frame_data = get_frame_buffer(read_index) + sizeof(ShmFrameHeader);

    // 获取内存屏障，确保看到最新的数据
    std::atomic_thread_fence(std::memory_order_acquire);

    if (!header->is_valid()) {
        return std::unexpected(ShmTransportError::ShmCorrupted);
    }

    // 仅在启用校验和时验证
    if (config_.enable_checksum && !header->verify_data(frame_data)) {
        stats_.checksum_errors++;
        // 跳过损坏的帧
        control_block_->read_sequence.fetch_add(1, std::memory_order_release);
        return std::unexpected(ShmTransportError::ShmCorrupted);
    }

    FrameMetadata metadata;
    metadata.width = header->width;
    metadata.height = header->height;
    metadata.stride = header->stride;
    metadata.pixel_format = static_cast<PixelFormat>(header->pixel_format);
    metadata.capture_timestamp = core::Timestamp(std::chrono::nanoseconds(header->capture_timestamp_ns));
    metadata.process_timestamp = core::Clock::now();
    metadata.frame_number = header->frame_number;
    metadata.sequence_id = header->sequence_id;
    metadata.data_size = header->data_size;

    ReadBuffer buffer;
    buffer.metadata = metadata;
    buffer.data = std::span<const std::byte>(frame_data, header->data_size);
    buffer.buffer_index = read_index;

    return std::make_optional(buffer);
}

ShmTransport::Result ShmTransport::ReleaseReadBuffer(std::uint32_t buffer_index)
{
    if (!initialized_.load(std::memory_order_acquire) || is_producer_) {
        return std::unexpected(ShmTransportError::NotInitialized);
    }

    [[maybe_unused]] const auto start_time = core::Clock::now();
    ShmFrameHeader* header = get_frame_header(buffer_index);

    control_block_->read_sequence.fetch_add(1, std::memory_order_release);
    control_block_->total_frames_read.fetch_add(1, std::memory_order_relaxed);
    control_block_->total_bytes_read.fetch_add(header->data_size, std::memory_order_relaxed);

    const auto latency = core::Clock::now() - start_time;
    update_stats(false, header->data_size, latency);

    return {};
}

}  // namespace aam::l0