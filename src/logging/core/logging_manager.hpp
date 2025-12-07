/*
 * logging_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Central Logging Manager - orchestrates all logging components

**************************************************/

#ifndef LITHIUM_LOGGING_LOGGING_MANAGER_HPP
#define LITHIUM_LOGGING_LOGGING_MANAGER_HPP

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "../sinks/ring_buffer_sink.hpp"
#include "../sinks/sink_factory.hpp"
#include "../utils/log_exporter.hpp"
#include "../utils/log_statistics.hpp"
#include "logger_registry.hpp"
#include "types.hpp"

namespace lithium::logging {

/**
 * @brief Central logging manager with spdlog integration
 *
 * Provides:
 * - Named logger management via LoggerRegistry
 * - Runtime level/pattern configuration
 * - Multiple sink support via SinkFactory
 * - In-memory log buffer via RingBufferSink for HTTP/WebSocket streaming
 * - Thread-safe operations
 *
 * This is the main entry point for the logging system.
 */
class LoggingManager {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> LoggingManager&;

    /**
     * @brief Initialize logging system with configuration
     * @param config Logging configuration
     */
    void initialize(const LoggingConfig& config);

    /**
     * @brief Shutdown logging system gracefully
     */
    void shutdown();

    /**
     * @brief Check if manager is initialized
     */
    [[nodiscard]] auto isInitialized() const -> bool;

    // ========== Logger Management ==========

    /**
     * @brief Get or create a named logger
     * @param name Logger name
     * @return Shared pointer to logger
     */
    auto getLogger(const std::string& name) -> std::shared_ptr<spdlog::logger>;

    /**
     * @brief List all registered loggers
     * @return Vector of logger info
     */
    [[nodiscard]] auto listLoggers() const -> std::vector<LoggerInfo>;

    /**
     * @brief Set log level for a specific logger
     * @param name Logger name
     * @param level New level
     * @return true if successful
     */
    auto setLoggerLevel(const std::string& name,
                        spdlog::level::level_enum level) -> bool;

    /**
     * @brief Set log level for all loggers
     * @param level New level
     */
    void setGlobalLevel(spdlog::level::level_enum level);

    /**
     * @brief Set pattern for a specific logger
     * @param name Logger name
     * @param pattern New pattern
     * @return true if successful
     */
    auto setLoggerPattern(const std::string& name, const std::string& pattern)
        -> bool;

    /**
     * @brief Remove a logger
     * @param name Logger name
     * @return true if removed
     */
    auto removeLogger(const std::string& name) -> bool;

    // ========== Sink Management ==========

    /**
     * @brief Add a new sink
     * @param config Sink configuration
     * @return true if added
     */
    auto addSink(const SinkConfig& config) -> bool;

    /**
     * @brief Remove a sink by name
     * @param name Sink name
     * @return true if removed
     */
    auto removeSink(const std::string& name) -> bool;

    /**
     * @brief List all sinks
     * @return Vector of sink configs
     */
    [[nodiscard]] auto listSinks() const -> std::vector<SinkConfig>;

    // ========== Log Buffer Operations ==========

    /**
     * @brief Get recent log entries from ring buffer
     * @param count Maximum entries to return (0 = all)
     * @return Vector of log entries
     */
    [[nodiscard]] auto getRecentLogs(size_t count = 100) const
        -> std::vector<LogEntry>;

    /**
     * @brief Get log entries since a specific time
     * @param since Time point to filter from
     * @return Vector of log entries
     */
    [[nodiscard]] auto getLogsSince(std::chrono::system_clock::time_point since)
        const -> std::vector<LogEntry>;

    /**
     * @brief Get filtered log entries
     * @param level Optional minimum level filter
     * @param logger_name Optional logger name filter
     * @param max_count Maximum entries to return
     * @return Vector of matching log entries
     */
    [[nodiscard]] auto getLogsFiltered(
        std::optional<spdlog::level::level_enum> level,
        std::optional<std::string> logger_name, size_t max_count = 100) const
        -> std::vector<LogEntry>;

    /**
     * @brief Clear the log buffer
     */
    void clearLogBuffer();

    /**
     * @brief Get buffer statistics
     * @return JSON with size, capacity, usage_percent
     */
    [[nodiscard]] auto getBufferStats() const -> nlohmann::json;

    // ========== Real-time Streaming ==========

    /**
     * @brief Subscribe to real-time log stream
     * @param subscriber_id Unique subscriber identifier
     * @param callback Function to call for each log entry
     */
    void subscribe(const std::string& subscriber_id, LogCallback callback);

    /**
     * @brief Unsubscribe from log stream
     * @param subscriber_id Subscriber identifier
     */
    void unsubscribe(const std::string& subscriber_id);

    // ========== Utility ==========

    /**
     * @brief Flush all loggers
     */
    void flush();

    /**
     * @brief Trigger log rotation (for file sinks)
     */
    void rotate();

    /**
     * @brief Get current configuration
     * @return Current logging config
     */
    [[nodiscard]] auto getConfig() const -> LoggingConfig;

    // ========== Statistics ==========

    /**
     * @brief Get log statistics
     * @return Reference to statistics collector
     */
    [[nodiscard]] auto getStatistics() -> LogStatistics&;

    /**
     * @brief Get statistics summary as JSON
     * @return JSON with statistics summary
     */
    [[nodiscard]] auto getStatsSummary() const -> nlohmann::json;

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

    // ========== Search ==========

    /**
     * @brief Search logs with query
     * @param query Search query parameters
     * @return Search result with matching entries
     */
    [[nodiscard]] auto searchLogs(const LogSearchQuery& query) const
        -> LogSearchResult;

    // ========== Export ==========

    /**
     * @brief Export logs to string
     * @param options Export options
     * @param count Number of recent logs to export (0 = all in buffer)
     * @return Export result with content
     */
    [[nodiscard]] auto exportLogs(const ExportOptions& options,
                                  size_t count = 0) const -> ExportResult;

    /**
     * @brief Export logs to file
     * @param file_path Output file path
     * @param options Export options
     * @param count Number of recent logs to export (0 = all in buffer)
     * @return Export result
     */
    [[nodiscard]] auto exportLogsToFile(const std::string& file_path,
                                        const ExportOptions& options,
                                        size_t count = 0) const -> ExportResult;

    // ========== Level Conversion Helpers ==========

    /**
     * @brief Convert level string to enum
     */
    [[nodiscard]] static auto levelFromString(const std::string& level)
        -> spdlog::level::level_enum;

    /**
     * @brief Convert level enum to string
     */
    [[nodiscard]] static auto levelToString(spdlog::level::level_enum level)
        -> std::string;

private:
    LoggingManager() = default;
    ~LoggingManager();

    LoggingManager(const LoggingManager&) = delete;
    LoggingManager& operator=(const LoggingManager&) = delete;

    void setupDefaultLogger();

    mutable std::shared_mutex mutex_;
    LoggingConfig config_;
    bool initialized_{false};

    LoggerRegistry registry_;
    std::shared_ptr<RingBufferSink> ring_buffer_sink_;
    std::unordered_map<std::string, spdlog::sink_ptr> sinks_;
};

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_LOGGING_MANAGER_HPP
