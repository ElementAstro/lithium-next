/*
 * spdlog_config.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Global spdlog configuration implementation

**************************************************/

#include "spdlog_config.hpp"

#include <unordered_map>
#include <shared_mutex>
#include <filesystem>
#include <thread>

#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/callback_sink.h>

#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace lithium::logging {

namespace {
    // Thread-safe logger registry with heterogeneous lookup
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>,
                       std::hash<std::string_view>, std::equal_to<>> logger_registry_;
    std::shared_mutex registry_mutex_;
}

void LogConfig::initialize(const LoggerConfig& config) {
    if (initialized_.exchange(true, std::memory_order_acq_rel)) {
        return;  // Already initialized
    }

    try {
        // Create logs directory if it doesn't exist
        std::filesystem::create_directories(
            std::filesystem::path(config.log_file_path).parent_path());

        // Initialize thread pool for async logging with optimized settings
        if (config.async) {
            spdlog::init_thread_pool(config.queue_size, config.thread_count);
        }

        // Set global log level
        setGlobalLevel(config.level);

        // Configure periodic flushing for all thread-safe loggers
        spdlog::flush_every(config.flush_interval);

        // Create default logger
        auto default_logger = getLogger("lithium", config);
        spdlog::set_default_logger(default_logger);

        // Set global error handler
        spdlog::set_error_handler([](const std::string& msg) {
            error_count_.fetch_add(1, std::memory_order_relaxed);
            // Fallback to stderr if default logger fails
            std::fprintf(stderr, "spdlog error: %s\n", msg.c_str());
        });

        LITHIUM_LOG_INFO(default_logger,
                        "High-performance logging initialized with C++23 optimizations");

    } catch (const std::exception& e) {
        std::fprintf(stderr, "Failed to initialize logging: %s\n", e.what());
        initialized_.store(false, std::memory_order_release);
        throw;
    }
}

auto LogConfig::getLogger(std::string_view name, const LoggerConfig& config)
    -> std::shared_ptr<spdlog::logger> {

    std::string nameStr{name}; // Convert to string for map lookup

    // Fast path: check with shared lock first
    {
        std::shared_lock lock(registry_mutex_);
        if (auto it = logger_registry_.find(nameStr); it != logger_registry_.end()) {
            return it->second;
        }
    }

    // Slow path: create new logger with unique lock
    std::unique_lock lock(registry_mutex_);

    // Double-check pattern
    if (auto it = logger_registry_.find(nameStr); it != logger_registry_.end()) {
        return it->second;
    }

    try {
        std::shared_ptr<spdlog::logger> logger;

        if (config.async) {
            logger = createAsyncLogger(name, config);
        } else {
            // Create sinks
            std::vector<spdlog::sink_ptr> sinks;

            if (config.console_output) {
                auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
                console_sink->set_level(convertLevel(config.level));
                console_sink->set_pattern(config.pattern);
                sinks.push_back(console_sink);
            }

            if (config.file_output) {
                auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    config.log_file_path, config.max_file_size, config.max_files);
                file_sink->set_level(spdlog::level::trace);  // Log everything to file
                file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] [%n] %v");
                sinks.push_back(file_sink);
            }

            // Add callback sink for error monitoring
            auto callback_sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
                [](const spdlog::details::log_msg& msg) {
                    total_logs_.fetch_add(1, std::memory_order_relaxed);
                    if (msg.level >= spdlog::level::err) {
                        error_count_.fetch_add(1, std::memory_order_relaxed);
                    }
                });
            callback_sink->set_level(spdlog::level::trace);
            sinks.push_back(callback_sink);

            logger = std::make_shared<spdlog::logger>(std::string{name},
                                                     sinks.begin(), sinks.end());
        }

        logger->set_level(convertLevel(config.level));

        if (config.flush_on_error) {
            logger->flush_on(spdlog::level::err);
        }

        // Register logger
        spdlog::register_logger(logger);
        logger_registry_.emplace(nameStr, logger);

        return logger;

    } catch (const std::exception& e) {
        throw std::runtime_error(std::format("Failed to create logger '{}': {}",
                                            name, e.what()));
    }
}

