/*
 * async_system_command.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Async System Command - Execute system commands asynchronously

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_MANAGER_ASYNC_SYSTEM_COMMAND_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_ASYNC_SYSTEM_COMMAND_HPP

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

namespace lithium::client::indi_manager {

/**
 * @class AsyncSystemCommand
 * @brief A class to execute system commands asynchronously.
 *
 * This class provides functionality to run system commands asynchronously,
 * check their running status, and terminate them if needed.
 */
class AsyncSystemCommand {
public:
    /**
     * @brief Constructs an AsyncSystemCommand with the given command.
     * @param cmd The system command to be executed.
     */
    explicit AsyncSystemCommand(const std::string& cmd);

    /**
     * @brief Destructor for AsyncSystemCommand.
     */
    ~AsyncSystemCommand();

    /**
     * @brief Runs the system command asynchronously.
     */
    void run();

    /**
     * @brief Terminates the running system command.
     */
    void terminate();

    /**
     * @brief Checks if the system command is currently running.
     * @return True if the command is running, false otherwise.
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Sets environment variables for the command execution
     * @param envVars Map of environment variables
     */
    void setEnvironmentVariables(
        const std::unordered_map<std::string, std::string>& envVars);

    /**
     * @brief Checks if the command is available in the system
     * @return True if command is available
     */
    [[nodiscard]] bool isCommandValid() const;

    /**
     * @brief Gets the exit status of the last execution
     * @return Exit status code
     */
    [[nodiscard]] int getLastExitStatus() const;

    /**
     * @brief Gets the output of the last execution
     * @return Command output string
     */
    [[nodiscard]] std::string getLastOutput() const;

private:
    std::string cmd_;
    std::atomic<pid_t> pid_;
    std::atomic<bool> running_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> envVars_;
    std::string lastOutput_;
    int lastExitStatus_;
};

}  // namespace lithium::client::indi_manager

#endif  // LITHIUM_CLIENT_INDI_MANAGER_ASYNC_SYSTEM_COMMAND_HPP
