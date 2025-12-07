/*
 * terminal_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Unified terminal configuration

**************************************************/

#ifndef LITHIUM_CONFIG_SECTIONS_TERMINAL_CONFIG_HPP
#define LITHIUM_CONFIG_SECTIONS_TERMINAL_CONFIG_HPP

#include <string>
#include <vector>

#include "../core/config_section.hpp"

namespace lithium::config {

/**
 * @brief Theme configuration for terminal appearance
 */
struct ThemeConfig {
    std::string name{"default"};  ///< Theme name

    // Colors (ANSI color names)
    std::string promptColor{"bright_cyan"};
    std::string promptSymbolColor{"bright_green"};
    std::string successColor{"bright_green"};
    std::string errorColor{"bright_red"};
    std::string warningColor{"bright_yellow"};
    std::string infoColor{"bright_blue"};
    std::string debugColor{"bright_magenta"};
    std::string headerColor{"bright_blue"};
    std::string borderColor{"blue"};
    std::string highlightColor{"bright_cyan"};
    std::string suggestionColor{"cyan"};
    std::string historyColor{"bright_black"};

    // Styles
    bool useBoldHeaders{true};

    // UI characters
    std::string promptSymbol{">"};  ///< Prompt character (use > for ASCII)
    std::string successSymbol{"[OK]"};
    std::string errorSymbol{"[ERR]"};
    std::string warningSymbol{"[WARN]"};
    std::string infoSymbol{"[INFO]"};

    // Feature flags
    bool useUnicode{true};  ///< Use Unicode characters
    bool useColors{true};   ///< Enable colored output

    [[nodiscard]] json toJson() const {
        return {{"name", name},
                {"promptColor", promptColor},
                {"promptSymbolColor", promptSymbolColor},
                {"successColor", successColor},
                {"errorColor", errorColor},
                {"warningColor", warningColor},
                {"infoColor", infoColor},
                {"debugColor", debugColor},
                {"headerColor", headerColor},
                {"borderColor", borderColor},
                {"highlightColor", highlightColor},
                {"suggestionColor", suggestionColor},
                {"historyColor", historyColor},
                {"useBoldHeaders", useBoldHeaders},
                {"promptSymbol", promptSymbol},
                {"successSymbol", successSymbol},
                {"errorSymbol", errorSymbol},
                {"warningSymbol", warningSymbol},
                {"infoSymbol", infoSymbol},
                {"useUnicode", useUnicode},
                {"useColors", useColors}};
    }

    [[nodiscard]] static ThemeConfig fromJson(const json& j) {
        ThemeConfig cfg;
        cfg.name = j.value("name", cfg.name);
        cfg.promptColor = j.value("promptColor", cfg.promptColor);
        cfg.promptSymbolColor =
            j.value("promptSymbolColor", cfg.promptSymbolColor);
        cfg.successColor = j.value("successColor", cfg.successColor);
        cfg.errorColor = j.value("errorColor", cfg.errorColor);
        cfg.warningColor = j.value("warningColor", cfg.warningColor);
        cfg.infoColor = j.value("infoColor", cfg.infoColor);
        cfg.debugColor = j.value("debugColor", cfg.debugColor);
        cfg.headerColor = j.value("headerColor", cfg.headerColor);
        cfg.borderColor = j.value("borderColor", cfg.borderColor);
        cfg.highlightColor = j.value("highlightColor", cfg.highlightColor);
        cfg.suggestionColor = j.value("suggestionColor", cfg.suggestionColor);
        cfg.historyColor = j.value("historyColor", cfg.historyColor);
        cfg.useBoldHeaders = j.value("useBoldHeaders", cfg.useBoldHeaders);
        cfg.promptSymbol = j.value("promptSymbol", cfg.promptSymbol);
        cfg.successSymbol = j.value("successSymbol", cfg.successSymbol);
        cfg.errorSymbol = j.value("errorSymbol", cfg.errorSymbol);
        cfg.warningSymbol = j.value("warningSymbol", cfg.warningSymbol);
        cfg.infoSymbol = j.value("infoSymbol", cfg.infoSymbol);
        cfg.useUnicode = j.value("useUnicode", cfg.useUnicode);
        cfg.useColors = j.value("useColors", cfg.useColors);
        return cfg;
    }
};

/**
 * @brief TUI layout configuration
 */
struct LayoutConfig {
    bool showStatusBar{true};      ///< Show status bar
    bool showHistory{false};       ///< Show history panel
    bool showSuggestions{true};    ///< Show suggestions panel
    bool showHelp{false};          ///< Show help panel
    bool splitVertical{false};     ///< Split panels vertically
    int historyPanelWidth{30};     ///< History panel width
    int suggestionPanelHeight{5};  ///< Suggestion panel height
    int statusBarHeight{1};        ///< Status bar height

    [[nodiscard]] json toJson() const {
        return {{"showStatusBar", showStatusBar},
                {"showHistory", showHistory},
                {"showSuggestions", showSuggestions},
                {"showHelp", showHelp},
                {"splitVertical", splitVertical},
                {"historyPanelWidth", historyPanelWidth},
                {"suggestionPanelHeight", suggestionPanelHeight},
                {"statusBarHeight", statusBarHeight}};
    }

