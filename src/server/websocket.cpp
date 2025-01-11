#include "websocket.hpp"

#include <chrono>
#include <thread>

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

WebSocketServer::WebSocketServer(
    crow::SimpleApp& app, std::shared_ptr<atom::async::MessageBus> message_bus,
    std::shared_ptr<lithium::app::CommandDispatcher> command_dispatcher,
    const Config& config)
    : app_(app),
      message_bus_(message_bus),
      command_dispatcher_(command_dispatcher),
      config_(config) {
    LOG_F(INFO, "WebSocketServer initialized with config: {}", config);
    setup_message_bus_handlers();
    setup_command_handlers();
}

void WebSocketServer::on_open(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    clients_.insert(&conn);
    LOG_F(INFO, "New client connected: {}", &conn);
}

void WebSocketServer::on_close(crow::websocket::connection& conn,
                               const std::string& reason, uint16_t code) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    clients_.erase(&conn);
    LOG_F(INFO, "Client disconnected: {}, reason: {}, code: {}", &conn, reason,
          code);
}

// 处理 WebSocket 消息
void WebSocketServer::on_message(crow::websocket::connection& conn,
                                 const std::string& message, bool is_binary) {
    LOG_F(INFO, "Received message from client {}: {}", &conn, message);
    try {
        auto json = nlohmann::json::parse(message);

        if (json.contains("type") && json.contains("payload")) {
            std::string type = json["type"];

            if (type == "command") {
                // 处理命令
                if (json.contains("command")) {
                    handle_command(conn, json["command"],
                                   json["payload"].dump());
                }
            } else if (type == "message") {
                // 转发到消息总线
                if (json.contains("topic")) {
                    forward_to_message_bus(json["topic"],
                                           json["payload"].dump());
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Message parsing error from client {}: {}", &conn,
              e.what());
        on_error(conn, std::string("Message parsing error: ") + e.what());
    }
}

void WebSocketServer::handle_command(crow::websocket::connection& conn,
                                     const std::string& command,
                                     const std::string& payload) {
    LOG_F(INFO, "Handling command from client {}: command: {}, payload: {}",
          &conn, command, payload);
    auto callback =
        [this, &conn](
            const std::string& cmd_id,
            const lithium::app::CommandDispatcher::ResultType& result) {
            nlohmann::json response = {{"type", "command_result"},
                                       {"command", cmd_id},
                                       {"status", "completed"}};

            if (std::holds_alternative<std::any>(result)) {
                try {
                    response["result"] = std::any_cast<nlohmann::json>(
                        std::get<std::any>(result));
                } catch (const std::bad_any_cast&) {
                    response["result"] =
                        std::any_cast<std::string>(std::get<std::any>(result));
                }
            } else {
                try {
                    std::rethrow_exception(
                        std::get<std::exception_ptr>(result));
                } catch (const std::exception& e) {
                    response["status"] = "error";
                    response["error"] = e.what();
                }
            }

            LOG_F(INFO, "Sending command result to client {}: {}", &conn,
                  response.dump());
            send_to_client(conn, response.dump());
        };

    command_dispatcher_->dispatch(command, payload, 0, std::nullopt, callback);
}

void WebSocketServer::forward_to_message_bus(const std::string& topic,
                                             const std::string& message) {
    LOG_F(INFO, "Forwarding message to message bus: topic: {}, message: {}",
          topic, message);
    message_bus_->publish(topic, message);
}

// 启动 WebSocket 服务器
void WebSocketServer::start() {
    auto& route =
        CROW_WEBSOCKET_ROUTE(app_, "/ws")
            .onopen(
                [this](crow::websocket::connection& conn) { on_open(conn); })
            .onclose([this](crow::websocket::connection& conn,
                            const std::string& reason,
                            uint16_t code) { on_close(conn, reason, code); })
            .onmessage([this](crow::websocket::connection& conn,
                              const std::string& message, bool is_binary) {
                on_message(conn, message, is_binary);
            })
            .onerror([this](crow::websocket::connection& conn,
                            const std::string& error_message) {
                on_error(conn, error_message);
            });

    // 设置最大负载大小
    if (max_payload_size_ != UINT64_MAX) {
        route.max_payload(max_payload_size_);
        LOG_F(INFO, "Set max payload size: {}", max_payload_size_);
    }

    // 设置子协议
    if (!subprotocols_.empty()) {
        route.subprotocols(subprotocols_);
        LOG_F(INFO, "Set subprotocols: {}", subprotocols_);
    }

    // 订阅MessageBus消息用于广播
    message_bus_->subscribe<std::string>(
        "broadcast",
        [this](const std::string& message) { broadcast(message); });
    LOG_F(INFO, "WebSocket server started");
}

// 广播消息到所有客户端
void WebSocketServer::broadcast(const std::string& msg) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    LOG_F(INFO, "Broadcasting message to all clients: {}", msg);
    for (auto* conn : clients_) {
        conn->send_text(msg);
    }
}

// 向特定客户端发送消息
void WebSocketServer::send_to_client(crow::websocket::connection& conn,
                                     const std::string& msg) {
    LOG_F(INFO, "Sending message to client {}: {}", &conn, msg);
    conn.send_text(msg);
}

void WebSocketServer::set_max_payload(uint64_t size) {
    max_payload_size_ = size;
    LOG_F(INFO, "Set max payload size to: {}", size);
}

void WebSocketServer::set_subprotocols(
    const std::vector<std::string>& protocols) {
    subprotocols_ = protocols;
    LOG_F(INFO, "Set subprotocols to: {}", protocols);
}

void WebSocketServer::on_error(crow::websocket::connection& conn,
                               const std::string& error_message) {
    LOG_F(ERROR, "WebSocket error from client {}: {}", &conn, error_message);
    conn.close("Error occurred");
}

// 身份验证与授权

bool validate_token(const std::string& token) {
    bool is_valid = token == "valid_token";  // 假设令牌校验
    LOG_F(INFO, "Token validation result for token {}: {}", token, is_valid);
    return is_valid;
}

// 命令处理逻辑

void handle_ping(crow::websocket::connection& conn, const std::string& msg) {
    LOG_F(INFO, "Handling PING command: {}", msg);
    conn.send_text("PING response: Command completed.");
}

void handle_echo(crow::websocket::connection& conn, const std::string& msg) {
    LOG_F(INFO, "ECHO command received: {}", msg);
    conn.send_text("ECHO response: " + msg);
}

void handle_long_task(crow::websocket::connection& conn,
                      const std::string& msg) {
    LOG_F(INFO, "Starting long task with message: {}", msg);
    // 使用异步方式处理长任务
    std::thread([&conn, msg]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        LOG_F(INFO, "Long task completed with message: {}", msg);
        conn.send_text("Long task completed: " + msg);
    }).detach();
}

