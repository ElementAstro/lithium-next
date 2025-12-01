/*
 * logging_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Unified logging configuration (merges lithium::log and lithium::logging)

**************************************************/

#ifndef LITHIUM_CONFIG_SECTIONS_LOGGING_CONFIG_HPP
#define LITHIUM_CONFIG_SECTIONS_LOGGING_CONFIG_HPP

#include <string>
#include <vector>

#include "../core/config_section.hpp"

namespace lithium::config {

/**
 * @brief Log level enumeration
 */
enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off
};

/**
 * @brief Convert LogLevel to string
 */
[[nodiscard]] inline std::string logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "trace";
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warn: return "warn";
        case LogLevel::Error: return "error";
        case LogLevel::Critical: return "critical";
        case LogLevel::Off: return "off";
    }
    return "info";
}

/**
 * @brief Convert string to LogLevel
 */
[[nodiscard]] inline LogLevel logLevelFromString(const std::string& str) {
    if (str == "trace") return LogLevel::Trace;
    if (str == "debug") return LogLevel::Debug;
    if (str == "info") return LogLevel::Info;
    if (str == "warn" || str == "warning") return LogLevel::Warn;
    if (str == "error" || str == "err") return LogLevel::Error;
    if (str == "critical" || str == "fatal") return LogLevel::Critical;
    if (str == "off" || str == "none") return LogLevel::Off;
    return LogLevel::Info;
}

/**
 * @brief Sink configuration for additional log sinks
 */
struct LogSinkConfig {
    std::string name;       ///< Sink identifier
    std::string type;       ///< Type: "console", "file", "rotating_file", "daily_file"
    std::string level;      ///< Log level for this sink
    std::string pattern;    ///< Log pattern (optional, uses default if empty)

    // File sink options
    std::string filePath;        ///< File path (for file sinks)
    size_t maxFileSize{10 * 1024 * 1024};  ///< Max file size for rotation
    size_t maxFiles{5};          ///< Max number of rotated files

    // Daily file options
    int rotationHour{0};         ///< Hour for daily rotation
    int rotationMinute{0};       ///< Minute for daily rotation

    [[nodiscard]] json toJson() const {
        return {
            {"name", name},
            {"type", type},
            {"level", level},
            {"pattern", pattern},
            {"filePath", filePath},
            {"maxFileSize", maxFileSize},
            {"maxFiles", maxFiles},
            {"rotationHour", rotationHour},
            {"rotationMinute", rotationMinute}
        };
    }

    [[nodiscard]] static LogSinkConfig fromJson(const json& j) {
        LogSinkConfig cfg;
        cfg.name = j.value("name", "");
        cfg.type = j.value("type", "console");
        cfg.level = j.value("level", "info");
        cfg.pattern = j.value("pattern", "");
        cfg.filePath = j.value("filePath", "");
        cfg.maxFileSize = j.value("maxFileSize", 10 * 1024 * 1024);
        cfg.maxFiles = j.value("maxFiles", 5);
        cfg.rotationHour = j.value("rotationHour", 0);
        cfg.rotationMinute = j.value("rotationMinute", 0);
        return cfg;
    }
};

/**
 * @brief Unified logging configuration
 *
 * This configuration struct consolidates the previously separate:
 * - lithium::log::LogConfig (from atom/log/spdlog_logger.hpp)
 * - lithium::logging::LoggingConfig (from src/logging/types.hpp)
 *
 * It provides a single, comprehensive logging configuration that supports:
 * - Console and file output with separate log levels
 * - File rotation (by size or daily)
 * - Async logging with configurable queue
 * - Custom log patterns
 * - Multiple additional sinks
 *
 * @example
 * ```yaml
 * lithium:
 *   logging:
 *     enableConsole: true
 *     consoleLevel: info
 *     consoleColor: true
 *     enableFile: true
 *     logDir: logs
 *     logFilename: lithium
 *     fileLevel: trace
 *     maxFileSize: 10485760  # 10 MB
 *     maxFiles: 5
 *     asyncMode: true
 *     asyncQueueSize: 8192
 * ```
 */
struct LoggingConfig : ConfigSection<LoggingConfig> {
    /// Configuration path in the config tree
    static constexpr std::string_view PATH = "/lithium/logging";

