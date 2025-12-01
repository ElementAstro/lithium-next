/**
 * @file test_types.cpp
 * @brief Comprehensive unit tests for terminal types
 *
 * Tests for:
 * - Color enum
 * - Style enum
 * - Theme struct and factory methods
 * - Key enum
 * - InputEvent struct
 * - TerminalSize struct
 * - CursorPosition struct
 * - CommandResult struct
 * - HistoryEntry struct
 * - PanelType enum
 * - LayoutConfig struct
 * - Callback types
 */

#include <gtest/gtest.h>
#include <chrono>
#include <functional>

#include "debug/terminal/types.hpp"

namespace lithium::debug::terminal::test {

// ============================================================================
// Color Enum Tests
// ============================================================================

class ColorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ColorTest, DefaultColorValue) {
    EXPECT_EQ(static_cast<int>(Color::Default), 0);
}

TEST_F(ColorTest, StandardColorValues) {
    EXPECT_EQ(static_cast<int>(Color::Black), 30);
    EXPECT_EQ(static_cast<int>(Color::Red), 31);
    EXPECT_EQ(static_cast<int>(Color::Green), 32);
    EXPECT_EQ(static_cast<int>(Color::Yellow), 33);
    EXPECT_EQ(static_cast<int>(Color::Blue), 34);
    EXPECT_EQ(static_cast<int>(Color::Magenta), 35);
    EXPECT_EQ(static_cast<int>(Color::Cyan), 36);
    EXPECT_EQ(static_cast<int>(Color::White), 37);
}

TEST_F(ColorTest, BrightColorValues) {
    EXPECT_EQ(static_cast<int>(Color::BrightBlack), 90);
    EXPECT_EQ(static_cast<int>(Color::BrightRed), 91);
    EXPECT_EQ(static_cast<int>(Color::BrightGreen), 92);
    EXPECT_EQ(static_cast<int>(Color::BrightYellow), 93);
    EXPECT_EQ(static_cast<int>(Color::BrightBlue), 94);
    EXPECT_EQ(static_cast<int>(Color::BrightMagenta), 95);
    EXPECT_EQ(static_cast<int>(Color::BrightCyan), 96);
    EXPECT_EQ(static_cast<int>(Color::BrightWhite), 97);
}

// ============================================================================
// Style Enum Tests
// ============================================================================

class StyleTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(StyleTest, StyleValues) {
    EXPECT_EQ(static_cast<int>(Style::Normal), 0);
    EXPECT_EQ(static_cast<int>(Style::Bold), 1);
    EXPECT_EQ(static_cast<int>(Style::Dim), 2);
    EXPECT_EQ(static_cast<int>(Style::Italic), 3);
    EXPECT_EQ(static_cast<int>(Style::Underline), 4);
    EXPECT_EQ(static_cast<int>(Style::Blink), 5);
    EXPECT_EQ(static_cast<int>(Style::Reverse), 7);
    EXPECT_EQ(static_cast<int>(Style::Hidden), 8);
    EXPECT_EQ(static_cast<int>(Style::Strikethrough), 9);
}

// ============================================================================
// Theme Tests
// ============================================================================

class ThemeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ThemeTest, DefaultThemeConstruction) {
    Theme theme;
    EXPECT_EQ(theme.name, "default");
    EXPECT_EQ(theme.promptColor, Color::BrightCyan);
    EXPECT_EQ(theme.promptSymbolColor, Color::BrightGreen);
    EXPECT_EQ(theme.successColor, Color::BrightGreen);
    EXPECT_EQ(theme.errorColor, Color::BrightRed);
    EXPECT_EQ(theme.warningColor, Color::BrightYellow);
    EXPECT_EQ(theme.infoColor, Color::BrightBlue);
    EXPECT_EQ(theme.debugColor, Color::BrightMagenta);
}

