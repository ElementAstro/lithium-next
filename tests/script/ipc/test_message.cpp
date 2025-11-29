/*
 * test_message.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_message.cpp
 * @brief Comprehensive tests for IPC Message structures
 */

#include <gtest/gtest.h>
#include "script/ipc/message.hpp"

#include <chrono>
#include <vector>

using namespace lithium::ipc;

// =============================================================================
// MessageHeader Tests
// =============================================================================

class MessageHeaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        header_.magic = MessageHeader::MAGIC;
        header_.version = MessageHeader::VERSION;
        header_.type = MessageType::Execute;
        header_.payloadSize = 100;
        header_.sequenceId = 42;
        header_.flags = 0;
        header_.reserved = 0;
    }

    MessageHeader header_;
};

TEST_F(MessageHeaderTest, DefaultConstruction) {
    MessageHeader h;
    EXPECT_EQ(h.magic, MessageHeader::MAGIC);
    EXPECT_EQ(h.version, MessageHeader::VERSION);
    EXPECT_EQ(h.payloadSize, 0);
    EXPECT_EQ(h.sequenceId, 0);
    EXPECT_EQ(h.flags, 0);
    EXPECT_EQ(h.reserved, 0);
}

TEST_F(MessageHeaderTest, IsValidWithCorrectMagicAndVersion) {
    EXPECT_TRUE(header_.isValid());
}

TEST_F(MessageHeaderTest, IsInvalidWithWrongMagic) {
    header_.magic = 0x12345678;
    EXPECT_FALSE(header_.isValid());
}

TEST_F(MessageHeaderTest, IsInvalidWithWrongVersion) {
    header_.version = 99;
    EXPECT_FALSE(header_.isValid());
}

TEST_F(MessageHeaderTest, SerializeProducesCorrectSize) {
    auto serialized = header_.serialize();
    EXPECT_EQ(serialized.size(), MessageHeader::SIZE);
}

TEST_F(MessageHeaderTest, SerializeDeserializeRoundTrip) {
    auto serialized = header_.serialize();
    auto result = MessageHeader::deserialize(serialized);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->magic, header_.magic);
    EXPECT_EQ(result->version, header_.version);
    EXPECT_EQ(result->type, header_.type);
    EXPECT_EQ(result->payloadSize, header_.payloadSize);
    EXPECT_EQ(result->sequenceId, header_.sequenceId);
}

TEST_F(MessageHeaderTest, DeserializeFailsWithTooSmallData) {
    std::vector<uint8_t> smallData(5, 0);
    auto result = MessageHeader::deserialize(smallData);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageHeaderTest, DeserializeFailsWithInvalidMagic) {
    auto serialized = header_.serialize();
    // Corrupt magic number
    serialized[0] = 0xFF;
    serialized[1] = 0xFF;
    auto result = MessageHeader::deserialize(serialized);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageHeaderTest, HeaderSizeConstant) {
    EXPECT_EQ(MessageHeader::SIZE, 16);
}

TEST_F(MessageHeaderTest, MagicConstant) {
    EXPECT_EQ(MessageHeader::MAGIC, 0x4C495448);
}

TEST_F(MessageHeaderTest, VersionConstant) {
    EXPECT_EQ(MessageHeader::VERSION, 1);
}

// =============================================================================
// Message Tests
// =============================================================================

class MessageTest : public ::testing::Test {
protected:
    json testPayload_ = {{"key", "value"}, {"number", 42}};
};

TEST_F(MessageTest, CreateWithJsonPayload) {
    auto msg = Message::create(MessageType::Execute, testPayload_, 1);

    EXPECT_EQ(msg.header.type, MessageType::Execute);
    EXPECT_EQ(msg.header.sequenceId, 1);
    EXPECT_GT(msg.payload.size(), 0);
}

TEST_F(MessageTest, CreateWithBinaryPayload) {
    std::vector<uint8_t> binaryData = {0x01, 0x02, 0x03, 0x04};
    auto msg = Message::create(MessageType::DataChunk, binaryData, 5);

    EXPECT_EQ(msg.header.type, MessageType::DataChunk);
    EXPECT_EQ(msg.header.sequenceId, 5);
    EXPECT_EQ(msg.payload, binaryData);
}

TEST_F(MessageTest, GetPayloadAsJsonSuccess) {
    auto msg = Message::create(MessageType::Execute, testPayload_, 1);
    auto result = msg.getPayloadAsJson();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->at("key"), "value");
    EXPECT_EQ(result->at("number"), 42);
}

TEST_F(MessageTest, SerializeDeserializeRoundTrip) {
    auto original = Message::create(MessageType::Result, testPayload_, 10);
    auto serialized = original.serialize();
    auto result = Message::deserialize(serialized);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->header.type, original.header.type);
    EXPECT_EQ(result->header.sequenceId, original.header.sequenceId);
    EXPECT_EQ(result->payload, original.payload);
}

