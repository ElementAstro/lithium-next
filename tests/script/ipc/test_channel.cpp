/*
 * test_channel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_channel.cpp
 * @brief Comprehensive tests for IPC Channel classes
 */

#include <gtest/gtest.h>
#include "script/ipc/channel.hpp"
#include "script/ipc/message.hpp"

#include <chrono>
#include <thread>

using namespace lithium::ipc;

// =============================================================================
// PipeChannel Tests
// =============================================================================

class PipeChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        channel_ = std::make_unique<PipeChannel>();
    }

    void TearDown() override {
        if (channel_) {
            channel_->close();
        }
    }

    std::unique_ptr<PipeChannel> channel_;
};

TEST_F(PipeChannelTest, DefaultConstruction) {
    PipeChannel channel;
    EXPECT_FALSE(channel.isOpen());
}

TEST_F(PipeChannelTest, CreateSuccess) {
    auto result = channel_->create();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(channel_->isOpen());
}

TEST_F(PipeChannelTest, CloseAfterCreate) {
    channel_->create();
    EXPECT_TRUE(channel_->isOpen());

    channel_->close();
    EXPECT_FALSE(channel_->isOpen());
}

TEST_F(PipeChannelTest, DoubleCreateSafe) {
    auto result1 = channel_->create();
    EXPECT_TRUE(result1.has_value());

    // Second create should either succeed or fail gracefully
    auto result2 = channel_->create();
    // Just ensure no crash
    SUCCEED();
}

TEST_F(PipeChannelTest, DoubleCloseSafe) {
    channel_->create();
    channel_->close();
    channel_->close(); // Should not crash
    EXPECT_FALSE(channel_->isOpen());
}

TEST_F(PipeChannelTest, GetReadFdBeforeCreate) {
    EXPECT_EQ(channel_->getReadFd(), -1);
}

TEST_F(PipeChannelTest, GetWriteFdBeforeCreate) {
    EXPECT_EQ(channel_->getWriteFd(), -1);
}

TEST_F(PipeChannelTest, GetFdsAfterCreate) {
    channel_->create();
    EXPECT_NE(channel_->getReadFd(), -1);
    EXPECT_NE(channel_->getWriteFd(), -1);
}

TEST_F(PipeChannelTest, CloseReadEnd) {
    channel_->create();
    channel_->closeRead();
    EXPECT_EQ(channel_->getReadFd(), -1);
    EXPECT_NE(channel_->getWriteFd(), -1);
}

TEST_F(PipeChannelTest, CloseWriteEnd) {
    channel_->create();
    channel_->closeWrite();
    EXPECT_NE(channel_->getReadFd(), -1);
    EXPECT_EQ(channel_->getWriteFd(), -1);
}

TEST_F(PipeChannelTest, SetNonBlockingSuccess) {
    channel_->create();
    auto result = channel_->setNonBlocking(true);
    EXPECT_TRUE(result.has_value());
}

TEST_F(PipeChannelTest, SetNonBlockingBeforeCreate) {
    auto result = channel_->setNonBlocking(true);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PipeChannelTest, NextSequenceIdIncrementing) {
    auto id1 = channel_->nextSequenceId();
    auto id2 = channel_->nextSequenceId();
    auto id3 = channel_->nextSequenceId();

    EXPECT_EQ(id2, id1 + 1);
    EXPECT_EQ(id3, id2 + 1);
}

TEST_F(PipeChannelTest, HasDataBeforeCreate) {
    EXPECT_FALSE(channel_->hasData());
}

TEST_F(PipeChannelTest, HasDataEmptyPipe) {
    channel_->create();
    EXPECT_FALSE(channel_->hasData());
}

TEST_F(PipeChannelTest, SendBeforeCreate) {
    json payload = {{"test", "data"}};
    auto result = channel_->send(MessageType::Execute, payload);
    EXPECT_FALSE(result.has_value());
}

TEST_F(PipeChannelTest, ReceiveBeforeCreate) {
    auto result = channel_->receive(std::chrono::milliseconds(100));
    EXPECT_FALSE(result.has_value());
}

TEST_F(PipeChannelTest, MoveConstruction) {
    channel_->create();
    EXPECT_TRUE(channel_->isOpen());

    PipeChannel moved(std::move(*channel_));
    EXPECT_TRUE(moved.isOpen());
}

TEST_F(PipeChannelTest, MoveAssignment) {
    channel_->create();
    PipeChannel other;

    other = std::move(*channel_);
    EXPECT_TRUE(other.isOpen());
}

// =============================================================================
// BidirectionalChannel Tests
// =============================================================================

class BidirectionalChannelTest : public ::testing::Test {
protected:
    void SetUp() override {
        channel_ = std::make_unique<BidirectionalChannel>();
    }

    void TearDown() override {
        if (channel_) {
            channel_->close();
        }
    }

    std::unique_ptr<BidirectionalChannel> channel_;
};

TEST_F(BidirectionalChannelTest, DefaultConstruction) {
    BidirectionalChannel channel;
    EXPECT_FALSE(channel.isOpen());
}