TEST_F(ThemeTest, DefaultThemeUIColors) {
    Theme theme;
    EXPECT_EQ(theme.headerColor, Color::BrightBlue);
    EXPECT_EQ(theme.borderColor, Color::Blue);
    EXPECT_EQ(theme.highlightColor, Color::BrightCyan);
    EXPECT_EQ(theme.suggestionColor, Color::Cyan);
    EXPECT_EQ(theme.historyColor, Color::BrightBlack);
}

TEST_F(ThemeTest, DefaultThemeStyles) {
    Theme theme;
    EXPECT_EQ(theme.headerStyle, Style::Bold);
    EXPECT_EQ(theme.errorStyle, Style::Bold);
    EXPECT_EQ(theme.promptStyle, Style::Bold);
}

TEST_F(ThemeTest, DefaultThemeSymbols) {
    Theme theme;
    EXPECT_EQ(theme.promptSymbol, "❯");
    EXPECT_EQ(theme.successSymbol, "✓");
    EXPECT_EQ(theme.errorSymbol, "✗");
    EXPECT_EQ(theme.warningSymbol, "⚠");
    EXPECT_EQ(theme.infoSymbol, "ℹ");
    EXPECT_EQ(theme.arrowSymbol, "→");
    EXPECT_EQ(theme.bulletSymbol, "•");
}

TEST_F(ThemeTest, DefaultThemeBorderCharacters) {
    Theme theme;
    EXPECT_EQ(theme.borderTopLeft, "╭");
    EXPECT_EQ(theme.borderTopRight, "╮");
    EXPECT_EQ(theme.borderBottomLeft, "╰");
    EXPECT_EQ(theme.borderBottomRight, "╯");
    EXPECT_EQ(theme.borderHorizontal, "─");
    EXPECT_EQ(theme.borderVertical, "│");
}

TEST_F(ThemeTest, DefaultThemeFeatureFlags) {
    Theme theme;
    EXPECT_TRUE(theme.useUnicode);
    EXPECT_TRUE(theme.useColors);
    EXPECT_TRUE(theme.useBoldHeaders);
}

TEST_F(ThemeTest, AsciiThemeFactory) {
    Theme theme = Theme::ascii();
    EXPECT_EQ(theme.name, "ascii");
    EXPECT_FALSE(theme.useUnicode);
    EXPECT_EQ(theme.promptSymbol, ">");
    EXPECT_EQ(theme.successSymbol, "[OK]");
    EXPECT_EQ(theme.errorSymbol, "[ERR]");
    EXPECT_EQ(theme.warningSymbol, "[WARN]");
    EXPECT_EQ(theme.infoSymbol, "[INFO]");
    EXPECT_EQ(theme.arrowSymbol, "->");
    EXPECT_EQ(theme.bulletSymbol, "*");
}

TEST_F(ThemeTest, AsciiThemeBorders) {
    Theme theme = Theme::ascii();
    EXPECT_EQ(theme.borderTopLeft, "+");
    EXPECT_EQ(theme.borderTopRight, "+");
    EXPECT_EQ(theme.borderBottomLeft, "+");
    EXPECT_EQ(theme.borderBottomRight, "+");
    EXPECT_EQ(theme.borderHorizontal, "-");
    EXPECT_EQ(theme.borderVertical, "|");
}

TEST_F(ThemeTest, DarkThemeFactory) {
    Theme theme = Theme::dark();
    EXPECT_EQ(theme.name, "dark");
    EXPECT_EQ(theme.promptColor, Color::BrightBlue);
    EXPECT_EQ(theme.headerColor, Color::Magenta);
    EXPECT_EQ(theme.borderColor, Color::BrightBlack);
}

TEST_F(ThemeTest, LightThemeFactory) {
    Theme theme = Theme::light();
    EXPECT_EQ(theme.name, "light");
    EXPECT_EQ(theme.promptColor, Color::Blue);
    EXPECT_EQ(theme.headerColor, Color::Blue);
    EXPECT_EQ(theme.borderColor, Color::Black);
    EXPECT_EQ(theme.highlightColor, Color::Cyan);
}

