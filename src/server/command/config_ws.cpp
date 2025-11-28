/*
 * config_ws.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "config_ws.hpp"

#include <chrono>

#include "atom/async/message_bus.hpp"
#include "crow.h"

namespace lithium::server::config {

// ============================================================================
// ConfigWebSocketService Implementation
// ============================================================================

ConfigWebSocketService::ConfigWebSocketService(crow::SimpleApp& app,
                                               const Config& config)
    : app_(app), config_(config) {
    spdlog::info("ConfigWebSocketService created");
}

ConfigWebSocketService::~ConfigWebSocketService() {
    stop();
}

bool ConfigWebSocketService::start() {
    if (running_.load()) {
        spdlog::warn("ConfigWebSocketService already running");
        return false;
    }

    // Get ConfigManager instance
    GET_OR_CREATE_WEAK_PTR(configManager_, lithium::config::ConfigManager,
                           Constants::CONFIG_MANAGER);

    setupRoutes();
    registerConfigHooks();

    running_.store(true);
    spdlog::info("ConfigWebSocketService started");
    return true;
}

void ConfigWebSocketService::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    unregisterConfigHooks();

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.clear();
    }

    spdlog::info("ConfigWebSocketService stopped");
}

size_t ConfigWebSocketService::getClientCount() const {
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return clients_.size();
}

void ConfigWebSocketService::setupRoutes() {
    CROW_WEBSOCKET_ROUTE(app_, "/ws/config")
        .onopen([this](crow::websocket::connection& conn) {
            this->onOpen(conn);
        })
        .onclose([this](crow::websocket::connection& conn,
                        const std::string& reason, uint16_t /*code*/) {
            this->onClose(conn, reason);
        })
        .onmessage([this](crow::websocket::connection& conn,
                          const std::string& message, bool isBinary) {
            this->onMessage(conn, message, isBinary);
        });

    spdlog::info("ConfigWebSocketService routes registered at /ws/config");
}

void ConfigWebSocketService::registerConfigHooks() {
    auto configManager = configManager_.lock();
    if (!configManager) {
        spdlog::error("Failed to get ConfigManager for hook registration");
        return;
    }

    configHookId_ = configManager->addHook(
        [this](lithium::config::ConfigManager::ConfigEvent event,
               std::string_view path,
               const std::optional<nlohmann::json>& value) {
            if (!running_.load()) {
                return;
            }

            NotificationType notifType;
            nlohmann::json data;

            switch (event) {
                case lithium::config::ConfigManager::ConfigEvent::VALUE_SET:
                    notifType = NotificationType::VALUE_CHANGED;
                    if (value) {
                        data["value"] = *value;
                    }
                    break;

                case lithium::config::ConfigManager::ConfigEvent::VALUE_REMOVED:
                    notifType = NotificationType::VALUE_REMOVED;
                    break;

                case lithium::config::ConfigManager::ConfigEvent::FILE_LOADED:
                    notifType = NotificationType::FILE_LOADED;
                    break;

                case lithium::config::ConfigManager::ConfigEvent::FILE_SAVED:
                    notifType = NotificationType::FILE_SAVED;
                    break;

                case lithium::config::ConfigManager::ConfigEvent::CONFIG_CLEARED:
                    notifType = NotificationType::CONFIG_CLEARED;
                    break;

                case lithium::config::ConfigManager::ConfigEvent::CONFIG_MERGED:
                    notifType = NotificationType::CONFIG_MERGED;
                    break;

                case lithium::config::ConfigManager::ConfigEvent::VALIDATION_DONE:
                    notifType = NotificationType::VALIDATION_RESULT;
                    break;

                default:
                    // Ignore other events
                    return;
            }

            // Notify subscribers
            notifySubscribers(notifType, std::string(path), data);
        });

    spdlog::info("ConfigManager hook registered with ID: {}", configHookId_);
}

