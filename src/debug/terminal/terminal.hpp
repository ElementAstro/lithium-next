/**
 * @file terminal.hpp
 * @brief Main terminal facade integrating all components
 *
 * This is the main entry point for the debug terminal, providing a unified
 * interface that combines renderer, input, history, TUI, and command execution.
 */

#ifndef LITHIUM_DEBUG_TERMINAL_TERMINAL_HPP
#define LITHIUM_DEBUG_TERMINAL_TERMINAL_HPP

#include <memory>
#include <string>
#include <vector>

#include "command_executor.hpp"
#include "history_manager.hpp"
#include "input_controller.hpp"
#include "renderer.hpp"
#include "tui_manager.hpp"
#include "types.hpp"

namespace lithium::debug::terminal {

/**
 * @brief Terminal configuration
 */
struct TerminalConfig {
    // Display settings
    Theme theme;
    bool enableTui{true};
    bool enableColors{true};
    bool enableUnicode{true};

    // Input settings
    bool enableHistory{true};
    bool enableCompletion{true};
    bool enableSuggestions{true};

    // Execution settings
    std::chrono::milliseconds commandTimeout{5000};
    bool enableCommandCheck{true};

    // Persistence
    std::string historyFile;
    std::string configFile;

    // UI settings
    LayoutConfig layout;
};

/**
 * @brief Terminal mode
 */
enum class TerminalMode {
    Interactive,  ///< Full interactive mode
    Batch,        ///< Batch/script mode
    Tui,          ///< Full TUI mode
    Simple        ///< Simple line mode (no TUI)
};

/**
 * @brief Main debug terminal class
 */
class Terminal {
public:
    /**
     * @brief Construct terminal with configuration
     */
    explicit Terminal(const TerminalConfig& config = TerminalConfig{});

    ~Terminal();

    // Non-copyable, movable
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&&) noexcept;
    Terminal& operator=(Terminal&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the terminal
     */
    bool initialize();

    /**
     * @brief Shutdown the terminal
     */
    void shutdown();

    /**
     * @brief Check if terminal is initialized
     */
    [[nodiscard]] bool isInitialized() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set configuration
     */
    void setConfig(const TerminalConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const TerminalConfig& getConfig() const;

    /**
     * @brief Set theme
     */
    void setTheme(const Theme& theme);

    /**
     * @brief Set terminal mode
     */
    void setMode(TerminalMode mode);

    /**
     * @brief Get current mode
     */
    [[nodiscard]] TerminalMode getMode() const;

    /**
     * @brief Load configuration from file
     */
    bool loadConfig(const std::string& path);

    /**
     * @brief Save configuration to file
     */
    bool saveConfig(const std::string& path) const;

    // =========================================================================
    // Main Loop
    // =========================================================================

    /**
     * @brief Run the terminal main loop
     */
    void run();

    /**
     * @brief Stop the terminal
     */
    void stop();

    /**
     * @brief Check if terminal is running
     */
    [[nodiscard]] bool isRunning() const;

    /**
     * @brief Process single input (for batch mode)
     */
    CommandResult processInput(const std::string& input);

    /**
     * @brief Execute a script file
     */
    bool executeScript(const std::string& path);

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
     * @brief Unregister a command
     */
    bool unregisterCommand(const std::string& name);

    /**
     * @brief Get registered commands
     */
    [[nodiscard]] std::vector<std::string> getCommands() const;

    // =========================================================================
    // Output
    // =========================================================================

    /**
     * @brief Print text
     */
    void print(const std::string& text);

    /**
     * @brief Print line
     */
    void println(const std::string& text = "");

    /**
     * @brief Print styled text
     */
    void printStyled(const std::string& text, Color fg,
                     Style style = Style::Normal);

    /**
     * @brief Print success message
     */
    void success(const std::string& message);

    /**
     * @brief Print error message
     */
    void error(const std::string& message);

    /**
     * @brief Print warning message
     */
    void warning(const std::string& message);

    /**
     * @brief Print info message
     */
    void info(const std::string& message);

    /**
     * @brief Clear screen
     */
    void clear();

    // =========================================================================
    // Component Access
    // =========================================================================

    /**
     * @brief Get renderer
     */
    [[nodiscard]] ConsoleRenderer& getRenderer();

    /**
     * @brief Get input controller
     */
    [[nodiscard]] InputController& getInput();

    /**
     * @brief Get history manager
     */
    [[nodiscard]] HistoryManager& getHistory();

    /**
     * @brief Get command executor
     */
    [[nodiscard]] CommandExecutor& getExecutor();

    /**
     * @brief Get TUI manager
     */
    [[nodiscard]] TuiManager& getTui();

    // =========================================================================
    // Callbacks
    // =========================================================================

    /**
     * @brief Set prompt callback
     */
    void setPromptCallback(std::function<std::string()> callback);

    /**
     * @brief Set completion callback
     */
    void setCompletionCallback(CompletionCallback callback);

    /**
     * @brief Set pre-command callback
     */
    void setPreCommandCallback(
        std::function<bool(const std::string&)> callback);

    /**
     * @brief Set post-command callback
     */
    void setPostCommandCallback(
        std::function<void(const std::string&, const CommandResult&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * @brief Global terminal instance
 */
extern Terminal* globalTerminal;

}  // namespace lithium::debug::terminal

#endif  // LITHIUM_DEBUG_TERMINAL_TERMINAL_HPP
