#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atom/async/message_bus.hpp"
#include "atom/async/pool.hpp"
#include "atom/type/json_fwd.hpp"

#include "command.hpp"
#include "rate_limiter.hpp"
#include "crow.h"

/**
 * @brief WebSocket server class for managing WebSocket connections and
 * communication.
 */
class WebSocketServer {
public:
    /**
     * @brief Configuration structure for WebSocketServer.
     */
    struct Config {
        uint64_t max_payload_size =
            UINT64_MAX;  ///< Maximum payload size for WebSocket messages.
        std::vector<std::string>
            subprotocols;  ///< List of supported subprotocols.
        size_t max_retry_attempts =
            3;  ///< Maximum number of retry attempts for failed connections.
        std::chrono::milliseconds retry_delay{
            1000};  ///< Delay between retry attempts.
        bool enable_compression =
            false;  ///< Flag to enable or disable compression.
        size_t max_connections =
            1000;  ///< Maximum number of concurrent connections.

        // 新增配置选项
        size_t thread_pool_size = 4;       // 线程池大小
        size_t message_queue_size = 1000;  // 消息队列大小
        bool enable_ssl = false;           // 是否启用SSL
        std::string ssl_cert;              // SSL证书路径
        std::string ssl_key;               // SSL密钥路径
        size_t ping_interval = 30;         // 心跳间隔(秒)
        size_t connection_timeout = 60;    // 连接超时(秒)
    };

    /**
     * @brief Constructs a WebSocketServer instance.
     *
     * @param app Reference to the crow::SimpleApp instance.
     * @param message_bus Shared pointer to the message bus.
     * @param command_dispatcher Shared pointer to the command dispatcher.
     * @param config Configuration settings for the WebSocket server.
     */
    WebSocketServer(
        crow::SimpleApp& app,
        std::shared_ptr<atom::async::MessageBus> message_bus,
        std::shared_ptr<lithium::app::CommandDispatcher> command_dispatcher,
        const Config& config);

    /**
     * @brief Starts the WebSocket server.
     */
    void start();

    /**
     * @brief Stops the WebSocket server.
     */
    void stop();

    /**
     * @brief Checks if the WebSocket server is running.
     *
     * @return True if the server is running, false otherwise.
     */
    bool is_running() const;

    /**
     * @brief Broadcasts a message to all connected clients.
     *
     * @param msg The message to broadcast.
     */
    void broadcast(const std::string& msg);

    /**
     * @brief Broadcasts a batch of messages to all connected clients.
     *
     * @param messages The messages to broadcast.
     */
    void broadcast_batch(const std::vector<std::string>& messages);

    /**
     * @brief Sends a message to a specific client.
     *
     * @param conn Reference to the WebSocket connection.
     * @param msg The message to send.
     */
    void send_to_client(crow::websocket::connection& conn,
                        const std::string& msg);

    /**
     * @brief Sets the maximum payload size for WebSocket messages.
     *
     * @param size The maximum payload size in bytes.
     */
    void set_max_payload(uint64_t size);

    /**
     * @brief Sets the supported subprotocols for the WebSocket server.
     *
     * @param protocols A vector of subprotocol strings.
     */
    void set_subprotocols(const std::vector<std::string>& protocols);

    /**
     * @brief Subscribes to a specific topic.
     *
     * @param topic The topic to subscribe to.
     */
    void subscribe_to_topic(const std::string& topic);

    /**
     * @brief Unsubscribes from a specific topic.
     *
     * @param topic The topic to unsubscribe from.
     */
    void unsubscribe_from_topic(const std::string& topic);

    /**
     * @brief Publishes data to a specific topic.
     *
     * @tparam T The type of data to publish.
     * @param topic The topic to publish to.
     * @param data The data to publish.
     */
    template <typename T>
    void publish_to_topic(const std::string& topic, const T& data);

    /**
     * @brief Registers a message handler for a specific message type.
     *
     * @param message_type The type of message to handle.
     * @param handler The handler function to call when a message of the
     * specified type is received.
     */
    void register_message_handler(
        const std::string& message_type,
        std::function<void(crow::websocket::connection&, const nlohmann::json&)>
            handler);