// 处理 JSON 格式的命令

void handle_json(crow::websocket::connection& conn, const std::string& msg) {
    LOG_F(INFO, "Handling JSON command: {}", msg);

    // 使用 nlohmann/json 解析接收到的 JSON 数据
    try {
        auto json_data = nlohmann::json::parse(msg);
        LOG_F(INFO, "Received JSON: {}", json_data.dump());

        // 返回一些处理结果（例如返回处理的命令回显）
        nlohmann::json response = {{"status", "success"},
                                   {"received", json_data}};
        conn.send_text(response.dump());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error parsing JSON: {}", e.what());
        conn.send_text(
            "{\"status\": \"error\", \"message\": \"Invalid JSON data\"}");
    }
}

void WebSocketServer::setup_message_bus_handlers() {
    // 订阅通用广播消息
    bus_subscriptions_["broadcast"] = message_bus_->subscribe<std::string>(
        "broadcast",
        [this](const std::string& message) { broadcast(message); });
    LOG_F(INFO, "Subscribed to broadcast messages");

    // 订阅命令执行结果
    bus_subscriptions_["command_result"] =
        message_bus_->subscribe<nlohmann::json>(
            "command.result",
            [this](const nlohmann::json& result) { broadcast(result.dump()); });
    LOG_F(INFO, "Subscribed to command result messages");
}

void WebSocketServer::setup_command_handlers() {
    // 注册基础命令处理器
    command_dispatcher_->registerCommand<std::string>(
        "ping", [this](const std::string& payload) { return "pong"; });
    LOG_F(INFO, "Registered command handler for 'ping'");

    command_dispatcher_->registerCommand<nlohmann::json>(
        "subscribe", [this](const nlohmann::json& payload) {
            if (payload.contains("topic")) {
                subscribe_to_topic(payload["topic"]);
                return nlohmann::json{{"status", "subscribed"}};
            }
            throw std::runtime_error("Invalid subscribe command payload");
        });
    LOG_F(INFO, "Registered command handler for 'subscribe'");
}

void WebSocketServer::subscribe_to_topic(const std::string& topic) {
    bus_subscriptions_[topic] = message_bus_->subscribe<nlohmann::json>(
        topic, [this, topic](const nlohmann::json& message) {
            broadcast_to_topic(topic, message);
        });
    LOG_F(INFO, "Subscribed to topic: {}", topic);
}

void WebSocketServer::unsubscribe_from_topic(const std::string& topic) {
    if (auto it = bus_subscriptions_.find(topic);
        it != bus_subscriptions_.end()) {
        message_bus_->unsubscribe<nlohmann::json>(it->second);
        bus_subscriptions_.erase(it);
        LOG_F(INFO, "Unsubscribed from topic: {}", topic);
    }
}

template <typename T>
void WebSocketServer::broadcast_to_topic(const std::string& topic,
                                         const T& data) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    if (auto it = topic_subscribers_.find(topic);
        it != topic_subscribers_.end()) {
        nlohmann::json message = {
            {"type", "topic_message"}, {"topic", topic}, {"payload", data}};

        std::string msg = message.dump();
        LOG_F(INFO, "Broadcasting message to topic {}: {}", topic, msg);
        for (auto* conn : it->second) {
            conn->send_text(msg);
        }
    }
}

bool WebSocketServer::authenticate_client(crow::websocket::connection& conn,
                                          const std::string& token) {
    bool authenticated = validate_token(token);
    if (authenticated) {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        client_tokens_[&conn] = token;
        LOG_F(INFO, "Client {} authenticated with token: {}", &conn, token);
    } else {
        LOG_F(WARNING, "Client {} failed authentication with token: {}", &conn,
              token);
    }
    return authenticated;
}