#pragma once

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atom/async/message_bus.hpp"
#include "atom/async/pool.hpp"
#include "atom/type/json_fwd.hpp"

#include "command.hpp"
#include "crow.h"
#include "rate_limiter.hpp"

/**
 * @brief WebSocketServer provides a high-level interface for managing WebSocket
 * connections, broadcasting messages, handling commands, authentication, topic
 * subscriptions, and integration with a message bus and command dispatcher.
 *
 * This class is thread-safe and designed for high concurrency and scalability.
 * It supports rate limiting, connection timeouts, message compression, and
 * custom command handlers.
 */
class WebSocketServer {
public:
    /**
     * @brief Configuration structure for WebSocketServer.
     *
     * Contains all tunable parameters for server operation, including payload
     * size, thread pool, SSL, connection timeouts, and more.
     */
    struct Config {
        uint64_t max_payload_size =
            UINT64_MAX;  ///< Maximum payload size for WebSocket messages
                         ///< (bytes).
        std::vector<std::string>
            subprotocols;  ///< List of supported WebSocket subprotocols.
        size_t max_retry_attempts =
            3;  ///< Maximum retry attempts for failed connections.
        std::chrono::milliseconds retry_delay{
            1000};  ///< Delay between retry attempts (milliseconds).
        bool enable_compression =
            false;  ///< Enable or disable per-message compression.
        size_t max_connections =
            1000;  ///< Maximum number of concurrent client connections.
        size_t thread_pool_size =
            4;  ///< Number of threads in the server's thread pool.
        size_t message_queue_size =
            1000;  ///< Maximum size of the internal message queue.
        bool enable_ssl =
            false;  ///< Enable SSL/TLS for secure WebSocket connections.
        std::string ssl_cert;  ///< Path to the SSL certificate file.
        std::string ssl_key;   ///< Path to the SSL private key file.
        size_t ping_interval =
            30;  ///< Interval (seconds) between ping frames for keepalive.
        size_t connection_timeout =
            60;  ///< Connection timeout (seconds) for idle clients.
    };

    /**
     * @brief Constructs a WebSocketServer instance.
     *
     * @param app Reference to the Crow application instance.
     * @param message_bus Shared pointer to the message bus for inter-component
     * communication.
     * @param command_dispatcher Shared pointer to the command dispatcher for
     * handling commands.
     * @param config Configuration settings for the WebSocket server.
     */
    WebSocketServer(
        crow::SimpleApp& app,
        std::shared_ptr<atom::async::MessageBus> message_bus,
        std::shared_ptr<lithium::app::CommandDispatcher> command_dispatcher,
        const Config& config);

    /**
     * @brief Starts the WebSocket server in a background thread.
     *
     * Initializes the thread pool and begins listening for WebSocket
     * connections. This method is thread-safe and idempotent.
     */
    void start();

    /**
     * @brief Stops the WebSocket server and joins the background thread.
     *
     * Cleans up resources, stops the thread pool, and disconnects all clients.
     * This method is thread-safe and idempotent.
     */
    void stop();

    /**
     * @brief Checks if the WebSocket server is currently running.
     *
     * @return True if the server is running, false otherwise.
     */
    bool is_running() const;

    /**
     * @brief Broadcasts a message to all connected clients.
     *
     * @param msg The message to broadcast (as a string).
     *
     * This method is thread-safe and will respect the configured rate limit.
     */
    void broadcast(const std::string& msg);

    /**
     * @brief Broadcasts a batch of messages to all connected clients.
     *
     * @param messages The messages to broadcast.
     *
     * Each message in the batch is sent to all clients. Rate limiting is
     * enforced per message.
     */
    void broadcast_batch(const std::vector<std::string>& messages);

    /**
     * @brief Sends a message to a specific client.
     *
     * @param conn Reference to the WebSocket connection.
     * @param msg The message to send.
     *
     * If sending fails, the connection error handler will be invoked.
     */
    void send_to_client(crow::websocket::connection& conn,
                        const std::string& msg);

    /**
     * @brief Sets the maximum payload size for WebSocket messages.
     *
     * @param size The maximum payload size in bytes.
     *
     * Messages exceeding this size will be rejected.
     */
    void set_max_payload(uint64_t size);

    /**
     * @brief Sets the supported subprotocols for the WebSocket server.
     *
     * @param protocols A vector of subprotocol strings.
     *
     * These are advertised to clients during the WebSocket handshake.
     */
    void set_subprotocols(const std::vector<std::string>& protocols);

    /**
     * @brief Subscribes the server to a specific topic on the message bus.
     *
     * @param topic The topic to subscribe to.
     *
     * All messages published to this topic will be broadcast to relevant
     * clients.
     */
    void subscribe_to_topic(const std::string& topic);

