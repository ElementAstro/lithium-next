/*
 * config_ws.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Description: Configuration WebSocket Commands and Notifications
             Integrates with the main WebSocketServer for config updates

**************************************************/

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "atom/function/global_ptr.hpp"
#include "atom/type/json.hpp"
#include "config/config.hpp"
#include "constant/constant.hpp"

namespace lithium::server::config {

/**
 * @brief WebSocket-based configuration notification service
 *
 * This class provides real-time configuration change notifications via
 * WebSocket. Clients can subscribe to specific configuration paths or receive
 * all updates.
 *
 * Features:
 * - Real-time configuration change notifications
 * - Path-based subscription filtering
 * - Automatic hook registration with ConfigManager
 * - Thread-safe client management
 * - JSON-formatted notification messages
 */
class ConfigWebSocketService {
public:
    /**
     * @brief Configuration for the WebSocket service
     */
    struct Config {
        bool enableBroadcast{true};  ///< Broadcast all changes to all clients
        bool enableFiltering{true};  ///< Allow clients to filter by path
        size_t maxClients{100};      ///< Maximum number of WebSocket clients
        bool includeOldValue{
            false};  ///< Include old value in change notifications
        bool includeTimestamp{true};  ///< Include timestamp in notifications
    };

    /**
     * @brief Notification message types
     */
    enum class NotificationType {
        VALUE_CHANGED,      ///< Configuration value changed
        VALUE_REMOVED,      ///< Configuration value removed
        FILE_LOADED,        ///< Configuration file loaded
        FILE_SAVED,         ///< Configuration file saved
        CONFIG_CLEARED,     ///< Configuration cleared
        CONFIG_MERGED,      ///< Configuration merged
        VALIDATION_RESULT,  ///< Validation result
        SNAPSHOT_CREATED,   ///< Snapshot created
        SNAPSHOT_RESTORED,  ///< Snapshot restored
        SUBSCRIPTION_ACK,   ///< Subscription acknowledgment
        ERROR               ///< Error notification
    };

    /**
     * @brief Constructor
     * @param app Reference to Crow application
     * @param config Service configuration
     */
    explicit ConfigWebSocketService(crow::SimpleApp& app,
                                    const Config& config = {});

    /**
     * @brief Destructor - cleans up hooks and connections
     */
    ~ConfigWebSocketService();

    ConfigWebSocketService(const ConfigWebSocketService&) = delete;
    ConfigWebSocketService& operator=(const ConfigWebSocketService&) = delete;

    /**
     * @brief Start the WebSocket service
     * @return True if started successfully
     */
    bool start();

    /**
     * @brief Stop the WebSocket service
     */
    void stop();

    /**
     * @brief Check if service is running
     * @return True if running
     */
    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

    /**
     * @brief Get number of connected clients
     * @return Number of clients
     */
    [[nodiscard]] size_t getClientCount() const;

    /**
     * @brief Broadcast a notification to all clients
     * @param type Notification type
     * @param path Configuration path
     * @param data Additional data
     */
    void broadcastNotification(NotificationType type, const std::string& path,
                               const nlohmann::json& data = {});

    /**
     * @brief Send notification to clients subscribed to a path
     * @param type Notification type
     * @param path Configuration path
     * @param data Additional data
     */
    void notifySubscribers(NotificationType type, const std::string& path,
                           const nlohmann::json& data = {});

    /**
     * @brief Get service statistics
     * @return JSON object with statistics
     */
    [[nodiscard]] nlohmann::json getStatistics() const;

    /**
     * @brief Update service configuration
     * @param newConfig New configuration
     */
    void updateConfig(const Config& newConfig);

private:
    /**
     * @brief Client subscription information
     */
    struct ClientInfo {
        std::set<std::string>
            subscribedPaths;       ///< Paths client is subscribed to
        bool subscribeAll{false};  ///< Subscribe to all changes
        std::chrono::steady_clock::time_point connectedAt;
        size_t notificationsSent{0};
    };

    /**
     * @brief Setup WebSocket routes
     */
    void setupRoutes();

    /**
     * @brief Register hooks with ConfigManager
     */
    void registerConfigHooks();

    /**
     * @brief Unregister hooks from ConfigManager
     */
    void unregisterConfigHooks();

