/*
 * test_message_types.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_message_types.cpp
 * @brief Comprehensive tests for IPC message types and error codes
 */

#include <gtest/gtest.h>
#include "script/ipc/message_types.hpp"

using namespace lithium::ipc;

// =============================================================================
// IPCError Tests
// =============================================================================

class IPCErrorTest : public ::testing::Test {};

TEST_F(IPCErrorTest, ErrorToStringSuccess) {
    EXPECT_EQ(ipcErrorToString(IPCError::Success), "Success");
}

TEST_F(IPCErrorTest, ErrorToStringConnectionFailed) {
    EXPECT_EQ(ipcErrorToString(IPCError::ConnectionFailed),
              "Connection failed");
}

TEST_F(IPCErrorTest, ErrorToStringMessageTooLarge) {
    EXPECT_EQ(ipcErrorToString(IPCError::MessageTooLarge), "Message too large");
}

TEST_F(IPCErrorTest, ErrorToStringSerializationFailed) {
    EXPECT_EQ(ipcErrorToString(IPCError::SerializationFailed),
              "Serialization failed");
}

TEST_F(IPCErrorTest, ErrorToStringDeserializationFailed) {
    EXPECT_EQ(ipcErrorToString(IPCError::DeserializationFailed),
              "Deserialization failed");
}

TEST_F(IPCErrorTest, ErrorToStringTimeout) {
    EXPECT_EQ(ipcErrorToString(IPCError::Timeout), "Timeout");
}

TEST_F(IPCErrorTest, ErrorToStringPipeError) {
    EXPECT_EQ(ipcErrorToString(IPCError::PipeError), "Pipe error");
}

TEST_F(IPCErrorTest, ErrorToStringInvalidMessage) {
    EXPECT_EQ(ipcErrorToString(IPCError::InvalidMessage), "Invalid message");
}

TEST_F(IPCErrorTest, ErrorToStringChannelClosed) {
    EXPECT_EQ(ipcErrorToString(IPCError::ChannelClosed), "Channel closed");
}

TEST_F(IPCErrorTest, ErrorToStringProcessNotRunning) {
    EXPECT_EQ(ipcErrorToString(IPCError::ProcessNotRunning),
              "Process not running");
}

TEST_F(IPCErrorTest, ErrorToStringUnknownError) {
    EXPECT_EQ(ipcErrorToString(IPCError::UnknownError), "Unknown error");
}

// =============================================================================
// MessageType Tests
// =============================================================================

class MessageTypeTest : public ::testing::Test {};

TEST_F(MessageTypeTest, MessageTypeNameHandshake) {
    EXPECT_EQ(messageTypeName(MessageType::Handshake), "Handshake");
}

TEST_F(MessageTypeTest, MessageTypeNameHandshakeAck) {
    EXPECT_EQ(messageTypeName(MessageType::HandshakeAck), "HandshakeAck");
}

TEST_F(MessageTypeTest, MessageTypeNameShutdown) {
    EXPECT_EQ(messageTypeName(MessageType::Shutdown), "Shutdown");
}

TEST_F(MessageTypeTest, MessageTypeNameShutdownAck) {
    EXPECT_EQ(messageTypeName(MessageType::ShutdownAck), "ShutdownAck");
}

TEST_F(MessageTypeTest, MessageTypeNameHeartbeat) {
    EXPECT_EQ(messageTypeName(MessageType::Heartbeat), "Heartbeat");
}

TEST_F(MessageTypeTest, MessageTypeNameHeartbeatAck) {
    EXPECT_EQ(messageTypeName(MessageType::HeartbeatAck), "HeartbeatAck");
}

TEST_F(MessageTypeTest, MessageTypeNameExecute) {
    EXPECT_EQ(messageTypeName(MessageType::Execute), "Execute");
}

TEST_F(MessageTypeTest, MessageTypeNameResult) {
    EXPECT_EQ(messageTypeName(MessageType::Result), "Result");
}

