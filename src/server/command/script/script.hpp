/*
 * script.hpp - Script Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_COMMAND_SCRIPT_HPP
#define LITHIUM_SERVER_COMMAND_SCRIPT_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

/**
 * @brief Registers all script-related WebSocket commands
 *
 * Commands registered:
 * - script.execute         Execute Python script content
 * - script.executeFile     Execute Python script from file
 * - script.executeFunction Execute specific function from module
 * - script.cancel          Cancel running script execution
 * - script.status          Query execution status
 * - script.shell           Execute shell command
 * - script.tool.list       List available Python tools
 * - script.tool.invoke     Invoke Python tool function
 * - script.venv.list       List virtual environments
 * - script.venv.packages   List packages in venv
 *
 * @param dispatcher Shared pointer to command dispatcher
 */
void registerScript(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_SCRIPT_HPP
