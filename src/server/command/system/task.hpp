/*
 * task.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-02

Description: WebSocket command handlers for task management

**************************************************/

#ifndef LITHIUM_SERVER_COMMAND_TASK_HPP
#define LITHIUM_SERVER_COMMAND_TASK_HPP

#include <memory>

namespace lithium::app {
class CommandDispatcher;
}

namespace lithium::server {
class TaskManager;
}

namespace lithium::app {

/**
 * @brief Register task management commands with the command dispatcher
 *
 * Commands registered:
 * - task.list: List all tasks with optional filters
 * - task.get: Get details of a specific task
 * - task.cancel: Cancel a task
 * - task.stats: Get task manager statistics
 * - task.cleanup: Clean up old completed tasks
 * - task.progress: Update task progress
 *
 * @param dispatcher The command dispatcher to register with
 * @param task_manager The task manager instance
 */
void registerTaskCommands(std::shared_ptr<CommandDispatcher> dispatcher,
                          std::shared_ptr<lithium::server::TaskManager> task_manager);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_TASK_HPP