TEST_F(MessageTypeTest, MessageTypeNameError) {
    EXPECT_EQ(messageTypeName(MessageType::Error), "Error");
}

TEST_F(MessageTypeTest, MessageTypeNameCancel) {
    EXPECT_EQ(messageTypeName(MessageType::Cancel), "Cancel");
}

TEST_F(MessageTypeTest, MessageTypeNameCancelAck) {
    EXPECT_EQ(messageTypeName(MessageType::CancelAck), "CancelAck");
}

TEST_F(MessageTypeTest, MessageTypeNameProgress) {
    EXPECT_EQ(messageTypeName(MessageType::Progress), "Progress");
}

TEST_F(MessageTypeTest, MessageTypeNameLog) {
    EXPECT_EQ(messageTypeName(MessageType::Log), "Log");
}

TEST_F(MessageTypeTest, MessageTypeNameDataChunk) {
    EXPECT_EQ(messageTypeName(MessageType::DataChunk), "DataChunk");
}

TEST_F(MessageTypeTest, MessageTypeNameDataEnd) {
    EXPECT_EQ(messageTypeName(MessageType::DataEnd), "DataEnd");
}

TEST_F(MessageTypeTest, MessageTypeNameDataAck) {
    EXPECT_EQ(messageTypeName(MessageType::DataAck), "DataAck");
}

TEST_F(MessageTypeTest, MessageTypeNameQuery) {
    EXPECT_EQ(messageTypeName(MessageType::Query), "Query");
}

TEST_F(MessageTypeTest, MessageTypeNameQueryResponse) {
    EXPECT_EQ(messageTypeName(MessageType::QueryResponse), "QueryResponse");
}

// =============================================================================
// Message Category Tests
// =============================================================================

class MessageCategoryTest : public ::testing::Test {};

TEST_F(MessageCategoryTest, IsControlMessageHandshake) {
    EXPECT_TRUE(isControlMessage(MessageType::Handshake));
}

TEST_F(MessageCategoryTest, IsControlMessageHandshakeAck) {
    EXPECT_TRUE(isControlMessage(MessageType::HandshakeAck));
}

TEST_F(MessageCategoryTest, IsControlMessageShutdown) {
    EXPECT_TRUE(isControlMessage(MessageType::Shutdown));
}

TEST_F(MessageCategoryTest, IsControlMessageShutdownAck) {
    EXPECT_TRUE(isControlMessage(MessageType::ShutdownAck));
}

TEST_F(MessageCategoryTest, IsControlMessageHeartbeat) {
    EXPECT_TRUE(isControlMessage(MessageType::Heartbeat));
}

TEST_F(MessageCategoryTest, IsControlMessageHeartbeatAck) {
    EXPECT_TRUE(isControlMessage(MessageType::HeartbeatAck));
}

TEST_F(MessageCategoryTest, IsControlMessageExecuteIsFalse) {
    EXPECT_FALSE(isControlMessage(MessageType::Execute));
}

TEST_F(MessageCategoryTest, IsExecutionMessageExecute) {
    EXPECT_TRUE(isExecutionMessage(MessageType::Execute));
}

TEST_F(MessageCategoryTest, IsExecutionMessageResult) {
    EXPECT_TRUE(isExecutionMessage(MessageType::Result));
}

TEST_F(MessageCategoryTest, IsExecutionMessageError) {
    EXPECT_TRUE(isExecutionMessage(MessageType::Error));
}

TEST_F(MessageCategoryTest, IsExecutionMessageCancel) {
    EXPECT_TRUE(isExecutionMessage(MessageType::Cancel));
}

TEST_F(MessageCategoryTest, IsExecutionMessageCancelAck) {
    EXPECT_TRUE(isExecutionMessage(MessageType::CancelAck));
}

TEST_F(MessageCategoryTest, IsExecutionMessageHandshakeIsFalse) {
    EXPECT_FALSE(isExecutionMessage(MessageType::Handshake));
}