    [[nodiscard]] static LayoutConfig fromJson(const json& j) {
        LayoutConfig cfg;
        cfg.showStatusBar = j.value("showStatusBar", cfg.showStatusBar);
        cfg.showHistory = j.value("showHistory", cfg.showHistory);
        cfg.showSuggestions = j.value("showSuggestions", cfg.showSuggestions);
        cfg.showHelp = j.value("showHelp", cfg.showHelp);
        cfg.splitVertical = j.value("splitVertical", cfg.splitVertical);
        cfg.historyPanelWidth =
            j.value("historyPanelWidth", cfg.historyPanelWidth);
        cfg.suggestionPanelHeight =
            j.value("suggestionPanelHeight", cfg.suggestionPanelHeight);
        cfg.statusBarHeight = j.value("statusBarHeight", cfg.statusBarHeight);
        return cfg;
    }
};

/**
 * @brief History configuration
 */
struct HistoryConfig {
    size_t maxSize{1000};      ///< Maximum history entries
    std::string historyFile;   ///< History file path (empty = in-memory only)
    bool persistOnExit{true};  ///< Save history on exit
    bool ignoreDuplicates{true};   ///< Don't add consecutive duplicates
    bool ignoreSpacePrefix{true};  ///< Ignore commands starting with space
    std::vector<std::string> ignorePatterns;  ///< Patterns to ignore

    [[nodiscard]] json toJson() const {
        return {{"maxSize", maxSize},
                {"historyFile", historyFile},
                {"persistOnExit", persistOnExit},
                {"ignoreDuplicates", ignoreDuplicates},
                {"ignoreSpacePrefix", ignoreSpacePrefix},
                {"ignorePatterns", ignorePatterns}};
    }

    [[nodiscard]] static HistoryConfig fromJson(const json& j) {
        HistoryConfig cfg;
        cfg.maxSize = j.value("maxSize", cfg.maxSize);
        cfg.historyFile = j.value("historyFile", cfg.historyFile);
        cfg.persistOnExit = j.value("persistOnExit", cfg.persistOnExit);
        cfg.ignoreDuplicates =
            j.value("ignoreDuplicates", cfg.ignoreDuplicates);
        cfg.ignoreSpacePrefix =
            j.value("ignoreSpacePrefix", cfg.ignoreSpacePrefix);
        if (j.contains("ignorePatterns") && j["ignorePatterns"].is_array()) {
            cfg.ignorePatterns =
                j["ignorePatterns"].get<std::vector<std::string>>();
        }
        return cfg;
    }
};

/**
 * @brief Completion configuration
 */
struct CompletionConfig {
    bool enabled{true};          ///< Enable auto-completion
    bool caseSensitive{false};   ///< Case-sensitive matching
    size_t maxSuggestions{10};   ///< Maximum suggestions to show
    size_t minChars{1};          ///< Minimum characters before suggestions
    bool showDescription{true};  ///< Show description in suggestions
    bool fuzzyMatching{true};    ///< Enable fuzzy matching

    [[nodiscard]] json toJson() const {
        return {{"enabled", enabled},
                {"caseSensitive", caseSensitive},
                {"maxSuggestions", maxSuggestions},
                {"minChars", minChars},
                {"showDescription", showDescription},
                {"fuzzyMatching", fuzzyMatching}};
    }

    [[nodiscard]] static CompletionConfig fromJson(const json& j) {
        CompletionConfig cfg;
        cfg.enabled = j.value("enabled", cfg.enabled);
        cfg.caseSensitive = j.value("caseSensitive", cfg.caseSensitive);
        cfg.maxSuggestions = j.value("maxSuggestions", cfg.maxSuggestions);
        cfg.minChars = j.value("minChars", cfg.minChars);
        cfg.showDescription = j.value("showDescription", cfg.showDescription);
        cfg.fuzzyMatching = j.value("fuzzyMatching", cfg.fuzzyMatching);
        return cfg;
    }
};

/**
 * @brief Unified terminal configuration
 *
 * Consolidates terminal display, input, history, and TUI settings.
 */
struct TerminalConfig : ConfigSection<TerminalConfig> {
    /// Configuration path
    static constexpr std::string_view PATH = "/lithium/terminal";

    // ========================================================================
    // General Settings
    // ========================================================================

    bool enableTui{true};           ///< Enable full TUI mode
    bool enableColors{true};        ///< Enable colored output
    bool enableUnicode{true};       ///< Enable Unicode characters
    size_t commandTimeoutMs{5000};  ///< Default command timeout
    bool enableCommandCheck{true};  ///< Enable command validation
    std::string configFile;         ///< Terminal config file path

    // ========================================================================
    // Theme
    // ========================================================================

    ThemeConfig theme;

    // ========================================================================
    // Layout
    // ========================================================================

    LayoutConfig layout;

    // ========================================================================
    // History
    // ========================================================================

    HistoryConfig history;

    // ========================================================================
    // Completion
    // ========================================================================

    CompletionConfig completion;

