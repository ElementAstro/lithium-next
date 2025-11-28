#include "websocket.hpp"

#include <chrono>
#include <thread>

#include "atom/type/json.hpp"
#include "atom/utils/to_string.hpp"
#include "command/device.hpp"
#include "middleware/auth.hpp"
#include <spdlog/spdlog.h>

using namespace std::chrono_literals;

WebSocketServer::WebSocketServer(
    crow::SimpleApp& app, std::shared_ptr<atom::async::MessageBus> message_bus,
    std::shared_ptr<lithium::app::CommandDispatcher> command_dispatcher,
    const Config& config)
    : app_(app),
      message_bus_(message_bus),
      command_dispatcher_(command_dispatcher),
      config_(config) {
    setup_message_bus_handlers();
    setup_command_handlers();
}

void WebSocketServer::on_open(crow::websocket::connection& conn) {
    std::unique_lock lock(conn_mutex_);
    clients_.insert(&conn);
    update_activity_time(&conn);
    spdlog::info("New client connected: {}", conn.get_remote_ip());
}

void WebSocketServer::on_close(crow::websocket::connection& conn,
                               const std::string& reason, uint16_t code) {
    std::unique_lock lock(conn_mutex_);
    clients_.erase(&conn);
    last_activity_times_.erase(&conn);
    client_tokens_.erase(&conn);
    
    for (auto& [topic, subscribers] : topic_subscribers_) {
        subscribers.erase(&conn);
    }
    
    spdlog::info("Client disconnected: {}, reason: {}, code: {}",
          conn.get_remote_ip(), reason, code);
}

void WebSocketServer::on_message(crow::websocket::connection& conn,
                                 const std::string& message, bool is_binary) {
    update_activity_time(&conn);
    spdlog::debug("Received message from client {}: {}", conn.get_remote_ip(), message);
    
    try {
        auto json = nlohmann::json::parse(message);

        if (!validate_message_format(json)) {
            handle_connection_error(conn, "Invalid message format");
            return;
        }

        std::string type = json["type"];
        if (type == "command" && json.contains("command")) {
            const nlohmann::json& payload =
                json.contains("payload") ? json["payload"] : json["params"];
            std::string requestId = json.value("requestId", std::string());
            handle_command(conn, json["command"], payload, requestId);
        } else if (type == "message" && json.contains("topic")) {
            forward_to_message_bus(json["topic"], json["payload"].dump());
        } else if (type == "auth" && json.contains("token")) {
            authenticate_client(conn, json["token"]);
        } else if (type == "subscribe" && json.contains("topic")) {
            subscribe_client_to_topic(&conn, json["topic"]);
        } else if (type == "unsubscribe" && json.contains("topic")) {
            unsubscribe_client_from_topic(&conn, json["topic"]);
        }
    } catch (const std::exception& e) {
        spdlog::error("Message parsing error from client {}: {}",
              conn.get_remote_ip(), e.what());
        handle_connection_error(conn, std::string("Message parsing error: ") + e.what());
    }
}

void WebSocketServer::handle_command(crow::websocket::connection& conn,
                                     const std::string& command,
                                     const nlohmann::json& payload,
                                     const std::string& request_id) {
    spdlog::info(
        "Handling command from client {}: command: {}, payload: {}",
        conn.get_remote_ip(), command, payload.dump());
          
    auto callback = [this, conn_ptr = &conn, request_id](
        const std::string& cmd_id,
        const lithium::app::CommandDispatcher::ResultType& result) {
        
        nlohmann::json response;
        response["type"] = "response";
        response["command"] = cmd_id;
        if (!request_id.empty()) {
            response["requestId"] = request_id;
        }
        response["timestamp"] =
            std::chrono::system_clock::now().time_since_epoch().count();

        if (std::holds_alternative<std::any>(result)) {
            try {
                auto payload_json = std::any_cast<nlohmann::json>(
                    std::get<std::any>(result));

                std::string status = payload_json.value("status", "success");
                bool success = (status == "success");
                response["success"] = success;

                if (success) {
                    response["data"] =
                        payload_json.value("data", nlohmann::json::object());
                    if (payload_json.contains("message")) {
                        response["message"] = payload_json["message"];
                    }
                } else {
                    response["error"] =
                        payload_json.value("error", nlohmann::json::object());
                }
            } catch (const std::bad_any_cast&) {
                response["success"] = false;
                response["error"] =
                    nlohmann::json{{"code", "result_cast_error"},
                                   {"message",
                                    "Result type not supported for JSON response"}};
            }
        } else {
            try {
                std::rethrow_exception(std::get<std::exception_ptr>(result));
            } catch (const std::exception& e) {
                response["success"] = false;
                response["error"] =
                    nlohmann::json{{"code", "internal_error"},
                                   {"message", e.what()}};
            }
        }

        spdlog::info("Sending command result to client {}: {}",
              conn_ptr->get_remote_ip(), response.dump());
        send_to_client(*conn_ptr, response.dump());
    };

    command_dispatcher_->dispatch(command, payload, 0, std::nullopt, callback);
}

