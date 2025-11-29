/**
 * @file input_controller.hpp
 * @brief Cross-platform input handling for terminal
 *
 * This component provides unified input handling across different platforms
 * and terminal types (readline, ncurses, Windows console).
 */

#ifndef LITHIUM_DEBUG_TERMINAL_INPUT_CONTROLLER_HPP
#define LITHIUM_DEBUG_TERMINAL_INPUT_CONTROLLER_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace lithium::debug::terminal {

/**
 * @brief Input mode configuration
 */
enum class InputMode {
    Line,       ///< Line-by-line input (default)
    Character,  ///< Character-by-character input
    Raw         ///< Raw input without processing
};

/**
 * @brief Input controller configuration
 */
struct InputConfig {
    InputMode mode{InputMode::Line};
    bool enableHistory{true};
    bool enableCompletion{true};
    bool enableEditing{true};
    bool echoInput{true};
    int maxLineLength{4096};
    std::string prompt{">"};
};

/**
 * @brief Completion result
 */
struct CompletionResult {
    std::vector<std::string> matches;
    std::string commonPrefix;
    bool hasMultiple{false};
};

/**
 * @brief Cross-platform input controller
 */
class InputController {
public:
    /**
     * @brief Callback for tab completion
     */
    using CompletionHandler =
        std::function<CompletionResult(const std::string&, size_t)>;

    /**
     * @brief Callback for input validation
     */
    using ValidationHandler = std::function<bool(const std::string&)>;

    /**
     * @brief Callback for key events
     */
    using KeyHandler = std::function<bool(const InputEvent&)>;

    /**
     * @brief Construct input controller with configuration
     */
    explicit InputController(const InputConfig& config = InputConfig{});

    ~InputController();

    // Non-copyable, movable
    InputController(const InputController&) = delete;
    InputController& operator=(const InputController&) = delete;
    InputController(InputController&&) noexcept;
    InputController& operator=(InputController&&) noexcept;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set input configuration
     */
    void setConfig(const InputConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] const InputConfig& getConfig() const;

    /**
     * @brief Set the prompt string
     */
    void setPrompt(const std::string& prompt);

    /**
     * @brief Set input mode
     */
    void setMode(InputMode mode);

    // =========================================================================
    // Input Reading
    // =========================================================================

    /**
     * @brief Read a line of input (blocking)
     * @return The input line, or nullopt if EOF/interrupted
     */
    [[nodiscard]] std::optional<std::string> readLine();

    /**
     * @brief Read a line with custom prompt
     */
    [[nodiscard]] std::optional<std::string> readLine(
        const std::string& prompt);

    /**
     * @brief Read a single character (blocking)
     */
    [[nodiscard]] std::optional<char> readChar();

    /**
     * @brief Read a key event (blocking)
     */
    [[nodiscard]] std::optional<InputEvent> readKey();

    /**
     * @brief Check if input is available (non-blocking)
     */
    [[nodiscard]] bool hasInput() const;

    /**
     * @brief Read password (no echo)
     */
    [[nodiscard]] std::optional<std::string> readPassword(
        const std::string& prompt = "Password: ");

    // =========================================================================
    // Line Editing
    // =========================================================================

    /**
     * @brief Get current input buffer
     */
    [[nodiscard]] std::string getBuffer() const;

    /**
     * @brief Set input buffer content
     */
    void setBuffer(const std::string& content);

    /**
     * @brief Clear input buffer
     */
    void clearBuffer();

    /**
     * @brief Get cursor position in buffer
     */
    [[nodiscard]] size_t getCursorPosition() const;

    /**
     * @brief Set cursor position
     */
    void setCursorPosition(size_t pos);

    /**
     * @brief Insert text at cursor
     */
    void insertText(const std::string& text);

    /**
     * @brief Delete character at cursor
     */
    void deleteChar();

    /**
     * @brief Delete character before cursor (backspace)
     */
    void backspace();

    // =========================================================================
    // History Management
    // =========================================================================

    /**
     * @brief Add entry to history
     */
    void addToHistory(const std::string& entry);

    /**
     * @brief Get history entries
     */
    [[nodiscard]] std::vector<std::string> getHistory() const;

    /**
     * @brief Clear history
     */
    void clearHistory();

    /**
     * @brief Set maximum history size
     */
    void setMaxHistorySize(size_t size);

    /**
     * @brief Load history from file
     */
    bool loadHistory(const std::string& path);

    /**
     * @brief Save history to file
     */
    bool saveHistory(const std::string& path) const;

    /**
     * @brief Navigate to previous history entry
     */
    void historyPrevious();

    /**
     * @brief Navigate to next history entry
     */
    void historyNext();

    /**
     * @brief Search history
     */
    [[nodiscard]] std::vector<std::string> searchHistory(
        const std::string& pattern) const;

    // =========================================================================
    // Completion
    // =========================================================================

    /**
     * @brief Set completion handler
     */
    void setCompletionHandler(CompletionHandler handler);

    /**
     * @brief Trigger completion at current position
     */
    void triggerCompletion();

    /**
     * @brief Get completion suggestions for current input
     */
    [[nodiscard]] CompletionResult getCompletions() const;

    // =========================================================================
    // Event Handlers
    // =========================================================================

    /**
     * @brief Set key event handler
     */
    void setKeyHandler(KeyHandler handler);

    /**
     * @brief Set validation handler
     */
    void setValidationHandler(ValidationHandler handler);

    // =========================================================================
    // Terminal Control
    // =========================================================================

    /**
     * @brief Initialize terminal for input
     */
    void initialize();

    /**
     * @brief Restore terminal to original state
     */
    void restore();

    /**
     * @brief Check if terminal is in raw mode
     */
    [[nodiscard]] bool isRawMode() const;

    /**
     * @brief Enable/disable raw mode
     */
    void setRawMode(bool enable);

    /**
     * @brief Refresh the input line display
     */
    void refresh();

    /**
     * @brief Ring the terminal bell
     */
    void bell();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::debug::terminal

#endif  // LITHIUM_DEBUG_TERMINAL_INPUT_CONTROLLER_HPP