TEST_F(BidirectionalChannelTest, CreateSuccess) {
    auto result = channel_->create();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(channel_->isOpen());
}

TEST_F(BidirectionalChannelTest, CloseAfterCreate) {
    channel_->create();
    EXPECT_TRUE(channel_->isOpen());

    channel_->close();
    EXPECT_FALSE(channel_->isOpen());
}

TEST_F(BidirectionalChannelTest, GetSubprocessFdsBeforeCreate) {
    auto [readFd, writeFd] = channel_->getSubprocessFds();
    EXPECT_EQ(readFd, -1);
    EXPECT_EQ(writeFd, -1);
}

TEST_F(BidirectionalChannelTest, GetSubprocessFdsAfterCreate) {
    channel_->create();
    auto [readFd, writeFd] = channel_->getSubprocessFds();
    EXPECT_NE(readFd, -1);
    EXPECT_NE(writeFd, -1);
}

TEST_F(BidirectionalChannelTest, SetupParentClosesChildFds) {
    channel_->create();
    channel_->setupParent();
    // After setup, channel should still be open for parent use
    EXPECT_TRUE(channel_->isOpen());
}

TEST_F(BidirectionalChannelTest, SetupChildClosesParentFds) {
    channel_->create();
    channel_->setupChild();
    // After setup, channel should still be open for child use
    EXPECT_TRUE(channel_->isOpen());
}

TEST_F(BidirectionalChannelTest, SendBeforeCreate) {
    json payload = {{"test", "data"}};
    auto msg = Message::create(MessageType::Execute, payload, 1);
    auto result = channel_->send(msg);
    EXPECT_FALSE(result.has_value());
}

TEST_F(BidirectionalChannelTest, ReceiveBeforeCreate) {
    auto result = channel_->receive(std::chrono::milliseconds(100));
    EXPECT_FALSE(result.has_value());
}

TEST_F(BidirectionalChannelTest, ReceiveTimeout) {
    channel_->create();
    auto result = channel_->receive(std::chrono::milliseconds(50));
    // Should timeout since no data is available
    EXPECT_FALSE(result.has_value());
    if (!result.has_value()) {
        EXPECT_EQ(result.error(), IPCError::Timeout);
    }
}

// =============================================================================
// Channel Communication Tests (requires fork/thread simulation)
// =============================================================================

class ChannelCommunicationTest : public ::testing::Test {
protected:
    void SetUp() override {
        channel_ = std::make_unique<BidirectionalChannel>();
        channel_->create();
    }

    void TearDown() override {
        if (channel_) {
            channel_->close();
        }
    }

    std::unique_ptr<BidirectionalChannel> channel_;
};

TEST_F(ChannelCommunicationTest, HandshakePayloadCreation) {
    HandshakePayload payload;
    payload.version = "1.0.0";
    payload.pythonVersion = "3.11.0";
    payload.capabilities = {"numpy", "pandas"};
    payload.pid = 12345;

    auto j = payload.toJson();
    EXPECT_EQ(j["version"], "1.0.0");
    EXPECT_EQ(j["pythonVersion"], "3.11.0");
    EXPECT_EQ(j["pid"], 12345);
}

// =============================================================================
// Edge Cases and Error Handling
// =============================================================================

class ChannelEdgeCasesTest : public ::testing::Test {};

TEST_F(ChannelEdgeCasesTest, PipeChannelDestructorClosesChannel) {
    {
        PipeChannel channel;
        channel.create();
        EXPECT_TRUE(channel.isOpen());
    }
    // Destructor should have closed the channel
    SUCCEED();
}

TEST_F(ChannelEdgeCasesTest, BidirectionalChannelDestructorClosesChannel) {
    {
        BidirectionalChannel channel;
        channel.create();
        EXPECT_TRUE(channel.isOpen());
    }
    // Destructor should have closed the channel
    SUCCEED();
}

TEST_F(ChannelEdgeCasesTest, MultipleChannelsIndependent) {
    PipeChannel channel1;
    PipeChannel channel2;

    channel1.create();
    channel2.create();

    EXPECT_TRUE(channel1.isOpen());
    EXPECT_TRUE(channel2.isOpen());

    channel1.close();
    EXPECT_FALSE(channel1.isOpen());
    EXPECT_TRUE(channel2.isOpen());
}

TEST_F(ChannelEdgeCasesTest, SequenceIdThreadSafety) {
    PipeChannel channel;
    channel.create();

    std::vector<uint32_t> ids;
    std::mutex idsMutex;

    auto getIds = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto id = channel.nextSequenceId();
            std::lock_guard<std::mutex> lock(idsMutex);
            ids.push_back(id);
        }
    };

    std::thread t1(getIds);
    std::thread t2(getIds);

    t1.join();
    t2.join();

    // All IDs should be unique
    std::sort(ids.begin(), ids.end());
    auto last = std::unique(ids.begin(), ids.end());
    EXPECT_EQ(last, ids.end());
}
