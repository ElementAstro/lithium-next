/*
 * resource_monitor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_ISOLATED_RESOURCE_MONITOR_HPP
#define LITHIUM_SCRIPT_ISOLATED_RESOURCE_MONITOR_HPP

#include "types.hpp"

#include <optional>

namespace lithium::isolated {

/**
 * @brief Resource monitoring utilities for subprocess
 */
class ResourceMonitor {
public:
    /**
     * @brief Get current memory usage of a process
     * @param processId Process ID to query
     * @return Memory usage in bytes or nullopt if unavailable
     */
    [[nodiscard]] static std::optional<size_t> getMemoryUsage(int processId);

    /**
     * @brief Get current CPU usage of a process
     * @param processId Process ID to query
     * @return CPU usage percentage (0-100) or nullopt if unavailable
     */
    [[nodiscard]] static std::optional<double> getCpuUsage(int processId);

    /**
     * @brief Check if process exceeds memory limit
     * @param processId Process ID to check
     * @param limitMB Memory limit in megabytes
     * @return True if limit exceeded
     */
    [[nodiscard]] static bool isMemoryLimitExceeded(int processId, size_t limitMB);

    /**
     * @brief Get peak memory usage of a process
     * @param processId Process ID to query
     * @return Peak memory usage in bytes or nullopt if unavailable
     */
    [[nodiscard]] static std::optional<size_t> getPeakMemoryUsage(int processId);
};

}  // namespace lithium::isolated

#endif  // LITHIUM_SCRIPT_ISOLATED_RESOURCE_MONITOR_HPP