auto LogConfig::createAsyncLogger(std::string_view name, const LoggerConfig& config)
    -> std::shared_ptr<spdlog::async_logger> {

    try {
        // Create sinks
        std::vector<spdlog::sink_ptr> sinks;

        if (config.console_output) {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(convertLevel(config.level));
            console_sink->set_pattern(config.pattern);
            sinks.push_back(console_sink);
        }

        if (config.file_output) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.log_file_path, config.max_file_size, config.max_files);
            file_sink->set_level(spdlog::level::trace);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [thread %t] [%n] %v");
            sinks.push_back(file_sink);
        }

        // Add callback sink for metrics
        auto callback_sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
            [](const spdlog::details::log_msg& msg) {
                total_logs_.fetch_add(1, std::memory_order_relaxed);
                if (msg.level >= spdlog::level::err) {
                    error_count_.fetch_add(1, std::memory_order_relaxed);
                }
            });
        callback_sink->set_level(spdlog::level::trace);
        sinks.push_back(callback_sink);

        // Create async logger with optimized overflow policy
        auto logger = std::make_shared<spdlog::async_logger>(
            std::string{name},
            sinks.begin(),
            sinks.end(),
            spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        return logger;

    } catch (const std::exception& e) {
        throw std::runtime_error(std::format("Failed to create async logger '{}': {}",
                                            name, e.what()));
    }
}

void LogConfig::setGlobalLevel(LogLevel level) noexcept {
    global_level_.store(level, std::memory_order_release);
    spdlog::set_level(convertLevel(level));
}

void LogConfig::flushAll() noexcept {
    try {
        spdlog::apply_all([](std::shared_ptr<spdlog::logger> l) {
            l->flush();
        });
    } catch (...) {
        // Ignore flush errors
    }
}

auto LogConfig::getMetrics() noexcept -> json {
    json metrics;
    try {
        metrics["total_logs"] = total_logs_.load(std::memory_order_relaxed);
        metrics["error_count"] = error_count_.load(std::memory_order_relaxed);
        metrics["global_level"] = static_cast<int>(global_level_.load(std::memory_order_relaxed));
        metrics["initialized"] = initialized_.load(std::memory_order_relaxed);

        std::shared_lock lock(registry_mutex_);
        metrics["registered_loggers"] = logger_registry_.size();

        std::vector<std::string> logger_names;
        logger_names.reserve(logger_registry_.size());
        for (const auto& [name, logger] : logger_registry_) {
            logger_names.push_back(name);
        }
        metrics["logger_names"] = std::move(logger_names);

    } catch (...) {
        metrics["error"] = "Failed to collect metrics";
    }
    return metrics;
}

auto LogConfig::asyncLog(std::shared_ptr<spdlog::logger> logger,
                        LogLevel level,
                        std::string message) -> AsyncLogAwaitable {
    return AsyncLogAwaitable{std::move(logger), std::move(message), level};
}

void LogConfig::AsyncLogAwaitable::await_suspend(std::coroutine_handle<> handle) noexcept {
    // Schedule async logging
    std::thread([this, handle]() mutable {
        try {
            switch (level) {
                case LogLevel::TRACE:
                    logger->trace(message);
                    break;
                case LogLevel::DEBUG:
                    logger->debug(message);
                    break;
                case LogLevel::INFO:
                    logger->info(message);
                    break;
                case LogLevel::WARN:
                    logger->warn(message);
                    break;
                case LogLevel::ERROR:
                    logger->error(message);
                    break;
                case LogLevel::CRITICAL:
                    logger->critical(message);
                    break;
                case LogLevel::OFF:
                    break;
            }
        } catch (...) {
            // Ignore logging errors in async context
        }
        handle.resume();
    }).detach();
}

auto LogConfig::convertLevel(LogLevel level) noexcept -> spdlog::level::level_enum {
    switch (level) {
        case LogLevel::TRACE: return spdlog::level::trace;
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO: return spdlog::level::info;
        case LogLevel::WARN: return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::OFF: return spdlog::level::off;
        default: return spdlog::level::info;
    }
}

auto LogConfig::convertLevel(spdlog::level::level_enum level) noexcept -> LogLevel {
    switch (level) {
        case spdlog::level::trace: return LogLevel::TRACE;
        case spdlog::level::debug: return LogLevel::DEBUG;
        case spdlog::level::info: return LogLevel::INFO;
        case spdlog::level::warn: return LogLevel::WARN;
        case spdlog::level::err: return LogLevel::ERROR;
        case spdlog::level::critical: return LogLevel::CRITICAL;
        case spdlog::level::off: return LogLevel::OFF;
        default: return LogLevel::INFO;
    }
}

}  // namespace lithium::logging
