#include "crow/websocket.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>  // 引入nlohmann/json库
#include <random>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include "crow.h"
#include "websocket.hpp"

// WebSocket 服务器

WebSocketServer::WebSocketServer(crow::SimpleApp& app, MessageBus& bus,
                    bool enable_token_validation)
        : app_(app),
          bus_(bus),
          enable_token_validation_(enable_token_validation) {}

void WebSocketServer::on_open(crow::websocket::connection& conn) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    clients_.insert(&conn);
    std::cout << "New client connected!" << std::endl;
}

void WebSocketServer::on_close(crow::websocket::connection& conn,
                  const std::string& reason, uint16_t code) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    clients_.erase(&conn);
    std::cout << "Client disconnected!" << std::endl;
}

// 处理 WebSocket 消息
void WebSocketServer::on_message(crow::websocket::connection& conn,
                    const std::string& message, bool is_binary) {
    if (enable_token_validation_) {
        if (!validate_token(message)) {
            std::cerr << "Invalid token!" << std::endl;
            return;
        }
    }

    // 假设命令以"command:"开头
    if (message.substr(0, 8) == "command:") {
        std::string command = message.substr(8);
        bus_.dispatch_command(conn, command, message);
    }
}

// 启动 WebSocket 服务器
void WebSocketServer::start() {
    auto& route = CROW_WEBSOCKET_ROUTE(app_, "/ws")
        .onopen([this](crow::websocket::connection& conn) { 
            on_open(conn); 
        })
        .onclose([this](crow::websocket::connection& conn, const std::string& reason, uint16_t code) { 
            on_close(conn, reason, code); 
        })
        .onmessage([this](crow::websocket::connection& conn, const std::string& message, bool is_binary) {
            on_message(conn, message, is_binary);
        })
        .onerror([this](crow::websocket::connection& conn, const std::string& error_message) {
            on_error(conn, error_message);
        });

    // 设置最大负载大小
    if (max_payload_size_ != UINT64_MAX) {
        route.max_payload(max_payload_size_);
    }

    // 设置子协议
    if (!subprotocols_.empty()) {
        route.subprotocols(subprotocols_);
    }
}

// 广播消息到所有客户端
void WebSocketServer::broadcast(const std::string& msg) {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    for (auto* conn : clients_) {
        conn->send_text(msg);
    }
}

// 向特定客户端发送消息
void WebSocketServer::send_to_client(crow::websocket::connection& conn,
                        const std::string& msg) {
    conn.send_text(msg);
}

void WebSocketServer::set_max_payload(uint64_t size) {
    max_payload_size_ = size;
}

void WebSocketServer::set_subprotocols(const std::vector<std::string>& protocols) {
    subprotocols_ = protocols;
}

void WebSocketServer::on_error(crow::websocket::connection& conn, const std::string& error_message) {
    std::cerr << "WebSocket error: " << error_message << std::endl;
    conn.close("Error occurred");
}

bool WebSocketServer::validate_token(const std::string& token) {
    // 简单验证
    return token == "valid_token";  // 替换为实际的验证逻辑
}

// 身份验证与授权

bool validate_token(const std::string& token) {
    return token == "valid_token";  // 假设令牌校验
}

// 命令处理逻辑

void handle_ping(crow::websocket::connection& conn, const std::string& msg) {
    std::cout << "Handling PING command: " << msg << std::endl;
    conn.send_text("PING response: Command completed.");
}

void handle_echo(crow::websocket::connection& conn, const std::string& msg) {
    std::cout << "ECHO command received: " << msg << std::endl;
    conn.send_text("ECHO response: " + msg);
}

void handle_long_task(crow::websocket::connection& conn,
                      const std::string& msg) {
    std::cout << "Starting long task..." << std::endl;
    // 使用异步方式处理长任务
    std::thread([&conn, msg]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        conn.send_text("Long task completed: " + msg);
    }).detach();
}

// 处理 JSON 格式的命令

void handle_json(crow::websocket::connection& conn, const std::string& msg) {
    std::cout << "Handling JSON command: " << msg << std::endl;

    // 使用 nlohmann/json 解析接收到的 JSON 数据
    try {
        auto json_data = nlohmann::json::parse(msg);
        std::cout << "Received JSON: " << json_data.dump() << std::endl;

        // 返回一些处理结果（例如返回处理的命令回显）
        nlohmann::json response = {{"status", "success"},
                                   {"received", json_data}};
        conn.send_text(response.dump());
    } catch (const std::exception& e) {
        std::cerr << "Error parsing JSON: " << e.what() << std::endl;
        conn.send_text(
            "{\"status\": \"error\", \"message\": \"Invalid JSON data\"}");
    }
}

// 启动 WebSocket 服务和命令派发

int main() {
    crow::SimpleApp app;

    #define CROW_ENFORCE_WS_SPEC  // 强制执行WebSocket规范

    // 是否启用令牌校验
    bool enable_token_validation = true;  // 可以根据需要设置为 false

    // 初始化消息总线
    MessageBus bus;
    bus.register_command("ping", handle_ping);
    bus.register_command("echo", handle_echo);
    bus.register_command("long_task", handle_long_task);
    bus.register_command("json", handle_json);

    // 初始化 WebSocket 服务器
    WebSocketServer ws_server(app, bus, enable_token_validation);

    // 配置WebSocket服务器
    ws_server.set_max_payload(1024 * 1024);  // 设置最大负载为1MB
    ws_server.set_subprotocols({"v1.protocol", "v2.protocol"});  // 设置支持的子协议

    ws_server.start();

    // 启动 HTTP 服务
    app.port(8080).multithreaded().run();

    return 0;
}