    // ========================================================================
    // Console Settings
    // ========================================================================

    bool enableConsole{true};          ///< Enable console output
    std::string consoleLevel{"info"};  ///< Console log level
    bool consoleColor{true};           ///< Enable ANSI color codes

    // ========================================================================
    // File Settings
    // ========================================================================

    bool enableFile{true};               ///< Enable file output
    std::string logDir{"logs"};          ///< Log directory path
    std::string logFilename{"lithium"};  ///< Base filename (without extension)
    std::string fileLevel{"trace"};      ///< File log level

    // ========================================================================
    // Rotation Settings
    // ========================================================================

    size_t maxFileSize{10 * 1024 * 1024};  ///< Max file size before rotation (10 MB)
    size_t maxFiles{5};                     ///< Max number of rotated files
    bool useDailyRotation{false};           ///< Use daily rotation instead of size-based
    int rotationHour{0};                    ///< Hour for daily rotation (0-23)
    int rotationMinute{0};                  ///< Minute for daily rotation (0-59)

    // ========================================================================
    // Format Settings
    // ========================================================================

    /// Default log pattern
    /// Available placeholders: %Y %m %d %H %M %S %e (milliseconds)
    ///                        %l (level), %n (logger name), %t (thread id)
    ///                        %s (source file), %# (line number), %v (message)
    std::string pattern{"[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v"};

    // ========================================================================
    // Async Settings
    // ========================================================================

    bool asyncMode{true};             ///< Enable async logging
    size_t asyncQueueSize{8192};      ///< Async queue size
    size_t asyncThreadCount{1};       ///< Number of async worker threads

    /// Overflow policy: "block" or "discard"
    std::string overflowPolicy{"block"};

    // ========================================================================
    // Ring Buffer (for in-memory logging)
    // ========================================================================

    bool enableRingBuffer{false};    ///< Enable ring buffer sink
    size_t ringBufferSize{1000};     ///< Ring buffer capacity

    // ========================================================================
    // Additional Sinks
    // ========================================================================

    std::vector<LogSinkConfig> additionalSinks;  ///< Extra sinks for LoggingManager

    // ========================================================================
    // Thread Naming
    // ========================================================================

    std::string mainThreadName{"main"};  ///< Name for the main thread

    // ========================================================================
    // Serialization
    // ========================================================================

    /**
     * @brief Serialize configuration to JSON
     */
    [[nodiscard]] json serialize() const {
        json sinksArray = json::array();
        for (const auto& sink : additionalSinks) {
            sinksArray.push_back(sink.toJson());
        }

        return {
            // Console
            {"enableConsole", enableConsole},
            {"consoleLevel", consoleLevel},
            {"consoleColor", consoleColor},
            // File
            {"enableFile", enableFile},
            {"logDir", logDir},
            {"logFilename", logFilename},
            {"fileLevel", fileLevel},
            // Rotation
            {"maxFileSize", maxFileSize},
            {"maxFiles", maxFiles},
            {"useDailyRotation", useDailyRotation},
            {"rotationHour", rotationHour},
            {"rotationMinute", rotationMinute},
            // Format
            {"pattern", pattern},
            // Async
            {"asyncMode", asyncMode},
            {"asyncQueueSize", asyncQueueSize},
            {"asyncThreadCount", asyncThreadCount},
            {"overflowPolicy", overflowPolicy},
            // Ring buffer
            {"enableRingBuffer", enableRingBuffer},
            {"ringBufferSize", ringBufferSize},
            // Additional
            {"additionalSinks", sinksArray},
            {"mainThreadName", mainThreadName}
        };
    }