void ConfigWebSocketService::unregisterConfigHooks() {
    if (configHookId_ == 0) {
        return;
    }

    auto configManager = configManager_.lock();
    if (configManager) {
        configManager->removeHook(configHookId_);
        spdlog::info("ConfigManager hook unregistered");
    }

    configHookId_ = 0;
}

void ConfigWebSocketService::onOpen(crow::websocket::connection& conn) {
    if (!running_.load()) {
        conn.close("Service not running");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);

        if (clients_.size() >= config_.maxClients) {
            spdlog::warn("Max clients reached, rejecting connection");
            conn.close("Max clients reached");
            return;
        }

        ClientInfo info;
        info.connectedAt = std::chrono::steady_clock::now();
        info.subscribeAll = config_.enableBroadcast;
        clients_[&conn] = std::move(info);
    }

    totalConnections_.fetch_add(1);
    spdlog::info("Config WebSocket client connected. Total: {}", getClientCount());

    // Send welcome message
    nlohmann::json welcome;
    welcome["type"] = "connected";
    welcome["message"] = "Connected to config notification service";
    welcome["features"]["broadcast"] = config_.enableBroadcast;
    welcome["features"]["filtering"] = config_.enableFiltering;
    sendToClient(conn, welcome.dump());
}

void ConfigWebSocketService::onClose(crow::websocket::connection& conn,
                                     const std::string& reason) {
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clients_.erase(&conn);
    }

    spdlog::info("Config WebSocket client disconnected: {}. Total: {}",
                 reason, getClientCount());
}

void ConfigWebSocketService::onMessage(crow::websocket::connection& conn,
                                       const std::string& message,
                                       bool /*isBinary*/) {
    totalMessages_.fetch_add(1);

    try {
        auto json = nlohmann::json::parse(message);
        std::string action = json.value("action", "");

        if (action == "subscribe") {
            std::vector<std::string> paths;
            if (json.contains("paths") && json["paths"].is_array()) {
                for (const auto& p : json["paths"]) {
                    paths.push_back(p.get<std::string>());
                }
            }
            handleSubscribe(conn, paths);
        } else if (action == "unsubscribe") {
            std::vector<std::string> paths;
            if (json.contains("paths") && json["paths"].is_array()) {
                for (const auto& p : json["paths"]) {
                    paths.push_back(p.get<std::string>());
                }
            }
            handleUnsubscribe(conn, paths);
        } else if (action == "subscribe_all") {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            auto it = clients_.find(&conn);
            if (it != clients_.end()) {
                it->second.subscribeAll = true;
                it->second.subscribedPaths.clear();
            }

            nlohmann::json ack;
            ack["type"] = "subscription_ack";
            ack["action"] = "subscribe_all";
            ack["success"] = true;
            sendToClient(conn, ack.dump());
        } else if (action == "unsubscribe_all") {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            auto it = clients_.find(&conn);
            if (it != clients_.end()) {
                it->second.subscribeAll = false;
                it->second.subscribedPaths.clear();
            }

            nlohmann::json ack;
            ack["type"] = "subscription_ack";
            ack["action"] = "unsubscribe_all";
            ack["success"] = true;
            sendToClient(conn, ack.dump());
        } else if (action == "get_subscriptions") {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            auto it = clients_.find(&conn);
            if (it != clients_.end()) {
                nlohmann::json response;
                response["type"] = "subscriptions";
                response["subscribe_all"] = it->second.subscribeAll;
                response["paths"] = nlohmann::json::array();
                for (const auto& p : it->second.subscribedPaths) {
                    response["paths"].push_back(p);
                }
                sendToClient(conn, response.dump());
            }
        } else if (action == "ping") {
            nlohmann::json pong;
            pong["type"] = "pong";
            pong["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            sendToClient(conn, pong.dump());
        } else {
            nlohmann::json error;
            error["type"] = "error";
            error["message"] = "Unknown action: " + action;
            sendToClient(conn, error.dump());
        }
    } catch (const nlohmann::json::parse_error& e) {
        nlohmann::json error;
        error["type"] = "error";
        error["message"] = "Invalid JSON: " + std::string(e.what());
        sendToClient(conn, error.dump());
    } catch (const std::exception& e) {
        nlohmann::json error;
        error["type"] = "error";
        error["message"] = "Error processing message: " + std::string(e.what());
        sendToClient(conn, error.dump());
    }
}

void ConfigWebSocketService::handleSubscribe(
    crow::websocket::connection& conn,
    const std::vector<std::string>& paths) {

    if (!config_.enableFiltering) {
        nlohmann::json error;
        error["type"] = "error";
        error["message"] = "Path filtering is disabled";
        sendToClient(conn, error.dump());
        return;
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(&conn);
        if (it != clients_.end()) {
            it->second.subscribeAll = false;
            for (const auto& path : paths) {
                it->second.subscribedPaths.insert(path);
            }
        }
    }

    nlohmann::json ack;
    ack["type"] = "subscription_ack";
    ack["action"] = "subscribe";
    ack["paths"] = paths;
    ack["success"] = true;
    sendToClient(conn, ack.dump());

    spdlog::debug("Client subscribed to {} paths", paths.size());
}

void ConfigWebSocketService::handleUnsubscribe(
    crow::websocket::connection& conn,
    const std::vector<std::string>& paths) {

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto it = clients_.find(&conn);
        if (it != clients_.end()) {
            for (const auto& path : paths) {
                it->second.subscribedPaths.erase(path);
            }
        }
    }

    nlohmann::json ack;
    ack["type"] = "subscription_ack";
    ack["action"] = "unsubscribe";
    ack["paths"] = paths;
    ack["success"] = true;
    sendToClient(conn, ack.dump());

    spdlog::debug("Client unsubscribed from {} paths", paths.size());
}

