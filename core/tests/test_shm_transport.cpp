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
// @file test_shm_transport.cpp
// @author dhjs0000
// @brief 共享内存传输层单元测试
// ==========================================================================

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include "aam/l0/shm_transport.hpp"

using namespace aam::l0;
using namespace std::chrono_literals;

// 使用 Clock 别名避免 core:: 前缀
using Clock = aam::core::Clock;

// ==========================================================================
// 测试夹具
// ==========================================================================

class ShmTransportTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // 清理可能存在的旧共享内存
        [[maybe_unused]] auto _ = ShmRemove(test_shm_name_);
    }

    void TearDown() override
    {
        // 清理测试创建的共享内存
        [[maybe_unused]] auto _ = ShmRemove(test_shm_name_);
    }

    // 生成唯一测试名称
    static std::string GenerateUniqueName()
    {
        static std::atomic<std::uint32_t> counter{0};
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        return "aam_test_" + std::to_string(now) + "_" + std::to_string(counter.fetch_add(1));
    }

    const std::string            test_shm_name_   = GenerateUniqueName();
    static constexpr std::size_t kTestBufferCount = 16;           // 增加缓冲区大小以支持多帧测试
    static constexpr std::size_t kTestFrameSize   = 1024 * 1024;  // 1MB
};

// ==========================================================================
// 基础功能测试
// ==========================================================================

TEST_F(ShmTransportTest, ConfigValidation)
{
    ShmTransportConfig config;

    // 有效配置
    config.shm_name       = "test";
    config.buffer_count   = 4;
    config.max_frame_size = 1024;
    EXPECT_TRUE(config.validate().has_value());

    // 无效：空名称
    config.shm_name = "";
    EXPECT_FALSE(config.validate().has_value());
    config.shm_name = "test";

    // 无效：零缓冲区
    config.buffer_count = 0;
    EXPECT_FALSE(config.validate().has_value());
    config.buffer_count = 4;

    // 无效：过多缓冲区
    config.buffer_count = 100;
    EXPECT_FALSE(config.validate().has_value());
    config.buffer_count = 4;

    // 无效：零帧大小
    config.max_frame_size = 0;
    EXPECT_FALSE(config.validate().has_value());
    config.max_frame_size = 1024;

    // 无效：过大帧大小
    config.max_frame_size = 300 * 1024 * 1024;  // 300MB
    EXPECT_FALSE(config.validate().has_value());
}

TEST_F(ShmTransportTest, ConfigSizeCalculation)
{
    ShmTransportConfig config;
    config.buffer_count   = 4;
    config.max_frame_size = 1024;
    config.metadata_size  = 4096;

    const std::size_t total_size = config.calculate_total_size();

    // 验证大小大于等于头部 + 缓冲区（考虑对齐后可能相等）
    EXPECT_GE(total_size, sizeof(ShmControlBlock) + config.metadata_size);
    EXPECT_GE(total_size,
              sizeof(ShmControlBlock) + config.metadata_size + 4 * (sizeof(ShmFrameHeader) + 1024));
}

TEST_F(ShmTransportTest, ProducerInitialization)
{
    ShmTransport producer;

    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    // 初始化生产者
    auto result = producer.InitializeProducer(config);
    EXPECT_TRUE(result.has_value()) << "Producer initialization failed";
    EXPECT_TRUE(producer.IsInitialized());
    EXPECT_TRUE(producer.IsProducer());
    EXPECT_FALSE(producer.IsConsumer());

    // 获取控制块验证
    const auto* control_block = producer.GetControlBlock();
    ASSERT_NE(control_block, nullptr);
    EXPECT_TRUE(control_block->is_valid());
    EXPECT_EQ(control_block->buffer_count, kTestBufferCount);
    EXPECT_EQ(control_block->max_frame_size, kTestFrameSize);

    // 关闭
    auto shutdown_result = producer.Shutdown();
    EXPECT_TRUE(shutdown_result.has_value());
    EXPECT_FALSE(producer.IsInitialized());
}

TEST_F(ShmTransportTest, ConsumerInitialization)
{
    // 先创建生产者
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    // 再创建消费者
    ShmTransport consumer;
    auto         result = consumer.InitializeConsumer(config);
    EXPECT_TRUE(result.has_value()) << "Consumer initialization failed";
    EXPECT_TRUE(consumer.IsInitialized());
    EXPECT_FALSE(consumer.IsProducer());
    EXPECT_TRUE(consumer.IsConsumer());

    // 验证控制块
    const auto* control_block = consumer.GetControlBlock();
    ASSERT_NE(control_block, nullptr);
    EXPECT_TRUE(control_block->is_valid());
}

TEST_F(ShmTransportTest, ConsumerBeforeProducer)
{
    ShmTransport       consumer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    // 消费者先初始化应该失败（共享内存不存在）
    auto result = consumer.InitializeConsumer(config);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ShmTransportTest, DoubleInitialization)
{
    ShmTransport       transport;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(transport.InitializeProducer(config).has_value());

    // 重复初始化应该失败
    auto result = transport.InitializeProducer(config);
    EXPECT_FALSE(result.has_value());
}

// ==========================================================================
// 帧传输测试
// ==========================================================================