    /**
     * @brief Unsubscribes the server from a specific topic on the message bus.
     *
     * @param topic The topic to unsubscribe from.
     */
    void unsubscribe_from_topic(const std::string& topic);

    /**
     * @brief Publishes data to a specific topic on the message bus.
     *
     * @tparam T The type of data to publish (must be serializable to JSON).
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
     *
     * Allows for custom message processing logic.
     */
    void register_message_handler(
        const std::string& message_type,
        std::function<void(crow::websocket::connection&, const nlohmann::json&)>
            handler);

    /**
     * @brief Authenticates a client connection using a token.
     *
     * @param conn Reference to the WebSocket connection.
     * @param token The authentication token.
     * @return True if the client is authenticated, false otherwise.
     *
     * If authentication fails, the connection will be closed with an error.
     */
    bool authenticate_client(crow::websocket::connection& conn,
                             const std::string& token);

    /**
     * @brief Disconnects a client connection.
     *
     * @param conn Reference to the WebSocket connection.
     *
     * Removes the client from all internal tracking structures and closes the
     * connection.
     */
    void disconnect_client(crow::websocket::connection& conn);

    /**
     * @brief Gets the number of active client connections.
     *
     * @return The number of active connections.
     */
    size_t get_active_connections() const;

    /**
     * @brief Gets the list of topics to which clients are currently subscribed.
     *
     * @return A vector of subscribed topic strings.
     */
    std::vector<std::string> get_subscribed_topics() const;

    /**
     * @brief Sets the rate limit for outgoing messages.
     *
     * @param messages_per_second The maximum number of messages per second.
     *
     * Rate limiting is enforced for broadcast and batch operations.
     */
    void set_rate_limit(size_t messages_per_second);

    /**
     * @brief Gets the server's performance statistics.
     *
     * @return A JSON object containing statistics such as total messages sent,
     * error count, and active connections.
     */
    auto get_stats() const -> nlohmann::json;

    /**
     * @brief Sets the compression settings for outgoing messages.
     *
     * @param enable Flag to enable or disable compression.
     * @param level The compression level (default is 6).
     *
     * Compression can reduce bandwidth usage at the cost of CPU.
     */
    void set_compression(bool enable, int level = 6);

private:
    /**
     * @brief Handles a new client connection.
     *
     * @param conn Reference to the WebSocket connection.
     */
    void on_open(crow::websocket::connection& conn);

    /**
     * @brief Handles a client disconnection.
     *
     * @param conn Reference to the WebSocket connection.
     * @param reason The reason for disconnection.
     * @param code The close code.
     */
    void on_close(crow::websocket::connection& conn, const std::string& reason,
                  uint16_t code);

    /**
     * @brief Handles an incoming message from a client.
     *
     * @param conn Reference to the WebSocket connection.
     * @param message The received message.
     * @param is_binary True if the message is binary, false if text.
     */
    void on_message(crow::websocket::connection& conn,
                    const std::string& message, bool is_binary);

    /**
     * @brief Handles an error on a client connection.
     *
     * @param conn Reference to the WebSocket connection.
     * @param error_message The error message.
     */
    void on_error(crow::websocket::connection& conn,
                  const std::string& error_message);

    /**
     * @brief Handles a command received from a client.
     *
     * @param conn Reference to the WebSocket connection.
     * @param command The command name.
     * @param payload The command payload as JSON.
     */
    void handle_command(crow::websocket::connection& conn,
                        const std::string& command,
                        const nlohmann::json& payload,
                        const std::string& request_id);

    /**
     * @brief Forwards a message to the message bus.
     *
     * @param topic The topic to forward to.
     * @param message The message to forward.
     */
    void forward_to_message_bus(const std::string& topic,
                                const std::string& message);

    crow::SimpleApp& app_;  ///< Reference to the Crow application instance.
    std::shared_ptr<atom::async::MessageBus>
        message_bus_;  ///< Shared pointer to the message bus.
    std::shared_ptr<lithium::app::CommandDispatcher>
        command_dispatcher_;  ///< Shared pointer to the command dispatcher.
    std::unordered_set<crow::websocket::connection*>
        clients_;  ///< Set of active client connections.
    std::shared_mutex
        conn_mutex_;  ///< Mutex for synchronizing access to client data.

    uint64_t max_payload_size_ = UINT64_MAX;  ///< Maximum allowed payload size.
    std::vector<std::string>
        subprotocols_;  ///< Supported WebSocket subprotocols.

    Config config_;  ///< Server configuration.

    std::unordered_map<std::string,
                       std::function<void(crow::websocket::connection&,
                                          const nlohmann::json&)>>
        message_handlers_;  ///< Custom message handlers by type.

    std::unordered_map<crow::websocket::connection*, std::string>
        client_tokens_;  ///< Authentication tokens for each client.
    std::unordered_map<std::string, std::set<crow::websocket::connection*>>
        topic_subscribers_;  ///< Mapping from topic to set of subscribed
                             ///< clients.

