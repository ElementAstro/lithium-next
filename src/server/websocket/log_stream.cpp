/*
 * log_stream.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "log_stream.hpp"

#include <sstream>
#include <vector>

#include "atom/log/spdlog_logger.hpp"

namespace lithium::server {

// ============================================================================
// LogStreamManager Implementation
// ============================================================================

auto LogStreamManager::getInstance() -> LogStreamManager& {
    static LogStreamManager instance;
    return instance;
}

LogStreamManager::~LogStreamManager() {
    if (initialized_) {
        shutdown();
    }
}

void LogStreamManager::initialize() {
    std::unique_lock lock(mutex_);

    if (initialized_) {
        return;
    }

    // Register callback with LoggingManager to receive log entries
    logging::LoggingManager::getInstance().subscribe(
        "log_stream_manager",
        [this](const logging::LogEntry& entry) { onLogEntry(entry); });

    initialized_ = true;
}

void LogStreamManager::shutdown() {
    std::unique_lock lock(mutex_);

    if (!initialized_) {
        return;
    }

    // Unsubscribe from LoggingManager
    logging::LoggingManager::getInstance().unsubscribe("log_stream_manager");

    // Clear all subscribers
    subscribers_.clear();

    initialized_ = false;
}

auto LogStreamManager::isInitialized() const -> bool {
    return initialized_.load();
}

void LogStreamManager::subscribe(
    const std::string& conn_id, const LogStreamSubscription& subscription,
    std::function<void(const std::string&)> send_callback) {
    std::unique_lock lock(mutex_);

    subscribers_[conn_id] =
        SubscriberInfo{.subscription = subscription,
                       .send_callback = std::move(send_callback)};
}

void LogStreamManager::updateSubscription(
    const std::string& conn_id, const LogStreamSubscription& subscription) {
    std::unique_lock lock(mutex_);

    auto it = subscribers_.find(conn_id);
    if (it != subscribers_.end()) {
        it->second.subscription = subscription;
    }
}

void LogStreamManager::unsubscribe(const std::string& conn_id) {
    std::unique_lock lock(mutex_);
    subscribers_.erase(conn_id);
}

auto LogStreamManager::isSubscribed(const std::string& conn_id) const -> bool {
    std::shared_lock lock(mutex_);
    return subscribers_.contains(conn_id);
}

auto LogStreamManager::getSubscription(const std::string& conn_id) const
    -> std::optional<LogStreamSubscription> {
    std::shared_lock lock(mutex_);

    auto it = subscribers_.find(conn_id);
    if (it != subscribers_.end()) {
        return it->second.subscription;
    }
    return std::nullopt;
}

auto LogStreamManager::getSubscriberCount() const -> size_t {
    std::shared_lock lock(mutex_);
    return subscribers_.size();
}

auto LogStreamManager::getStats() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    return {{"subscriber_count", subscribers_.size()},
            {"total_entries_sent", total_entries_sent_.load()},
            {"total_entries_filtered", total_entries_filtered_.load()},
            {"initialized", initialized_.load()}};
}

auto LogStreamManager::handleMessage(
    const std::string& conn_id, const nlohmann::json& message,
    std::function<void(const std::string&)> send_callback) -> bool {
    // Check if this is a log stream message
    if (!message.contains("type")) {
        return false;
    }

    std::string type = message["type"];

    // Handle topic-based subscription
    if (type == "subscribe" && message.contains("topic")) {
        std::string topic = message["topic"];
        if (topic == "logs") {
            LogStreamSubscription sub;
            if (message.contains("options")) {
                sub = LogStreamSubscription::fromJson(message["options"]);
            }
            subscribe(conn_id, sub, send_callback);

            nlohmann::json response = {{"type", "subscribed"},
                                       {"topic", "logs"},
                                       {"subscription", sub.toJson()}};
            send_callback(response.dump());
            return true;
        }
    }

    if (type == "unsubscribe" && message.contains("topic")) {
        std::string topic = message["topic"];
        if (topic == "logs") {
            unsubscribe(conn_id);

            nlohmann::json response = {{"type", "unsubscribed"},
                                       {"topic", "logs"}};
            send_callback(response.dump());
            return true;
        }
    }

    // Handle command-based messages
    if (type == "command" && message.contains("command")) {
        std::string command = message["command"];

        if (command == "logs.subscribe") {
            LogStreamSubscription sub;
            if (message.contains("payload")) {
                sub = LogStreamSubscription::fromJson(message["payload"]);
            }
            subscribe(conn_id, sub, send_callback);

            nlohmann::json response = {
                {"type", "response"},
                {"command", "logs.subscribe"},
                {"status", "success"},
                {"data", {{"subscription", sub.toJson()}}}};
            send_callback(response.dump());
            return true;
        }

        if (command == "logs.unsubscribe") {
            unsubscribe(conn_id);

            nlohmann::json response = {{"type", "response"},
                                       {"command", "logs.unsubscribe"},
                                       {"status", "success"}};
            send_callback(response.dump());
            return true;
        }

        if (command == "logs.update") {
            if (message.contains("payload")) {
                auto sub = LogStreamSubscription::fromJson(message["payload"]);
                updateSubscription(conn_id, sub);

                nlohmann::json response = {
                    {"type", "response"},
                    {"command", "logs.update"},
                    {"status", "success"},
                    {"data", {{"subscription", sub.toJson()}}}};
                send_callback(response.dump());
                return true;
            }
        }

        if (command == "logs.status") {
            auto sub = getSubscription(conn_id);

            nlohmann::json response = {
                {"type", "response"},
                {"command", "logs.status"},
                {"status", "success"},
                {"data",
                 {{"subscribed", sub.has_value()}, {"stats", getStats()}}}};
            if (sub) {
                response["data"]["subscription"] = sub->toJson();
            }
            send_callback(response.dump());
            return true;
        }
    }

    return false;
}

void LogStreamManager::onLogEntry(const logging::LogEntry& entry) {
    std::shared_lock lock(mutex_);

    if (subscribers_.empty()) {
        return;
    }

    // Track failed connections for cleanup
    std::vector<std::string> failed_connections;

    for (const auto& [conn_id, info] : subscribers_) {
        if (!info.subscription.enabled) {
            continue;
        }

        if (!matchesFilter(entry, info.subscription)) {
            ++total_entries_filtered_;
            continue;
        }

        try {
            auto formatted = formatLogEntry(entry, info.subscription);
            nlohmann::json message = {
                {"type", "event"}, {"event", "log"}, {"data", formatted}};
            info.send_callback(message.dump());
            ++total_entries_sent_;
        } catch (const std::exception& e) {
            // Log the error but don't spam - connection may be closed
            LOG_DEBUG("Failed to send log to subscriber {}: {}", conn_id,
                      e.what());
            failed_connections.push_back(conn_id);
        } catch (...) {
            // Unknown error - mark for cleanup
            LOG_DEBUG("Unknown error sending log to subscriber {}", conn_id);
            failed_connections.push_back(conn_id);
        }
    }

    // Note: Cleanup of failed connections should be handled by the
    // connection manager when WebSocket close events occur
}

auto LogStreamManager::formatLogEntry(const logging::LogEntry& entry,
                                      const LogStreamSubscription& sub) const
    -> nlohmann::json {
    nlohmann::json j = entry.toJson();

    // Remove source info if not requested
    if (!sub.include_source) {
        j.erase("source_file");
        j.erase("source_line");
    }

    return j;
}

auto LogStreamManager::matchesFilter(const logging::LogEntry& entry,
                                     const LogStreamSubscription& sub) const
    -> bool {
    // Check level filter
    if (sub.level_filter && entry.level < *sub.level_filter) {
        return false;
    }

    // Check logger filter
    if (sub.logger_filter) {
        if (entry.logger_name.find(*sub.logger_filter) == std::string::npos) {
            return false;
        }
    }

    return true;
}

}  // namespace lithium::server
