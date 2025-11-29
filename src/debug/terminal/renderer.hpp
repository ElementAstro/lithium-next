/**
 * @file renderer.hpp
 * @brief Console renderer for beautified terminal output
 *
 * This component handles all visual output including colors, styles,
 * formatted messages, tables, progress bars, and themed UI elements.
 */

#ifndef LITHIUM_DEBUG_TERMINAL_RENDERER_HPP
#define LITHIUM_DEBUG_TERMINAL_RENDERER_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace lithium::debug::terminal {

/**
 * @brief Table cell alignment
 */
enum class Alignment { Left, Center, Right };

/**
 * @brief Table column definition
 */
struct TableColumn {
    std::string header;
    int width{0};  // 0 = auto
    Alignment alignment{Alignment::Left};
};

/**
 * @brief Progress bar style
 */
struct ProgressStyle {
    std::string fillChar{"█"};
    std::string emptyChar{"░"};
    std::string leftBracket{"["};
    std::string rightBracket{"]"};
    Color fillColor{Color::BrightGreen};
    Color emptyColor{Color::BrightBlack};
    bool showPercentage{true};
    bool showEta{false};
    int width{40};
};

/**
 * @brief Spinner animation frames
 */
struct SpinnerStyle {
    std::vector<std::string> frames{"⠋", "⠙", "⠹", "⠸", "⠼",
                                    "⠴", "⠦", "⠧", "⠇", "⠏"};
    Color color{Color::BrightCyan};
    int intervalMs{80};
};

/**
 * @brief Console renderer for styled terminal output
 */
class ConsoleRenderer {
public:
    /**
     * @brief Construct renderer with optional theme
     */
    explicit ConsoleRenderer(const Theme& theme = Theme{});

    ~ConsoleRenderer();

    // Non-copyable, movable
    ConsoleRenderer(const ConsoleRenderer&) = delete;
    ConsoleRenderer& operator=(const ConsoleRenderer&) = delete;
    ConsoleRenderer(ConsoleRenderer&&) noexcept;
    ConsoleRenderer& operator=(ConsoleRenderer&&) noexcept;

    // =========================================================================
    // Theme Management
    // =========================================================================

    /**
     * @brief Set the current theme
     */
    void setTheme(const Theme& theme);

    /**
     * @brief Get the current theme
     */
    [[nodiscard]] const Theme& getTheme() const;

    /**
     * @brief Load theme from JSON file
     */
    bool loadTheme(const std::string& path);

    /**
     * @brief Save current theme to JSON file
     */
    bool saveTheme(const std::string& path) const;

    // =========================================================================
    // Basic Output
    // =========================================================================

    /**
     * @brief Print text with color and style
     */
    void print(const std::string& text, Color fg = Color::Default,
               std::optional<Color> bg = std::nullopt,
               Style style = Style::Normal);

    /**
     * @brief Print text followed by newline
     */
    void println(const std::string& text = "", Color fg = Color::Default,
                 std::optional<Color> bg = std::nullopt,
                 Style style = Style::Normal);

    /**
     * @brief Print formatted text (printf-style)
     */
    template <typename... Args>
    void printf(const std::string& format, Args&&... args);

    /**
     * @brief Clear the screen
     */
    void clear();

    /**
     * @brief Clear current line
     */
    void clearLine();

    /**
     * @brief Move cursor to position
     */
    void moveCursor(int x, int y);

    /**
     * @brief Move cursor up N lines
     */
    void moveCursorUp(int lines = 1);

    /**
     * @brief Move cursor down N lines
     */
    void moveCursorDown(int lines = 1);

    /**
     * @brief Save cursor position
     */
    void saveCursor();

    /**
     * @brief Restore cursor position
     */
    void restoreCursor();

    /**
     * @brief Hide cursor
     */
    void hideCursor();

    /**
     * @brief Show cursor
     */
    void showCursor();

    // =========================================================================
    // Styled Messages
    // =========================================================================

    /**
     * @brief Print success message with icon
     */
    void success(const std::string& message);

    /**
     * @brief Print error message with icon
     */
    void error(const std::string& message);

    /**
     * @brief Print warning message with icon
     */
    void warning(const std::string& message);

    /**
     * @brief Print info message with icon
     */
    void info(const std::string& message);

    /**
     * @brief Print debug message with icon
     */
    void debug(const std::string& message);

    // =========================================================================
    // UI Elements
    // =========================================================================

    /**
     * @brief Print a styled header
     */
    void header(const std::string& title, char fillChar = '=');

    /**
     * @brief Print a subheader
     */
    void subheader(const std::string& title);

