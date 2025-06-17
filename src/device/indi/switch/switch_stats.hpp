/*
 * switch_stats.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Stats - Statistics Tracking Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_SWITCH_STATS_HPP
#define LITHIUM_DEVICE_INDI_SWITCH_STATS_HPP

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations
class INDISwitchClient;

/**
 * @class SwitchStats
 * @brief Switch statistics tracking component for INDI switches.
 *
 * This class provides functionality for tracking switch operation counts,
 * uptime, and managing statistics for INDI switches. It supports querying
 * statistics by switch index or name, resetting statistics, and updating
 * statistics on switch state changes. Thread safety is ensured via internal
 * mutex locking.
 */
class SwitchStats {
public:
    /**
     * @brief Construct a new SwitchStats object.
     * @param client Pointer to the associated INDISwitchClient.
     */
    explicit SwitchStats(INDISwitchClient* client);
    /**
     * @brief Destroy the SwitchStats object.
     */
    ~SwitchStats() = default;

    /**
     * @brief Get the operation count for a switch by index.
     * @param index The switch index.
     * @return The number of operations performed on the switch.
     */
    auto getSwitchOperationCount(uint32_t index) -> uint64_t;
    /**
     * @brief Get the operation count for a switch by name.
     * @param name The switch name.
     * @return The number of operations performed on the switch.
     */
    auto getSwitchOperationCount(const std::string& name) -> uint64_t;
    /**
     * @brief Get the uptime (in milliseconds) for a switch by index.
     * @param index The switch index.
     * @return The total uptime in milliseconds for the switch.
     */
    auto getSwitchUptime(uint32_t index) -> uint64_t;
    /**
     * @brief Get the uptime (in milliseconds) for a switch by name.
     * @param name The switch name.
     * @return The total uptime in milliseconds for the switch.
     */
    auto getSwitchUptime(const std::string& name) -> uint64_t;
    /**
     * @brief Get the total operation count for all switches.
     * @return The total number of operations performed on all switches.
     */
    auto getTotalOperationCount() -> uint64_t;

    /**
     * @brief Reset all switch statistics (operation counts and uptimes).
     * @return True if successful, false otherwise.
     */
    auto resetStatistics() -> bool;
    /**
     * @brief Reset statistics for a specific switch by index.
     * @param index The switch index.
     * @return True if successful, false otherwise.
     */
    auto resetSwitchStatistics(uint32_t index) -> bool;
    /**
     * @brief Reset statistics for a specific switch by name.
     * @param name The switch name.
     * @return True if successful, false otherwise.
     */
    auto resetSwitchStatistics(const std::string& name) -> bool;

    /**
     * @brief Update statistics for a switch when its state changes.
     * @param index The switch index.
     * @param switchedOn True if the switch was turned ON, false if turned OFF.
     */
    void updateStatistics(uint32_t index, bool switchedOn);
    /**
     * @brief Increment the operation count for a switch.
     * @param index The switch index.
     */
    void trackSwitchOperation(uint32_t index);
    /**
     * @brief Start uptime tracking for a switch (called when switch turns ON).
     * @param index The switch index.
     */
    void startSwitchUptime(uint32_t index);
    /**
     * @brief Stop uptime tracking for a switch (called when switch turns OFF).
     * @param index The switch index.
     */
    void stopSwitchUptime(uint32_t index);

private:
    INDISwitchClient* client_;  ///< Pointer to the associated INDISwitchClient.
    mutable std::mutex
        stats_mutex_;  ///< Mutex for thread-safe access to statistics.

    // Statistics data
    std::vector<uint64_t>
        switch_operation_counts_;  ///< Operation counts for each switch.
    std::vector<uint64_t> switch_uptimes_;  ///< Uptime (ms) for each switch.
    std::vector<std::chrono::steady_clock::time_point>
        switch_on_times_;  ///< Last ON time for each switch.
    uint64_t total_operation_count_{
        0};  ///< Total operation count for all switches.

    /**
     * @brief Ensure internal vectors are large enough for the given index.
     * @param index The switch index.
     */
    void ensureVectorSize(uint32_t index);
};

#endif  // LITHIUM_DEVICE_INDI_SWITCH_STATS_HPP
