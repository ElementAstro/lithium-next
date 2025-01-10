#pragma once

#include "crow/websocket.h"
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "crow.h"

class MessageBus {
public:
    using Message = std::string;
    using Handler = std::function<void(crow::websocket::connection&, const Message&)>;

    void register_command(const std::string& command, Handler handler);
    void dispatch_command(crow::websocket::connection& conn, const std::string& command, const Message& msg);

private:
    std::unordered_map<std::string, Handler> handlers_;
    std::mutex mutex_;
};

class WebSocketServer {
public:
    WebSocketServer(crow::SimpleApp& app, MessageBus& bus, bool enable_token_validation);
    
    void on_open(crow::websocket::connection& conn);
    void on_close(crow::websocket::connection& conn, const std::string& reason, uint16_t code);
    void on_message(crow::websocket::connection& conn, const std::string& message, bool is_binary);
    void on_error(crow::websocket::connection& conn, const std::string& error_message);
    
    void start();
    void broadcast(const std::string& msg);
    void send_to_client(crow::websocket::connection& conn, const std::string& msg);
    
    // 新增配置方法
    void set_max_payload(uint64_t size);
    void set_subprotocols(const std::vector<std::string>& protocols);

private:
    bool validate_token(const std::string& token);

    crow::SimpleApp& app_;
    MessageBus& bus_;
    bool enable_token_validation_;
    std::unordered_set<crow::websocket::connection*> clients_;
    std::mutex conn_mutex_;
    
    uint64_t max_payload_size_ = UINT64_MAX;
    std::vector<std::string> subprotocols_;
};

// 命令处理函数声明
void handle_ping(crow::websocket::connection& conn, const std::string& msg);
void handle_echo(crow::websocket::connection& conn, const std::string& msg);
void handle_long_task(crow::websocket::connection& conn, const std::string& msg);
void handle_json(crow::websocket::connection& conn, const std::string& msg);