    /**
     * @brief Print a horizontal rule
     */
    void horizontalRule(char ch = '-', int width = 0);

    /**
     * @brief Print a box around text
     */
    void box(const std::string& content, const std::string& title = "");

    /**
     * @brief Print a multi-line box
     */
    void box(const std::vector<std::string>& lines,
             const std::string& title = "");

    /**
     * @brief Print a bullet list
     */
    void bulletList(const std::vector<std::string>& items, int indent = 0);

    /**
     * @brief Print a numbered list
     */
    void numberedList(const std::vector<std::string>& items, int startNum = 1);

    /**
     * @brief Print key-value pairs
     */
    void keyValue(const std::string& key, const std::string& value,
                  int keyWidth = 20);

    /**
     * @brief Print multiple key-value pairs
     */
    void keyValueList(
        const std::vector<std::pair<std::string, std::string>>& pairs,
        int keyWidth = 20);

    // =========================================================================
    // Tables
    // =========================================================================

    /**
     * @brief Print a formatted table
     */
    void table(const std::vector<TableColumn>& columns,
               const std::vector<std::vector<std::string>>& rows);

    /**
     * @brief Print a simple table with auto-detected columns
     */
    void simpleTable(const std::vector<std::string>& headers,
                     const std::vector<std::vector<std::string>>& rows);

    // =========================================================================
    // Progress Indicators
    // =========================================================================

    /**
     * @brief Print a progress bar
     */
    void progressBar(float progress, const std::string& label = "",
                     const ProgressStyle& style = ProgressStyle{});

    /**
     * @brief Update progress bar in place
     */
    void updateProgress(float progress, const std::string& label = "");

    /**
     * @brief Start a spinner animation
     */
    void startSpinner(const std::string& message,
                      const SpinnerStyle& style = SpinnerStyle{});

    /**
     * @brief Stop the spinner animation
     */
    void stopSpinner(bool success = true, const std::string& message = "");

    // =========================================================================
    // Terminal Prompt
    // =========================================================================

    /**
     * @brief Print the command prompt
     */
    void prompt(const std::string& prefix = "");

    /**
     * @brief Print the welcome header
     */
    void welcomeHeader(const std::string& title, const std::string& version,
                       const std::string& description = "");

    /**
     * @brief Print command suggestions
     */
    void suggestions(const std::vector<std::string>& items,
                     const std::string& prefix = "Did you mean:");

    /**
     * @brief Print command help
     */
    void commandHelp(
        const std::string& command, const std::string& description,
        const std::vector<std::pair<std::string, std::string>>& options = {});

    // =========================================================================
    // Syntax Highlighting
    // =========================================================================

    /**
     * @brief Print command with syntax highlighting
     */
    void highlightedCommand(const std::string& command,
                            const std::vector<std::string>& keywords = {});

    /**
     * @brief Print error with position indicator
     */
    void errorWithPosition(const std::string& input, size_t position,
                           const std::string& message);

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get terminal size
     */
    [[nodiscard]] TerminalSize getTerminalSize() const;

    /**
     * @brief Check if terminal supports colors
     */
    [[nodiscard]] bool supportsColors() const;

    /**
     * @brief Check if terminal supports Unicode
     */
    [[nodiscard]] bool supportsUnicode() const;

    /**
     * @brief Enable/disable colors
     */
    void enableColors(bool enable);

    /**
     * @brief Enable/disable Unicode
     */
    void enableUnicode(bool enable);

    /**
     * @brief Flush output buffer
     */
    void flush();

    /**
     * @brief Get ANSI escape code for color
     */
    [[nodiscard]] std::string colorCode(Color fg,
                                        std::optional<Color> bg = std::nullopt,
                                        Style style = Style::Normal) const;

    /**
     * @brief Get ANSI reset code
     */
    [[nodiscard]] std::string resetCode() const;

    /**
     * @brief Strip ANSI codes from string
     */
    [[nodiscard]] static std::string stripAnsi(const std::string& text);

    /**
     * @brief Calculate visible length (excluding ANSI codes)
     */
    [[nodiscard]] static size_t visibleLength(const std::string& text);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Template implementation
template <typename... Args>
void ConsoleRenderer::printf(const std::string& format, Args&&... args) {
    // Simple printf-style formatting
    char buffer[4096];
    std::snprintf(buffer, sizeof(buffer), format.c_str(),
                  std::forward<Args>(args)...);
    print(buffer);
}

}  // namespace lithium::debug::terminal

#endif  // LITHIUM_DEBUG_TERMINAL_RENDERER_HPP
