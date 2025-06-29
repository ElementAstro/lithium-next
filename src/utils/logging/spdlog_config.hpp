/*
 * spdlog_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Global spdlog configuration for high-performance logging
with C++23 optimizations

**************************************************/

#pragma once

#include <memory>
#include <string_view>
#include <atomic>
#include <chrono>
#include <format>
#include <coroutine>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/stopwatch.h>
#include <spdlog/fmt/ostr.h>

#include "atom/type/json.hpp"

namespace lithium::logging {

// C++20 concept for type safety
template<typename T>
concept LoggableType = requires(T&& t) {
    { std::format("{}", std::forward<T>(t)) } -> std::convertible_to<std::string>;
};

// C++23 enum class with underlying type specification
enum class LogLevel : std::uint8_t {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

struct LoggerConfig {
    std::string name{};
    LogLevel level = LogLevel::INFO;
    std::string pattern = "[%H:%M:%S.%e] [%^%l%$] [%n] %v";
    bool async = true;
    std::size_t queue_size = 8192;
    std::size_t thread_count = 1;
    bool console_output = true;
    bool file_output = true;
    std::string log_file_path = "logs/lithium.log";
    std::size_t max_file_size = 1048576 * 10;  // 10MB
    std::size_t max_files = 5;
    bool flush_on_error = true;
    std::chrono::seconds flush_interval{3};
};

/**
 * @brief High-performance spdlog configuration with C++23 features
 */
class LogConfig {
public:

    /**
     * @brief Initialize global spdlog configuration
     * @param config Logger configuration
     */
    static void initialize(const LoggerConfig& config = LoggerConfig{});

    /**
     * @brief Get or create logger with optimized settings
     * @param name Logger name
     * @param config Optional custom configuration
     * @return Shared pointer to logger
     */
    static auto getLogger(std::string_view name, 
                         const LoggerConfig& config = LoggerConfig{}) 
        -> std::shared_ptr<spdlog::logger>;

    /**
     * @brief Create high-performance async logger
     * @param name Logger name  
     * @param config Logger configuration
     * @return Async logger instance
     */
    static auto createAsyncLogger(std::string_view name,
                                 const LoggerConfig& config) 
        -> std::shared_ptr<spdlog::async_logger>;

    /**
     * @brief Set global log level
     * @param level New log level
     */
    static void setGlobalLevel(LogLevel level) noexcept;

    /**
     * @brief Flush all loggers
     */
    static void flushAll() noexcept;

    /**
     * @brief Get performance metrics
     * @return JSON object with metrics
     */
    static auto getMetrics() noexcept -> nlohmann::json;

    /**
     * @brief Create timed scope logger for performance measurement
     * @param logger Logger to use
     * @param scope_name Name of the scope
     * @return RAII scope timer
     */
    template<LoggableType... Args>
    static auto createScopeTimer(std::shared_ptr<spdlog::logger> logger,
                                std::string_view scope_name,
                                Args&&... args) {
        return ScopeTimer{logger, scope_name, std::forward<Args>(args)...};
    }

    // C++20 coroutine support for async logging
    struct AsyncLogAwaitable {
        std::shared_ptr<spdlog::logger> logger;
        std::string message;
        LogLevel level;

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) noexcept;
        void await_resume() const noexcept {}
    };

    /**
     * @brief Async log operation (C++20 coroutine)
     * @param logger Logger instance
     * @param level Log level
     * @param message Message to log
     * @return Awaitable for coroutine
     */
    static auto asyncLog(std::shared_ptr<spdlog::logger> logger,
                        LogLevel level,
                        std::string message) -> AsyncLogAwaitable;

private:
    // RAII scope timer for performance measurement
    class ScopeTimer {
    public:
        template<LoggableType... Args>
        ScopeTimer(std::shared_ptr<spdlog::logger> logger,
                  std::string_view scope_name,
                  Args&&... args)
            : logger_(std::move(logger))
            , scope_name_(scope_name)
            , start_time_(std::chrono::high_resolution_clock::now()) {
            if constexpr (sizeof...(args) > 0) {
                logger_->debug("Entering scope: {} with args: {}", 
                              scope_name_, 
                              std::format("{}", std::forward<Args>(args)...));
            } else {
                logger_->debug("Entering scope: {}", scope_name_);
            }
        }

        ~ScopeTimer() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time_);
            logger_->debug("Exiting scope: {} [{}μs]", scope_name_, duration.count());
        }

        // Non-copyable, movable
        ScopeTimer(const ScopeTimer&) = delete;
        ScopeTimer& operator=(const ScopeTimer&) = delete;
        ScopeTimer(ScopeTimer&&) = default;
        ScopeTimer& operator=(ScopeTimer&&) = default;

    private:
        std::shared_ptr<spdlog::logger> logger_;
        std::string scope_name_;
        std::chrono::high_resolution_clock::time_point start_time_;
    };

    static inline std::atomic<bool> initialized_{false};
    static inline std::atomic<LogLevel> global_level_{LogLevel::INFO};
    
    // Performance metrics
    static inline std::atomic<std::uint64_t> total_logs_{0};
    static inline std::atomic<std::uint64_t> error_count_{0};
    
    static auto convertLevel(LogLevel level) noexcept -> spdlog::level::level_enum;
    static auto convertLevel(spdlog::level::level_enum level) noexcept -> LogLevel;
};

// Convenience macros for high-performance logging
#define LITHIUM_LOG_TRACE(logger, ...) \
    if (logger && logger->should_log(spdlog::level::trace)) { \
        logger->trace(__VA_ARGS__); \
    }

#define LITHIUM_LOG_DEBUG(logger, ...) \
    if (logger && logger->should_log(spdlog::level::debug)) { \
        logger->debug(__VA_ARGS__); \
    }

#define LITHIUM_LOG_INFO(logger, ...) \
    if (logger && logger->should_log(spdlog::level::info)) { \
        logger->info(__VA_ARGS__); \
    }

#define LITHIUM_LOG_WARN(logger, ...) \
    if (logger && logger->should_log(spdlog::level::warn)) { \
        logger->warn(__VA_ARGS__); \
    }

#define LITHIUM_LOG_ERROR(logger, ...) \
    if (logger && logger->should_log(spdlog::level::err)) { \
        logger->error(__VA_ARGS__); \
        ::lithium::logging::LogConfig::error_count_.fetch_add(1, std::memory_order_relaxed); \
    }

#define LITHIUM_LOG_CRITICAL(logger, ...) \
    if (logger && logger->should_log(spdlog::level::critical)) { \
        logger->critical(__VA_ARGS__); \
        ::lithium::logging::LogConfig::error_count_.fetch_add(1, std::memory_order_relaxed); \
    }

// RAII scope timer macro
#define LITHIUM_SCOPE_TIMER(logger, scope_name, ...) \
    auto _scope_timer = ::lithium::logging::LogConfig::createScopeTimer( \
        logger, scope_name, ##__VA_ARGS__)

}  // namespace lithium::logging