TEST_F(MessageTest, DeserializeFailsWithEmptyData) {
    std::vector<uint8_t> emptyData;
    auto result = Message::deserialize(emptyData);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageTest, DeserializeFailsWithTruncatedData) {
    auto original = Message::create(MessageType::Execute, testPayload_, 1);
    auto serialized = original.serialize();
    // Truncate the data
    serialized.resize(MessageHeader::SIZE / 2);
    auto result = Message::deserialize(serialized);
    EXPECT_FALSE(result.has_value());
}

TEST_F(MessageTest, CreateWithEmptyPayload) {
    json emptyJson = json::object();
    auto msg = Message::create(MessageType::Heartbeat, emptyJson, 0);

    EXPECT_EQ(msg.header.type, MessageType::Heartbeat);
    auto result = msg.getPayloadAsJson();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST_F(MessageTest, CreateWithLargePayload) {
    json largePayload;
    for (int i = 0; i < 1000; ++i) {
        largePayload["key" + std::to_string(i)] = std::string(100, 'x');
    }

    auto msg = Message::create(MessageType::DataChunk, largePayload, 1);
    EXPECT_GT(msg.payload.size(), 0);

    auto result = msg.getPayloadAsJson();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 1000);
}

// =============================================================================
// ExecuteRequest Tests
// =============================================================================

class ExecuteRequestTest : public ::testing::Test {
protected:
    void SetUp() override {
        request_.scriptContent = "print('hello')";
        request_.scriptPath = "/path/to/script.py";
        request_.functionName = "main";
        request_.arguments = {{"arg1", "value1"}};
        request_.timeout = std::chrono::seconds(60);
        request_.captureOutput = true;
        request_.allowedImports = {"os", "sys"};
        request_.workingDirectory = "/tmp";
    }

    ExecuteRequest request_;
};

TEST_F(ExecuteRequestTest, ToJsonContainsAllFields) {
    auto j = request_.toJson();

    EXPECT_EQ(j["scriptContent"], "print('hello')");
    EXPECT_EQ(j["scriptPath"], "/path/to/script.py");
    EXPECT_EQ(j["functionName"], "main");
    EXPECT_EQ(j["arguments"]["arg1"], "value1");
    EXPECT_EQ(j["timeout"], 60);
    EXPECT_EQ(j["captureOutput"], true);
    EXPECT_EQ(j["workingDirectory"], "/tmp");
}

TEST_F(ExecuteRequestTest, FromJsonRoundTrip) {
    auto j = request_.toJson();
    auto result = ExecuteRequest::fromJson(j);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scriptContent, request_.scriptContent);
    EXPECT_EQ(result->scriptPath, request_.scriptPath);
    EXPECT_EQ(result->functionName, request_.functionName);
    EXPECT_EQ(result->captureOutput, request_.captureOutput);
    EXPECT_EQ(result->workingDirectory, request_.workingDirectory);
}

TEST_F(ExecuteRequestTest, FromJsonWithMissingOptionalFields) {
    json j = {{"scriptContent", "test"}};
    auto result = ExecuteRequest::fromJson(j);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->scriptContent, "test");
    EXPECT_TRUE(result->scriptPath.empty());
}

TEST_F(ExecuteRequestTest, DefaultTimeout) {
    ExecuteRequest req;
    EXPECT_EQ(req.timeout, std::chrono::seconds(300));
}

TEST_F(ExecuteRequestTest, DefaultCaptureOutput) {
    ExecuteRequest req;
    EXPECT_TRUE(req.captureOutput);
}

// =============================================================================
// ExecuteResult Tests
// =============================================================================

class ExecuteResultTest : public ::testing::Test {
protected:
    void SetUp() override {
        result_.success = true;
        result_.result = {{"output", "hello"}};
        result_.output = "stdout content";
        result_.errorOutput = "stderr content";
        result_.exception = "";
        result_.exceptionType = "";
        result_.traceback = "";
        result_.executionTimeMs = 1500;
        result_.peakMemoryBytes = 1024 * 1024;
    }

    ExecuteResult result_;
};

TEST_F(ExecuteResultTest, ToJsonContainsAllFields) {
    auto j = result_.toJson();

    EXPECT_EQ(j["success"], true);
    EXPECT_EQ(j["output"], "stdout content");
    EXPECT_EQ(j["errorOutput"], "stderr content");
    EXPECT_EQ(j["executionTimeMs"], 1500);
    EXPECT_EQ(j["peakMemoryBytes"], 1024 * 1024);
}

