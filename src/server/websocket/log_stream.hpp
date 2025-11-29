/*
 * log_stream.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: WebSocket Log Streaming Handler for real-time log delivery

**************************************************/

#ifndef LITHIUM_SERVER_WEBSOCKET_LOG_STREAM_HPP
#define LITHIUM_SERVER_WEBSOCKET_LOG_STREAM_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "atom/type/json.hpp"
#include "crow.h"
#include "logging/logging_manager.hpp"

namespace lithium::server {

/**
 * @brief Log stream subscription configuration per client
 */
struct LogStreamSubscription {
    std::optional<spdlog::level::level_enum> level_filter;
    std::optional<std::string> logger_filter;
    bool include_source{false};
    bool enabled{true};

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j = {{"enabled", enabled},
                            {"include_source", include_source}};
        if (level_filter) {
            j["level"] = logging::LoggingManager::levelToString(*level_filter);
        }
        if (logger_filter) {
            j["logger"] = *logger_filter;
        }
        return j;
    }

    static auto fromJson(const nlohmann::json& j) -> LogStreamSubscription {
        LogStreamSubscription sub;
        sub.enabled = j.value("enabled", true);
        sub.include_source = j.value("include_source", false);

        if (j.contains("level")) {
            sub.level_filter =
                logging::LoggingManager::levelFromString(j["level"]);
        }
        if (j.contains("logger")) {
            sub.logger_filter = j["logger"].get<std::string>();
        }
        return sub;
    }
};

/**
 * @brief WebSocket Log Stream Manager
 *
 * Manages real-time log streaming to WebSocket clients.
 * Supports per-client filtering by log level and logger name.
 *
 * Usage:
 * 1. Create instance and call initialize() with WebSocket server reference
 * 2. Clients subscribe via WebSocket message: {"type": "subscribe", "topic":
 * "logs", ...}
 * 3. Log entries are pushed to subscribed clients in real-time
 * 4. Clients unsubscribe via: {"type": "unsubscribe", "topic": "logs"}
 */
class LogStreamManager {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> LogStreamManager&;

    /**
     * @brief Initialize the log stream manager
     *
     * Registers with LoggingManager to receive log entries
     */
    void initialize();

    /**
     * @brief Shutdown the log stream manager
     */
    void shutdown();

    /**
     * @brief Check if manager is initialized
     */
    [[nodiscard]] auto isInitialized() const -> bool;

    /**
     * @brief Subscribe a WebSocket connection to log stream
     *
     * @param conn_id Unique connection identifier
     * @param subscription Subscription configuration
     * @param send_callback Callback to send messages to the client
     */
    void subscribe(const std::string& conn_id,
                   const LogStreamSubscription& subscription,
                   std::function<void(const std::string&)> send_callback);

    /**
     * @brief Update subscription for a connection
     */
    void updateSubscription(const std::string& conn_id,
                            const LogStreamSubscription& subscription);

    /**
     * @brief Unsubscribe a connection from log stream
     */
    void unsubscribe(const std::string& conn_id);

    /**
     * @brief Check if a connection is subscribed
     */
    [[nodiscard]] auto isSubscribed(const std::string& conn_id) const -> bool;

    /**
     * @brief Get subscription info for a connection
     */
    [[nodiscard]] auto getSubscription(const std::string& conn_id) const
        -> std::optional<LogStreamSubscription>;

    /**
     * @brief Get number of active subscribers
     */
    [[nodiscard]] auto getSubscriberCount() const -> size_t;

    /**
     * @brief Get statistics
     */
    [[nodiscard]] auto getStats() const -> nlohmann::json;

    /**
     * @brief Handle incoming WebSocket message for log streaming
     *
     * Processes subscribe/unsubscribe/update commands
     *
     * @param conn_id Connection identifier
     * @param message Parsed JSON message
     * @param send_callback Callback to send response
     * @return true if message was handled, false if not a log stream message
     */
    auto handleMessage(const std::string& conn_id,
                       const nlohmann::json& message,
                       std::function<void(const std::string&)> send_callback)
        -> bool;

private:
    LogStreamManager() = default;
    ~LogStreamManager();

    LogStreamManager(const LogStreamManager&) = delete;
    LogStreamManager& operator=(const LogStreamManager&) = delete;

    /**
     * @brief Called when a new log entry is received
     */
    void onLogEntry(const logging::LogEntry& entry);

    /**
     * @brief Format log entry for WebSocket transmission
     */
    [[nodiscard]] auto formatLogEntry(const logging::LogEntry& entry,
                                      const LogStreamSubscription& sub) const
        -> nlohmann::json;

