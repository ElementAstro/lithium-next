/**
 * @file types.hpp
 * @brief Common types and definitions for terminal components
 *
 * This file contains shared type definitions, enums, and structures
 * used across all terminal components.
 */

#ifndef LITHIUM_DEBUG_TERMINAL_TYPES_HPP
#define LITHIUM_DEBUG_TERMINAL_TYPES_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace lithium::debug::terminal {

/**
 * @brief Terminal color codes for styled output
 */
enum class Color {
    Default = 0,
    Black = 30,
    Red = 31,
    Green = 32,
    Yellow = 33,
    Blue = 34,
    Magenta = 35,
    Cyan = 36,
    White = 37,
    BrightBlack = 90,
    BrightRed = 91,
    BrightGreen = 92,
    BrightYellow = 93,
    BrightBlue = 94,
    BrightMagenta = 95,
    BrightCyan = 96,
    BrightWhite = 97
};

/**
 * @brief Text style attributes
 */
enum class Style {
    Normal = 0,
    Bold = 1,
    Dim = 2,
    Italic = 3,
    Underline = 4,
    Blink = 5,
    Reverse = 7,
    Hidden = 8,
    Strikethrough = 9
};

/**
 * @brief Theme configuration for terminal appearance
 */
struct Theme {
    std::string name{"default"};

    // Prompt colors
    Color promptColor{Color::BrightCyan};
    Color promptSymbolColor{Color::BrightGreen};

    // Output colors
    Color successColor{Color::BrightGreen};
    Color errorColor{Color::BrightRed};
    Color warningColor{Color::BrightYellow};
    Color infoColor{Color::BrightBlue};
    Color debugColor{Color::BrightMagenta};

    // UI element colors
    Color headerColor{Color::BrightBlue};
    Color borderColor{Color::Blue};
    Color highlightColor{Color::BrightCyan};
    Color suggestionColor{Color::Cyan};
    Color historyColor{Color::BrightBlack};

    // Styles
    Style headerStyle{Style::Bold};
    Style errorStyle{Style::Bold};
    Style promptStyle{Style::Bold};

    // UI characters
    std::string promptSymbol{"❯"};
    std::string successSymbol{"✓"};
    std::string errorSymbol{"✗"};
    std::string warningSymbol{"⚠"};
    std::string infoSymbol{"ℹ"};
    std::string arrowSymbol{"→"};
    std::string bulletSymbol{"•"};

    // Border characters (box drawing)
    std::string borderTopLeft{"╭"};
    std::string borderTopRight{"╮"};
    std::string borderBottomLeft{"╰"};
    std::string borderBottomRight{"╯"};
    std::string borderHorizontal{"─"};
    std::string borderVertical{"│"};

    // Feature flags
    bool useUnicode{true};
    bool useColors{true};
    bool useBoldHeaders{true};

    /**
     * @brief Create a minimal ASCII-only theme
     */
    static Theme ascii() {
        Theme t;
        t.name = "ascii";
        t.promptSymbol = ">";
        t.successSymbol = "[OK]";
        t.errorSymbol = "[ERR]";
        t.warningSymbol = "[WARN]";
        t.infoSymbol = "[INFO]";
        t.arrowSymbol = "->";
        t.bulletSymbol = "*";
        t.borderTopLeft = "+";
        t.borderTopRight = "+";
        t.borderBottomLeft = "+";
        t.borderBottomRight = "+";
        t.borderHorizontal = "-";
        t.borderVertical = "|";
        t.useUnicode = false;
        return t;
    }

    /**
     * @brief Create a dark theme with muted colors
     */
    static Theme dark() {
        Theme t;
        t.name = "dark";
        t.promptColor = Color::BrightBlue;
        t.headerColor = Color::Magenta;
        t.borderColor = Color::BrightBlack;
        return t;
    }

    /**
     * @brief Create a light theme
     */
    static Theme light() {
        Theme t;
        t.name = "light";
        t.promptColor = Color::Blue;
        t.headerColor = Color::Blue;
        t.borderColor = Color::Black;
        t.highlightColor = Color::Cyan;
        return t;
    }
};

/**
 * @brief Key codes for special keys
 */
enum class Key {
    Unknown = 0,
    Enter = 10,
    Tab = 9,
    Backspace = 127,
    Delete = 330,
    Escape = 27,
    Up = 259,
    Down = 258,
    Left = 260,
    Right = 261,
    Home = 262,
    End = 360,
    PageUp = 339,
    PageDown = 338,
    Insert = 331,
    F1 = 265,
    F2 = 266,
    F3 = 267,
    F4 = 268,
    F5 = 269,
    F6 = 270,
    F7 = 271,
    F8 = 272,
    F9 = 273,
    F10 = 274,
    F11 = 275,
    F12 = 276,
    CtrlA = 1,
    CtrlB = 2,
    CtrlC = 3,
    CtrlD = 4,
    CtrlE = 5,
    CtrlF = 6,
    CtrlG = 7,
    CtrlH = 8,
    CtrlK = 11,
    CtrlL = 12,
    CtrlN = 14,
    CtrlP = 16,
    CtrlR = 18,
    CtrlU = 21,
    CtrlW = 23
};

/**
 * @brief Input event structure
 */
struct InputEvent {
    Key key{Key::Unknown};
    char character{'\0'};
    bool isSpecialKey{false};
    bool hasModifier{false};
    bool ctrl{false};
    bool alt{false};
    bool shift{false};
};

/**
 * @brief Terminal size information
 */
struct TerminalSize {
    int width{80};
    int height{24};
};

/**
 * @brief Cursor position
 */
struct CursorPosition {
    int x{0};
    int y{0};
};

/**
 * @brief Command execution result
 */
struct CommandResult {
    bool success{false};
    std::string output;
    std::string error;
    std::chrono::milliseconds executionTime{0};
    int exitCode{0};
};

/**
 * @brief History entry with metadata
 */
struct HistoryEntry {
    std::string command;
    std::chrono::system_clock::time_point timestamp;
    std::optional<CommandResult> result;
    bool favorite{false};
    std::vector<std::string> tags;
};

/**
 * @brief TUI panel types
 */
enum class PanelType {
    Command,      ///< Command input panel
    Output,       ///< Command output panel
    History,      ///< Command history panel
    Suggestions,  ///< Suggestions panel
    Status,       ///< Status bar panel
    Help,         ///< Help panel
    Log           ///< Log viewer panel
};

/**
 * @brief TUI layout configuration
 */
struct LayoutConfig {
    bool showStatusBar{true};
    bool showHistory{false};
    bool showSuggestions{true};
    bool showHelp{false};
    bool splitVertical{false};
    int historyPanelWidth{30};
    int suggestionPanelHeight{5};
    int statusBarHeight{1};
};

/**
 * @brief Callback types
 */
using CommandCallback = std::function<CommandResult(
    const std::string&, const std::vector<std::string>&)>;
using CompletionCallback =
    std::function<std::vector<std::string>(const std::string&)>;
using HistorySearchCallback =
    std::function<std::vector<HistoryEntry>(const std::string&)>;

}  // namespace lithium::debug::terminal

#endif  // LITHIUM_DEBUG_TERMINAL_TYPES_HPP