bool ConfigWebSocketService::shouldNotifyClient(
    crow::websocket::connection* conn,
    const std::string& path) const {

    auto it = clients_.find(conn);
    if (it == clients_.end()) {
        return false;
    }

    const auto& info = it->second;

    // If subscribed to all, always notify
    if (info.subscribeAll) {
        return true;
    }

    // Check if path matches any subscribed paths
    for (const auto& subscribedPath : info.subscribedPaths) {
        if (path == subscribedPath) {
            return true;
        }
        if (path.find(subscribedPath + "/") == 0) {
            return true;
        }
        if (subscribedPath.find(path + "/") == 0) {
            return true;
        }
    }

    return false;
}

nlohmann::json ConfigWebSocketService::createNotification(
    NotificationType type,
    const std::string& path,
    const nlohmann::json& data) const {

    nlohmann::json notification;
    notification["type"] = notificationTypeToString(type);
    notification["path"] = path;

    if (config_.includeTimestamp) {
        notification["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    if (!data.empty()) {
        for (auto& [key, value] : data.items()) {
            notification[key] = value;
        }
    }

    return notification;
}

std::string ConfigWebSocketService::notificationTypeToString(NotificationType type) {
    switch (type) {
        case NotificationType::VALUE_CHANGED: return "value_changed";
        case NotificationType::VALUE_REMOVED: return "value_removed";
        case NotificationType::FILE_LOADED: return "file_loaded";
        case NotificationType::FILE_SAVED: return "file_saved";
        case NotificationType::CONFIG_CLEARED: return "config_cleared";
        case NotificationType::CONFIG_MERGED: return "config_merged";
        case NotificationType::VALIDATION_RESULT: return "validation_result";
        case NotificationType::SNAPSHOT_CREATED: return "snapshot_created";
        case NotificationType::SNAPSHOT_RESTORED: return "snapshot_restored";
        case NotificationType::SUBSCRIPTION_ACK: return "subscription_ack";
        case NotificationType::ERROR: return "error";
        default: return "unknown";
    }
}

void ConfigWebSocketService::broadcastNotification(
    NotificationType type,
    const std::string& path,
    const nlohmann::json& data) {

    auto notification = createNotification(type, path, data);
    std::string message = notification.dump();

    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& [conn, info] : clients_) {
        try {
            conn->send_text(message);
            info.notificationsSent++;
            totalNotifications_.fetch_add(1);
        } catch (const std::exception& e) {
            spdlog::error("Failed to send notification to client: {}", e.what());
        }
    }
}