    /**
     * @brief Handle new WebSocket connection
     * @param conn WebSocket connection
     */
    void onOpen(crow::websocket::connection& conn);

    /**
     * @brief Handle WebSocket connection close
     * @param conn WebSocket connection
     * @param reason Close reason
     */
    void onClose(crow::websocket::connection& conn, const std::string& reason);

    /**
     * @brief Handle incoming WebSocket message
     * @param conn WebSocket connection
     * @param message Received message
     * @param isBinary Whether message is binary
     */
    void onMessage(crow::websocket::connection& conn,
                   const std::string& message, bool isBinary);

    /**
     * @brief Handle client subscription request
     * @param conn WebSocket connection
     * @param paths Paths to subscribe to
     */
    void handleSubscribe(crow::websocket::connection& conn,
                         const std::vector<std::string>& paths);

    /**
     * @brief Handle client unsubscription request
     * @param conn WebSocket connection
     * @param paths Paths to unsubscribe from
     */
    void handleUnsubscribe(crow::websocket::connection& conn,
                           const std::vector<std::string>& paths);

    /**
     * @brief Check if client should receive notification for path
     * @param conn WebSocket connection
     * @param path Configuration path
     * @return True if client should receive notification
     */
    bool shouldNotifyClient(crow::websocket::connection* conn,
                            const std::string& path) const;

    /**
     * @brief Create notification JSON message
     * @param type Notification type
     * @param path Configuration path
     * @param data Additional data
     * @return JSON message
     */
    nlohmann::json createNotification(NotificationType type,
                                      const std::string& path,
                                      const nlohmann::json& data) const;

    /**
     * @brief Get string representation of notification type
     * @param type Notification type
     * @return String representation
     */
    static std::string notificationTypeToString(NotificationType type);

    /**
     * @brief Send message to a client
     * @param conn WebSocket connection
     * @param message Message to send
     */
    void sendToClient(crow::websocket::connection& conn,
                      const std::string& message);

    crow::SimpleApp& app_;              ///< Reference to Crow app
    Config config_;                     ///< Service configuration
    std::atomic<bool> running_{false};  ///< Running state

    mutable std::mutex clientsMutex_;  ///< Mutex for clients map
    std::unordered_map<crow::websocket::connection*, ClientInfo> clients_;

    std::weak_ptr<lithium::config::ConfigManager> configManager_;
    size_t configHookId_{0};  ///< ConfigManager hook ID

    // Statistics
    std::atomic<size_t> totalNotifications_{0};
    std::atomic<size_t> totalConnections_{0};
    std::atomic<size_t> totalMessages_{0};
};

/**
 * @brief Register configuration-related WebSocket command handlers
 *
 * This function registers handlers for config-related WebSocket commands
 * with the main WebSocketServer. It should be called during server
 * initialization.
 *
 * Supported commands:
 * - config.subscribe: Subscribe to config path changes
 * - config.unsubscribe: Unsubscribe from config path changes
 * - config.get: Get configuration value
 * - config.set: Set configuration value
 * - config.list: List configuration paths
 *
 * @param registerHandler Function to register a command handler
 */
void registerConfigCommands(
    std::function<
        void(const std::string&,
             std::function<void(const nlohmann::json&,
                                std::function<void(const nlohmann::json&)>)>)>
        registerHandler);

/**
 * @brief Initialize configuration notification hooks
 *
 * Sets up hooks with ConfigManager to broadcast configuration changes
 * to subscribed WebSocket clients via the message bus.
 *
 * @param messageBus Shared pointer to the message bus for broadcasting
 * @return Hook ID for later cleanup
 */
size_t initConfigNotificationHooks(
    std::shared_ptr<atom::async::MessageBus> messageBus);

/**
 * @brief Cleanup configuration notification hooks
 * @param hookId The hook ID returned from initConfigNotificationHooks
 */
void cleanupConfigNotificationHooks(size_t hookId);

/**
 * @brief Configuration notification topic name
 */
constexpr const char* CONFIG_NOTIFICATION_TOPIC = "config.notifications";

/**
 * @brief Configuration subscription topic prefix
 */
constexpr const char* CONFIG_SUBSCRIPTION_TOPIC_PREFIX = "config.subscribe.";

}  // namespace lithium::server::config
