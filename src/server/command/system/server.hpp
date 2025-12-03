/*
 * server.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: WebSocket command handlers for server status and health

**************************************************/

#ifndef LITHIUM_SERVER_COMMAND_SERVER_HPP
#define LITHIUM_SERVER_COMMAND_SERVER_HPP

#include <memory>

namespace lithium::app {
class CommandDispatcher;
class EventLoop;
}  // namespace lithium::app

class WebSocketServer;

namespace lithium::server {
class TaskManager;
}

namespace lithium::app {

/**
 * @brief Register server status commands with the command dispatcher
 *
 * Commands registered:
 * - server.health: Get server health status
 * - server.uptime: Get server uptime
 * - server.stats: Get comprehensive server statistics
 * - websocket.stats: Get WebSocket server statistics
 * - websocket.connections: Get active WebSocket connections count
 *
 * @param dispatcher The command dispatcher to register with
 * @param websocket_server The WebSocket server instance
 * @param task_manager The task manager instance
 * @param event_loop The event loop instance
 */
void registerServerCommands(
    std::shared_ptr<CommandDispatcher> dispatcher,
    std::weak_ptr<WebSocketServer> websocket_server,
    std::weak_ptr<lithium::server::TaskManager> task_manager,
    std::weak_ptr<EventLoop> event_loop);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_SERVER_HPP
