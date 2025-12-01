/*
 * terminal_config_adapter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Adapter to convert unified TerminalConfig to debug terminal configs

**************************************************/

#ifndef LITHIUM_CONFIG_ADAPTERS_TERMINAL_CONFIG_ADAPTER_HPP
#define LITHIUM_CONFIG_ADAPTERS_TERMINAL_CONFIG_ADAPTER_HPP

#include "../sections/terminal_config.hpp"
#include "debug/terminal/types.hpp"
#include "debug/terminal/terminal.hpp"

namespace lithium::config {

/**
 * @brief Convert color name string to Color enum
 */
[[nodiscard]] inline lithium::debug::terminal::Color colorFromString(
    const std::string& name) {
    using Color = lithium::debug::terminal::Color;

    static const std::unordered_map<std::string, Color> colorMap = {
        {"default", Color::Default},
        {"black", Color::Black},
        {"red", Color::Red},
        {"green", Color::Green},
        {"yellow", Color::Yellow},
        {"blue", Color::Blue},
        {"magenta", Color::Magenta},
        {"cyan", Color::Cyan},
        {"white", Color::White},
        {"bright_black", Color::BrightBlack},
        {"bright_red", Color::BrightRed},
        {"bright_green", Color::BrightGreen},
        {"bright_yellow", Color::BrightYellow},
        {"bright_blue", Color::BrightBlue},
        {"bright_magenta", Color::BrightMagenta},
        {"bright_cyan", Color::BrightCyan},
        {"bright_white", Color::BrightWhite}
    };

    auto it = colorMap.find(name);
    return it != colorMap.end() ? it->second : Color::Default;
}

/**
 * @brief Convert unified ThemeConfig to terminal Theme
 */
[[nodiscard]] inline lithium::debug::terminal::Theme toTerminalTheme(
    const config::ThemeConfig& unified) {
    lithium::debug::terminal::Theme theme;

    theme.name = unified.name;

    // Convert colors
    theme.promptColor = colorFromString(unified.promptColor);
    theme.promptSymbolColor = colorFromString(unified.promptSymbolColor);
    theme.successColor = colorFromString(unified.successColor);
    theme.errorColor = colorFromString(unified.errorColor);
    theme.warningColor = colorFromString(unified.warningColor);
    theme.infoColor = colorFromString(unified.infoColor);
    theme.debugColor = colorFromString(unified.debugColor);
    theme.headerColor = colorFromString(unified.headerColor);
    theme.borderColor = colorFromString(unified.borderColor);
    theme.highlightColor = colorFromString(unified.highlightColor);
    theme.suggestionColor = colorFromString(unified.suggestionColor);
    theme.historyColor = colorFromString(unified.historyColor);

    // Styles
    theme.useBoldHeaders = unified.useBoldHeaders;

    // Symbols
    theme.promptSymbol = unified.promptSymbol;
    theme.successSymbol = unified.successSymbol;
    theme.errorSymbol = unified.errorSymbol;
    theme.warningSymbol = unified.warningSymbol;
    theme.infoSymbol = unified.infoSymbol;

    // Features
    theme.useUnicode = unified.useUnicode;
    theme.useColors = unified.useColors;

    return theme;
}

/**
 * @brief Convert unified LayoutConfig to terminal LayoutConfig
 */
[[nodiscard]] inline lithium::debug::terminal::LayoutConfig toTerminalLayout(
    const config::LayoutConfig& unified) {
    lithium::debug::terminal::LayoutConfig layout;
    layout.showStatusBar = unified.showStatusBar;
    layout.showHistory = unified.showHistory;
    layout.showSuggestions = unified.showSuggestions;
    layout.showHelp = unified.showHelp;
    layout.splitVertical = unified.splitVertical;
    layout.historyPanelWidth = unified.historyPanelWidth;
    layout.suggestionPanelHeight = unified.suggestionPanelHeight;
    layout.statusBarHeight = unified.statusBarHeight;
    return layout;
}

/**
 * @brief Convert unified TerminalConfig to debug terminal's TerminalConfig
 */
[[nodiscard]] inline lithium::debug::terminal::TerminalConfig toTerminalConfig(
    const config::TerminalConfig& unified) {
    lithium::debug::terminal::TerminalConfig config;

    // General settings
    config.enableTui = unified.enableTui;
    config.enableColors = unified.enableColors;
    config.enableUnicode = unified.enableUnicode;
    config.commandTimeout = std::chrono::milliseconds(unified.commandTimeoutMs);
    config.enableCommandCheck = unified.enableCommandCheck;
    config.configFile = unified.configFile;

    // Input settings (derived from completion config)
    config.enableHistory = true;  // Always enable if history is configured
    config.enableCompletion = unified.completion.enabled;
    config.enableSuggestions = unified.completion.enabled;

    // Theme and layout
    config.theme = toTerminalTheme(unified.theme);
    config.layout = toTerminalLayout(unified.layout);

    // History file
    config.historyFile = unified.history.historyFile;

    return config;
}

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_ADAPTERS_TERMINAL_CONFIG_ADAPTER_HPP