    /**
     * @brief Authenticates a client connection.
     *
     * @param conn Reference to the WebSocket connection.
     * @param token The authentication token.
     * @return True if the client is authenticated, false otherwise.
     */
    bool authenticate_client(crow::websocket::connection& conn,
                             const std::string& token);

    /**
     * @brief Disconnects a client connection.
     *
     * @param conn Reference to the WebSocket connection.
     */
    void disconnect_client(crow::websocket::connection& conn);

    /**
     * @brief Gets the number of active connections.
     *
     * @return The number of active connections.
     */
    size_t get_active_connections() const;

    /**
     * @brief Gets the list of subscribed topics.
     *
     * @return A vector of subscribed topic strings.
     */
    std::vector<std::string> get_subscribed_topics() const;

    /**
     * @brief Sets the rate limit for connections.
     *
     * @param messages_per_second The maximum number of messages per second.
     */
    void set_rate_limit(size_t messages_per_second);

    /**
     * @brief Gets the performance statistics.
     *
     * @return A JSON object containing the performance statistics.
     */
    auto get_stats() const -> nlohmann::json;

    /**
     * @brief Sets the compression settings for messages.
     *
     * @param enable Flag to enable or disable compression.
     * @param level The compression level (default is 6).
     */
    void set_compression(bool enable, int level = 6);

private:
    void on_open(crow::websocket::connection& conn);
    void on_close(crow::websocket::connection& conn, const std::string& reason,
                  uint16_t code);
    void on_message(crow::websocket::connection& conn,
                    const std::string& message, bool is_binary);
    void on_error(crow::websocket::connection& conn,
                  const std::string& error_message);

    void handle_command(crow::websocket::connection& conn,
                        const std::string& command, const std::string& payload);
    void forward_to_message_bus(const std::string& topic,
                                const std::string& message);

    crow::SimpleApp& app_;
    std::shared_ptr<atom::async::MessageBus> message_bus_;
    std::shared_ptr<lithium::app::CommandDispatcher> command_dispatcher_;
    std::unordered_set<crow::websocket::connection*> clients_;
    std::mutex conn_mutex_;

    uint64_t max_payload_size_ = UINT64_MAX;
    std::vector<std::string> subprotocols_;

    Config config_;
    std::unordered_map<std::string,
                       std::function<void(crow::websocket::connection&,
                                          const nlohmann::json&)>>
        message_handlers_;

    std::unordered_map<crow::websocket::connection*, std::string>
        client_tokens_;
    std::unordered_map<std::string, std::set<crow::websocket::connection*>>
        topic_subscribers_;

    std::atomic<size_t> retry_count_{0};
    std::unordered_map<std::string, atom::async::MessageBus::Token>
        bus_subscriptions_;

    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::unique_ptr<atom::async::ThreadPool<>> thread_pool_;

    std::atomic<size_t> total_messages_{0};
    std::atomic<size_t> error_count_{0};

    std::unique_ptr<RateLimiter> rate_limiter_;

    bool compression_enabled_{false};
    int compression_level_{6};

    void setup_message_bus_handlers();
    void setup_command_handlers();
    void handle_client_message(crow::websocket::connection& conn,
                               const nlohmann::json& message);

    template <typename T>
    void broadcast_to_topic(const std::string& topic, const T& data);

    bool validate_message_format(const nlohmann::json& message);
    void handle_connection_error(crow::websocket::connection& conn,
                                 const std::string& error);

    void run_server();        // 服务器主循环
    void check_timeouts();    // 检查超时连接
    void handle_ping_pong();  // 处理心跳
};

/**
 * @brief Handles ping command.
 *
 * @param conn Reference to the WebSocket connection.
 * @param msg The message received.
 */
void handle_ping(crow::websocket::connection& conn, const std::string& msg);

/**
 * @brief Handles echo command.
 *
 * @param conn Reference to the WebSocket connection.
 * @param msg The message received.
 */
void handle_echo(crow::websocket::connection& conn, const std::string& msg);

/**
 * @brief Handles long task command.
 *
 * @param conn Reference to the WebSocket connection.
 * @param msg The message received.
 */
void handle_long_task(crow::websocket::connection& conn,
                      const std::string& msg);

/**
 * @brief Handles JSON command.
 *
 * @param conn Reference to the WebSocket connection.
 * @param msg The message received.
 */
void handle_json(crow::websocket::connection& conn, const std::string& msg);