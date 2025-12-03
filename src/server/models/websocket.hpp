/*
 * websocket.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: WebSocket data models for messages and events

**************************************************/

#ifndef LITHIUM_SERVER_MODELS_WEBSOCKET_HPP
#define LITHIUM_SERVER_MODELS_WEBSOCKET_HPP

#include <string>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::models::websocket {

using json = nlohmann::json;

/**
 * @brief WebSocket message types
 */
enum class MessageType {
    Command,   ///< Client command request
    Response,  ///< Server response to command
    Event,     ///< Server-initiated event
    Error,     ///< Error message
    Ping,      ///< Keep-alive ping
    Pong       ///< Keep-alive pong
};

/**
 * @brief Convert MessageType to string
 */
inline auto messageTypeToString(MessageType type) -> std::string {
    switch (type) {
        case MessageType::Command:
            return "command";
        case MessageType::Response:
            return "response";
        case MessageType::Event:
            return "event";
        case MessageType::Error:
            return "error";
        case MessageType::Ping:
            return "ping";
        case MessageType::Pong:
            return "pong";
    }
    return "unknown";
}

/**
 * @brief WebSocket connection statistics
 */
struct ConnectionStats {
    size_t activeConnections;
    size_t totalConnections;
    size_t totalMessages;
    size_t totalErrors;
    size_t authenticatedConnections;
    std::vector<std::string> subscribedTopics;

    [[nodiscard]] auto toJson() const -> json {
        return {{"activeConnections", activeConnections},
                {"totalConnections", totalConnections},
                {"totalMessages", totalMessages},
                {"totalErrors", totalErrors},
                {"authenticatedConnections", authenticatedConnections},
                {"subscribedTopics", subscribedTopics}};
    }
};

/**
 * @brief WebSocket command request
 */
struct CommandRequest {
    std::string type;       ///< Should be "command"
    std::string command;    ///< Command name
    std::string requestId;  ///< Unique request ID for correlation
    json params;            ///< Command parameters

    static auto fromJson(const json& j) -> CommandRequest {
        CommandRequest req;
        req.type = j.value("type", "command");
        req.command = j.value("command", "");
        req.requestId = j.value("requestId", "");
        req.params = j.value("params", json::object());
        return req;
    }

    [[nodiscard]] auto isValid() const -> bool {
        return !command.empty() && type == "command";
    }
};

/**
 * @brief WebSocket command response
 */
struct CommandResponse {
    std::string type;          ///< "response"
    std::string command;       ///< Original command name
    std::string correlationId; ///< Request ID for correlation
    std::string status;        ///< "success" or "error"
    json data;                 ///< Response data
    std::string errorCode;     ///< Error code if status is "error"
    std::string errorMessage;  ///< Error message if status is "error"

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["type"] = "response";
        j["command"] = command;
        j["correlationId"] = correlationId;
        j["status"] = status;

        if (status == "success") {
            j["data"] = data;
        } else {
            j["error"] = {{"code", errorCode}, {"message", errorMessage}};
        }

        return j;
    }

    static auto success(const std::string& cmd, const std::string& reqId,
                        const json& responseData) -> CommandResponse {
        CommandResponse resp;
        resp.type = "response";
        resp.command = cmd;
        resp.correlationId = reqId;
        resp.status = "success";
        resp.data = responseData;
        return resp;
    }

    static auto error(const std::string& cmd, const std::string& reqId,
                      const std::string& code,
                      const std::string& message) -> CommandResponse {
        CommandResponse resp;
        resp.type = "response";
        resp.command = cmd;
        resp.correlationId = reqId;
        resp.status = "error";
        resp.errorCode = code;
        resp.errorMessage = message;
        return resp;
    }
};

/**
 * @brief WebSocket event message
 */
struct EventMessage {
    std::string type;       ///< "event"
    std::string eventName;  ///< Event name (e.g., "device.connected")
    json data;              ///< Event data
    int64_t timestamp;      ///< Event timestamp (ms since epoch)

    [[nodiscard]] auto toJson() const -> json {
        return {{"type", "event"},
                {"event", eventName},
                {"data", data},
                {"timestamp", timestamp}};
    }

    static auto create(const std::string& name, const json& eventData,
                       int64_t ts = 0) -> EventMessage {
        EventMessage msg;
        msg.type = "event";
        msg.eventName = name;
        msg.data = eventData;
        msg.timestamp = ts;
        return msg;
    }
};

/**
 * @brief WebSocket error message
 */
struct ErrorMessage {
    std::string type;     ///< "error"
    std::string code;     ///< Error code
    std::string message;  ///< Error message
    json details;         ///< Additional error details

    [[nodiscard]] auto toJson() const -> json {
        json j;
        j["type"] = "error";
        j["error"] = {{"code", code}, {"message", message}};

        if (!details.is_null()) {
            j["error"]["details"] = details;
        }

        return j;
    }

    static auto create(const std::string& errorCode,
                       const std::string& errorMessage,
                       const json& errorDetails = json()) -> ErrorMessage {
        ErrorMessage msg;
        msg.type = "error";
        msg.code = errorCode;
        msg.message = errorMessage;
        msg.details = errorDetails;
        return msg;
    }
};

/**
 * @brief Subscription acknowledgment
 */
struct SubscriptionAck {
    std::string topic;
    bool subscribed;
    std::string message;

    [[nodiscard]] auto toJson() const -> json {
        return {{"type", "response"},
                {"command", subscribed ? "subscribe" : "unsubscribe"},
                {"status", "success"},
                {"data",
                 {{"topic", topic},
                  {"subscribed", subscribed},
                  {"message", message}}}};
    }
};

}  // namespace lithium::models::websocket

#endif  // LITHIUM_SERVER_MODELS_WEBSOCKET_HPP
