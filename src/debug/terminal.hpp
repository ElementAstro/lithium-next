#ifndef LITHIUM_DEBUG_TERMINAL_HPP
#define LITHIUM_DEBUG_TERMINAL_HPP

#include <any>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace lithium::debug {
class CommandChecker;
class SuggestionEngine;
/**
 * @brief Class representing a console terminal for debugging purposes.
 */
class ConsoleTerminal {
public:
    /**
     * @brief Construct a new ConsoleTerminal object.
     */
    ConsoleTerminal();

    /**
     * @brief Destroy the ConsoleTerminal object.
     */
    ~ConsoleTerminal();

    /**
     * @brief Copy constructor (deleted).
     */
    ConsoleTerminal(const ConsoleTerminal&) = delete;

    /**
     * @brief Copy assignment operator (deleted).
     */
    auto operator=(const ConsoleTerminal&) -> ConsoleTerminal& = delete;

    /**
     * @brief Move constructor.
     */
    ConsoleTerminal(ConsoleTerminal&&) noexcept;

    /**
     * @brief Move assignment operator.
     */
    auto operator=(ConsoleTerminal&&) noexcept -> ConsoleTerminal&;

    /**
     * @brief Get the list of registered commands.
     *
     * @return std::vector<std::string> A vector of registered command names.
     */
    [[nodiscard]] auto getRegisteredCommands() const
        -> std::vector<std::string>;

    /**
     * @brief Call a registered command by name with the given arguments.
     *
     * @param name The name of the command to call.
     * @param args A vector of arguments to pass to the command.
     */
    void callCommand(std::string_view name, const std::vector<std::any>& args);

    /**
     * @brief Run the console terminal, processing input and executing commands.
     */
    void run();

    /**
     * @brief Set the command execution timeout.
     *
     * @param timeout The timeout duration in milliseconds.
     */
    void setCommandTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Enable or disable command history.
     *
     * @param enable True to enable command history, false to disable.
     */
    void enableHistory(bool enable);

    /**
     * @brief Enable or disable command suggestions.
     *
     * @param enable True to enable command suggestions, false to disable.
     */
    void enableSuggestions(bool enable);

    /**
     * @brief Enable or disable syntax highlighting.
     *
     * @param enable True to enable syntax highlighting, false to disable.
     */
    void enableSyntaxHighlight(bool enable);

    /**
     * @brief Load the terminal configuration from a file.
     *
     * @param configPath The path to the configuration file.
     */
    void loadConfig(const std::string& configPath);

    /**
     * @brief 设置命令检查器
     */
    void setCommandChecker(std::shared_ptr<CommandChecker> checker);

    /**
     * @brief 设置建议引擎
     */
    void setSuggestionEngine(std::shared_ptr<SuggestionEngine> engine);

    /**
     * @brief 启用或禁用命令检查
     */
    void enableCommandCheck(bool enable);

    /**
     * @brief 获取命令补全建议
     */
    std::vector<std::string> getCommandSuggestions(const std::string& prefix);

private:
    /**
     * @brief Implementation class for ConsoleTerminal.
     *
     * This class is used to hide the implementation details of ConsoleTerminal.
     */
    class ConsoleTerminalImpl;

    std::unique_ptr<ConsoleTerminalImpl>
        impl_;  ///< Pointer to the implementation of ConsoleTerminal.

    bool historyEnabled_{
        true};  ///< Flag indicating whether command history is enabled.
    bool suggestionsEnabled_{
        true};  ///< Flag indicating whether command suggestions are enabled.
    bool syntaxHighlightEnabled_{
        true};  ///< Flag indicating whether syntax highlighting is enabled.
    std::chrono::milliseconds commandTimeout_{
        5000};  ///< Command execution timeout duration.
    bool commandCheckEnabled_{true};
    std::shared_ptr<CommandChecker> commandChecker_;
    std::shared_ptr<SuggestionEngine> suggestionEngine_;
};

/**
 * @brief Global pointer to the console terminal instance.
 *
 * This pointer can be used to access the console terminal from anywhere in the
 * program.
 */
extern ConsoleTerminal* globalConsoleTerminal;

}  // namespace lithium::debug

#endif  // LITHIUM_DEBUG_TERMINAL_HPP
