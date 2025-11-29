/*
 * logger_registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Logger registry for managing named loggers

**************************************************/

#ifndef LITHIUM_LOGGING_LOGGER_REGISTRY_HPP
#define LITHIUM_LOGGING_LOGGER_REGISTRY_HPP

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include "types.hpp"

namespace lithium::logging {

/**
 * @brief Registry for managing named loggers
 *
 * Provides thread-safe access to loggers with:
 * - Logger creation and retrieval
 * - Level and pattern management
 * - Sink attachment
 */
class LoggerRegistry {
public:
    /**
     * @brief Get or create a logger by name
     * @param name Logger name
     * @param sinks Sinks to attach to new logger
     * @param default_level Default level for new logger
     * @param default_pattern Default pattern for new logger
     * @return Shared pointer to logger
     */
    auto getOrCreate(const std::string& name,
                     const std::vector<spdlog::sink_ptr>& sinks,
                     spdlog::level::level_enum default_level,
                     const std::string& default_pattern)
        -> std::shared_ptr<spdlog::logger>;

    /**
     * @brief Get existing logger by name
     * @param name Logger name
     * @return Logger if exists, nullptr otherwise
     */
    [[nodiscard]] auto get(const std::string& name) const
        -> std::shared_ptr<spdlog::logger>;

    /**
     * @brief Check if logger exists
     * @param name Logger name
     * @return true if logger exists
     */
    [[nodiscard]] auto exists(const std::string& name) const -> bool;

    /**
     * @brief Remove a logger
     * @param name Logger name
     * @return true if removed, false if not found or protected
     */
    auto remove(const std::string& name) -> bool;

    /**
     * @brief List all registered loggers
     * @return Vector of logger info
     */
    [[nodiscard]] auto list() const -> std::vector<LoggerInfo>;

    /**
     * @brief Set level for a specific logger
     * @param name Logger name
     * @param level New level
     * @return true if successful
     */
    auto setLevel(const std::string& name, spdlog::level::level_enum level)
        -> bool;

    /**
     * @brief Set level for all loggers
     * @param level New level
     */
    void setGlobalLevel(spdlog::level::level_enum level);

    /**
     * @brief Set pattern for a specific logger
     * @param name Logger name
     * @param pattern New pattern
     * @return true if successful
     */
    auto setPattern(const std::string& name, const std::string& pattern)
        -> bool;

    /**
     * @brief Get pattern for a logger
     * @param name Logger name
     * @return Pattern string, or empty if not found
     */
    [[nodiscard]] auto getPattern(const std::string& name) const -> std::string;

    /**
     * @brief Add sink to all loggers
     * @param sink Sink to add
     */
    void addSinkToAll(const spdlog::sink_ptr& sink);

    /**
     * @brief Remove sink from all loggers
     * @param sink Sink to remove
     */
    void removeSinkFromAll(const spdlog::sink_ptr& sink);

    /**
     * @brief Flush all loggers
     */
    void flushAll();

    /**
     * @brief Clear all loggers (except default)
     */
    void clear();

    /**
     * @brief Get count of registered loggers
     */
    [[nodiscard]] auto count() const -> size_t;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> patterns_;
};

}  // namespace lithium::logging

#endif  // LITHIUM_LOGGING_LOGGER_REGISTRY_HPP
