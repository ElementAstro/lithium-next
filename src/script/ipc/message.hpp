/*
 * message.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file message.hpp
 * @brief IPC Message structures and serialization
 * @date 2024
 * @version 1.0.0
 *
 * This module provides message structures for inter-process communication
 * between the main C++ process and isolated Python subprocesses:
 * - Message header with magic number and version validation
 * - Generic message container with binary/JSON payload support
 * - Specialized payload structures (ExecuteRequest, ExecuteResult, etc.)
 * - Serialization/deserialization methods for all structures
 */

#ifndef LITHIUM_SCRIPT_IPC_MESSAGE_HPP
#define LITHIUM_SCRIPT_IPC_MESSAGE_HPP

#include <nlohmann/json.hpp>

#include <chrono>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "message_types.hpp"

namespace lithium::ipc {

using json = nlohmann::json;

// Forward declaration to avoid circular dependencies
class IPCSerializer;

/**
 * @brief Message header structure
 *
 * Provides binary protocol framing with magic number validation
 * and protocol version checking.
 */
struct MessageHeader {
    static constexpr uint32_t MAGIC = 0x4C495448;  // "LITH"
    static constexpr uint8_t VERSION = 1;
    static constexpr size_t SIZE = 16;

    uint32_t magic{MAGIC};     ///< Magic number for validation
    uint8_t version{VERSION};  ///< Protocol version
    MessageType type;          ///< Message type
    uint32_t payloadSize{0};   ///< Size of payload in bytes
    uint32_t sequenceId{0};    ///< Message sequence number
    uint8_t flags{0};          ///< Message flags
    uint8_t reserved{0};       ///< Reserved for future use

    /**
     * @brief Serialize header to bytes
     * @return Serialized header as byte vector
     */
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserialize header from bytes
     * @param data Byte buffer to deserialize from
     * @return Deserialized header or error
     */
    [[nodiscard]] static IPCResult<MessageHeader> deserialize(
        std::span<const uint8_t> data);

    /**
     * @brief Validate the header
     * @return True if header is valid (magic and version match)
     */
    [[nodiscard]] bool isValid() const noexcept;
};

/**
 * @brief IPC Message structure
 *
 * Generic container for messages that can carry either binary or JSON payloads.
 * Supports creation from JSON or binary data with automatic serialization.
 */
struct Message {
    MessageHeader header;
    std::vector<uint8_t> payload;

    /**
     * @brief Create a message with JSON payload
     * @param type Message type
     * @param payload JSON payload to serialize
     * @param sequenceId Optional sequence ID
     * @return Constructed message
     */
    [[nodiscard]] static Message create(MessageType type, const json& payload,
                                        uint32_t sequenceId = 0);

    /**
     * @brief Create a message with binary payload
     * @param type Message type
     * @param payload Binary payload
     * @param sequenceId Optional sequence ID
     * @return Constructed message
     */
    [[nodiscard]] static Message create(MessageType type,
                                        std::vector<uint8_t> payload,
                                        uint32_t sequenceId = 0);

    /**
     * @brief Get payload as JSON
     * @return Deserialized JSON or error
     */
    [[nodiscard]] IPCResult<json> getPayloadAsJson() const;

    /**
     * @brief Serialize the entire message
     * @return Serialized message (header + payload)
     */
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /**
     * @brief Deserialize a message from bytes
     * @param data Byte buffer to deserialize from
     * @return Deserialized message or error
     */
    [[nodiscard]] static IPCResult<Message> deserialize(
        std::span<const uint8_t> data);
};

/**
 * @brief Execute request payload
 *
 * Contains all parameters needed to execute a Python script or function
 * in an isolated subprocess.
 */
struct ExecuteRequest {
    std::string scriptContent;                ///< Script content to execute
    std::string scriptPath;                   ///< Optional script file path
    std::string functionName;                 ///< Function to call (if any)
    json arguments;                           ///< Arguments as JSON
    std::chrono::seconds timeout{300};        ///< Execution timeout
    bool captureOutput{true};                 ///< Capture stdout/stderr
    std::vector<std::string> allowedImports;  ///< Allowed module imports
    std::string workingDirectory;             ///< Working directory

    /**
     * @brief Convert to JSON representation
     * @return JSON object
     */
    [[nodiscard]] json toJson() const;

    /**
     * @brief Create from JSON representation
     * @param j JSON object to deserialize from
     * @return Deserialized ExecuteRequest or error
     */
    [[nodiscard]] static IPCResult<ExecuteRequest> fromJson(const json& j);
};

/**
 * @brief Execution result payload
 *
 * Contains the result of script execution including output,
 * error information, and performance metrics.
 */
struct ExecuteResult {
    bool success{false};         ///< Whether execution succeeded
    json result;                 ///< Result data
    std::string output;          ///< Captured stdout
    std::string errorOutput;     ///< Captured stderr
    std::string exception;       ///< Exception message if any
    std::string exceptionType;   ///< Exception type
    std::string traceback;       ///< Python traceback
    int64_t executionTimeMs{0};  ///< Execution time in milliseconds
    size_t peakMemoryBytes{0};   ///< Peak memory usage

    /**
     * @brief Convert to JSON representation
     * @return JSON object
     */
    [[nodiscard]] json toJson() const;

    /**
     * @brief Create from JSON representation
     * @param j JSON object to deserialize from
     * @return Deserialized ExecuteResult or error
     */
    [[nodiscard]] static IPCResult<ExecuteResult> fromJson(const json& j);
};

/**
 * @brief Progress update payload
 *
 * Provides progress information during long-running script execution.
 */
struct ProgressUpdate {
    float percentage{0.0f};   ///< Progress 0.0 to 1.0
    std::string message;      ///< Status message
    std::string currentStep;  ///< Current step description
    int64_t elapsedMs{0};     ///< Elapsed time in milliseconds
    std::optional<int64_t> estimatedRemainingMs;  ///< Estimated remaining time

    /**
     * @brief Convert to JSON representation
     * @return JSON object
     */
    [[nodiscard]] json toJson() const;

    /**
     * @brief Create from JSON representation
     * @param j JSON object to deserialize from
     * @return Deserialized ProgressUpdate or error
     */
    [[nodiscard]] static IPCResult<ProgressUpdate> fromJson(const json& j);
};

/**
 * @brief Handshake payload
 *
 * Contains protocol and capability information exchanged during
 * the initial handshake between parent and child processes.
 */
struct HandshakePayload {
    std::string version;                    ///< Protocol version
    std::string pythonVersion;              ///< Python version
    std::vector<std::string> capabilities;  ///< Supported capabilities
    uint32_t pid{0};                        ///< Process ID

    /**
     * @brief Convert to JSON representation
     * @return JSON object
     */
    [[nodiscard]] json toJson() const;

    /**
     * @brief Create from JSON representation
     * @param j JSON object to deserialize from
     * @return Deserialized HandshakePayload or error
     */
    [[nodiscard]] static IPCResult<HandshakePayload> fromJson(const json& j);
};

}  // namespace lithium::ipc

#endif  // LITHIUM_SCRIPT_IPC_MESSAGE_HPP
