/*
 * server.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "server.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

#include "atom/type/json.hpp"
#include "server/command.hpp"
#include "server/models/server.hpp"
#include "server/task_manager.hpp"
#include "server/websocket.hpp"

namespace lithium::app {

namespace {

using namespace lithium::models::server;

// Server start time for uptime calculation
static std::chrono::steady_clock::time_point g_start_time =
    std::chrono::steady_clock::now();

auto getCurrentTimestamp() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

auto getUptimeSeconds() -> int64_t {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - g_start_time)
        .count();
}

}  // namespace

void registerServerCommands(
    std::shared_ptr<CommandDispatcher> dispatcher,
    std::weak_ptr<WebSocketServer> websocket_server,
    std::weak_ptr<lithium::server::TaskManager> task_manager,
    std::weak_ptr<EventLoop> event_loop) {
    if (!dispatcher) {
        spdlog::error("registerServerCommands: dispatcher is null");
        return;
    }

    // server.health - Get server health status
    dispatcher->registerCommand<nlohmann::json>(
        "server.health",
        [websocket_server, task_manager,
         event_loop](const nlohmann::json& /*payload*/) -> nlohmann::json {
            nlohmann::json result;
            result["status"] = "healthy";
            result["timestamp"] = getCurrentTimestamp();
            result["uptimeSeconds"] = getUptimeSeconds();

            // Check WebSocket server
            nlohmann::json ws_status;
            if (auto ws = websocket_server.lock()) {
                ws_status["available"] = true;
                ws_status["running"] = ws->is_running();
                ws_status["activeConnections"] = ws->get_active_connections();
            } else {
                ws_status["available"] = false;
            }
            result["websocket"] = ws_status;

            // Check TaskManager
            nlohmann::json tm_status;
            if (auto tm = task_manager.lock()) {
                tm_status["available"] = true;
                auto stats = tm->getStats();
                tm_status["activeTasks"] = stats["pending"].get<size_t>() +
                                           stats["running"].get<size_t>();
            } else {
                tm_status["available"] = false;
            }
            result["taskManager"] = tm_status;

            // Check EventLoop
            nlohmann::json el_status;
            if (auto el = event_loop.lock()) {
                el_status["available"] = true;
            } else {
                el_status["available"] = false;
            }
            result["eventLoop"] = el_status;

            return {{"status", "success"}, {"data", result}};
        });
    spdlog::info("Registered command: server.health");

    // server.uptime - Get server uptime
    dispatcher->registerCommand<nlohmann::json>(
        "server.uptime",
        [](const nlohmann::json& /*payload*/) -> nlohmann::json {
            auto uptime = getUptimeSeconds();
            UptimeInfo info;
            info.uptimeSeconds = uptime;
            info.uptimeFormatted = UptimeInfo::formatUptime(uptime);
            info.startTime = getCurrentTimestamp();  // Approximate

            return {{"status", "success"}, {"data", info.toJson()}};
        });
    spdlog::info("Registered command: server.uptime");

    // server.stats - Get comprehensive server statistics
    dispatcher->registerCommand<nlohmann::json>(
        "server.stats",
        [websocket_server,
         task_manager](const nlohmann::json& /*payload*/) -> nlohmann::json {
            nlohmann::json result;
            result["timestamp"] = getCurrentTimestamp();
            result["uptimeSeconds"] = getUptimeSeconds();

            // WebSocket stats
            if (auto ws = websocket_server.lock()) {
                result["websocket"] = ws->get_stats();
                result["websocket"]["running"] = ws->is_running();
            }

            // Task manager stats
            if (auto tm = task_manager.lock()) {
                result["tasks"] = tm->getStats();
            }

            return {{"status", "success"}, {"data", result}};
        });
    spdlog::info("Registered command: server.stats");

    // websocket.stats - Get WebSocket server statistics
    dispatcher->registerCommand<nlohmann::json>(
        "websocket.stats",
        [websocket_server](const nlohmann::json& /*payload*/) -> nlohmann::json {
            auto ws = websocket_server.lock();
            if (!ws) {
                return {
                    {"status", "error"},
                    {"error", {{"code", "unavailable"},
                               {"message", "WebSocket server not available"}}}};
            }

            nlohmann::json result = ws->get_stats();
            result["running"] = ws->is_running();
            result["subscribedTopics"] = ws->get_subscribed_topics();

            return {{"status", "success"}, {"data", result}};
        });
    spdlog::info("Registered command: websocket.stats");

    // websocket.connections - Get active WebSocket connections count
    dispatcher->registerCommand<nlohmann::json>(
        "websocket.connections",
        [websocket_server](const nlohmann::json& /*payload*/) -> nlohmann::json {
            auto ws = websocket_server.lock();
            if (!ws) {
                return {
                    {"status", "error"},
                    {"error", {{"code", "unavailable"},
                               {"message", "WebSocket server not available"}}}};
            }

            return {{"status", "success"},
                    {"data",
                     {{"activeConnections", ws->get_active_connections()},
                      {"running", ws->is_running()}}}};
        });
    spdlog::info("Registered command: websocket.connections");

    spdlog::info("Server status commands registered");
}

}  // namespace lithium::app