TEST_F(MessageCategoryTest, IsProgressMessageProgress) {
    EXPECT_TRUE(isProgressMessage(MessageType::Progress));
}

TEST_F(MessageCategoryTest, IsProgressMessageLog) {
    EXPECT_TRUE(isProgressMessage(MessageType::Log));
}

TEST_F(MessageCategoryTest, IsProgressMessageExecuteIsFalse) {
    EXPECT_FALSE(isProgressMessage(MessageType::Execute));
}

TEST_F(MessageCategoryTest, IsDataMessageDataChunk) {
    EXPECT_TRUE(isDataMessage(MessageType::DataChunk));
}

TEST_F(MessageCategoryTest, IsDataMessageDataEnd) {
    EXPECT_TRUE(isDataMessage(MessageType::DataEnd));
}

TEST_F(MessageCategoryTest, IsDataMessageDataAck) {
    EXPECT_TRUE(isDataMessage(MessageType::DataAck));
}

TEST_F(MessageCategoryTest, IsDataMessageProgressIsFalse) {
    EXPECT_FALSE(isDataMessage(MessageType::Progress));
}

// =============================================================================
// Protocol Constants Tests
// =============================================================================

class ProtocolConstantsTest : public ::testing::Test {};

TEST_F(ProtocolConstantsTest, MagicNumber) {
    EXPECT_EQ(ProtocolConstants::MAGIC, 0x4C495448);
}

TEST_F(ProtocolConstantsTest, Version) {
    EXPECT_EQ(ProtocolConstants::VERSION, 1);
}

TEST_F(ProtocolConstantsTest, HeaderSize) {
    EXPECT_EQ(ProtocolConstants::HEADER_SIZE, 16);
}

TEST_F(ProtocolConstantsTest, MaxPayloadSize) {
    EXPECT_EQ(ProtocolConstants::MAX_PAYLOAD_SIZE, 64 * 1024 * 1024);
}

TEST_F(ProtocolConstantsTest, CompressionThreshold) {
    EXPECT_EQ(ProtocolConstants::COMPRESSION_THRESHOLD, 1024);
}

// =============================================================================
// IPCResult Tests
// =============================================================================

class IPCResultTest : public ::testing::Test {};

TEST_F(IPCResultTest, SuccessResult) {
    IPCResult<int> result = 42;
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);
}

TEST_F(IPCResultTest, ErrorResult) {
    IPCResult<int> result = std::unexpected(IPCError::Timeout);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), IPCError::Timeout);
}

TEST_F(IPCResultTest, VoidSuccessResult) {
    IPCResult<void> result;
    EXPECT_TRUE(result.has_value());
}

TEST_F(IPCResultTest, VoidErrorResult) {
    IPCResult<void> result = std::unexpected(IPCError::PipeError);
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), IPCError::PipeError);
}

TEST_F(IPCResultTest, StringResult) {
    IPCResult<std::string> result = std::string("test");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "test");
}

// =============================================================================
// MessageType Value Tests
// =============================================================================

class MessageTypeValueTest : public ::testing::Test {};

TEST_F(MessageTypeValueTest, HandshakeValue) {
    EXPECT_EQ(static_cast<uint8_t>(MessageType::Handshake), 0x01);
}

TEST_F(MessageTypeValueTest, ExecuteValue) {
    EXPECT_EQ(static_cast<uint8_t>(MessageType::Execute), 0x10);
}

TEST_F(MessageTypeValueTest, ProgressValue) {
    EXPECT_EQ(static_cast<uint8_t>(MessageType::Progress), 0x20);
}

TEST_F(MessageTypeValueTest, DataChunkValue) {
    EXPECT_EQ(static_cast<uint8_t>(MessageType::DataChunk), 0x30);
}

TEST_F(MessageTypeValueTest, QueryValue) {
    EXPECT_EQ(static_cast<uint8_t>(MessageType::Query), 0x40);
}