    std::atomic<size_t> retry_count_{
        0};  ///< Retry counter for connection attempts.
    std::unordered_map<std::string, atom::async::MessageBus::Token>
        bus_subscriptions_;  ///< Tokens for message bus subscriptions.

    std::atomic<bool> running_{false};  ///< Server running state.
    std::thread server_thread_;         ///< Background server thread.
    std::unique_ptr<atom::async::ThreadPool<>>
        thread_pool_;  ///< Thread pool for async tasks.

    std::atomic<size_t> total_messages_{0};  ///< Total number of messages sent.
    std::atomic<size_t> error_count_{
        0};  ///< Total number of errors encountered.

    std::unique_ptr<RateLimiter>
        rate_limiter_;  ///< Rate limiter for outgoing messages.

    bool compression_enabled_{
        false};                 ///< Whether message compression is enabled.
    int compression_level_{6};  ///< Compression level.

    /**
     * @brief Tracks the last activity time for each client connection.
     */
    std::unordered_map<crow::websocket::connection*,
                       std::chrono::steady_clock::time_point>
        last_activity_times_;

    /**
     * @brief Updates the last activity time for a client connection.
     *
     * @param conn Pointer to the WebSocket connection.
     */
    void update_activity_time(crow::websocket::connection* conn);

    /**
     * @brief Gets the last activity time for a client connection.
     *
     * @param conn Pointer to the WebSocket connection.
     * @return The last activity time as a steady_clock::time_point.
     */
    std::chrono::steady_clock::time_point get_last_activity_time(
        crow::websocket::connection* conn) const;

    /**
     * @brief Sets up message bus handlers for internal topics.
     */
    void setup_message_bus_handlers();

    /**
     * @brief Sets up command handlers for built-in commands.
     */
    void setup_command_handlers();

    /**
     * @brief Handles a parsed JSON message from a client.
     *
     * @param conn Reference to the WebSocket connection.
     * @param message The parsed JSON message.
     */
    void handle_client_message(crow::websocket::connection& conn,
                               const nlohmann::json& message);

    /**
     * @brief Broadcasts a message to all clients subscribed to a topic.
     *
     * @tparam T The type of the message payload.
     * @param topic The topic name.
     * @param data The message payload.
     */
    template <typename T>
    void broadcast_to_topic(const std::string& topic, const T& data);

    /**
     * @brief Validates the format of a received JSON message.
     *
     * @param message The JSON message to validate.
     * @return True if the message format is valid, false otherwise.
     */
    bool validate_message_format(const nlohmann::json& message);

    /**
     * @brief Handles errors on a client connection.
     *
     * @param conn Reference to the WebSocket connection.
     * @param error The error message.
     */
    void handle_connection_error(crow::websocket::connection& conn,
                                 const std::string& error);

    /**
     * @brief Main server loop for accepting and managing connections.
     */
    void run_server();

    /**
     * @brief Checks for and disconnects timed-out client connections.
     */
    void check_timeouts();

    /**
     * @brief Sends ping frames to all clients to keep connections alive.
     */
    void handle_ping_pong();

    /**
     * @brief Subscribes a client to a topic.
     *
     * @param conn Pointer to the WebSocket connection.
     * @param topic The topic to subscribe to.
     */
    void subscribe_client_to_topic(crow::websocket::connection* conn,
                                   const std::string& topic);

    /**
     * @brief Unsubscribes a client from a topic.
     *
     * @param conn Pointer to the WebSocket connection.
     * @param topic The topic to unsubscribe from.
     */
    void unsubscribe_client_from_topic(crow::websocket::connection* conn,
                                       const std::string& topic);
};

/**
 * @brief Handles the "ping" command from a client.
 *
 * @param conn Reference to the WebSocket connection.
 * @param msg The message received (may be ignored).
 *
 * Sends a "pong" response to the client.
 */
void handle_ping(crow::websocket::connection& conn, const std::string& msg);

/**
 * @brief Handles the "echo" command from a client.
 *
 * @param conn Reference to the WebSocket connection.
 * @param msg The message received.
 *
 * Sends the received message back to the client.
 */
void handle_echo(crow::websocket::connection& conn, const std::string& msg);

/**
 * @brief Handles a long-running task command from a client.
 *
 * @param conn Reference to the WebSocket connection.
 * @param msg The message received.
 *
 * Simulates a long-running operation and notifies the client upon completion.
 */
void handle_long_task(crow::websocket::connection& conn,
                      const std::string& msg);

/**
 * @brief Handles a JSON command from a client.
 *
 * @param conn Reference to the WebSocket connection.
 * @param msg The message received (expected to be JSON).
 *
 * Parses the JSON and sends a response containing the received data.
 */
void handle_json(crow::websocket::connection& conn, const std::string& msg);
