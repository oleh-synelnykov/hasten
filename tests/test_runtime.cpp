#include "gtest/gtest.h"
#include "hasten/runtime/context.hpp"
#include "hasten/runtime/executor.hpp"
#include "hasten/runtime/frame.hpp"
#include "hasten/runtime/uds.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(RuntimeFrameTest, EncodeDecodeRoundTrip)
{
    hasten::runtime::FrameHeader header;
    header.type = hasten::runtime::FrameType::Ping;
    header.flags = hasten::runtime::FrameFlagEndStream;
    header.length = 123;
    header.stream_id = 0x123456789ABCDEF0ULL;

    std::array<std::uint8_t, hasten::runtime::FrameHeaderSize> buffer{};
    auto encode_result = hasten::runtime::encode_header(header, buffer);
    ASSERT_TRUE(encode_result);

    auto decoded = hasten::runtime::decode_header(buffer);
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->type, header.type);
    EXPECT_EQ(decoded->flags, header.flags);
    EXPECT_EQ(decoded->length, header.length);
    EXPECT_EQ(decoded->stream_id, header.stream_id);
}

TEST(RuntimeUdsTest, SendReceiveFrameAcrossChannel)
{
    auto channels = hasten::runtime::uds::socket_pair();
    ASSERT_TRUE(channels) << channels.error().message;
    auto& endpoints = *channels;
    auto server_channel = endpoints.first;
    auto client_channel = endpoints.second;

    hasten::runtime::Frame outbound;
    outbound.header.type = hasten::runtime::FrameType::Ping;
    outbound.header.stream_id = 7;
    outbound.payload = {1, 2, 3, 4};
    ASSERT_TRUE(client_channel->send(outbound));

    auto inbound = server_channel->receive();
    ASSERT_TRUE(inbound);
    EXPECT_EQ(inbound->header.type, hasten::runtime::FrameType::Ping);
    EXPECT_EQ(inbound->header.stream_id, outbound.header.stream_id);
    EXPECT_EQ(inbound->payload, outbound.payload);

    // Send response in the other direction.
    hasten::runtime::Frame reply;
    reply.header.type = hasten::runtime::FrameType::Settings;
    reply.header.stream_id = 9;
    reply.payload = {static_cast<std::uint8_t>(hasten::runtime::Encoding::Hb1)};
    ASSERT_TRUE(server_channel->send(reply));

    auto response = client_channel->receive();
    ASSERT_TRUE(response);
    EXPECT_EQ(response->header.type, hasten::runtime::FrameType::Settings);
    EXPECT_EQ(response->payload, reply.payload);
}

TEST(RuntimeContextTest, ProcessesSettingsHandshake)
{
    testing::internal::CaptureStderr();

    hasten::runtime::ContextConfig cfg;
    cfg.managed_reactor = false;
    cfg.worker_threads = 1;

    hasten::runtime::Context server{cfg};
    hasten::runtime::Context client{cfg};

    auto exec = std::make_shared<hasten::runtime::InlineExecutor>();
    server.set_executor(exec);
    client.set_executor(exec);

    auto channels = hasten::runtime::uds::socket_pair();
    ASSERT_TRUE(channels) << channels.error().message;
    auto server_attach = server.attach_channel(channels->first, true);
    ASSERT_TRUE(server_attach) << server_attach.error().message;
    auto client_attach = client.attach_channel(channels->second, false);
    ASSERT_TRUE(client_attach) << client_attach.error().message;

    bool server_seen = false;
    bool client_seen = false;
    for (int i = 0; i < 20 && (!server_seen || !client_seen); ++i) {
        server_seen = server_seen || (server.poll() > 0);
        client_seen = client_seen || (client.poll() > 0);
        std::this_thread::sleep_for(10ms);
    }

    EXPECT_TRUE(server_seen);
    EXPECT_TRUE(client_seen);

    client.stop();
    server.stop();
    client.join();
    server.join();

    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("Channel closed"), std::string::npos);
}

TEST(RuntimeContextTest, DataFramesAreDispatchedViaExecutor)
{
    testing::internal::CaptureStderr();

    struct RecordingExecutor : hasten::runtime::Executor {
        void schedule(std::function<void()> fn) override
        {
            scheduled.fetch_add(1, std::memory_order_relaxed);
            fn();
        }
        std::atomic<int> scheduled{0};
    };

    hasten::runtime::ContextConfig cfg;
    cfg.managed_reactor = false;

    hasten::runtime::Context server{cfg};
    hasten::runtime::Context client{cfg};

    auto recording = std::make_shared<RecordingExecutor>();
    server.set_executor(recording);

    auto channels = hasten::runtime::uds::socket_pair();
    ASSERT_TRUE(channels) << channels.error().message;
    auto server_attach = server.attach_channel(channels->first, true);
    ASSERT_TRUE(server_attach) << server_attach.error().message;
    auto client_channel = channels->second;
    ASSERT_TRUE(client.attach_channel(client_channel, false));

    for (int i = 0; i < 10; ++i) {
        server.poll();
        client.poll();
        std::this_thread::sleep_for(5ms);
    }

    hasten::runtime::Frame frame;
    frame.header.type = hasten::runtime::FrameType::Data;
    frame.header.stream_id = 42;
    frame.payload = {0x01, 0x02};
    ASSERT_TRUE(client_channel->send(frame));

    bool dispatched = false;
    for (int i = 0; i < 50; ++i) {
        if (server.poll() > 0 && recording->scheduled.load(std::memory_order_relaxed) > 0) {
            dispatched = true;
            break;
        }
        std::this_thread::sleep_for(5ms);
    }

    EXPECT_TRUE(dispatched);

    client.stop();
    server.stop();
    client.join();
    server.join();

    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("received DATA frame stream=42 len=2 (no handler bound)"), std::string::npos);
    EXPECT_NE(output.find("Channel closed"), std::string::npos);
}