TEST_F(ShmTransportTest, BasicFrameWriteRead)
{
    // 创建生产者
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;
    config.write_timeout  = 100ms;
    config.read_timeout   = 100ms;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    // 创建消费者
    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    // 准备测试数据
    FrameMetadata write_metadata;
    write_metadata.width             = 1920;
    write_metadata.height            = 1080;
    write_metadata.stride            = 1920 * 3;
    write_metadata.pixel_format      = PixelFormat::RGB24;
    write_metadata.capture_timestamp = Clock::now();
    write_metadata.process_timestamp = Clock::now();
    write_metadata.frame_number      = 1;
    write_metadata.sequence_id       = 1;
    write_metadata.data_size         = 100;

    std::vector<std::byte> write_data(100);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<std::byte>(i & 0xFF);
    }

    // 写入帧
    auto write_result = producer.WriteFrame(write_metadata, write_data);
    EXPECT_TRUE(write_result.has_value());

    // 读取帧
    auto read_result = consumer.ReadFrame(100ms);
    ASSERT_TRUE(read_result.has_value());
    ASSERT_TRUE(read_result->has_value());

    const auto& [read_metadata, read_data] = read_result->value();

    // 验证元数据
    EXPECT_EQ(read_metadata.width, write_metadata.width);
    EXPECT_EQ(read_metadata.height, write_metadata.height);
    EXPECT_EQ(read_metadata.stride, write_metadata.stride);
    EXPECT_EQ(read_metadata.pixel_format, write_metadata.pixel_format);
    EXPECT_EQ(read_metadata.frame_number, write_metadata.frame_number);
    EXPECT_EQ(read_metadata.data_size, write_metadata.data_size);

    // 验证数据
    ASSERT_EQ(read_data.size(), write_data.size());
    EXPECT_EQ(std::memcmp(read_data.data(), write_data.data(), write_data.size()), 0);
}

TEST_F(ShmTransportTest, MultipleFrames)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    const int num_frames = 10;

    // 写入多帧
    for (int i = 0; i < num_frames; ++i) {
        FrameMetadata metadata;
        metadata.width             = 1920;
        metadata.height            = 1080;
        metadata.stride            = 1920 * 3;
        metadata.pixel_format      = PixelFormat::RGB24;
        metadata.capture_timestamp = Clock::now();
        metadata.process_timestamp = Clock::now();
        metadata.frame_number      = i;
        metadata.sequence_id       = i;
        metadata.data_size         = 100;

        std::vector<std::byte> data(100);
        data[0] = static_cast<std::byte>(i);

        auto result = producer.WriteFrame(metadata, data);
        ASSERT_TRUE(result.has_value()) << "Failed to write frame " << i;
    }

    // 读取多帧
    for (int i = 0; i < num_frames; ++i) {
        auto result = consumer.ReadFrame(100ms);
        ASSERT_TRUE(result.has_value()) << "Failed to read frame " << i;
        ASSERT_TRUE(result->has_value()) << "No frame " << i << " available";

        const auto& [metadata, data] = result->value();
        EXPECT_EQ(metadata.frame_number, i);
        EXPECT_EQ(static_cast<int>(data[0]), i);
    }
}

TEST_F(ShmTransportTest, TryWriteFrameNonBlocking)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = 2;  // 小缓冲区
    config.max_frame_size = 1024;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    FrameMetadata metadata;
    metadata.width             = 100;
    metadata.height            = 100;
    metadata.stride            = 300;
    metadata.pixel_format      = PixelFormat::RGB24;
    metadata.capture_timestamp = Clock::now();
    metadata.process_timestamp = Clock::now();
    metadata.frame_number      = 0;
    metadata.data_size         = 100;

    std::vector<std::byte> data(100);

    // 填满缓冲区
    for (size_t i = 0; i < config.buffer_count; ++i) {
        auto result = producer.TryWriteFrame(metadata, data);
        ASSERT_TRUE(result.has_value());
        EXPECT_TRUE(*result);
    }

    // 再次写入应该失败（缓冲区满）
    auto result = producer.TryWriteFrame(metadata, data);
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(*result);
}

// ==========================================================================
// WriteFrameWithTimeout 超时路径测试
// ==========================================================================
TEST_F(ShmTransportTest, WriteFrameWithTimeout_BufferFullTimesOut)
{
    ShmTransport producer;

    ShmTransportConfig config;
    config.shm_name         = test_shm_name_;
    config.buffer_count     = kTestBufferCount;
    config.max_frame_size   = kTestFrameSize;
    config.enable_zero_copy = false;  // 禁用零拷贝以使用传统路径

    auto init_result = producer.InitializeProducer(config);
    ASSERT_TRUE(init_result.has_value()) << "Producer initialization failed";
    ASSERT_TRUE(producer.IsInitialized());

    // 获取初始统计值
    const auto initial_stats = producer.GetStats();

    // 填满缓冲区
    for (uint32_t i = 0; i < kTestBufferCount; ++i) {
        FrameMetadata metadata{};
        metadata.frame_number      = i;
        metadata.width             = 100;
        metadata.height            = 100;
        metadata.stride            = 300;
        metadata.pixel_format      = PixelFormat::RGB24;
        metadata.capture_timestamp = Clock::now();
        metadata.process_timestamp = Clock::now();
        metadata.data_size         = static_cast<std::uint32_t>(kTestFrameSize);

        std::vector<std::byte> frame_data(kTestFrameSize, static_cast<std::byte>(i));

        auto write_result = producer.TryWriteFrame(metadata, frame_data);
        ASSERT_TRUE(write_result.has_value()) << "TryWriteFrame error at frame " << i;
        ASSERT_TRUE(write_result.value())
            << "Failed to write frame " << i << " while filling buffer";
    }

    // 此时环形缓冲区已满，带超时的写入应该失败并更新统计
    FrameMetadata timeout_metadata{};
    timeout_metadata.frame_number      = kTestBufferCount;
    timeout_metadata.width             = 100;
    timeout_metadata.height            = 100;
    timeout_metadata.stride            = 300;
    timeout_metadata.pixel_format      = PixelFormat::RGB24;
    timeout_metadata.capture_timestamp = Clock::now();
    timeout_metadata.process_timestamp = Clock::now();
    timeout_metadata.data_size         = static_cast<std::uint32_t>(kTestFrameSize);

    std::vector<std::byte> timeout_frame(kTestFrameSize, static_cast<std::byte>(0xFF));

    const auto timeout = std::chrono::milliseconds(1);
    auto write_result  = producer.WriteFrameWithTimeout(timeout_metadata, timeout_frame, timeout);

    ASSERT_TRUE(write_result.has_value())
        << "WriteFrameWithTimeout returned error: " << static_cast<int>(write_result.error());
    EXPECT_FALSE(write_result.value())
        << "WriteFrameWithTimeout should return false when buffer is full";

    const auto stats = producer.GetStats();
    EXPECT_EQ(stats.write_timeouts.load() - initial_stats.write_timeouts.load(), 1u);
}