void WebSocketServer::forward_to_message_bus(const std::string& topic,
                                             const std::string& message) {
    spdlog::debug("Forwarding message to message bus: topic: {}, message: {}",
          topic, message);
    message_bus_->publish(topic, message);
}

void WebSocketServer::start() {
    if (running_.exchange(true)) {
        return;
    }

    thread_pool_ = std::make_unique<atom::async::ThreadPool<>>(config_.thread_pool_size);
    server_thread_ = std::thread(&WebSocketServer::run_server, this);

    spdlog::info("WebSocket server started in background thread");
}

void WebSocketServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    thread_pool_.reset();
    spdlog::info("WebSocket server stopped");
}

void WebSocketServer::run_server() {
    auto& route = CROW_WEBSOCKET_ROUTE(app_, "/api/v1/ws")
        .onopen([this](crow::websocket::connection& conn) { on_open(conn); })
        .onclose([this](crow::websocket::connection& conn,
                        const std::string& reason, uint16_t code) { 
            on_close(conn, reason, code); 
        })
        .onmessage([this](crow::websocket::connection& conn,
                          const std::string& message, bool is_binary) {
            on_message(conn, message, is_binary);
        })
        .onerror([this](crow::websocket::connection& conn,
                        const std::string& error_message) {
            handle_connection_error(conn, error_message);
        });

    std::thread ping_thread([this]() {
        while (running_) {
            handle_ping_pong();
            std::this_thread::sleep_for(std::chrono::seconds(config_.ping_interval));
        }
    });

    std::thread timeout_thread([this]() {
        while (running_) {
            check_timeouts();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    ping_thread.detach();
    timeout_thread.detach();
}

void WebSocketServer::broadcast(const std::string& msg) {
    if (!running_) return;

    if (rate_limiter_ && !rate_limiter_->allow_request()) {
        spdlog::warn("Broadcast rate limit exceeded");
        return;
    }

    std::shared_lock lock(conn_mutex_);
    if (clients_.empty()) return;

    std::vector<std::future<void>> futures;
    futures.reserve(clients_.size());
    
    for (auto* conn : clients_) {
        futures.emplace_back(thread_pool_->enqueue([conn, msg]() {
            try {
                conn->send_text(msg);
            } catch (const std::exception& e) {
                spdlog::error("Failed to send message to client: {}", e.what());
            }
        }));
    }

    for (auto& f : futures) {
        try {
            f.wait();
        } catch (const std::exception& e) {
            spdlog::error("Error waiting for broadcast completion: {}", e.what());
        }
    }

    total_messages_++;
}

auto WebSocketServer::get_stats() const -> nlohmann::json {
    std::shared_lock lock(conn_mutex_);
    return {
        {"total_messages", total_messages_.load()},
        {"error_count", error_count_.load()},
        {"active_connections", clients_.size()}
    };
}

void WebSocketServer::send_to_client(crow::websocket::connection& conn,
                                     const std::string& msg) {
    update_activity_time(&conn);
    spdlog::debug("Sending message to client {}: {}", conn.get_remote_ip(), msg);
    
    try {
        conn.send_text(msg);
    } catch (const std::exception& e) {
        spdlog::error("Failed to send message to client {}: {}", 
                     conn.get_remote_ip(), e.what());
        handle_connection_error(conn, "Send failed");
    }
}

void WebSocketServer::set_max_payload(uint64_t size) {
    max_payload_size_ = size;
    spdlog::info("Set max payload size to: {}", size);
}

void WebSocketServer::set_subprotocols(const std::vector<std::string>& protocols) {
    subprotocols_ = protocols;
    spdlog::info("Set subprotocols to: {}", atom::utils::toString(protocols));
}

bool validate_token(const std::string& token) {
    bool is_valid = lithium::server::middleware::ApiKeyAuth::isValidApiKey(token);
    spdlog::debug("Token validation result for token {}: {}", token, is_valid);
    return is_valid;
}

void handle_ping(crow::websocket::connection& conn, const std::string& msg) {
    spdlog::debug("Handling PING command: {}", msg);
    conn.send_text("PING response: Command completed.");
}

void handle_echo(crow::websocket::connection& conn, const std::string& msg) {
    spdlog::debug("ECHO command received: {}", msg);
    conn.send_text("ECHO response: " + msg);
}

void handle_long_task(crow::websocket::connection& conn, const std::string& msg) {
    spdlog::info("Starting long task with message: {}", msg);
    
    std::thread([&conn, msg]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        spdlog::info("Long task completed with message: {}", msg);
        conn.send_text("Long task completed: " + msg);
    }).detach();
}

void handle_json(crow::websocket::connection& conn, const std::string& msg) {
    spdlog::debug("Handling JSON command: {}", msg);

    try {
        auto json_data = nlohmann::json::parse(msg);
        spdlog::debug("Received JSON: {}", json_data.dump());

        nlohmann::json response = {
            {"status", "success"},
            {"received", json_data}
        };
        conn.send_text(response.dump());
    } catch (const std::exception& e) {
        spdlog::error("Error parsing JSON: {}", e.what());
        conn.send_text(R"({"status": "error", "message": "Invalid JSON data"})");
    }
}

void WebSocketServer::setup_message_bus_handlers() {
    bus_subscriptions_["broadcast"] = message_bus_->subscribe<std::string>(
        "broadcast", [this](const std::string& message) { broadcast(message); });
    spdlog::info("Subscribed to broadcast messages");

    bus_subscriptions_["command_result"] = message_bus_->subscribe<nlohmann::json>(
        "command.result", [this](const nlohmann::json& result) { 
            broadcast(result.dump()); 
        });
    spdlog::info("Subscribed to command result messages");
}

void WebSocketServer::setup_command_handlers() {
    // Simple ping command: logs payload; result echo is provided by dispatcher
    command_dispatcher_->registerCommand<nlohmann::json>(
        "ping", [this](const nlohmann::json& payload) {
            spdlog::info("Ping command received with payload: {}",
                         payload.dump());
        });
    spdlog::info("Registered command handler for 'ping'");

    // Subscribe command: subscribes server to a message-bus topic
    command_dispatcher_->registerCommand<nlohmann::json>(
        "subscribe", [this](const nlohmann::json& payload) {
            if (payload.contains("topic")) {
                subscribe_to_topic(payload["topic"]);
                spdlog::info("Subscribe command processed for topic: {}",
                             payload["topic"].get<std::string>());
                return;  // void
            }
            throw std::runtime_error("Invalid subscribe command payload");
        });
    spdlog::info("Registered command handler for 'subscribe'");

    // Device commands (camera, mount, focuser, filterwheel, etc.) are registered in command module
    lithium::app::registerCamera(command_dispatcher_);
    lithium::app::registerMount(command_dispatcher_);
    lithium::app::registerFocuser(command_dispatcher_);
    lithium::app::registerFilterWheel(command_dispatcher_);
    lithium::app::registerDome(command_dispatcher_);
}

void WebSocketServer::subscribe_to_topic(const std::string& topic) {
    bus_subscriptions_[topic] = message_bus_->subscribe<nlohmann::json>(
        topic, [this, topic](const nlohmann::json& message) {
            broadcast_to_topic(topic, message);
        });
    spdlog::info("Subscribed to topic: {}", topic);
}

void WebSocketServer::unsubscribe_from_topic(const std::string& topic) {
    if (auto it = bus_subscriptions_.find(topic); it != bus_subscriptions_.end()) {
        message_bus_->unsubscribe<nlohmann::json>(it->second);
        bus_subscriptions_.erase(it);
        spdlog::info("Unsubscribed from topic: {}", topic);
    }
}

void WebSocketServer::subscribe_client_to_topic(crow::websocket::connection* conn, 
                                               const std::string& topic) {
    std::unique_lock lock(conn_mutex_);
    topic_subscribers_[topic].insert(conn);
    spdlog::info("Client {} subscribed to topic: {}", conn->get_remote_ip(), topic);
}

void WebSocketServer::unsubscribe_client_from_topic(crow::websocket::connection* conn,
                                                   const std::string& topic) {
    std::unique_lock lock(conn_mutex_);
    if (auto it = topic_subscribers_.find(topic); it != topic_subscribers_.end()) {
        it->second.erase(conn);
        if (it->second.empty()) {
            topic_subscribers_.erase(it);
        }
    }
    spdlog::info("Client {} unsubscribed from topic: {}", conn->get_remote_ip(), topic);
}

template <typename T>
void WebSocketServer::broadcast_to_topic(const std::string& topic, const T& data) {
    std::shared_lock lock(conn_mutex_);
    if (auto it = topic_subscribers_.find(topic); it != topic_subscribers_.end()) {
        nlohmann::json message = {
            {"type", "topic_message"}, 
            {"topic", topic}, 
            {"payload", data}
        };

        std::string msg = message.dump();
        spdlog::debug("Broadcasting message to topic {}: {}", topic, msg);
        
        for (auto* conn : it->second) {
            try {
                conn->send_text(msg);
            } catch (const std::exception& e) {
                spdlog::error("Failed to send topic message to client: {}", e.what());
            }
        }
    }
}

bool WebSocketServer::authenticate_client(crow::websocket::connection& conn,
                                          const std::string& token) {
    bool authenticated = validate_token(token);
    if (authenticated) {
        std::unique_lock lock(conn_mutex_);
        client_tokens_[&conn] = token;
        spdlog::info("Client {} authenticated with token: {}",
              conn.get_remote_ip(), token);
    } else {
        spdlog::warn("Client {} failed authentication with token: {}",
              conn.get_remote_ip(), token);
        handle_connection_error(conn, "Authentication failed");
    }
    return authenticated;
}

void WebSocketServer::disconnect_client(crow::websocket::connection& conn) {
    std::unique_lock lock(conn_mutex_);
    if (clients_.find(&conn) != clients_.end()) {
        client_tokens_.erase(&conn);
        last_activity_times_.erase(&conn);
        
        for (auto& [topic, subscribers] : topic_subscribers_) {
            subscribers.erase(&conn);
        }
        
        conn.close("Server initiated disconnect");
        clients_.erase(&conn);
        spdlog::info("Client {} disconnected by server", conn.get_remote_ip());
    }
}

size_t WebSocketServer::get_active_connections() const {
    std::shared_lock lock(conn_mutex_);
    return clients_.size();
}

std::vector<std::string> WebSocketServer::get_subscribed_topics() const {
    std::shared_lock lock(conn_mutex_);
    std::vector<std::string> topics;
    topics.reserve(topic_subscribers_.size());
    
    for (const auto& [topic, _] : topic_subscribers_) {
        topics.push_back(topic);
    }
    return topics;
}

void WebSocketServer::set_rate_limit(size_t messages_per_second) {
    rate_limiter_ = std::make_unique<RateLimiter>(messages_per_second, 5ms);
    spdlog::info("Rate limit set to {} messages per second", messages_per_second);
}

void WebSocketServer::set_compression(bool enable, int level) {
    compression_enabled_ = enable;
    compression_level_ = level;
    spdlog::info("Compression {} with level {}", 
                enable ? "enabled" : "disabled", level);
}

void WebSocketServer::check_timeouts() {
    auto now = std::chrono::steady_clock::now();
    std::unique_lock lock(conn_mutex_);

    auto it = clients_.begin();
    while (it != clients_.end()) {
        auto* conn = *it;
        auto last_activity = get_last_activity_time(conn);
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                            now - last_activity).count();

        if (duration > static_cast<long>(config_.connection_timeout)) {
            spdlog::warn("Client {} timed out after {} seconds",
                  conn->get_remote_ip(), duration);
            last_activity_times_.erase(conn);
            try {
                conn->close("Connection timeout");
            } catch (const std::exception& e) {
                spdlog::error("Error closing timed out connection: {}", e.what());
            }
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

void WebSocketServer::handle_ping_pong() {
    std::shared_lock lock(conn_mutex_);
    
    for (auto* conn : clients_) {
        try {
            conn->send_ping("ping");
            spdlog::debug("Sent ping to client {}", conn->get_remote_ip());
        } catch (const std::exception& e) {
            spdlog::error("Error sending ping to client {}: {}",
                  conn->get_remote_ip(), e.what());
        }
    }
}

bool WebSocketServer::is_running() const { 
    return running_.load(); 
}

template <typename T>
void WebSocketServer::publish_to_topic(const std::string& topic, const T& data) {
    nlohmann::json message = {
        {"type", "topic_message"}, 
        {"topic", topic}, 
        {"payload", data}
    };

    message_bus_->publish(topic, message);
    spdlog::debug("Published message to topic {}: {}", topic, message.dump());
}

void WebSocketServer::broadcast_batch(const std::vector<std::string>& messages) {
    if (!running_ || messages.empty()) return;

    std::shared_lock lock(conn_mutex_);
    
    for (const auto& msg : messages) {
        if (rate_limiter_ && !rate_limiter_->allow_request()) {
            spdlog::warn("Batch broadcast rate limit exceeded");
            return;
        }

        for (auto* conn : clients_) {
            thread_pool_->enqueue([conn, msg]() {
                try {
                    conn->send_text(msg);
                } catch (const std::exception& e) {
                    spdlog::error("Error during batch broadcast: {}", e.what());
                }
            });
        }
    }

    total_messages_ += messages.size();
}

bool WebSocketServer::validate_message_format(const nlohmann::json& message) {
    if (!message.contains("type") || !message["type"].is_string()) {
        spdlog::error("Invalid message format: missing or invalid 'type': {}",
                      message.dump());
        return false;
    }

    std::string type = message["type"];

    if (type == "command") {
        bool hasCommand = message.contains("command") &&
                          message["command"].is_string();
        bool hasParams =
            (message.contains("payload") && !message["payload"].is_null()) ||
            (message.contains("params") && !message["params"].is_null());

        if (!hasCommand || !hasParams) {
            spdlog::error(
                "Invalid command message format (expect 'command' and 'payload' or 'params'): {}",
                message.dump());
            return false;
        }
    }

    return true;
}

void WebSocketServer::handle_connection_error(crow::websocket::connection& conn,
                                              const std::string& error) {
    spdlog::error("Connection error for client {}: {}", conn.get_remote_ip(), error);
    error_count_++;

    nlohmann::json error_message = {
        {"type", "error"},
        {"message", error},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };

    try {
        conn.send_text(error_message.dump());
    } catch (const std::exception& e) {
        spdlog::error("Failed to send error message: {}", e.what());
        try {
            conn.close("Error occurred");
        } catch (const std::exception& close_error) {
            spdlog::error("Failed to close connection: {}", close_error.what());
        }
    }
}

void WebSocketServer::update_activity_time(crow::websocket::connection* conn) {
    std::unique_lock lock(conn_mutex_);
    last_activity_times_[conn] = std::chrono::steady_clock::now();
}

std::chrono::steady_clock::time_point WebSocketServer::get_last_activity_time(
    crow::websocket::connection* conn) const {
    auto it = last_activity_times_.find(conn);
    if (it != last_activity_times_.end()) {
        return it->second;
    }
    return std::chrono::steady_clock::now() - std::chrono::hours(24);
}