    // ========================================================================
    // Serialization
    // ========================================================================

    [[nodiscard]] json serialize() const {
        return {// General
                {"enableTui", enableTui},
                {"enableColors", enableColors},
                {"enableUnicode", enableUnicode},
                {"commandTimeoutMs", commandTimeoutMs},
                {"enableCommandCheck", enableCommandCheck},
                {"configFile", configFile},
                // Nested configs
                {"theme", theme.toJson()},
                {"layout", layout.toJson()},
                {"history", history.toJson()},
                {"completion", completion.toJson()}};
    }

    [[nodiscard]] static TerminalConfig deserialize(const json& j) {
        TerminalConfig cfg;

        // General
        cfg.enableTui = j.value("enableTui", cfg.enableTui);
        cfg.enableColors = j.value("enableColors", cfg.enableColors);
        cfg.enableUnicode = j.value("enableUnicode", cfg.enableUnicode);
        cfg.commandTimeoutMs =
            j.value("commandTimeoutMs", cfg.commandTimeoutMs);
        cfg.enableCommandCheck =
            j.value("enableCommandCheck", cfg.enableCommandCheck);
        cfg.configFile = j.value("configFile", cfg.configFile);

        // Nested configs
        if (j.contains("theme")) {
            cfg.theme = ThemeConfig::fromJson(j["theme"]);
        }
        if (j.contains("layout")) {
            cfg.layout = LayoutConfig::fromJson(j["layout"]);
        }
        if (j.contains("history")) {
            cfg.history = HistoryConfig::fromJson(j["history"]);
        }
        if (j.contains("completion")) {
            cfg.completion = CompletionConfig::fromJson(j["completion"]);
        }

        return cfg;
    }

    [[nodiscard]] static json generateSchema() {
        return {
            {"type", "object"},
            {"properties",
             {// General
              {"enableTui", {{"type", "boolean"}, {"default", true}}},
              {"enableColors", {{"type", "boolean"}, {"default", true}}},
              {"enableUnicode", {{"type", "boolean"}, {"default", true}}},
              {"commandTimeoutMs",
               {{"type", "integer"}, {"minimum", 0}, {"default", 5000}}},
              {"enableCommandCheck", {{"type", "boolean"}, {"default", true}}},
              {"configFile", {{"type", "string"}}},
              // Theme
              {"theme",
               {{"type", "object"},
                {"properties",
                 {{"name", {{"type", "string"}, {"default", "default"}}},
                  {"promptColor", {{"type", "string"}}},
                  {"promptSymbolColor", {{"type", "string"}}},
                  {"successColor", {{"type", "string"}}},
                  {"errorColor", {{"type", "string"}}},
                  {"warningColor", {{"type", "string"}}},
                  {"infoColor", {{"type", "string"}}},
                  {"useUnicode", {{"type", "boolean"}, {"default", true}}},
                  {"useColors", {{"type", "boolean"}, {"default", true}}}}}}},
              // Layout
              {"layout",
               {{"type", "object"},
                {"properties",
                 {{"showStatusBar", {{"type", "boolean"}, {"default", true}}},
                  {"showHistory", {{"type", "boolean"}, {"default", false}}},
                  {"showSuggestions", {{"type", "boolean"}, {"default", true}}},
                  {"showHelp", {{"type", "boolean"}, {"default", false}}},
                  {"splitVertical", {{"type", "boolean"}, {"default", false}}},
                  {"historyPanelWidth",
                   {{"type", "integer"},
                    {"minimum", 10},
                    {"maximum", 100},
                    {"default", 30}}},
                  {"suggestionPanelHeight",
                   {{"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 20},
                    {"default", 5}}},
                  {"statusBarHeight",
                   {{"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 5},
                    {"default", 1}}}}}}},
              // History
              {"history",
               {{"type", "object"},
                {"properties",
                 {{"maxSize",
                   {{"type", "integer"},
                    {"minimum", 0},
                    {"maximum", 100000},
                    {"default", 1000}}},
                  {"historyFile", {{"type", "string"}}},
                  {"persistOnExit", {{"type", "boolean"}, {"default", true}}},
                  {"ignoreDuplicates",
                   {{"type", "boolean"}, {"default", true}}},
                  {"ignoreSpacePrefix",
                   {{"type", "boolean"}, {"default", true}}},
                  {"ignorePatterns",
                   {{"type", "array"}, {"items", {{"type", "string"}}}}}}}}},
              // Completion
              {"completion",
               {{"type", "object"},
                {"properties",
                 {{"enabled", {{"type", "boolean"}, {"default", true}}},
                  {"caseSensitive", {{"type", "boolean"}, {"default", false}}},
                  {"maxSuggestions",
                   {{"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 100},
                    {"default", 10}}},
                  {"minChars",
                   {{"type", "integer"},
                    {"minimum", 0},
                    {"maximum", 10},
                    {"default", 1}}},
                  {"showDescription", {{"type", "boolean"}, {"default", true}}},
                  {"fuzzyMatching",
                   {{"type", "boolean"}, {"default", true}}}}}}}}}};
    }
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_SECTIONS_TERMINAL_CONFIG_HPP
