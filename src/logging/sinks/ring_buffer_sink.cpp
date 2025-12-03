/*
 * ring_buffer_sink.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "ring_buffer_sink.hpp"

#include <sstream>

#include "../log_statistics.hpp"

namespace lithium::logging {

template <typename Mutex>
RingBufferSinkMt<Mutex>::RingBufferSinkMt(size_t max_items)
    : max_items_(max_items) {
    buffer_.resize(max_items);
}

template <typename Mutex>
void RingBufferSinkMt<Mutex>::sink_it_(const spdlog::details::log_msg& msg) {
    LogEntry entry;
    entry.timestamp = msg.time;
    entry.level = msg.level;
    entry.logger_name =
        std::string(msg.logger_name.data(), msg.logger_name.size());
    entry.message = std::string(msg.payload.data(), msg.payload.size());

    std::ostringstream oss;
    oss << msg.thread_id;
    entry.thread_id = oss.str();

    if (!msg.source.filename) {
        entry.source_file = "";
        entry.source_line = 0;
    } else {
        entry.source_file = msg.source.filename;
        entry.source_line = msg.source.line;
    }

    // Record statistics
    LogStatistics::getInstance().recordMessage(entry.level, entry.logger_name,
                                               entry.message.size());

    // Add to ring buffer
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        buffer_[head_] = entry;
        head_ = (head_ + 1) % max_items_;
        if (count_ < max_items_) {
            ++count_;
        }
    }

    // Notify callbacks
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        for (const auto& [id, callback] : callbacks_) {
            try {
                callback(entry);
            } catch (...) {
                // Ignore callback errors
            }
        }
    }
}

template <typename Mutex>
void RingBufferSinkMt<Mutex>::flush_() {
    // Nothing to flush for in-memory buffer
}

template <typename Mutex>
auto RingBufferSinkMt<Mutex>::getEntries(size_t count) const
    -> std::vector<LogEntry> {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    size_t actual_count = (count == 0 || count > count_) ? count_ : count;
    std::vector<LogEntry> result;
    result.reserve(actual_count);

    if (count_ == 0) {
        return result;
    }

    // Calculate start position
    size_t start = (head_ + max_items_ - actual_count) % max_items_;

    for (size_t i = 0; i < actual_count; ++i) {
        size_t idx = (start + i) % max_items_;
        result.push_back(buffer_[idx]);
    }

    return result;
}

template <typename Mutex>
auto RingBufferSinkMt<Mutex>::getEntriesSince(
    std::chrono::system_clock::time_point since) const
    -> std::vector<LogEntry> {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    std::vector<LogEntry> result;
    if (count_ == 0) {
        return result;
    }

    size_t start = (head_ + max_items_ - count_) % max_items_;

    for (size_t i = 0; i < count_; ++i) {
        size_t idx = (start + i) % max_items_;
        if (buffer_[idx].timestamp >= since) {
            result.push_back(buffer_[idx]);
        }
    }

    return result;
}

template <typename Mutex>
auto RingBufferSinkMt<Mutex>::getEntriesFiltered(
    std::optional<spdlog::level::level_enum> level_filter,
    std::optional<std::string> logger_filter, size_t max_count) const
    -> std::vector<LogEntry> {
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    std::vector<LogEntry> result;
    if (count_ == 0) {
        return result;
    }

    size_t start = (head_ + max_items_ - count_) % max_items_;

    for (size_t i = 0; i < count_ && result.size() < max_count; ++i) {
        size_t idx = (start + i) % max_items_;
        const auto& entry = buffer_[idx];

        bool matches = true;
        if (level_filter && entry.level < *level_filter) {
            matches = false;
        }
        if (logger_filter &&
            entry.logger_name.find(*logger_filter) == std::string::npos) {
            matches = false;
        }

        if (matches) {
            result.push_back(entry);
        }
    }

    return result;
}

template <typename Mutex>
void RingBufferSinkMt<Mutex>::clear() {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    head_ = 0;
    count_ = 0;
}

template <typename Mutex>
auto RingBufferSinkMt<Mutex>::size() const -> size_t {
    std::lock_guard<std::mutex> lock(buffer_mutex_);
    return count_;
}

template <typename Mutex>
auto RingBufferSinkMt<Mutex>::capacity() const -> size_t {
    return max_items_;
}

template <typename Mutex>
void RingBufferSinkMt<Mutex>::addCallback(const std::string& id,
                                          LogCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callbacks_[id] = std::move(callback);
}

template <typename Mutex>
void RingBufferSinkMt<Mutex>::removeCallback(const std::string& id) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    callbacks_.erase(id);
}

template <typename Mutex>
auto RingBufferSinkMt<Mutex>::hasCallback(const std::string& id) const -> bool {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    return callbacks_.contains(id);
}

template <typename Mutex>
auto RingBufferSinkMt<Mutex>::callbackCount() const -> size_t {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    return callbacks_.size();
}

// Explicit template instantiation
template class RingBufferSinkMt<std::mutex>;

}  // namespace lithium::logging