TEST_F(ThemeTest, ThemeCopyConstruction) {
    Theme original = Theme::dark();
    Theme copy = original;
    EXPECT_EQ(copy.name, original.name);
    EXPECT_EQ(copy.promptColor, original.promptColor);
}

TEST_F(ThemeTest, ThemeMoveConstruction) {
    Theme original = Theme::dark();
    std::string originalName = original.name;
    Theme moved = std::move(original);
    EXPECT_EQ(moved.name, originalName);
}

// ============================================================================
// Key Enum Tests
// ============================================================================

class KeyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(KeyTest, SpecialKeyValues) {
    EXPECT_EQ(static_cast<int>(Key::Unknown), 0);
    EXPECT_EQ(static_cast<int>(Key::Enter), 10);
    EXPECT_EQ(static_cast<int>(Key::Tab), 9);
    EXPECT_EQ(static_cast<int>(Key::Backspace), 127);
    EXPECT_EQ(static_cast<int>(Key::Escape), 27);
}

TEST_F(KeyTest, ArrowKeyValues) {
    EXPECT_EQ(static_cast<int>(Key::Up), 259);
    EXPECT_EQ(static_cast<int>(Key::Down), 258);
    EXPECT_EQ(static_cast<int>(Key::Left), 260);
    EXPECT_EQ(static_cast<int>(Key::Right), 261);
}

TEST_F(KeyTest, NavigationKeyValues) {
    EXPECT_EQ(static_cast<int>(Key::Home), 262);
    EXPECT_EQ(static_cast<int>(Key::End), 360);
    EXPECT_EQ(static_cast<int>(Key::PageUp), 339);
    EXPECT_EQ(static_cast<int>(Key::PageDown), 338);
    EXPECT_EQ(static_cast<int>(Key::Insert), 331);
    EXPECT_EQ(static_cast<int>(Key::Delete), 330);
}

TEST_F(KeyTest, FunctionKeyValues) {
    EXPECT_EQ(static_cast<int>(Key::F1), 265);
    EXPECT_EQ(static_cast<int>(Key::F2), 266);
    EXPECT_EQ(static_cast<int>(Key::F3), 267);
    EXPECT_EQ(static_cast<int>(Key::F4), 268);
    EXPECT_EQ(static_cast<int>(Key::F5), 269);
    EXPECT_EQ(static_cast<int>(Key::F6), 270);
    EXPECT_EQ(static_cast<int>(Key::F7), 271);
    EXPECT_EQ(static_cast<int>(Key::F8), 272);
    EXPECT_EQ(static_cast<int>(Key::F9), 273);
    EXPECT_EQ(static_cast<int>(Key::F10), 274);
    EXPECT_EQ(static_cast<int>(Key::F11), 275);
    EXPECT_EQ(static_cast<int>(Key::F12), 276);
}

TEST_F(KeyTest, ControlKeyValues) {
    EXPECT_EQ(static_cast<int>(Key::CtrlA), 1);
    EXPECT_EQ(static_cast<int>(Key::CtrlB), 2);
    EXPECT_EQ(static_cast<int>(Key::CtrlC), 3);
    EXPECT_EQ(static_cast<int>(Key::CtrlD), 4);
    EXPECT_EQ(static_cast<int>(Key::CtrlE), 5);
    EXPECT_EQ(static_cast<int>(Key::CtrlF), 6);
    EXPECT_EQ(static_cast<int>(Key::CtrlR), 18);
}

// ============================================================================
// InputEvent Tests
// ============================================================================

class InputEventTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(InputEventTest, DefaultConstruction) {
    InputEvent event;
    EXPECT_EQ(event.key, Key::Unknown);
    EXPECT_EQ(event.character, '\0');
    EXPECT_FALSE(event.isSpecialKey);
    EXPECT_FALSE(event.hasModifier);
    EXPECT_FALSE(event.ctrl);
    EXPECT_FALSE(event.alt);
    EXPECT_FALSE(event.shift);
}

TEST_F(InputEventTest, CharacterEvent) {
    InputEvent event;
    event.character = 'a';
    event.isSpecialKey = false;
    EXPECT_EQ(event.character, 'a');
    EXPECT_FALSE(event.isSpecialKey);
}