    /**
     * @brief Deserialize configuration from JSON
     */
    [[nodiscard]] static LoggingConfig deserialize(const json& j) {
        LoggingConfig cfg;

        // Console
        cfg.enableConsole = j.value("enableConsole", cfg.enableConsole);
        cfg.consoleLevel = j.value("consoleLevel", cfg.consoleLevel);
        cfg.consoleColor = j.value("consoleColor", cfg.consoleColor);

        // File
        cfg.enableFile = j.value("enableFile", cfg.enableFile);
        cfg.logDir = j.value("logDir", cfg.logDir);
        cfg.logFilename = j.value("logFilename", cfg.logFilename);
        cfg.fileLevel = j.value("fileLevel", cfg.fileLevel);

        // Rotation
        cfg.maxFileSize = j.value("maxFileSize", cfg.maxFileSize);
        cfg.maxFiles = j.value("maxFiles", cfg.maxFiles);
        cfg.useDailyRotation = j.value("useDailyRotation", cfg.useDailyRotation);
        cfg.rotationHour = j.value("rotationHour", cfg.rotationHour);
        cfg.rotationMinute = j.value("rotationMinute", cfg.rotationMinute);

        // Format
        cfg.pattern = j.value("pattern", cfg.pattern);

        // Async
        cfg.asyncMode = j.value("asyncMode", cfg.asyncMode);
        cfg.asyncQueueSize = j.value("asyncQueueSize", cfg.asyncQueueSize);
        cfg.asyncThreadCount = j.value("asyncThreadCount", cfg.asyncThreadCount);
        cfg.overflowPolicy = j.value("overflowPolicy", cfg.overflowPolicy);

        // Ring buffer
        cfg.enableRingBuffer = j.value("enableRingBuffer", cfg.enableRingBuffer);
        cfg.ringBufferSize = j.value("ringBufferSize", cfg.ringBufferSize);

        // Additional sinks
        if (j.contains("additionalSinks") && j["additionalSinks"].is_array()) {
            for (const auto& sinkJson : j["additionalSinks"]) {
                cfg.additionalSinks.push_back(LogSinkConfig::fromJson(sinkJson));
            }
        }

        cfg.mainThreadName = j.value("mainThreadName", cfg.mainThreadName);

        return cfg;
    }

    /**
     * @brief Generate JSON Schema for validation
     */
    [[nodiscard]] static json generateSchema() {
        return {
            {"type", "object"},
            {"properties", {
                // Console
                {"enableConsole", {{"type", "boolean"}, {"default", true}}},
                {"consoleLevel", {
                    {"type", "string"},
                    {"enum", {"trace", "debug", "info", "warn", "error", "critical", "off"}},
                    {"default", "info"}
                }},
                {"consoleColor", {{"type", "boolean"}, {"default", true}}},
                // File
                {"enableFile", {{"type", "boolean"}, {"default", true}}},
                {"logDir", {{"type", "string"}, {"default", "logs"}}},
                {"logFilename", {{"type", "string"}, {"default", "lithium"}}},
                {"fileLevel", {
                    {"type", "string"},
                    {"enum", {"trace", "debug", "info", "warn", "error", "critical", "off"}},
                    {"default", "trace"}
                }},
                // Rotation
                {"maxFileSize", {
                    {"type", "integer"},
                    {"minimum", 1024},
                    {"maximum", 1073741824},  // 1 GB
                    {"default", 10485760}
                }},
                {"maxFiles", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 100},
                    {"default", 5}
                }},
                {"useDailyRotation", {{"type", "boolean"}, {"default", false}}},
                {"rotationHour", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"maximum", 23},
                    {"default", 0}
                }},
                {"rotationMinute", {
                    {"type", "integer"},
                    {"minimum", 0},
                    {"maximum", 59},
                    {"default", 0}
                }},
                // Format
                {"pattern", {{"type", "string"}}},
                // Async
                {"asyncMode", {{"type", "boolean"}, {"default", true}}},
                {"asyncQueueSize", {
                    {"type", "integer"},
                    {"minimum", 128},
                    {"maximum", 1048576},
                    {"default", 8192}
                }},
                {"asyncThreadCount", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 16},
                    {"default", 1}
                }},
                {"overflowPolicy", {
                    {"type", "string"},
                    {"enum", {"block", "discard"}},
                    {"default", "block"}
                }},
                // Ring buffer
                {"enableRingBuffer", {{"type", "boolean"}, {"default", false}}},
                {"ringBufferSize", {
                    {"type", "integer"},
                    {"minimum", 10},
                    {"maximum", 100000},
                    {"default", 1000}
                }},
                // Additional
                {"additionalSinks", {{"type", "array"}}},
                {"mainThreadName", {{"type", "string"}, {"default", "main"}}}
            }}
        };
    }
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_SECTIONS_LOGGING_CONFIG_HPP