    /**
     * @brief Check if entry matches subscription filter
     */
    [[nodiscard]] auto matchesFilter(const logging::LogEntry& entry,
                                     const LogStreamSubscription& sub) const
        -> bool;

    struct SubscriberInfo {
        LogStreamSubscription subscription;
        std::function<void(const std::string&)> send_callback;
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, SubscriberInfo> subscribers_;
    std::atomic<bool> initialized_{false};

    // Statistics
    std::atomic<size_t> total_entries_sent_{0};
    std::atomic<size_t> total_entries_filtered_{0};
};

/**
 * @brief Helper to integrate LogStreamManager with WebSocketServer
 *
 * Call this in WebSocketServer message handler to process log stream commands
 */
inline auto handleLogStreamMessage(crow::websocket::connection& conn,
                                   const nlohmann::json& message) -> bool {
    // Generate connection ID from pointer
    std::ostringstream oss;
    oss << "ws_" << static_cast<const void*>(&conn);
    std::string conn_id = oss.str();

    auto send_callback = [&conn](const std::string& msg) {
        try {
            conn.send_text(msg);
        } catch (...) {
            // Connection may be closed
        }
    };

    return LogStreamManager::getInstance().handleMessage(conn_id, message,
                                                         send_callback);
}

/**
 * @brief Register log stream handlers with WebSocket server
 *
 * Sets up message handlers for log streaming commands
 */
inline void registerLogStreamHandlers(
    std::function<void(const std::string&,
                       std::function<void(crow::websocket::connection&,
                                          const nlohmann::json&)>)>
        register_handler) {
    // Handler for log stream subscription
    register_handler("logs.subscribe", [](crow::websocket::connection& conn,
                                          const nlohmann::json& payload) {
        std::ostringstream oss;
        oss << "ws_" << static_cast<const void*>(&conn);
        std::string conn_id = oss.str();

        LogStreamSubscription sub;
        if (payload.contains("level")) {
            sub.level_filter =
                logging::LoggingManager::levelFromString(payload["level"]);
        }
        if (payload.contains("logger")) {
            sub.logger_filter = payload["logger"].get<std::string>();
        }
        sub.include_source = payload.value("include_source", false);
        sub.enabled = true;

        auto send_callback = [&conn](const std::string& msg) {
            try {
                conn.send_text(msg);
            } catch (...) {
            }
        };

        LogStreamManager::getInstance().subscribe(conn_id, sub, send_callback);

        nlohmann::json response = {
            {"type", "response"},
            {"command", "logs.subscribe"},
            {"status", "success"},
            {"data", {{"subscribed", true}, {"subscription", sub.toJson()}}}};
        conn.send_text(response.dump());
    });

    // Handler for log stream unsubscription
    register_handler("logs.unsubscribe", [](crow::websocket::connection& conn,
                                            const nlohmann::json& /*payload*/) {
        std::ostringstream oss;
        oss << "ws_" << static_cast<const void*>(&conn);
        std::string conn_id = oss.str();

        LogStreamManager::getInstance().unsubscribe(conn_id);

        nlohmann::json response = {{"type", "response"},
                                   {"command", "logs.unsubscribe"},
                                   {"status", "success"},
                                   {"data", {{"unsubscribed", true}}}};
        conn.send_text(response.dump());
    });

    // Handler for updating log stream subscription
    register_handler("logs.update", [](crow::websocket::connection& conn,
                                       const nlohmann::json& payload) {
        std::ostringstream oss;
        oss << "ws_" << static_cast<const void*>(&conn);
        std::string conn_id = oss.str();

        auto sub = LogStreamSubscription::fromJson(payload);
        LogStreamManager::getInstance().updateSubscription(conn_id, sub);

        nlohmann::json response = {{"type", "response"},
                                   {"command", "logs.update"},
                                   {"status", "success"},
                                   {"data", {{"subscription", sub.toJson()}}}};
        conn.send_text(response.dump());
    });

    // Handler for getting log stream status
    register_handler("logs.status", [](crow::websocket::connection& conn,
                                       const nlohmann::json& /*payload*/) {
        std::ostringstream oss;
        oss << "ws_" << static_cast<const void*>(&conn);
        std::string conn_id = oss.str();

        auto& manager = LogStreamManager::getInstance();
        auto sub = manager.getSubscription(conn_id);

        nlohmann::json response = {
            {"type", "response"},
            {"command", "logs.status"},
            {"status", "success"},
            {"data",
             {{"subscribed", sub.has_value()}, {"stats", manager.getStats()}}}};
        if (sub) {
            response["data"]["subscription"] = sub->toJson();
        }
        conn.send_text(response.dump());
    });
}

}  // namespace lithium::server

#endif  // LITHIUM_SERVER_WEBSOCKET_LOG_STREAM_HPP