TEST_F(InputEventTest, SpecialKeyEvent) {
    InputEvent event;
    event.key = Key::Enter;
    event.isSpecialKey = true;
    EXPECT_EQ(event.key, Key::Enter);
    EXPECT_TRUE(event.isSpecialKey);
}

TEST_F(InputEventTest, ModifierEvent) {
    InputEvent event;
    event.key = Key::CtrlC;
    event.hasModifier = true;
    event.ctrl = true;
    EXPECT_TRUE(event.hasModifier);
    EXPECT_TRUE(event.ctrl);
    EXPECT_FALSE(event.alt);
    EXPECT_FALSE(event.shift);
}

TEST_F(InputEventTest, CombinedModifiers) {
    InputEvent event;
    event.hasModifier = true;
    event.ctrl = true;
    event.shift = true;
    EXPECT_TRUE(event.ctrl);
    EXPECT_TRUE(event.shift);
    EXPECT_FALSE(event.alt);
}

// ============================================================================
// TerminalSize Tests
// ============================================================================

class TerminalSizeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TerminalSizeTest, DefaultConstruction) {
    TerminalSize size;
    EXPECT_EQ(size.width, 80);
    EXPECT_EQ(size.height, 24);
}

TEST_F(TerminalSizeTest, CustomSize) {
    TerminalSize size;
    size.width = 120;
    size.height = 40;
    EXPECT_EQ(size.width, 120);
    EXPECT_EQ(size.height, 40);
}

TEST_F(TerminalSizeTest, CopyConstruction) {
    TerminalSize original;
    original.width = 100;
    original.height = 50;
    TerminalSize copy = original;
    EXPECT_EQ(copy.width, 100);
    EXPECT_EQ(copy.height, 50);
}

// ============================================================================
// CursorPosition Tests
// ============================================================================

class CursorPositionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CursorPositionTest, DefaultConstruction) {
    CursorPosition pos;
    EXPECT_EQ(pos.x, 0);
    EXPECT_EQ(pos.y, 0);
}

TEST_F(CursorPositionTest, CustomPosition) {
    CursorPosition pos;
    pos.x = 10;
    pos.y = 20;
    EXPECT_EQ(pos.x, 10);
    EXPECT_EQ(pos.y, 20);
}

// ============================================================================
// CommandResult Tests
// ============================================================================

class CommandResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResultTest, DefaultConstruction) {
    CommandResult result;
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.output.empty());
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(result.executionTime.count(), 0);
    EXPECT_EQ(result.exitCode, 0);
}

TEST_F(CommandResultTest, SuccessfulResult) {
    CommandResult result;
    result.success = true;
    result.output = "Command executed successfully";
    result.exitCode = 0;
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, "Command executed successfully");
    EXPECT_EQ(result.exitCode, 0);
}

TEST_F(CommandResultTest, FailedResult) {
    CommandResult result;
    result.success = false;
    result.error = "Command failed";
    result.exitCode = 1;
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error, "Command failed");
    EXPECT_EQ(result.exitCode, 1);
}

TEST_F(CommandResultTest, ExecutionTime) {
    CommandResult result;
    result.executionTime = std::chrono::milliseconds(150);
    EXPECT_EQ(result.executionTime.count(), 150);
}

// ============================================================================
// HistoryEntry Tests
// ============================================================================

class HistoryEntryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HistoryEntryTest, DefaultConstruction) {
    HistoryEntry entry;
    EXPECT_TRUE(entry.command.empty());
    EXPECT_FALSE(entry.favorite);
    EXPECT_TRUE(entry.tags.empty());
    EXPECT_FALSE(entry.result.has_value());
}

TEST_F(HistoryEntryTest, WithCommand) {
    HistoryEntry entry;
    entry.command = "ls -la";
    entry.timestamp = std::chrono::system_clock::now();
    EXPECT_EQ(entry.command, "ls -la");
}

