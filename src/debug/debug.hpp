/**
 * @file debug.hpp
 * @brief Unified facade header for the lithium debug module.
 *
 * This header provides access to all debug module components:
 * - CommandChecker: Command syntax and security validation
 * - SuggestionEngine: Command suggestion and completion
 * - ConsoleTerminal: Interactive terminal interface
 *
 * @par Usage Example:
 * @code
 * #include "debug/debug.hpp"
 *
 * using namespace lithium::debug;
 *
 * // Create a command checker
 * CommandChecker checker;
 * checker.setDangerousCommands({"rm -rf", "format"});
 * auto errors = checker.check("rm -rf /");
 *
 * // Create a suggestion engine
 * SuggestionEngine engine({"help", "exit", "list", "show"});
 * auto suggestions = engine.suggest("hel");
 *
 * // Create a console terminal
 * ConsoleTerminal terminal;
 * terminal.setCommandChecker(std::make_shared<CommandChecker>());
 * terminal.run();
 * @endcode
 *
 * @date 2024
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_DEBUG_HPP
#define LITHIUM_DEBUG_HPP

#include <memory>
#include <string>
#include <vector>

// Core debug components
#include "check.hpp"
#include "suggestion.hpp"
#include "terminal.hpp"

// Terminal submodule
#include "terminal/types.hpp"

namespace lithium::debug {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Debug module version.
 */
inline constexpr const char* DEBUG_VERSION = "1.1.0";

/**
 * @brief Get debug module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getDebugVersion() noexcept {
    return DEBUG_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Shared pointer to CommandChecker
using CommandCheckerPtr = std::shared_ptr<CommandChecker>;

/// Shared pointer to SuggestionEngine
using SuggestionEnginePtr = std::shared_ptr<SuggestionEngine>;

/// Shared pointer to ConsoleTerminal
using ConsoleTerminalPtr = std::shared_ptr<ConsoleTerminal>;

/// Unique pointer to ConsoleTerminal
using ConsoleTerminalUPtr = std::unique_ptr<ConsoleTerminal>;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new CommandChecker instance.
 * @return Shared pointer to the new CommandChecker.
 */
[[nodiscard]] inline CommandCheckerPtr createCommandChecker() {
    return std::make_shared<CommandChecker>();
}

/**
 * @brief Create a new SuggestionEngine instance.
 * @param dataset Initial dataset for suggestions.
 * @param config Configuration options.
 * @return Shared pointer to the new SuggestionEngine.
 */
[[nodiscard]] inline SuggestionEnginePtr createSuggestionEngine(
    const std::vector<std::string>& dataset,
    const SuggestionConfig& config = {}) {
    return std::make_shared<SuggestionEngine>(dataset, config);
}

/**
 * @brief Create a new ConsoleTerminal instance.
 * @return Unique pointer to the new ConsoleTerminal.
 */
[[nodiscard]] inline ConsoleTerminalUPtr createConsoleTerminal() {
    return std::make_unique<ConsoleTerminal>();
}

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Create default SuggestionConfig.
 * @return Default SuggestionConfig.
 */
[[nodiscard]] inline SuggestionConfig createDefaultSuggestionConfig() {
    return SuggestionConfig{};
}

/**
 * @brief Create SuggestionConfig with custom max suggestions.
 * @param maxSuggestions Maximum number of suggestions.
 * @return SuggestionConfig with custom max suggestions.
 */
[[nodiscard]] inline SuggestionConfig createSuggestionConfig(
    int maxSuggestions) {
    SuggestionConfig config;
    config.maxSuggestions = maxSuggestions;
    return config;
}

/**
 * @brief Create SuggestionConfig for fuzzy matching.
 * @param threshold Fuzzy match threshold (0.0-1.0).
 * @return SuggestionConfig for fuzzy matching.
 */
[[nodiscard]] inline SuggestionConfig createFuzzySuggestionConfig(
    float threshold) {
    SuggestionConfig config;
    config.fuzzyMatchThreshold = threshold;
    return config;
}

/**
 * @brief Convert ErrorSeverity to string.
 * @param severity The severity level.
 * @return String representation of the severity.
 */
[[nodiscard]] inline std::string errorSeverityToString(ErrorSeverity severity) {
    switch (severity) {
        case ErrorSeverity::WARNING:
            return "WARNING";
        case ErrorSeverity::ERROR:
            return "ERROR";
        case ErrorSeverity::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Check if an error severity is critical.
 * @param severity The severity level.
 * @return True if the severity is critical.
 */
[[nodiscard]] inline bool isCriticalError(ErrorSeverity severity) {
    return severity == ErrorSeverity::CRITICAL;
}

/**
 * @brief Check if an error severity is at least error level.
 * @param severity The severity level.
 * @return True if the severity is error or critical.
 */
[[nodiscard]] inline bool isError(ErrorSeverity severity) {
    return severity == ErrorSeverity::ERROR ||
           severity == ErrorSeverity::CRITICAL;
}

}  // namespace lithium::debug

#endif  // LITHIUM_DEBUG_HPP
