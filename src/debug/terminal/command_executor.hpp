/**
 * @file command_executor.hpp
 * @brief Command execution with timeout and async support
 *
 * This component handles command parsing, execution with timeout control,
 * and async execution support.
 */

#ifndef LITHIUM_DEBUG_TERMINAL_COMMAND_EXECUTOR_HPP
#define LITHIUM_DEBUG_TERMINAL_COMMAND_EXECUTOR_HPP

#include <any>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace lithium::debug::terminal {

/**
 * @brief Parsed command structure
 */
struct ParsedCommand {
    std::string name;
    std::vector<std::string> args;
    std::vector<std::any> typedArgs;
    std::string rawInput;
    bool isPiped{false};
    bool isBackground{false};
    std::string redirectOutput;
    std::string redirectInput;
};

/**
 * @brief Command definition
 */
struct CommandDef {
    std::string name;
    std::string description;
    std::string usage;
    std::vector<std::string> aliases;
    std::function<CommandResult(const std::vector<std::any>&)> handler;
    bool requiresArgs{false};
    int minArgs{0};
    int maxArgs{-1};  // -1 = unlimited
};

/**
 * @brief Executor configuration
 */
struct ExecutorConfig {
    std::chrono::milliseconds defaultTimeout{5000};
    bool allowBackground{true};
    bool allowPipes{false};
    bool allowRedirection{false};
    bool echoCommands{false};
    size_t maxOutputSize{1024 * 1024};  // 1MB
};

/**
 * @brief Command executor
 */
class CommandExecutor {
public:
    /**
     * @brief Construct executor with configuration
     */
    explicit CommandExecutor(const ExecutorConfig& config = ExecutorConfig{});

    ~CommandExecutor();

    // Non-copyable, movable
    CommandExecutor(const CommandExecutor&) = delete;
    CommandExecutor& operator=(const CommandExecutor&) = delete;
    CommandExecutor(CommandExecutor&&) noexcept;
    CommandExecutor& operator=(CommandExecutor&&) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set configuration
     */
    void setConfig(const ExecutorConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const ExecutorConfig& getConfig() const;

    /**
     * @brief Set default timeout
     */
    void setTimeout(std::chrono::milliseconds timeout);

    // =========================================================================
    // Command Registration
    // =========================================================================

    /**
     * @brief Register a command
     */
    void registerCommand(const CommandDef& command);

    /**
     * @brief Register a simple command
     */
    void registerCommand(
        const std::string& name, const std::string& description,
        std::function<CommandResult(const std::vector<std::any>&)> handler);

    /**
     * @brief Register command alias
     */
    void registerAlias(const std::string& alias, const std::string& command);

    /**
     * @brief Unregister a command
     */
    bool unregisterCommand(const std::string& name);

    /**
     * @brief Check if command exists
     */
    [[nodiscard]] bool hasCommand(const std::string& name) const;

    /**
     * @brief Get command definition
     */
    [[nodiscard]] std::optional<CommandDef> getCommand(
        const std::string& name) const;

    /**
     * @brief Get all registered commands
     */
    [[nodiscard]] std::vector<std::string> getCommands() const;

    /**
     * @brief Get command descriptions
     */
    [[nodiscard]] std::vector<std::pair<std::string, std::string>>
    getCommandDescriptions() const;

    // =========================================================================
    // Parsing
    // =========================================================================

    /**
     * @brief Parse command string
     */
    [[nodiscard]] ParsedCommand parse(const std::string& input) const;

    /**
     * @brief Parse argument to typed value
     */
    [[nodiscard]] std::any parseArgument(const std::string& arg) const;

    /**
     * @brief Validate parsed command
     */
    [[nodiscard]] std::optional<std::string> validate(
        const ParsedCommand& cmd) const;

    // =========================================================================
    // Execution
    // =========================================================================

    /**
     * @brief Execute command string
     */
    CommandResult execute(const std::string& input);

    /**
     * @brief Execute parsed command
     */
    CommandResult execute(const ParsedCommand& cmd);

    /**
     * @brief Execute with custom timeout
     */
    CommandResult execute(const std::string& input,
                          std::chrono::milliseconds timeout);

    /**
     * @brief Execute asynchronously
     */
    std::future<CommandResult> executeAsync(const std::string& input);

    /**
     * @brief Execute in background (fire and forget)
     */
    void executeBackground(const std::string& input);

    /**
     * @brief Cancel running command
     */
    bool cancel();

    /**
     * @brief Check if command is running
     */
    [[nodiscard]] bool isRunning() const;

    // =========================================================================
    // Built-in Commands
    // =========================================================================

    /**
     * @brief Register built-in commands (help, exit, etc.)
     */
    void registerBuiltins();

    /**
     * @brief Set exit callback
     */
    void setExitCallback(std::function<void()> callback);

    /**
     * @brief Set help callback
     */
    void setHelpCallback(std::function<void(const std::string&)> callback);

    // =========================================================================
    // Hooks
    // =========================================================================

    /**
     * @brief Set pre-execution hook
     */
    void setPreExecuteHook(std::function<bool(const ParsedCommand&)> hook);

    /**
     * @brief Set post-execution hook
     */
    void setPostExecuteHook(
        std::function<void(const ParsedCommand&, const CommandResult&)> hook);

    /**
     * @brief Set output handler
     */
    void setOutputHandler(std::function<void(const std::string&)> handler);

    /**
     * @brief Set error handler
     */
    void setErrorHandler(std::function<void(const std::string&)> handler);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::debug::terminal

#endif  // LITHIUM_DEBUG_TERMINAL_COMMAND_EXECUTOR_HPP