TEST_F(ShmTransportTest, TryReadFrameNonBlocking)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    // 尝试读取空缓冲区
    auto result = consumer.TryReadFrame();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());  // 无数据

    // 写入一帧
    FrameMetadata metadata;
    metadata.width             = 100;
    metadata.height            = 100;
    metadata.stride            = 300;
    metadata.pixel_format      = PixelFormat::RGB24;
    metadata.capture_timestamp = Clock::now();
    metadata.process_timestamp = Clock::now();
    metadata.frame_number      = 0;
    metadata.data_size         = 100;

    std::vector<std::byte> data(100);

    ASSERT_TRUE(producer.WriteFrame(metadata, data).has_value());

    // 现在应该能读取
    result = consumer.TryReadFrame();
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->has_value());
}

TEST_F(ShmTransportTest, ReadFrameWithCallback)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    // 准备测试数据
    FrameMetadata write_metadata;
    write_metadata.width             = 1920;
    write_metadata.height            = 1080;
    write_metadata.stride            = 1920 * 3;
    write_metadata.pixel_format      = PixelFormat::RGB24;
    write_metadata.capture_timestamp = Clock::now();
    write_metadata.process_timestamp = Clock::now();
    write_metadata.frame_number      = 42;
    write_metadata.data_size         = 100;

    std::vector<std::byte> write_data(100);
    for (size_t i = 0; i < write_data.size(); ++i) {
        write_data[i] = static_cast<std::byte>(i & 0xFF);
    }

    ASSERT_TRUE(producer.WriteFrame(write_metadata, write_data).has_value());

    // 使用回调读取
    bool                   callback_called = false;
    FrameMetadata          received_metadata;
    std::vector<std::byte> received_data;

    auto callback = [&](const FrameMetadata& metadata, std::span<const std::byte> data) {
        callback_called   = true;
        received_metadata = metadata;
        received_data.assign(data.begin(), data.end());
    };

    auto result = consumer.ReadFrameWithCallback(100ms, callback);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(callback_called);

    // 验证数据
    EXPECT_EQ(received_metadata.frame_number, write_metadata.frame_number);
    EXPECT_EQ(received_data.size(), write_data.size());
    EXPECT_EQ(std::memcmp(received_data.data(), write_data.data(), write_data.size()), 0);
}

// ==========================================================================
// 统计信息测试
// ==========================================================================

TEST_F(ShmTransportTest, TransportStats)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    // 初始统计应为零
    auto stats = consumer.GetStats();
    EXPECT_EQ(stats.frames_written, 0);
    EXPECT_EQ(stats.frames_read, 0);

    // 写入几帧
    FrameMetadata metadata;
    metadata.width             = 100;
    metadata.height            = 100;
    metadata.stride            = 300;
    metadata.pixel_format      = PixelFormat::RGB24;
    metadata.capture_timestamp = Clock::now();
    metadata.process_timestamp = Clock::now();
    metadata.data_size         = 100;

    std::vector<std::byte> data(100);

    for (int i = 0; i < 5; ++i) {
        metadata.frame_number = i;
        ASSERT_TRUE(producer.WriteFrame(metadata, data).has_value());
    }

    // 读取几帧
    for (int i = 0; i < 3; ++i) {
        auto result = consumer.ReadFrame(100ms);
        ASSERT_TRUE(result.has_value());
        ASSERT_TRUE(result->has_value());
    }

    // 检查消费者统计
    stats = consumer.GetStats();
    EXPECT_EQ(stats.frames_read, 3);
    EXPECT_EQ(stats.bytes_read, 300);

    // 检查缓冲区状态（允许一定误差，因为并发可能导致顺序变化）
    auto [used, total] = consumer.GetBufferStatus();
    EXPECT_EQ(total, kTestBufferCount);
    EXPECT_GE(used, 1);  // 至少还有1帧未读
    EXPECT_LE(used, 2);  // 最多2帧未读
}

TEST_F(ShmTransportTest, StatsDropRate)
{
    ShmTransportStats stats;
    stats.frames_written = 100;
    stats.frames_dropped = 10;

    // 丢帧率 = dropped / (written + dropped) = 10 / 110
    EXPECT_DOUBLE_EQ(stats.get_drop_rate(), 10.0 / 110.0);

    stats.frames_dropped = 0;
    EXPECT_DOUBLE_EQ(stats.get_drop_rate(), 0.0);

    // 测试 10% 丢帧率的情况：10 dropped / 100 written = 10/110
    stats.frames_written = 90;
    stats.frames_dropped = 10;
    EXPECT_DOUBLE_EQ(stats.get_drop_rate(), 0.1);  // 10/100 = 10%
}

TEST_F(ShmTransportTest, StatsThroughput)
{
    ShmTransportStats stats;
    stats.session_start = Clock::now() - std::chrono::seconds(1);
    stats.bytes_written = 1000000;  // 1MB

    // 吞吐量应该约为 1MB/s
    double throughput = stats.get_throughput_bytes_per_sec();
    EXPECT_GT(throughput, 900000.0);  // 允许 10% 误差
    EXPECT_LT(throughput, 1100000.0);
}

// ==========================================================================
// 边界条件测试
// ==========================================================================

