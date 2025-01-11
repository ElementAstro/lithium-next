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
#include "atom/type/json_fwd.hpp"
#include "command.hpp"
#include "crow.h"
#include "crow/websocket.h"

class WebSocketServer {
public:
    struct Config {
        uint64_t max_payload_size = UINT64_MAX;
        std::vector<std::string> subprotocols;
        size_t max_retry_attempts = 3;
        std::chrono::milliseconds retry_delay{1000};
        bool enable_compression = false;
        size_t max_connections = 1000;
    };

    WebSocketServer(
        crow::SimpleApp& app,
        std::shared_ptr<atom::async::MessageBus> message_bus,
        std::shared_ptr<lithium::app::CommandDispatcher> command_dispatcher,
        const Config& config);

    void start();
    void broadcast(const std::string& msg);
    void send_to_client(crow::websocket::connection& conn,
                        const std::string& msg);
    void set_max_payload(uint64_t size);
    void set_subprotocols(const std::vector<std::string>& protocols);

    // 新增方法
    void subscribe_to_topic(const std::string& topic);
    void unsubscribe_from_topic(const std::string& topic);

    template <typename T>
    void publish_to_topic(const std::string& topic, const T& data);

    void register_message_handler(
        const std::string& message_type,
        std::function<void(crow::websocket::connection&, const nlohmann::json&)>
            handler);

    bool authenticate_client(crow::websocket::connection& conn,
                             const std::string& token);

    void disconnect_client(crow::websocket::connection& conn);

    size_t get_active_connections() const;

    std::vector<std::string> get_subscribed_topics() const;

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

    // 新增私有成员
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

    void setup_message_bus_handlers();
    void setup_command_handlers();
    void handle_client_message(crow::websocket::connection& conn,
                               const nlohmann::json& message);

    template <typename T>
    void broadcast_to_topic(const std::string& topic, const T& data);

    bool validate_message_format(const nlohmann::json& message);
    void handle_connection_error(crow::websocket::connection& conn,
                                 const std::string& error);
};

// 命令处理函数声明
void handle_ping(crow::websocket::connection& conn, const std::string& msg);
void handle_echo(crow::websocket::connection& conn, const std::string& msg);
void handle_long_task(crow::websocket::connection& conn,
                      const std::string& msg);
void handle_json(crow::websocket::connection& conn, const std::string& msg);
