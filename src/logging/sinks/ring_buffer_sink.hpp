/*
 * ring_buffer_sink.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Custom ring buffer sink for in-memory log storage and streaming

**************************************************/

#ifndef LITHIUM_LOGGING_SINKS_RING_BUFFER_SINK_HPP
#define LITHIUM_LOGGING_SINKS_RING_BUFFER_SINK_HPP

#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

#include "../types.hpp"

namespace lithium::logging {

/**
 * @brief Callback type for real-time log streaming
 */
using LogCallback = std::function<void(const LogEntry&)>;

/**
 * @brief Custom ring buffer sink for in-memory log storage and streaming
 *
 * Features:
 * - Fixed-size circular buffer for log entries
 * - Thread-safe access
 * - Real-time callback notifications for log streaming
 * - Filtering by level, logger name, and time
 */
template <typename Mutex>
class RingBufferSinkMt : public spdlog::sinks::base_sink<Mutex> {
public:
    /**
     * @brief Construct ring buffer sink with specified capacity
     * @param max_items Maximum number of log entries to store
     */
    explicit RingBufferSinkMt(size_t max_items);

    /**
     * @brief Get recent log entries
     * @param count Number of entries to retrieve (0 = all)
     * @return Vector of log entries
     */
    [[nodiscard]] auto getEntries(size_t count = 0) const
        -> std::vector<LogEntry>;

    /**
     * @brief Get log entries since a specific time
     * @param since Time point to filter from
     * @return Vector of log entries after the specified time
     */
    [[nodiscard]] auto getEntriesSince(
        std::chrono::system_clock::time_point since) const
        -> std::vector<LogEntry>;

    /**
     * @brief Get filtered log entries
     * @param level_filter Optional minimum log level
     * @param logger_filter Optional logger name substring filter
     * @param max_count Maximum entries to return
     * @return Vector of matching log entries
     */
    [[nodiscard]] auto getEntriesFiltered(
        std::optional<spdlog::level::level_enum> level_filter,
        std::optional<std::string> logger_filter, size_t max_count = 100) const
        -> std::vector<LogEntry>;

    /**
     * @brief Clear all entries from buffer
     */
    void clear();

    /**
     * @brief Get current number of entries in buffer
     */
    [[nodiscard]] auto size() const -> size_t;

    /**
     * @brief Get buffer capacity
     */
    [[nodiscard]] auto capacity() const -> size_t;

    /**
     * @brief Add a callback for real-time log notifications
     * @param id Unique identifier for the callback
     * @param callback Function to call when new log entry arrives
     */
    void addCallback(const std::string& id, LogCallback callback);

    /**
     * @brief Remove a callback by ID
     * @param id Callback identifier to remove
     */
    void removeCallback(const std::string& id);

    /**
     * @brief Check if a callback exists
     * @param id Callback identifier
     * @return true if callback exists
     */
    [[nodiscard]] auto hasCallback(const std::string& id) const -> bool;

    /**
     * @brief Get number of registered callbacks
     */
    [[nodiscard]] auto callbackCount() const -> size_t;

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override;

private:
    size_t max_items_;
    std::vector<LogEntry> buffer_;
    size_t head_{0};
    size_t count_{0};
    mutable std::mutex buffer_mutex_;
    std::unordered_map<std::string, LogCallback> callbacks_;
    mutable std::mutex callback_mutex_;
};

// Type alias for convenience
using RingBufferSink = RingBufferSinkMt<std::mutex>;

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_SINKS_RING_BUFFER_SINK_HPP