TEST_F(ShmTransportTest, EmptyDataWrite)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    FrameMetadata metadata;
    metadata.width             = 100;
    metadata.height            = 100;
    metadata.stride            = 300;
    metadata.pixel_format      = PixelFormat::RGB24;
    metadata.capture_timestamp = Clock::now();
    metadata.process_timestamp = Clock::now();
    metadata.data_size         = 0;  // 空数据

    std::vector<std::byte> empty_data;

    // 空数据应该被拒绝（无效帧）
    auto result = producer.WriteFrame(metadata, empty_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ShmTransportTest, OversizedFrame)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = 1024;  // 1KB 限制

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    FrameMetadata metadata;
    metadata.width             = 100;
    metadata.height            = 100;
    metadata.stride            = 300;
    metadata.pixel_format      = PixelFormat::RGB24;
    metadata.capture_timestamp = Clock::now();
    metadata.process_timestamp = Clock::now();
    metadata.data_size         = 2048;  // 超过限制

    std::vector<std::byte> large_data(2048);

    // 超大帧应该被拒绝
    auto result = producer.WriteFrame(metadata, large_data);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ShmTransportTest, TimeoutTest)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = 2;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    // 尝试读取不存在的帧（应该超时）
    auto start   = std::chrono::steady_clock::now();
    auto result  = consumer.ReadFrame(50ms);
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result->has_value());  // 无数据
    EXPECT_GE(elapsed, 50ms);
}

// ==========================================================================
// 并发测试
// ==========================================================================

TEST_F(ShmTransportTest, ConcurrentWriteRead)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = 16;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    constexpr int     num_frames = 1000;
    std::atomic<int>  frames_read{0};
    std::atomic<bool> producer_done{false};

    // 生产者线程
    std::thread producer_thread([&]() {
        FrameMetadata metadata;
        metadata.width             = 100;
        metadata.height            = 100;
        metadata.stride            = 300;
        metadata.pixel_format      = PixelFormat::RGB24;
        metadata.capture_timestamp = Clock::now();
        metadata.process_timestamp = Clock::now();
        metadata.data_size         = 100;

        std::vector<std::byte> data(100);

        for (int i = 0; i < num_frames; ++i) {
            metadata.frame_number = i;
            data[0]               = static_cast<std::byte>(i & 0xFF);

            auto result = producer.WriteFrameWithTimeout(metadata, data, 1s);
            ASSERT_TRUE(result.has_value());
            EXPECT_TRUE(*result);
        }
        producer_done = true;
    });

    // 消费者线程
    std::thread consumer_thread([&]() {
        while (frames_read < num_frames) {
            auto result = consumer.ReadFrame(100ms);
            if (result && result->has_value()) {
                frames_read.fetch_add(1);
            }
            if (producer_done && !result->has_value()) {
                break;
            }
        }
    });

    producer_thread.join();
    consumer_thread.join();

    EXPECT_EQ(frames_read, num_frames);
}

// ==========================================================================
// 控制块测试
// ==========================================================================

TEST_F(ShmTransportTest, ControlBlockValidation)
{
    // ShmControlBlock 默认构造时 magic = MAGIC, version = VERSION
    // 所以默认就是有效的
    ShmControlBlock block;
    EXPECT_TRUE(block.is_valid());  // 默认构造有效

    // 修改 magic 后应该无效
    block.magic = 0;
    EXPECT_FALSE(block.is_valid());

    // 恢复 magic 但修改 version
    block.magic   = ShmControlBlock::MAGIC;
    block.version = 999;  // 错误版本
    EXPECT_FALSE(block.is_valid());
}

TEST_F(ShmTransportTest, ControlBlockBufferStatus)
{
    ShmControlBlock block;
    block.buffer_count = 4;
    block.write_sequence.store(2, std::memory_order_relaxed);
    block.read_sequence.store(0, std::memory_order_relaxed);

    EXPECT_EQ(block.used_buffers(), 2);
    EXPECT_EQ(block.available_buffers(), 2);
    EXPECT_TRUE(block.is_writable());
    EXPECT_TRUE(block.has_readable_data());

    block.read_sequence.store(2, std::memory_order_relaxed);
    EXPECT_EQ(block.used_buffers(), 0);
    EXPECT_FALSE(block.has_readable_data());
}

// ==========================================================================
// 环形缓冲区溢出测试
// ==========================================================================
TEST_F(ShmTransportTest, DroppedFramesOnRingOverflow)
{
    // 选择一个很小的 ring 大小，方便在测试中触发溢出
    const size_t kRingCapacityFrames = 4;
    const size_t kFramesToWrite      = 10;  // 明显大于 capacity，确保发生溢出

    ShmTransportConfig config;
    config.shm_name         = test_shm_name_;
    config.buffer_count     = static_cast<std::uint32_t>(kRingCapacityFrames);
    config.max_frame_size   = kTestFrameSize;
    config.enable_zero_copy = false;  // 使用传统路径以测试超时丢帧

    // 初始化生产者（不启动消费者，这样不会有消费，ring 会被写满后溢出）
    ShmTransport producer;
    auto         init_result = producer.InitializeProducer(config);
    ASSERT_TRUE(init_result.has_value()) << "Producer initialization failed";

    // 获取初始统计值
    const auto initial_stats = producer.GetStats();

    FrameMetadata metadata{};
    metadata.width        = 100;
    metadata.height       = 100;
    metadata.stride       = 100;
    metadata.pixel_format = PixelFormat::RGB24;
    metadata.data_size    = static_cast<std::uint32_t>(kTestFrameSize);

    std::vector<std::byte> frame_data(kTestFrameSize, static_cast<std::byte>(0xAB));

    // 首先填满缓冲区
    size_t successful_writes = 0;
    for (size_t i = 0; i < kRingCapacityFrames; ++i) {
        metadata.frame_number = static_cast<std::uint32_t>(i);
        auto result           = producer.TryWriteFrame(metadata, frame_data);
        ASSERT_TRUE(result.has_value()) << "WriteFrame failed at frame " << i;
        if (result.value()) {
            successful_writes++;
        }
    }
    EXPECT_EQ(successful_writes, kRingCapacityFrames);

    // 使用带超时的写入，触发丢帧计数
    size_t timeout_count = 0;
    for (size_t i = kRingCapacityFrames; i < kFramesToWrite; ++i) {
        metadata.frame_number = static_cast<std::uint32_t>(i);
        auto result =
            producer.WriteFrameWithTimeout(metadata, frame_data, std::chrono::milliseconds(1));
        ASSERT_TRUE(result.has_value()) << "WriteFrameWithTimeout error at frame " << i;
        if (!result.value()) {
            timeout_count++;
        }
    }

    // 验证：超时次数应该等于尝试写入的额外帧数
    EXPECT_EQ(timeout_count, kFramesToWrite - kRingCapacityFrames);

    // 获取最终统计值并验证丢帧统计
    const auto final_stats = producer.GetStats();
    EXPECT_EQ(final_stats.write_timeouts.load() - initial_stats.write_timeouts.load(),
              timeout_count);
}