TEST_F(ExecuteResultTest, FromJsonRoundTrip) {
    auto j = result_.toJson();
    auto parsed = ExecuteResult::fromJson(j);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->success, result_.success);
    EXPECT_EQ(parsed->output, result_.output);
    EXPECT_EQ(parsed->errorOutput, result_.errorOutput);
    EXPECT_EQ(parsed->executionTimeMs, result_.executionTimeMs);
}

TEST_F(ExecuteResultTest, DefaultValues) {
    ExecuteResult res;
    EXPECT_FALSE(res.success);
    EXPECT_EQ(res.executionTimeMs, 0);
    EXPECT_EQ(res.peakMemoryBytes, 0);
}

TEST_F(ExecuteResultTest, FailureWithException) {
    result_.success = false;
    result_.exception = "ValueError: invalid input";
    result_.exceptionType = "ValueError";
    result_.traceback = "Traceback (most recent call last):\n  File...";

    auto j = result_.toJson();
    EXPECT_EQ(j["success"], false);
    EXPECT_EQ(j["exception"], "ValueError: invalid input");
    EXPECT_EQ(j["exceptionType"], "ValueError");
}

// =============================================================================
// ProgressUpdate Tests
// =============================================================================

class ProgressUpdateTest : public ::testing::Test {
protected:
    void SetUp() override {
        progress_.percentage = 0.5f;
        progress_.message = "Processing...";
        progress_.currentStep = "Step 2 of 4";
        progress_.elapsedMs = 5000;
        progress_.estimatedRemainingMs = 5000;
    }

    ProgressUpdate progress_;
};

TEST_F(ProgressUpdateTest, ToJsonContainsAllFields) {
    auto j = progress_.toJson();

    EXPECT_FLOAT_EQ(j["percentage"].get<float>(), 0.5f);
    EXPECT_EQ(j["message"], "Processing...");
    EXPECT_EQ(j["currentStep"], "Step 2 of 4");
    EXPECT_EQ(j["elapsedMs"], 5000);
    EXPECT_EQ(j["estimatedRemainingMs"], 5000);
}

TEST_F(ProgressUpdateTest, FromJsonRoundTrip) {
    auto j = progress_.toJson();
    auto parsed = ProgressUpdate::fromJson(j);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_FLOAT_EQ(parsed->percentage, progress_.percentage);
    EXPECT_EQ(parsed->message, progress_.message);
    EXPECT_EQ(parsed->currentStep, progress_.currentStep);
    EXPECT_EQ(parsed->elapsedMs, progress_.elapsedMs);
}

TEST_F(ProgressUpdateTest, DefaultValues) {
    ProgressUpdate p;
    EXPECT_FLOAT_EQ(p.percentage, 0.0f);
    EXPECT_TRUE(p.message.empty());
    EXPECT_EQ(p.elapsedMs, 0);
    EXPECT_FALSE(p.estimatedRemainingMs.has_value());
}

TEST_F(ProgressUpdateTest, WithoutEstimatedRemaining) {
    progress_.estimatedRemainingMs = std::nullopt;
    auto j = progress_.toJson();

    // Should not contain estimatedRemainingMs or it should be null
    if (j.contains("estimatedRemainingMs")) {
        EXPECT_TRUE(j["estimatedRemainingMs"].is_null());
    }
}

// =============================================================================
// HandshakePayload Tests
// =============================================================================

class HandshakePayloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        handshake_.version = "1.0.0";
        handshake_.pythonVersion = "3.11.0";
        handshake_.capabilities = {"numpy", "pandas", "scipy"};
        handshake_.pid = 12345;
    }

    HandshakePayload handshake_;
};

TEST_F(HandshakePayloadTest, ToJsonContainsAllFields) {
    auto j = handshake_.toJson();

    EXPECT_EQ(j["version"], "1.0.0");
    EXPECT_EQ(j["pythonVersion"], "3.11.0");
    EXPECT_EQ(j["pid"], 12345);
    EXPECT_EQ(j["capabilities"].size(), 3);
}

TEST_F(HandshakePayloadTest, FromJsonRoundTrip) {
    auto j = handshake_.toJson();
    auto parsed = HandshakePayload::fromJson(j);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->version, handshake_.version);
    EXPECT_EQ(parsed->pythonVersion, handshake_.pythonVersion);
    EXPECT_EQ(parsed->pid, handshake_.pid);
    EXPECT_EQ(parsed->capabilities.size(), handshake_.capabilities.size());
}

TEST_F(HandshakePayloadTest, DefaultValues) {
    HandshakePayload h;
    EXPECT_TRUE(h.version.empty());
    EXPECT_TRUE(h.pythonVersion.empty());
    EXPECT_TRUE(h.capabilities.empty());
    EXPECT_EQ(h.pid, 0);
}

TEST_F(HandshakePayloadTest, EmptyCapabilities) {
    handshake_.capabilities.clear();
    auto j = handshake_.toJson();

    EXPECT_TRUE(j["capabilities"].empty());
}
