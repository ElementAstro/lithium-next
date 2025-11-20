#ifndef LITHIUM_DEBUG_TERMINAL_HPP
#define LITHIUM_DEBUG_TERMINAL_HPP

#include <any>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lithium::debug {
class CommandChecker;
struct CommandCheckerErrorProxy {
    using Error = struct {
        std::string message;
        size_t line;
        size_t column;
        int severity;
    };
};
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
     * @brief Set command checker for syntax validation
     */
    void setCommandChecker(std::shared_ptr<CommandChecker> checker);

    /**
     * @brief Set suggestion engine for command completion
     */
    void setSuggestionEngine(std::shared_ptr<SuggestionEngine> engine);

    /**
     * @brief Enable or disable command checking
     */
    void enableCommandCheck(bool enable);

    /**
     * @brief Get command completion suggestions
     */
    std::vector<std::string> getCommandSuggestions(const std::string& prefix);

    // --- Unified Debugging Integration ---
    /**
     * @brief Load the full debug configuration (checker + suggestion) from a JSON file.
     */
    void loadDebugConfig(const std::string& configPath);

    /**
     * @brief Save the full debug configuration (checker + suggestion) to a JSON file.
     */
    void saveDebugConfig(const std::string& configPath) const;

    /**
     * @brief Export the current debug state (rules, suggestion config, history, stats) as JSON.
     */
    std::string exportDebugStateJson() const;

    /**
     * @brief Import the debug state from a JSON string.
     */
    void importDebugStateJson(const std::string& jsonStr);

    /**
     * @brief Add a command check rule at runtime.
     */
    void addCommandCheckRule(const std::string& name, std::function<std::optional<CommandCheckerErrorProxy::Error>(const std::string&, size_t)> check);

    /**
     * @brief Remove a command check rule by name at runtime.
     */
    bool removeCommandCheckRule(const std::string& name);

    /**
     * @brief Add a suggestion filter at runtime.
     */
    void addSuggestionFilter(std::function<bool(const std::string&)> filter);

    /**
     * @brief Remove all suggestion filters at runtime.
     */
    void clearSuggestionFilters();

    /**
     * @brief Update the suggestion dataset interactively.
     */
    void updateSuggestionDataset(const std::vector<std::string>& newItems);

    /**
     * @brief Update the command checker dangerous commands interactively.
     */
    void updateDangerousCommands(const std::vector<std::string>& commands);

    /**
     * @brief Print a unified debug report (errors + suggestions + stats).
     */
    void printDebugReport(const std::string& input, bool useColor = true) const;

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
    bool commandCheckEnabled_{
        true};  ///< Flag indicating whether command checking is enabled.
    std::shared_ptr<CommandChecker>
        commandChecker_;  ///< Command checker for syntax validation
    std::shared_ptr<SuggestionEngine>
        suggestionEngine_;  ///< Suggestion engine for command completion
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