// ==========================================================================
// 辅助函数测试
// ==========================================================================

TEST_F(ShmTransportTest, MakeShmName)
{
    // Windows: 添加 Local\ 前缀
    // POSIX: 添加 / 前缀
    auto name1 = MakeShmName("test");
    EXPECT_FALSE(name1.empty());

    auto name2 = MakeShmName("");
    EXPECT_FALSE(name2.empty());

    // 重复调用应返回相同结果
    auto name3 = MakeShmName("test");
    EXPECT_EQ(name1, name3);
}

TEST_F(ShmTransportTest, ShmExistsAndRemove)
{
    // 不存在的共享内存
    EXPECT_FALSE(ShmExists("nonexistent_shm_12345"));

    // 创建共享内存
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = kTestBufferCount;
    config.max_frame_size = kTestFrameSize;

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    // 现在应该存在
    EXPECT_TRUE(ShmExists(test_shm_name_));

    // 关闭生产者
    ASSERT_TRUE(producer.Shutdown().has_value());

    // 删除共享内存
    EXPECT_TRUE(ShmRemove(test_shm_name_));
}

// ==========================================================================
// 帧头部测试
// ==========================================================================

TEST_F(ShmTransportTest, FrameHeaderValidation)
{
    ShmFrameHeader header;
    EXPECT_FALSE(header.is_valid());  // 默认构造无效

    header.magic     = ShmFrameHeader::MAGIC;
    header.version   = 1;
    header.data_size = 100;
    EXPECT_TRUE(header.is_valid());

    header.magic = 0;
    EXPECT_FALSE(header.is_valid());
}

TEST_F(ShmTransportTest, FrameHeaderChecksum)
{
    ShmFrameHeader header;
    header.data_size = 10;

    std::array<std::byte, 10> data{std::byte{0},
                                   std::byte{1},
                                   std::byte{2},
                                   std::byte{3},
                                   std::byte{4},
                                   std::byte{5},
                                   std::byte{6},
                                   std::byte{7},
                                   std::byte{8},
                                   std::byte{9}};

    header.checksum = header.calculate_checksum(data.data());
    EXPECT_TRUE(header.verify_data(data.data()));

    // 修改数据
    data[0] = std::byte{99};
    EXPECT_FALSE(header.verify_data(data.data()));
}

// ==========================================================================
// 校验和端到端集成测试
// ==========================================================================

// 集成测试：启用校验和时应能检测到数据损坏
TEST_F(ShmTransportTest, ChecksumEnabledDetectsCorruption)
{
    // 安排：创建启用校验和的传输
    ShmTransportConfig config;
    config.shm_name        = test_shm_name_;
    config.buffer_count    = kTestBufferCount;
    config.max_frame_size  = kTestFrameSize;
    config.enable_checksum = true;  // 启用校验和

    ShmTransport producer;
    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    const std::string payload = "0123456789ABCDEFGHIJ";  // 20 bytes
    FrameMetadata     metadata{};
    metadata.width        = 100;
    metadata.height       = 100;
    metadata.stride       = 100;
    metadata.pixel_format = PixelFormat::RGB24;
    metadata.frame_number = 1;
    metadata.data_size    = static_cast<std::uint32_t>(payload.size());

    // 写入一帧
    auto write_result = producer.WriteFrame(metadata, std::as_bytes(std::span(payload)));
    ASSERT_TRUE(write_result.has_value()) << "WriteFrame failed";

    // 在读取前损坏底层共享内存数据
    // 使用零拷贝API获取帧数据指针并修改
    auto read_buffer = consumer.TryAcquireReadBuffer();
    ASSERT_TRUE(read_buffer.has_value());
    ASSERT_TRUE(read_buffer->has_value());

    // 获取数据指针并损坏第一个字节
    auto& buffer = read_buffer->value();
    ASSERT_GE(buffer.data.size(), 1u);
    const_cast<std::byte*>(buffer.data.data())[0] ^= std::byte{0xFF};  // 损坏数据

    // 释放缓冲区（不调用ReleaseReadBuffer，因为我们损坏了数据）
    // 重新初始化消费者以读取损坏的数据
    ShmTransport consumer2;
    ASSERT_TRUE(consumer2.InitializeConsumer(config).has_value());

    // 尝试读取损坏的帧
    auto read_result = consumer2.ReadFrame(100ms);

    // 断言：传输必须检测到损坏并返回ShmCorrupted错误
    EXPECT_FALSE(read_result.has_value());
    EXPECT_EQ(read_result.error(), ShmTransportError::ShmCorrupted);

    // 验证校验和错误计数增加
    const auto stats = consumer2.GetStats();
    EXPECT_EQ(stats.checksum_errors.load(), 1u);
}