void ConfigWebSocketService::notifySubscribers(
    NotificationType type,
    const std::string& path,
    const nlohmann::json& data) {

    auto notification = createNotification(type, path, data);
    std::string message = notification.dump();

    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& [conn, info] : clients_) {
        if (shouldNotifyClient(conn, path)) {
            try {
                conn->send_text(message);
                info.notificationsSent++;
                totalNotifications_.fetch_add(1);
            } catch (const std::exception& e) {
                spdlog::error("Failed to send notification to client: {}", e.what());
            }
        }
    }
}

void ConfigWebSocketService::sendToClient(crow::websocket::connection& conn,
                                          const std::string& message) {
    try {
        conn.send_text(message);
    } catch (const std::exception& e) {
        spdlog::error("Failed to send message to client: {}", e.what());
    }
}

nlohmann::json ConfigWebSocketService::getStatistics() const {
    nlohmann::json stats;
    stats["running"] = running_.load();
    stats["total_connections"] = totalConnections_.load();
    stats["total_notifications"] = totalNotifications_.load();
    stats["total_messages"] = totalMessages_.load();

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        stats["active_clients"] = clients_.size();

        nlohmann::json clientStats = nlohmann::json::array();
        for (const auto& [conn, info] : clients_) {
            nlohmann::json client;
            client["subscribe_all"] = info.subscribeAll;
            client["subscribed_paths"] = info.subscribedPaths.size();
            client["notifications_sent"] = info.notificationsSent;
            auto connectedDuration = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - info.connectedAt).count();
            client["connected_seconds"] = connectedDuration;
            clientStats.push_back(client);
        }
        stats["clients"] = clientStats;
    }

    return stats;
}

void ConfigWebSocketService::updateConfig(const Config& newConfig) {
    config_ = newConfig;
    spdlog::info("ConfigWebSocketService configuration updated");
}

// ============================================================================
// Integration Functions for Main WebSocketServer
// ============================================================================

void registerConfigCommands(
    std::function<void(const std::string&,
                       std::function<void(const nlohmann::json&,
                                          std::function<void(const nlohmann::json&)>)>)>
        registerHandler) {

    // config.get - Get configuration value
    registerHandler("config.get", [](const nlohmann::json& params,
                                     std::function<void(const nlohmann::json&)> respond) {
        std::string path = params.value("path", "");
        if (path.empty()) {
            respond({{"success", false}, {"error", "Missing path parameter"}});
            return;
        }

        std::weak_ptr<lithium::config::ConfigManager> configManager;
        GET_OR_CREATE_WEAK_PTR(configManager, lithium::config::ConfigManager,
                               Constants::CONFIG_MANAGER);

        auto cm = configManager.lock();
        if (!cm) {
            respond({{"success", false}, {"error", "ConfigManager not available"}});
            return;
        }

        auto value = cm->get(path);
        if (value) {
            respond({{"success", true}, {"path", path}, {"value", *value}});
        } else {
            respond({{"success", false}, {"error", "Path not found"}, {"path", path}});
        }
    });

    // config.set - Set configuration value
    registerHandler("config.set", [](const nlohmann::json& params,
                                     std::function<void(const nlohmann::json&)> respond) {
        std::string path = params.value("path", "");
        if (path.empty() || !params.contains("value")) {
            respond({{"success", false}, {"error", "Missing path or value parameter"}});
            return;
        }

        std::weak_ptr<lithium::config::ConfigManager> configManager;
        GET_OR_CREATE_WEAK_PTR(configManager, lithium::config::ConfigManager,
                               Constants::CONFIG_MANAGER);

        auto cm = configManager.lock();
        if (!cm) {
            respond({{"success", false}, {"error", "ConfigManager not available"}});
            return;
        }

        bool success = cm->set(path, params["value"]);
        respond({{"success", success}, {"path", path}});
    });

    // config.list - List configuration keys
    registerHandler("config.list", [](const nlohmann::json& /*params*/,
                                      std::function<void(const nlohmann::json&)> respond) {
        std::weak_ptr<lithium::config::ConfigManager> configManager;
        GET_OR_CREATE_WEAK_PTR(configManager, lithium::config::ConfigManager,
                               Constants::CONFIG_MANAGER);

        auto cm = configManager.lock();
        if (!cm) {
            respond({{"success", false}, {"error", "ConfigManager not available"}});
            return;
        }

        auto keys = cm->getKeys();
        respond({{"success", true}, {"keys", keys}});
    });

    spdlog::info("Config WebSocket commands registered");
}

