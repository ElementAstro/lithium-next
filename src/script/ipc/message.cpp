/*
 * message.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "message.hpp"
#include "serializer.hpp"

#include <spdlog/spdlog.h>

namespace lithium::ipc {

// ============================================================================
// MessageHeader Implementation
// ============================================================================

std::vector<uint8_t> MessageHeader::serialize() const {
    std::vector<uint8_t> data(SIZE);
    size_t offset = 0;

    // Magic (4 bytes, big-endian)
    data[offset++] = static_cast<uint8_t>((magic >> 24) & 0xFF);
    data[offset++] = static_cast<uint8_t>((magic >> 16) & 0xFF);
    data[offset++] = static_cast<uint8_t>((magic >> 8) & 0xFF);
    data[offset++] = static_cast<uint8_t>(magic & 0xFF);

    // Version
    data[offset++] = version;

    // Type
    data[offset++] = static_cast<uint8_t>(type);

    // Payload size (4 bytes, big-endian)
    data[offset++] = static_cast<uint8_t>((payloadSize >> 24) & 0xFF);
    data[offset++] = static_cast<uint8_t>((payloadSize >> 16) & 0xFF);
    data[offset++] = static_cast<uint8_t>((payloadSize >> 8) & 0xFF);
    data[offset++] = static_cast<uint8_t>(payloadSize & 0xFF);

    // Sequence ID (4 bytes, big-endian)
    data[offset++] = static_cast<uint8_t>((sequenceId >> 24) & 0xFF);
    data[offset++] = static_cast<uint8_t>((sequenceId >> 16) & 0xFF);
    data[offset++] = static_cast<uint8_t>((sequenceId >> 8) & 0xFF);
    data[offset++] = static_cast<uint8_t>(sequenceId & 0xFF);

    // Flags and reserved
    data[offset++] = flags;
    data[offset++] = reserved;

    return data;
}

IPCResult<MessageHeader> MessageHeader::deserialize(std::span<const uint8_t> data) {
    if (data.size() < SIZE) {
        return std::unexpected(IPCError::InvalidMessage);
    }

    MessageHeader header;
    size_t offset = 0;

    // Magic
    header.magic = (static_cast<uint32_t>(data[offset]) << 24) |
                   (static_cast<uint32_t>(data[offset + 1]) << 16) |
                   (static_cast<uint32_t>(data[offset + 2]) << 8) |
                   static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    if (header.magic != MAGIC) {
        return std::unexpected(IPCError::InvalidMessage);
    }

    // Version
    header.version = data[offset++];

    // Type
    header.type = static_cast<MessageType>(data[offset++]);

    // Payload size
    header.payloadSize = (static_cast<uint32_t>(data[offset]) << 24) |
                         (static_cast<uint32_t>(data[offset + 1]) << 16) |
                         (static_cast<uint32_t>(data[offset + 2]) << 8) |
                         static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    // Sequence ID
    header.sequenceId = (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    // Flags and reserved
    header.flags = data[offset++];
    header.reserved = data[offset++];

    return header;
}

bool MessageHeader::isValid() const noexcept {
    return magic == MAGIC && version == VERSION;
}

// ============================================================================
// Message Implementation
// ============================================================================

Message Message::create(MessageType type, const json& payload, uint32_t sequenceId) {
    Message msg;
    msg.header.type = type;
    msg.header.sequenceId = sequenceId;
    msg.payload = IPCSerializer::serialize(payload);
    msg.header.payloadSize = static_cast<uint32_t>(msg.payload.size());
    return msg;
}

Message Message::create(MessageType type, std::vector<uint8_t> payload, uint32_t sequenceId) {
    Message msg;
    msg.header.type = type;
    msg.header.sequenceId = sequenceId;
    msg.payload = std::move(payload);
    msg.header.payloadSize = static_cast<uint32_t>(msg.payload.size());
    return msg;
}

IPCResult<json> Message::getPayloadAsJson() const {
    if (payload.empty()) {
        return json::object();
    }
    return IPCSerializer::deserialize(payload);
}

std::vector<uint8_t> Message::serialize() const {
    auto headerData = header.serialize();
    std::vector<uint8_t> result;
    result.reserve(headerData.size() + payload.size());
    result.insert(result.end(), headerData.begin(), headerData.end());
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

IPCResult<Message> Message::deserialize(std::span<const uint8_t> data) {
    auto headerResult = MessageHeader::deserialize(data);
    if (!headerResult) {
        return std::unexpected(headerResult.error());
    }

    Message msg;
    msg.header = *headerResult;

    size_t expectedSize = MessageHeader::SIZE + msg.header.payloadSize;
    if (data.size() < expectedSize) {
        return std::unexpected(IPCError::InvalidMessage);
    }

    msg.payload.assign(data.begin() + MessageHeader::SIZE,
                       data.begin() + expectedSize);

    return msg;
}

// ============================================================================
// Payload Structures Implementation
// ============================================================================

json ExecuteRequest::toJson() const {
    return {
        {"script_content", scriptContent},
        {"script_path", scriptPath},
        {"function_name", functionName},
        {"arguments", arguments},
        {"timeout_seconds", timeout.count()},
        {"capture_output", captureOutput},
        {"allowed_imports", allowedImports},
        {"working_directory", workingDirectory}
    };
}

IPCResult<ExecuteRequest> ExecuteRequest::fromJson(const json& j) {
    try {
        ExecuteRequest req;
        if (j.contains("script_content")) req.scriptContent = j["script_content"].get<std::string>();
        if (j.contains("script_path")) req.scriptPath = j["script_path"].get<std::string>();
        if (j.contains("function_name")) req.functionName = j["function_name"].get<std::string>();
        if (j.contains("arguments")) req.arguments = j["arguments"];
        if (j.contains("timeout_seconds")) {
            req.timeout = std::chrono::seconds(j["timeout_seconds"].get<int64_t>());
        }
        if (j.contains("capture_output")) req.captureOutput = j["capture_output"].get<bool>();
        if (j.contains("allowed_imports")) {
            req.allowedImports = j["allowed_imports"].get<std::vector<std::string>>();
        }
        if (j.contains("working_directory")) {
            req.workingDirectory = j["working_directory"].get<std::string>();
        }
        return req;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse ExecuteRequest: {}", e.what());
        return std::unexpected(IPCError::DeserializationFailed);
    }
}

json ExecuteResult::toJson() const {
    return {
        {"success", success},
        {"result", result},
        {"output", output},
        {"error_output", errorOutput},
        {"exception", exception},
        {"exception_type", exceptionType},
        {"traceback", traceback},
        {"execution_time_ms", executionTimeMs},
        {"peak_memory_bytes", peakMemoryBytes}
    };
}

IPCResult<ExecuteResult> ExecuteResult::fromJson(const json& j) {
    try {
        ExecuteResult res;
        if (j.contains("success")) res.success = j["success"].get<bool>();
        if (j.contains("result")) res.result = j["result"];
        if (j.contains("output")) res.output = j["output"].get<std::string>();
        if (j.contains("error_output")) res.errorOutput = j["error_output"].get<std::string>();
        if (j.contains("exception")) res.exception = j["exception"].get<std::string>();
        if (j.contains("exception_type")) res.exceptionType = j["exception_type"].get<std::string>();
        if (j.contains("traceback")) res.traceback = j["traceback"].get<std::string>();
        if (j.contains("execution_time_ms")) res.executionTimeMs = j["execution_time_ms"].get<int64_t>();
        if (j.contains("peak_memory_bytes")) res.peakMemoryBytes = j["peak_memory_bytes"].get<size_t>();
        return res;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse ExecuteResult: {}", e.what());
        return std::unexpected(IPCError::DeserializationFailed);
    }
}

json ProgressUpdate::toJson() const {
    json j = {
        {"percentage", percentage},
        {"message", message},
        {"current_step", currentStep},
        {"elapsed_ms", elapsedMs}
    };
    if (estimatedRemainingMs) {
        j["estimated_remaining_ms"] = *estimatedRemainingMs;
    }
    return j;
}

IPCResult<ProgressUpdate> ProgressUpdate::fromJson(const json& j) {
    try {
        ProgressUpdate update;
        if (j.contains("percentage")) update.percentage = j["percentage"].get<float>();
        if (j.contains("message")) update.message = j["message"].get<std::string>();
        if (j.contains("current_step")) update.currentStep = j["current_step"].get<std::string>();
        if (j.contains("elapsed_ms")) update.elapsedMs = j["elapsed_ms"].get<int64_t>();
        if (j.contains("estimated_remaining_ms")) {
            update.estimatedRemainingMs = j["estimated_remaining_ms"].get<int64_t>();
        }
        return update;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse ProgressUpdate: {}", e.what());
        return std::unexpected(IPCError::DeserializationFailed);
    }
}

json HandshakePayload::toJson() const {
    return {
        {"version", version},
        {"python_version", pythonVersion},
        {"capabilities", capabilities},
        {"pid", pid}
    };
}

IPCResult<HandshakePayload> HandshakePayload::fromJson(const json& j) {
    try {
        HandshakePayload payload;
        if (j.contains("version")) payload.version = j["version"].get<std::string>();
        if (j.contains("python_version")) payload.pythonVersion = j["python_version"].get<std::string>();
        if (j.contains("capabilities")) {
            payload.capabilities = j["capabilities"].get<std::vector<std::string>>();
        }
        if (j.contains("pid")) payload.pid = j["pid"].get<uint32_t>();
        return payload;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse HandshakePayload: {}", e.what());
        return std::unexpected(IPCError::DeserializationFailed);
    }
}

}  // namespace lithium::ipc