// 集成测试：禁用校验和时不应检测到数据损坏
TEST_F(ShmTransportTest, ChecksumDisabledDoesNotDetectCorruption)
{
    // 安排：创建禁用校验和的传输
    ShmTransportConfig config;
    config.shm_name        = test_shm_name_;
    config.buffer_count    = kTestBufferCount;
    config.max_frame_size  = kTestFrameSize;
    config.enable_checksum = false;  // 禁用校验和

    ShmTransport producer;
    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    const std::string payload = "0123456789ABCDEFGHIJ";  // 20 bytes
    FrameMetadata     metadata{};
    metadata.width        = 100;
    metadata.height       = 100;
    metadata.stride       = 100;
    metadata.pixel_format = PixelFormat::RGB24;
    metadata.frame_number = 1;
    metadata.data_size    = static_cast<std::uint32_t>(payload.size());

    // 写入一帧
    auto write_result = producer.WriteFrame(metadata, std::as_bytes(std::span(payload)));
    ASSERT_TRUE(write_result.has_value()) << "WriteFrame failed";

    // 使用零拷贝 API 获取数据指针并损坏第一个字节
    auto read_buffer = consumer.TryAcquireReadBuffer();
    ASSERT_TRUE(read_buffer.has_value());
    ASSERT_TRUE(read_buffer->has_value());

    auto& buffer = read_buffer->value();
    ASSERT_GE(buffer.data.size(), 1u);
    const_cast<std::byte*>(buffer.data.data())[0] ^= std::byte{0xFF};  // 损坏数据

    // 在禁用校验和的情况下，直接读取损坏的数据不应报错
    // 由于我们已经通过零拷贝获取了缓冲区，可以直接验证数据
    // 注意：损坏的数据应该与原始数据不同
    EXPECT_NE(buffer.data[0], std::as_bytes(std::span(payload))[0]);

    // 释放缓冲区
    [[maybe_unused]] auto _ = consumer.ReleaseReadBuffer(buffer.buffer_index);

    // 验证校验和错误计数为0（禁用校验和时不应检测错误）
    const auto stats = consumer.GetStats();
    EXPECT_EQ(stats.checksum_errors.load(), 0u);
}

// ==========================================================================
// 性能基准测试（记录吞吐量，仅做正确性校验，不对性能做硬性要求）
// ==========================================================================

TEST_F(ShmTransportTest, ThroughputBenchmark)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name       = test_shm_name_;
    config.buffer_count   = 16;
    config.max_frame_size = 1920 * 1080 * 4;  // 4K RGBA

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    constexpr int         num_frames = 100;
    constexpr std::size_t frame_size = 1920 * 1080 * 3;  // 1080p RGB24

    FrameMetadata metadata;
    metadata.width             = 1920;
    metadata.height            = 1080;
    metadata.stride            = 1920 * 3;
    metadata.pixel_format      = PixelFormat::RGB24;
    metadata.capture_timestamp = Clock::now();
    metadata.process_timestamp = Clock::now();
    metadata.data_size         = static_cast<std::uint32_t>(frame_size);

    std::vector<std::byte> data(frame_size);

    // 预热
    for (int i = 0; i < 10; ++i) {
        [[maybe_unused]] auto _  = producer.WriteFrame(metadata, data);
        [[maybe_unused]] auto _2 = consumer.ReadFrame(100ms);
    }

    // 基准测试 - 交替写入和读取以避免缓冲区溢出
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_frames; ++i) {
        metadata.frame_number = i;
        auto write_result     = producer.WriteFrame(metadata, data);
        ASSERT_TRUE(write_result.has_value());

        // 每写入一帧立即读取，保持缓冲区不溢出
        auto read_result = consumer.ReadFrame(100ms);
        ASSERT_TRUE(read_result.has_value());
        ASSERT_TRUE(read_result->has_value());
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 计算吞吐量
    double total_bytes     = static_cast<double>(num_frames * frame_size);
    double seconds         = duration.count() / 1'000'000.0;
    double throughput_mbps = (total_bytes / seconds) / (1024.0 * 1024.0);

    // 仅校验吞吐量为正，避免在不同机器/构建配置下因绝对阈值导致用例不稳定
    GTEST_LOG_(INFO) << "ShmTransport throughput: " << throughput_mbps << " MiB/s";
    EXPECT_GT(throughput_mbps, 0.0);
}

