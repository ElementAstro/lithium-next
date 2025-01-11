// async_system_command.hpp
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

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
     *
     * Ensures that any running command is terminated before the object is
     * destroyed.
     */
    ~AsyncSystemCommand();

    /**
     * @brief Runs the system command asynchronously.
     *
     * If a command is already running, this function will log a warning and
     * return.
     */
    void run();

    /**
     * @brief Terminates the running system command.
     *
     * If no command is running, this function will log an informational message
     * and return.
     */
    void terminate();

    /**
     * @brief Checks if the system command is currently running.
     * @return True if the command is running, false otherwise.
     */
    bool isRunning() const;

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
    bool isCommandValid() const;

    /**
     * @brief Gets the exit status of the last execution
     * @return Exit status code
     */
    int getLastExitStatus() const;

    /**
     * @brief Gets the output of the last execution
     * @return Command output string
     */
    std::string getLastOutput() const;

private:
    std::string cmd_;         ///< The system command to be executed.
    std::atomic<pid_t> pid_;  ///< The process ID of the running command.
    std::atomic<bool>
        running_;  ///< Indicates whether the command is currently running.
    mutable std::mutex
        mutex_;  ///< Mutex to protect access to shared resources.
    std::unordered_map<std::string, std::string> env_vars_;
    std::string last_output_;
    int last_exit_status_;
};