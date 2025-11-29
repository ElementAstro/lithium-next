/*
 * message_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file message_types.hpp
 * @brief IPC Message type definitions
 * @date 2024
 * @version 1.1.0
 */

#ifndef LITHIUM_SCRIPT_IPC_MESSAGE_TYPES_HPP
#define LITHIUM_SCRIPT_IPC_MESSAGE_TYPES_HPP

#include <cstdint>
#include <expected>
#include <string_view>

namespace lithium::ipc {

/**
 * @brief IPC error codes
 */
enum class IPCError {
    Success = 0,
    ConnectionFailed,
    MessageTooLarge,
    SerializationFailed,
    DeserializationFailed,
    Timeout,
    PipeError,
    InvalidMessage,
    ChannelClosed,
    ProcessNotRunning,
    UnknownError
};

/**
 * @brief Get string representation of IPCError
 */
[[nodiscard]] constexpr std::string_view ipcErrorToString(IPCError error) noexcept {
    switch (error) {
        case IPCError::Success: return "Success";
        case IPCError::ConnectionFailed: return "Connection failed";
        case IPCError::MessageTooLarge: return "Message too large";
        case IPCError::SerializationFailed: return "Serialization failed";
        case IPCError::DeserializationFailed: return "Deserialization failed";
        case IPCError::Timeout: return "Timeout";
        case IPCError::PipeError: return "Pipe error";
        case IPCError::InvalidMessage: return "Invalid message";
        case IPCError::ChannelClosed: return "Channel closed";
        case IPCError::ProcessNotRunning: return "Process not running";
        case IPCError::UnknownError: return "Unknown error";
    }
    return "Unknown error";
}

/**
 * @brief Result type for IPC operations
 */
template<typename T>
using IPCResult = std::expected<T, IPCError>;

/**
 * @brief Message types for IPC communication
 */
enum class MessageType : uint8_t {
    // Control messages (0x01-0x0F)
    Handshake = 0x01,    ///< Initial handshake
    HandshakeAck = 0x02, ///< Handshake acknowledgment
    Shutdown = 0x03,     ///< Shutdown request
    ShutdownAck = 0x04,  ///< Shutdown acknowledgment
    Heartbeat = 0x05,    ///< Heartbeat ping
    HeartbeatAck = 0x06, ///< Heartbeat response

    // Execution messages (0x10-0x1F)
    Execute = 0x10,      ///< Execute script/function
    Result = 0x11,       ///< Execution result
    Error = 0x12,        ///< Error message
    Cancel = 0x13,       ///< Cancel execution
    CancelAck = 0x14,    ///< Cancel acknowledgment

    // Progress messages (0x20-0x2F)
    Progress = 0x20,     ///< Progress update
    Log = 0x21,          ///< Log message

    // Data transfer messages (0x30-0x3F)
    DataChunk = 0x30,    ///< Data chunk (for large transfers)
    DataEnd = 0x31,      ///< End of data transfer
    DataAck = 0x32,      ///< Data acknowledgment

    // Query messages (0x40-0x4F)
    Query = 0x40,        ///< Query request
    QueryResponse = 0x41 ///< Query response
};

/**
 * @brief Get string name for message type
 */
[[nodiscard]] constexpr std::string_view messageTypeName(MessageType type) noexcept {
    switch (type) {
        case MessageType::Handshake: return "Handshake";
        case MessageType::HandshakeAck: return "HandshakeAck";
        case MessageType::Shutdown: return "Shutdown";
        case MessageType::ShutdownAck: return "ShutdownAck";
        case MessageType::Heartbeat: return "Heartbeat";
        case MessageType::HeartbeatAck: return "HeartbeatAck";
        case MessageType::Execute: return "Execute";
        case MessageType::Result: return "Result";
        case MessageType::Error: return "Error";
        case MessageType::Cancel: return "Cancel";
        case MessageType::CancelAck: return "CancelAck";
        case MessageType::Progress: return "Progress";
        case MessageType::Log: return "Log";
        case MessageType::DataChunk: return "DataChunk";
        case MessageType::DataEnd: return "DataEnd";
        case MessageType::DataAck: return "DataAck";
        case MessageType::Query: return "Query";
        case MessageType::QueryResponse: return "QueryResponse";
    }
    return "Unknown";
}

/**
 * @brief Check if message type is a control message
 */
[[nodiscard]] constexpr bool isControlMessage(MessageType type) noexcept {
    return static_cast<uint8_t>(type) >= 0x01 &&
           static_cast<uint8_t>(type) <= 0x0F;
}

/**
 * @brief Check if message type is an execution message
 */
[[nodiscard]] constexpr bool isExecutionMessage(MessageType type) noexcept {
    return static_cast<uint8_t>(type) >= 0x10 &&
           static_cast<uint8_t>(type) <= 0x1F;
}

/**
 * @brief Check if message type is a progress message
 */
[[nodiscard]] constexpr bool isProgressMessage(MessageType type) noexcept {
    return static_cast<uint8_t>(type) >= 0x20 &&
           static_cast<uint8_t>(type) <= 0x2F;
}

/**
 * @brief Check if message type is a data transfer message
 */
[[nodiscard]] constexpr bool isDataMessage(MessageType type) noexcept {
    return static_cast<uint8_t>(type) >= 0x30 &&
           static_cast<uint8_t>(type) <= 0x3F;
}

/**
 * @brief Protocol constants
 */
struct ProtocolConstants {
    static constexpr uint32_t MAGIC = 0x4C495448;  // "LITH"
    static constexpr uint8_t VERSION = 1;
    static constexpr size_t HEADER_SIZE = 16;
    static constexpr size_t MAX_PAYLOAD_SIZE = 64 * 1024 * 1024;  // 64MB
    static constexpr size_t COMPRESSION_THRESHOLD = 1024;  // 1KB
};

}  // namespace lithium::ipc

#endif  // LITHIUM_SCRIPT_IPC_MESSAGE_TYPES_HPP