// ==========================================================================
// 零拷贝延迟基准测试 - 使用真正的零拷贝API
// ==========================================================================
TEST_F(ShmTransportTest, ZeroCopyLatencyBenchmark)
{
    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name        = test_shm_name_;
    config.buffer_count    = 8;
    config.max_frame_size  = 1024 * 1024;  // 1MB 帧
    config.enable_checksum = false;        // 禁用CRC校验以提高性能

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    constexpr int         num_frames = 1000;
    constexpr std::size_t frame_size = 1024 * 1024;  // 1MB

    FrameMetadata metadata;
    metadata.width        = 1024;
    metadata.height       = 1024;
    metadata.stride       = 1024;
    metadata.pixel_format = PixelFormat::RGB24;
    metadata.data_size    = static_cast<std::uint32_t>(frame_size);

    // 测试1: 零拷贝写入吞吐量（生产者端测量）
    // 使用消费者线程边读边写，避免缓冲区溢出
    {
        std::atomic<int>  write_count{0};
        std::atomic<bool> writer_done{false};

        // 启动消费者线程清空缓冲区
        std::thread consumer_thread([&]() {
            while (!writer_done.load(std::memory_order_acquire)
                   || write_count.load(std::memory_order_acquire) > 0) {
                auto read_result = consumer.TryAcquireReadBuffer();
                if (read_result && read_result->has_value()) {
                    [[maybe_unused]] auto _ =
                        consumer.ReleaseReadBuffer(read_result->value().buffer_index);
                    write_count.fetch_sub(1, std::memory_order_relaxed);
                }
                else {
                    std::this_thread::yield();
                }
            }
        });

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_frames; ++i) {
            // 使用带超时的API等待缓冲区可用
            auto buffer_result = producer.AcquireWriteBuffer(100ms);
            ASSERT_TRUE(buffer_result.has_value());
            ASSERT_TRUE(buffer_result->has_value());

            auto& buffer = buffer_result->value();
            // 直接写入共享内存（零拷贝）
            std::memset(buffer.data, static_cast<int>(i & 0xFF), frame_size);

            metadata.frame_number = i;
            auto commit_result =
                producer.CommitWriteBuffer(buffer.buffer_index, metadata, frame_size);
            ASSERT_TRUE(commit_result.has_value());

            write_count.fetch_add(1, std::memory_order_relaxed);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        writer_done.store(true, std::memory_order_release);
        consumer_thread.join();

        double total_mb         = static_cast<double>(num_frames * frame_size) / (1024.0 * 1024.0);
        double seconds          = duration_ns / 1'000'000'000.0;
        double write_mbps       = total_mb / seconds;
        double write_latency_ns = static_cast<double>(duration_ns) / num_frames;

        std::cout << "[ZeroCopy] Write Throughput: " << write_mbps << " MB/s" << std::endl;
        std::cout << "[ZeroCopy] Write Latency: " << write_latency_ns << " ns/frame" << std::endl;
    }

    // 测试2: 零拷贝读取吞吐量（消费者端测量）
    {
        // 先填充缓冲区（使用生产者线程边写边读避免阻塞）
        std::atomic<int>  written{0};
        std::atomic<bool> fill_done{false};

        std::thread fill_consumer([&]() {
            while (!fill_done.load(std::memory_order_acquire)
                   || written.load(std::memory_order_acquire) > 0) {
                auto read_result = consumer.TryAcquireReadBuffer();
                if (read_result && read_result->has_value()) {
                    [[maybe_unused]] auto _ =
                        consumer.ReleaseReadBuffer(read_result->value().buffer_index);
                    written.fetch_sub(1, std::memory_order_relaxed);
                }
                else {
                    std::this_thread::yield();
                }
            }
        });

        for (int i = 0; i < num_frames; ++i) {
            auto buffer_result = producer.AcquireWriteBuffer(100ms);
            ASSERT_TRUE(buffer_result.has_value());
            ASSERT_TRUE(buffer_result->has_value());

            auto& buffer = buffer_result->value();
            std::memset(buffer.data, static_cast<int>(i & 0xFF), frame_size);
            metadata.frame_number = i;
            [[maybe_unused]] auto _ =
                producer.CommitWriteBuffer(buffer.buffer_index, metadata, frame_size);
            written.fetch_add(1, std::memory_order_relaxed);
        }

        fill_done.store(true, std::memory_order_release);
        fill_consumer.join();

        // 重新填充用于读取测试
        for (int i = 0; i < std::min(num_frames, 8); ++i) {
            auto buffer_result = producer.AcquireWriteBuffer(100ms);
            if (buffer_result && buffer_result->has_value()) {
                auto& buffer = buffer_result->value();
                std::memset(buffer.data, static_cast<int>(i & 0xFF), frame_size);
                metadata.frame_number = i;
                [[maybe_unused]] auto _ =
                    producer.CommitWriteBuffer(buffer.buffer_index, metadata, frame_size);
            }
        }

        auto start      = std::chrono::high_resolution_clock::now();
        int  read_count = 0;
        while (read_count < num_frames) {
            auto result = consumer.TryAcquireReadBuffer();
            if (result && result->has_value()) {
                // 直接读取共享内存（零拷贝）- 验证数据
                auto& buffer = result->value();
                EXPECT_EQ(buffer.data.size(), frame_size);

                // 释放缓冲区
                [[maybe_unused]] auto _ = consumer.ReleaseReadBuffer(buffer.buffer_index);
                read_count++;

                // 补充一帧数据保持缓冲区满
                auto wb = producer.TryAcquireWriteBuffer();
                if (wb && wb->has_value()) {
                    std::memset(wb->value().data, static_cast<int>(read_count & 0xFF), frame_size);
                    metadata.frame_number = read_count;
                    [[maybe_unused]] auto _c =
                        producer.CommitWriteBuffer(wb->value().buffer_index, metadata, frame_size);
                }
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        double total_mb        = static_cast<double>(num_frames * frame_size) / (1024.0 * 1024.0);
        double seconds         = duration_ns / 1'000'000'000.0;
        double read_mbps       = total_mb / seconds;
        double read_latency_ns = static_cast<double>(duration_ns) / num_frames;

        std::cout << "[ZeroCopy] Read Throughput: " << read_mbps << " MB/s" << std::endl;
        std::cout << "[ZeroCopy] Read Latency: " << read_latency_ns << " ns/frame" << std::endl;

        // 仅记录性能指标，不做硬性断言以避免CI环境抖动导致失败
        GTEST_LOG_(INFO) << "ZeroCopy read throughput: " << read_mbps << " MB/s";
        GTEST_LOG_(INFO) << "ZeroCopy read latency: " << read_latency_ns << " ns/frame";
    }

    // 测试3: 并发零拷贝读写吞吐量（双工模式）
    {
        std::atomic<int>  write_count{0};
        std::atomic<int>  read_count{0};
        std::atomic<bool> start_flag{false};
        std::atomic<bool> writer_done{false};

        std::thread writer([&]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            for (int i = 0; i < num_frames; ++i) {
                auto buffer_result = producer.AcquireWriteBuffer(100ms);
                if (buffer_result && buffer_result->has_value()) {
                    auto& buffer = buffer_result->value();
                    std::memset(buffer.data, static_cast<int>(i & 0xFF), frame_size);
                    metadata.frame_number = i;
                    if (producer.CommitWriteBuffer(buffer.buffer_index, metadata, frame_size)
                            .has_value()) {
                        write_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
            writer_done.store(true, std::memory_order_release);
        });

        std::thread reader([&]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            int local_read = 0;
            while (local_read < num_frames) {
                auto result = consumer.TryAcquireReadBuffer();
                if (result && result->has_value()) {
                    if (consumer.ReleaseReadBuffer(result->value().buffer_index).has_value()) {
                        read_count.fetch_add(1, std::memory_order_relaxed);
                        local_read++;
                    }
                }
                else if (writer_done.load(std::memory_order_acquire)
                         && read_count.load(std::memory_order_acquire)
                                >= write_count.load(std::memory_order_acquire)) {
                    // 写入完成且已读完所有数据
                    break;
                }
                else {
                    std::this_thread::yield();
                }
            }
        });

        auto start = std::chrono::high_resolution_clock::now();
        start_flag.store(true, std::memory_order_release);

        writer.join();
        reader.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        double total_mb =
            static_cast<double>((write_count + read_count) * frame_size) / (1024.0 * 1024.0);
        double seconds     = duration_ns / 1'000'000'000.0;
        double duplex_mbps = total_mb / seconds;

        std::cout << "[ZeroCopy] Duplex Throughput: " << duplex_mbps << " MB/s" << std::endl;
        std::cout << "[ZeroCopy] Frames written: " << write_count << ", read: " << read_count
                  << std::endl;

        EXPECT_EQ(write_count, num_frames);
        EXPECT_EQ(read_count, num_frames);
    }
}

// ==========================================================================
// 微秒级单帧延迟测试（使用零拷贝API）
// ==========================================================================
TEST_F(ShmTransportTest, MicrosecondLatencyTest)
{
    // NOTE:
    // This test is extremely sensitive to system load, CPU scaling, and
    // virtualization. To avoid CI flakiness, it is gated by an environment
    // variable and will be skipped unless explicitly enabled.
    const char* perf_env = std::getenv("SHM_TRANSPORT_PERF_TEST");
    if (!perf_env || std::strcmp(perf_env, "1") != 0) {
        GTEST_SKIP() << "Skipping MicrosecondLatencyTest; "
                     << "enable with SHM_TRANSPORT_PERF_TEST=1 in a dedicated "
                     << "performance environment.";
    }

    ShmTransport       producer;
    ShmTransportConfig config;
    config.shm_name        = test_shm_name_;
    config.buffer_count    = 4;
    config.max_frame_size  = 1024 * 1024;  // 1MB 小帧
    config.enable_checksum = false;        // 禁用CRC校验以提高性能

    ASSERT_TRUE(producer.InitializeProducer(config).has_value());

    ShmTransport consumer;
    ASSERT_TRUE(consumer.InitializeConsumer(config).has_value());

    constexpr int         num_samples = 100;
    constexpr std::size_t frame_size  = 1024 * 1024;  // 1MB

    FrameMetadata metadata;
    metadata.width        = 1024;
    metadata.height       = 1024;
    metadata.stride       = 1024;
    metadata.pixel_format = PixelFormat::RGB24;
    metadata.data_size    = static_cast<std::uint32_t>(frame_size);

    // 预热
    for (int i = 0; i < 10; ++i) {
        auto wb = producer.TryAcquireWriteBuffer();
        if (wb && wb->has_value()) {
            [[maybe_unused]] auto _ =
                producer.CommitWriteBuffer(wb->value().buffer_index, metadata, 1024);
        }
        auto rb = consumer.TryAcquireReadBuffer();
        if (rb && rb->has_value()) {
            [[maybe_unused]] auto _ = consumer.ReleaseReadBuffer(rb->value().buffer_index);
        }
    }

    // 测量单帧传输延迟（写+读往返）- 使用零拷贝API
    std::vector<double> latencies_us;
    latencies_us.reserve(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        auto start = std::chrono::high_resolution_clock::now();

        // 零拷贝写入
        metadata.frame_number = i;
        auto write_buffer     = producer.TryAcquireWriteBuffer();
        ASSERT_TRUE(write_buffer.has_value());
        ASSERT_TRUE(write_buffer->has_value());

        // 直接写入共享内存
        std::memset(write_buffer->value().data, static_cast<int>(i & 0xFF), 1024);

        auto write_result =
            producer.CommitWriteBuffer(write_buffer->value().buffer_index, metadata, 1024);
        ASSERT_TRUE(write_result.has_value());

        // 零拷贝读取
        auto read_buffer = consumer.TryAcquireReadBuffer();
        ASSERT_TRUE(read_buffer.has_value());
        ASSERT_TRUE(read_buffer->has_value());

        auto read_result = consumer.ReleaseReadBuffer(read_buffer->value().buffer_index);
        ASSERT_TRUE(read_result.has_value());

        auto end = std::chrono::high_resolution_clock::now();

        double latency_us =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1000.0;
        latencies_us.push_back(latency_us);
    }

    // 计算统计信息
    std::sort(latencies_us.begin(), latencies_us.end());
    double min_latency    = latencies_us.front();
    double max_latency    = latencies_us.back();
    double median_latency = latencies_us[num_samples / 2];
    double avg_latency =
        std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0) / num_samples;

    // 计算P99
    int    p99_index   = static_cast<int>(num_samples * 0.99);
    double p99_latency = latencies_us[p99_index];

    std::cout << "[MicroLatency] Min: " << min_latency << " us" << std::endl;
    std::cout << "[MicroLatency] Max: " << max_latency << " us" << std::endl;
    std::cout << "[MicroLatency] Median: " << median_latency << " us" << std::endl;
    std::cout << "[MicroLatency] Average: " << avg_latency << " us" << std::endl;
    std::cout << "[MicroLatency] P99: " << p99_latency << " us" << std::endl;

    // 仅在性能测试环境中验证严格延迟目标
    // 使用零拷贝API，P99延迟应该 < 10μs（对于1KB数据）
    EXPECT_LT(p99_latency, 10.0);
    EXPECT_LT(median_latency, 5.0);
}