size_t initConfigNotificationHooks(
    std::shared_ptr<atom::async::MessageBus> messageBus) {

    if (!messageBus) {
        spdlog::error("MessageBus is null, cannot initialize config hooks");
        return 0;
    }

    std::weak_ptr<lithium::config::ConfigManager> configManager;
    GET_OR_CREATE_WEAK_PTR(configManager, lithium::config::ConfigManager,
                           Constants::CONFIG_MANAGER);

    auto cm = configManager.lock();
    if (!cm) {
        spdlog::error("ConfigManager not available for hook initialization");
        return 0;
    }

    size_t hookId = cm->addHook(
        [messageBus](lithium::config::ConfigManager::ConfigEvent event,
                     std::string_view path,
                     const std::optional<nlohmann::json>& value) {
            nlohmann::json notification;
            notification["path"] = std::string(path);
            notification["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            switch (event) {
                case lithium::config::ConfigManager::ConfigEvent::VALUE_SET:
                    notification["event"] = "value_changed";
                    if (value) {
                        notification["value"] = *value;
                    }
                    break;
                case lithium::config::ConfigManager::ConfigEvent::VALUE_REMOVED:
                    notification["event"] = "value_removed";
                    break;
                case lithium::config::ConfigManager::ConfigEvent::FILE_LOADED:
                    notification["event"] = "file_loaded";
                    break;
                case lithium::config::ConfigManager::ConfigEvent::FILE_SAVED:
                    notification["event"] = "file_saved";
                    break;
                case lithium::config::ConfigManager::ConfigEvent::CONFIG_CLEARED:
                    notification["event"] = "config_cleared";
                    break;
                case lithium::config::ConfigManager::ConfigEvent::CONFIG_MERGED:
                    notification["event"] = "config_merged";
                    break;
                default:
                    return;  // Ignore other events
            }

            // Publish to message bus
            messageBus->publish(CONFIG_NOTIFICATION_TOPIC, notification);

            // Also publish to path-specific topic
            std::string pathTopic = std::string(CONFIG_SUBSCRIPTION_TOPIC_PREFIX) + std::string(path);
            messageBus->publish(pathTopic, notification);
        });

    spdlog::info("Config notification hooks initialized with ID: {}", hookId);
    return hookId;
}

void cleanupConfigNotificationHooks(size_t hookId) {
    if (hookId == 0) {
        return;
    }

    std::weak_ptr<lithium::config::ConfigManager> configManager;
    GET_OR_CREATE_WEAK_PTR(configManager, lithium::config::ConfigManager,
                           Constants::CONFIG_MANAGER);

    auto cm = configManager.lock();
    if (cm) {
        cm->removeHook(hookId);
        spdlog::info("Config notification hooks cleaned up");
    }
}

}  // namespace lithium::server::config
