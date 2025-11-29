/**
 * @file tui_manager.hpp
 * @brief TUI (Text User Interface) manager for terminal
 *
 * This component provides a full-featured TUI interface with panels,
 * windows, and interactive elements using ncurses when available.
 */

#ifndef LITHIUM_DEBUG_TERMINAL_TUI_MANAGER_HPP
#define LITHIUM_DEBUG_TERMINAL_TUI_MANAGER_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "types.hpp"

namespace lithium::debug::terminal {

// Forward declarations
class ConsoleRenderer;
class InputController;
class HistoryManager;

/**
 * @brief Panel information
 */
struct Panel {
    PanelType type;
    std::string title;
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    bool visible{true};
    bool focused{false};
    bool scrollable{true};
    int scrollOffset{0};
    std::vector<std::string> content;
};

/**
 * @brief Status bar item
 */
struct StatusItem {
    std::string label;
    std::string value;
    Color color{Color::Default};
};

/**
 * @brief Menu item
 */
struct MenuItem {
    std::string label;
    std::string shortcut;
    std::function<void()> action;
    bool enabled{true};
    bool separator{false};
};

/**
 * @brief TUI event types
 */
enum class TuiEvent {
    None,
    Resize,
    KeyPress,
    MouseClick,
    FocusChange,
    Scroll,
    Refresh
};

/**
 * @brief TUI manager for terminal interface
 */
class TuiManager {
public:
    /**
     * @brief Construct TUI manager
     */
    TuiManager();

    /**
     * @brief Construct with shared components
     */
    TuiManager(std::shared_ptr<ConsoleRenderer> renderer,
               std::shared_ptr<InputController> input,
               std::shared_ptr<HistoryManager> history);

    ~TuiManager();

    // Non-copyable, movable
    TuiManager(const TuiManager&) = delete;
    TuiManager& operator=(const TuiManager&) = delete;
    TuiManager(TuiManager&&) noexcept;
    TuiManager& operator=(TuiManager&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Check if TUI is available (ncurses installed)
     */
    [[nodiscard]] static bool isAvailable();

    /**
     * @brief Initialize TUI mode
     */
    bool initialize();

    /**
     * @brief Shutdown TUI mode
     */
    void shutdown();

    /**
     * @brief Check if TUI is active
     */
    [[nodiscard]] bool isActive() const;

    // =========================================================================
    // Layout Configuration
    // =========================================================================

    /**
     * @brief Set layout configuration
     */
    void setLayout(const LayoutConfig& config);

    /**
     * @brief Get current layout configuration
     */
    [[nodiscard]] const LayoutConfig& getLayout() const;

    /**
     * @brief Set theme
     */
    void setTheme(const Theme& theme);

    /**
     * @brief Apply layout changes
     */
    void applyLayout();

    // =========================================================================
    // Panel Management
    // =========================================================================

    /**
     * @brief Create a panel
     */
    Panel& createPanel(PanelType type, const std::string& title = "");

    /**
     * @brief Get panel by type
     */
    [[nodiscard]] Panel* getPanel(PanelType type);

    /**
     * @brief Show/hide panel
     */
    void showPanel(PanelType type, bool show = true);

    /**
     * @brief Toggle panel visibility
     */
    void togglePanel(PanelType type);

    /**
     * @brief Focus panel
     */
    void focusPanel(PanelType type);

    /**
     * @brief Get focused panel
     */
    [[nodiscard]] PanelType getFocusedPanel() const;

    /**
     * @brief Cycle focus to next panel
     */
    void focusNext();

    /**
     * @brief Cycle focus to previous panel
     */
    void focusPrevious();

    // =========================================================================
    // Content Management
    // =========================================================================

    /**
     * @brief Set panel content
     */
    void setPanelContent(PanelType type, const std::vector<std::string>& lines);

    /**
     * @brief Append to panel content
     */
    void appendToPanel(PanelType type, const std::string& line);

    /**
     * @brief Clear panel content
     */
    void clearPanel(PanelType type);

    /**
     * @brief Scroll panel
     */
    void scrollPanel(PanelType type, int delta);

    /**
     * @brief Scroll to top
     */
    void scrollToTop(PanelType type);

    /**
     * @brief Scroll to bottom
     */
    void scrollToBottom(PanelType type);

    // =========================================================================
    // Status Bar
    // =========================================================================

    /**
     * @brief Set status bar items
     */
    void setStatusItems(const std::vector<StatusItem>& items);

    /**
     * @brief Update single status item
     */
    void updateStatus(const std::string& label, const std::string& value);

    /**
     * @brief Set status message (temporary)
     */
    void setStatusMessage(const std::string& message,
                          Color color = Color::Default, int durationMs = 3000);

    // =========================================================================
    // Command Input
    // =========================================================================

    /**
     * @brief Set command prompt
     */
    void setPrompt(const std::string& prompt);

    /**
     * @brief Get current input
     */
    [[nodiscard]] std::string getInput() const;

    /**
     * @brief Set input content
     */
    void setInput(const std::string& content);

    /**
     * @brief Clear input
     */
    void clearInput();

    /**
     * @brief Show suggestions popup
     */
    void showSuggestions(const std::vector<std::string>& suggestions);

    /**
     * @brief Hide suggestions popup
     */
    void hideSuggestions();

    /**
     * @brief Select suggestion
     */
    void selectSuggestion(int index);

    // =========================================================================
    // Output
    // =========================================================================

    /**
     * @brief Print to output panel
     */
    void print(const std::string& text);

    /**
     * @brief Print line to output panel
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

    // =========================================================================
    // Event Handling
    // =========================================================================

    /**
     * @brief Process events (non-blocking)
     */
    TuiEvent processEvents();

    /**
     * @brief Wait for event (blocking)
     */
    TuiEvent waitForEvent(int timeoutMs = -1);

    /**
     * @brief Handle resize event
     */
    void handleResize();

    /**
     * @brief Set key handler
     */
    void setKeyHandler(std::function<bool(const InputEvent&)> handler);

    // =========================================================================
    // Rendering
    // =========================================================================

    /**
     * @brief Refresh display
     */
    void refresh();

    /**
     * @brief Force full redraw
     */
    void redraw();

    /**
     * @brief Clear screen
     */
    void clear();

    // =========================================================================
    // Dialogs
    // =========================================================================

    /**
     * @brief Show message box
     */
    void messageBox(const std::string& title, const std::string& message);

    /**
     * @brief Show confirmation dialog
     */
    bool confirm(const std::string& title, const std::string& message);

    /**
     * @brief Show input dialog
     */
    std::string inputDialog(const std::string& title, const std::string& prompt,
                            const std::string& defaultValue = "");

    /**
     * @brief Show menu
     */
    int showMenu(const std::string& title, const std::vector<MenuItem>& items);

    // =========================================================================
    // Help System
    // =========================================================================

    /**
     * @brief Show help panel
     */
    void showHelp();

    /**
     * @brief Hide help panel
     */
    void hideHelp();

    /**
     * @brief Set help content
     */
    void setHelpContent(
        const std::vector<std::pair<std::string, std::string>>& shortcuts);

    // =========================================================================
    // Fallback Mode
    // =========================================================================

    /**
     * @brief Check if running in fallback (non-TUI) mode
     */
    [[nodiscard]] bool isFallbackMode() const;

    /**
     * @brief Force fallback mode
     */
    void setFallbackMode(bool fallback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::debug::terminal

#endif  // LITHIUM_DEBUG_TERMINAL_TUI_MANAGER_HPP
