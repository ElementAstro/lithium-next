/*
 * process_utils.hpp
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_VENV_PROCESS_UTILS_HPP
#define LITHIUM_SCRIPT_VENV_PROCESS_UTILS_HPP

#include <chrono>
#include <string>

namespace lithium::venv {

/**
 * @brief Result of a command execution
 */
struct CommandResult {
    int exitCode{-1};
    std::string output;
    std::string errorOutput;
};

/**
 * @brief Execute a command and capture output
 * @param command Command to execute
 * @param timeout Maximum execution time
 * @return CommandResult with exit code and output
 */
CommandResult executeCommand(const std::string& command,
                             std::chrono::seconds timeout = std::chrono::seconds{300});

}  // namespace lithium::venv

#endif