TEST_F(HistoryEntryTest, WithResult) {
    HistoryEntry entry;
    entry.command = "echo hello";
    CommandResult result;
    result.success = true;
    result.output = "hello";
    entry.result = result;
    ASSERT_TRUE(entry.result.has_value());
    EXPECT_TRUE(entry.result->success);
    EXPECT_EQ(entry.result->output, "hello");
}

TEST_F(HistoryEntryTest, WithFavorite) {
    HistoryEntry entry;
    entry.command = "important_command";
    entry.favorite = true;
    EXPECT_TRUE(entry.favorite);
}

TEST_F(HistoryEntryTest, WithTags) {
    HistoryEntry entry;
    entry.command = "tagged_command";
    entry.tags = {"git", "important", "daily"};
    EXPECT_EQ(entry.tags.size(), 3);
    EXPECT_EQ(entry.tags[0], "git");
    EXPECT_EQ(entry.tags[1], "important");
    EXPECT_EQ(entry.tags[2], "daily");
}

// ============================================================================
// PanelType Tests
// ============================================================================

class PanelTypeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PanelTypeTest, PanelTypeValues) {
    // Verify all panel types exist
    PanelType command = PanelType::Command;
    PanelType output = PanelType::Output;
    PanelType history = PanelType::History;
    PanelType suggestions = PanelType::Suggestions;
    PanelType status = PanelType::Status;
    PanelType help = PanelType::Help;
    PanelType log = PanelType::Log;

    EXPECT_NE(command, output);
    EXPECT_NE(output, history);
    EXPECT_NE(history, suggestions);
    EXPECT_NE(suggestions, status);
    EXPECT_NE(status, help);
    EXPECT_NE(help, log);
}

// ============================================================================
// LayoutConfig Tests
// ============================================================================

class LayoutConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(LayoutConfigTest, DefaultConstruction) {
    LayoutConfig config;
    EXPECT_TRUE(config.showStatusBar);
    EXPECT_FALSE(config.showHistory);
    EXPECT_TRUE(config.showSuggestions);
    EXPECT_FALSE(config.showHelp);
    EXPECT_FALSE(config.splitVertical);
    EXPECT_EQ(config.historyPanelWidth, 30);
    EXPECT_EQ(config.suggestionPanelHeight, 5);
    EXPECT_EQ(config.statusBarHeight, 1);
}

TEST_F(LayoutConfigTest, CustomConfiguration) {
    LayoutConfig config;
    config.showStatusBar = false;
    config.showHistory = true;
    config.splitVertical = true;
    config.historyPanelWidth = 50;
    EXPECT_FALSE(config.showStatusBar);
    EXPECT_TRUE(config.showHistory);
    EXPECT_TRUE(config.splitVertical);
    EXPECT_EQ(config.historyPanelWidth, 50);
}

// ============================================================================
// Callback Type Tests
// ============================================================================

class CallbackTypeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CallbackTypeTest, CommandCallbackType) {
    CommandCallback callback = [](const std::string& cmd,
                                  const std::vector<std::string>& args) {
        CommandResult result;
        result.success = true;
        result.output = cmd;
        return result;
    };

    auto result = callback("test", {"arg1", "arg2"});
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.output, "test");
}

TEST_F(CallbackTypeTest, CompletionCallbackType) {
    CompletionCallback callback = [](const std::string& prefix) {
        std::vector<std::string> completions;
        if (prefix == "he") {
            completions = {"help", "hello"};
        }
        return completions;
    };

    auto completions = callback("he");
    EXPECT_EQ(completions.size(), 2);
    EXPECT_EQ(completions[0], "help");
    EXPECT_EQ(completions[1], "hello");
}

TEST_F(CallbackTypeTest, HistorySearchCallbackType) {
    HistorySearchCallback callback = [](const std::string& pattern) {
        std::vector<HistoryEntry> results;
        HistoryEntry entry;
        entry.command = "git " + pattern;
        results.push_back(entry);
        return results;
    };

    auto results = callback("status");
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].command, "git status");
}

}  // namespace lithium::debug::terminal::test
