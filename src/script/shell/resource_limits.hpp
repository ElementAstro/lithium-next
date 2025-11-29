/*
 * resource_limits.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file resource_limits.hpp
 * @brief Resource limit management for script execution
 * @date 2024-1-13
 * @version 2.1.0
 */

#ifndef LITHIUM_SCRIPT_SHELL_RESOURCE_LIMITS_HPP
#define LITHIUM_SCRIPT_SHELL_RESOURCE_LIMITS_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace lithium::shell {

/**
 * @brief Resource usage statistics
 */
struct ResourceUsage {
    size_t currentMemoryMB{0};        ///< Current memory usage
    double cpuPercent{0.0};           ///< Current CPU percentage
    size_t runningScripts{0};         ///< Number of running scripts
    size_t totalScripts{0};           ///< Total registered scripts
    size_t outputSizeBytes{0};        ///< Accumulated output size
};

/**
 * @brief Manages resource limits and monitors usage
 *
 * Thread-safe resource manager that tracks:
 * - Memory usage
 * - CPU percentage
 * - Concurrent script count
 * - Output size limits
 */
class ResourceManager {
public:
    /**
     * @brief Construct with default limits
     */
    ResourceManager();

    /**
     * @brief Construct with custom limits
     * @param maxMemoryMB Maximum memory in MB
     * @param maxCpuPercent Maximum CPU percentage
     * @param maxExecutionTime Maximum execution time
     * @param maxOutputSize Maximum output size in bytes
     * @param maxConcurrent Maximum concurrent scripts
     */
    ResourceManager(size_t maxMemoryMB,
                    int maxCpuPercent,
                    std::chrono::seconds maxExecutionTime,
                    size_t maxOutputSize,
                    int maxConcurrent);

    ~ResourceManager();

    // Non-copyable, movable
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept;
    ResourceManager& operator=(ResourceManager&&) noexcept;

    /**
     * @brief Check if resources are available for new execution
     * @return true if resources are available
     */
    [[nodiscard]] auto canExecute() const -> bool;

    /**
     * @brief Acquire resources for script execution
     * @return true if acquired successfully
     */
    auto acquire() -> bool;

    /**
     * @brief Release resources after execution
     */
    void release();

    /**
     * @brief Get current resource usage
     * @return ResourceUsage statistics
     */
    [[nodiscard]] auto getUsage() const -> ResourceUsage;

    /**
     * @brief Get usage as a map (for compatibility)
     * @return Map of resource name to value
     */
    [[nodiscard]] auto getUsageMap() const
        -> std::unordered_map<std::string, double>;

    /**
     * @brief Set maximum memory limit
     * @param megabytes Memory limit in MB
     */
    void setMaxMemory(size_t megabytes);

    /**
     * @brief Get maximum memory limit
     * @return Memory limit in MB
     */
    [[nodiscard]] auto getMaxMemory() const -> size_t;

    /**
     * @brief Set maximum CPU percentage
     * @param percent CPU percentage (0-100)
     */
    void setMaxCpuPercent(int percent);

    /**
     * @brief Get maximum CPU percentage
     * @return CPU percentage limit
     */
    [[nodiscard]] auto getMaxCpuPercent() const -> int;

    /**
     * @brief Set maximum execution time
     * @param duration Max execution duration
     */
    void setMaxExecutionTime(std::chrono::seconds duration);

    /**
     * @brief Get maximum execution time
     * @return Max execution duration
     */
    [[nodiscard]] auto getMaxExecutionTime() const -> std::chrono::seconds;

    /**
     * @brief Set maximum output size
     * @param bytes Max output size in bytes
     */
    void setMaxOutputSize(size_t bytes);

    /**
     * @brief Get maximum output size
     * @return Max output size in bytes
     */
    [[nodiscard]] auto getMaxOutputSize() const -> size_t;

    /**
     * @brief Set maximum concurrent scripts
     * @param count Max concurrent scripts
     */
    void setMaxConcurrent(int count);

    /**
     * @brief Get maximum concurrent scripts
     * @return Max concurrent script count
     */
    [[nodiscard]] auto getMaxConcurrent() const -> int;

    /**
     * @brief Get current running script count
     * @return Number of running scripts
     */
    [[nodiscard]] auto getRunningCount() const -> size_t;

    /**
     * @brief Update memory usage (call periodically for monitoring)
     * @param megabytes Current memory usage
     */
    void updateMemoryUsage(size_t megabytes);

    /**
     * @brief Update CPU usage
     * @param percent Current CPU percentage
     */
    void updateCpuUsage(double percent);

    /**
     * @brief Check if memory limit exceeded
     * @return true if over limit
     */
    [[nodiscard]] auto isMemoryExceeded() const -> bool;

    /**
     * @brief Check if CPU limit exceeded
     * @return true if over limit
     */
    [[nodiscard]] auto isCpuExceeded() const -> bool;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::shell

#endif  // LITHIUM_SCRIPT_SHELL_RESOURCE_LIMITS_HPP
