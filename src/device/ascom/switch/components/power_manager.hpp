/*
 * power_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Power Manager Component

This component manages power consumption monitoring, power limits,
and power-related safety features for ASCOM switch devices.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <atomic>
#include <mutex>
#include <unordered_map>

#include "device/template/switch.hpp"

namespace lithium::device::ascom::sw::components {

// Forward declarations
class SwitchManager;

/**
 * @brief Power consumption data for a switch
 */
struct PowerData {
    uint32_t switch_index;
    double nominal_power{0.0};      // watts when ON
    double standby_power{0.0};      // watts when OFF
    double current_power{0.0};      // current consumption
    std::chrono::steady_clock::time_point last_update;
    bool monitoring_enabled{true};
};

/**
 * @brief Power limit configuration
 */
struct PowerLimit {
    double max_total_power{1000.0}; // watts
    double warning_threshold{0.8};  // 80% of max
    double critical_threshold{0.95}; // 95% of max
    bool enforce_limits{true};
    bool auto_shutdown{false};
};

/**
 * @brief Power Manager Component
 *
 * This component handles power consumption monitoring, limits,
 * and power-related safety features for switch devices.
 */
class PowerManager {
public:
    explicit PowerManager(std::shared_ptr<SwitchManager> switch_manager);
    ~PowerManager() = default;

    // Non-copyable and non-movable
    PowerManager(const PowerManager&) = delete;
    PowerManager& operator=(const PowerManager&) = delete;
    PowerManager(PowerManager&&) = delete;
    PowerManager& operator=(PowerManager&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    auto initialize() -> bool;
    auto destroy() -> bool;
    auto reset() -> bool;

    // =========================================================================
    // Power Monitoring
    // =========================================================================

    auto getTotalPowerConsumption() -> double;
    auto getSwitchPowerConsumption(uint32_t index) -> std::optional<double>;
    auto getSwitchPowerConsumption(const std::string& name) -> std::optional<double>;
    auto updatePowerConsumption() -> bool;
    auto enablePowerMonitoring(bool enable) -> bool;
    auto isPowerMonitoringEnabled() -> bool;

    // =========================================================================
    // Power Configuration
    // =========================================================================

    auto setSwitchPowerData(uint32_t index, double nominalPower, double standbyPower) -> bool;
    auto setSwitchPowerData(const std::string& name, double nominalPower, double standbyPower) -> bool;
    auto getSwitchPowerData(uint32_t index) -> std::optional<PowerData>;
    auto getSwitchPowerData(const std::string& name) -> std::optional<PowerData>;
    auto getAllPowerData() -> std::vector<PowerData>;

    // =========================================================================
    // Power Limits
    // =========================================================================

    auto setPowerLimit(double maxWatts) -> bool;
    auto getPowerLimit() -> double;
    auto setPowerThresholds(double warning, double critical) -> bool;
    auto getPowerThresholds() -> std::pair<double, double>;
    auto enablePowerLimits(bool enforce) -> bool;
    auto arePowerLimitsEnabled() -> bool;
    auto enableAutoShutdown(bool enable) -> bool;
    auto isAutoShutdownEnabled() -> bool;

    // =========================================================================
    // Power Safety
    // =========================================================================

    auto checkPowerLimits() -> bool;
    auto isPowerLimitExceeded() -> bool;
    auto getPowerUtilization() -> double; // percentage of max power
    auto getAvailablePower() -> double;
    auto canSwitchBeActivated(uint32_t index) -> bool;
    auto canSwitchBeActivated(const std::string& name) -> bool;

    // =========================================================================
    // Power Statistics
    // =========================================================================

    auto getTotalEnergyConsumed() -> double; // kWh
    auto getSwitchEnergyConsumed(uint32_t index) -> std::optional<double>;
    auto getSwitchEnergyConsumed(const std::string& name) -> std::optional<double>;
    auto resetEnergyCounters() -> bool;
    auto getPowerHistory(uint32_t samples = 100) -> std::vector<std::pair<std::chrono::steady_clock::time_point, double>>;

    // =========================================================================
    // Emergency Features
    // =========================================================================

    auto emergencyPowerOff() -> bool;
    auto powerOffNonEssentialSwitches() -> bool;
    auto markSwitchAsEssential(uint32_t index, bool essential) -> bool;
    auto markSwitchAsEssential(const std::string& name, bool essential) -> bool;
    auto isSwitchEssential(uint32_t index) -> bool;
    auto isSwitchEssential(const std::string& name) -> bool;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using PowerLimitCallback = std::function<void(double currentPower, double limit, bool exceeded)>;
    using PowerWarningCallback = std::function<void(double currentPower, double threshold)>;
    using EmergencyShutdownCallback = std::function<void(const std::string& reason)>;

    void setPowerLimitCallback(PowerLimitCallback callback);
    void setPowerWarningCallback(PowerWarningCallback callback);
    void setEmergencyShutdownCallback(EmergencyShutdownCallback callback);

    // =========================================================================
    // Error Handling
    // =========================================================================

    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Switch manager reference
    std::shared_ptr<SwitchManager> switch_manager_;

    // Power data
    std::unordered_map<uint32_t, PowerData> power_data_;
    mutable std::mutex power_data_mutex_;

    // Power limits and configuration
    PowerLimit power_limit_;
    mutable std::mutex power_limit_mutex_;

    // Power monitoring
    std::atomic<bool> monitoring_enabled_{true};
    std::atomic<double> total_power_consumption_{0.0};
    std::atomic<double> total_energy_consumed_{0.0}; // kWh
    std::chrono::steady_clock::time_point last_energy_update_;

    // Power history
    std::vector<std::pair<std::chrono::steady_clock::time_point, double>> power_history_;
    mutable std::mutex history_mutex_;
    static constexpr size_t MAX_HISTORY_SIZE = 1000;

    // Essential switches
    std::unordered_map<uint32_t, bool> essential_switches_;
    mutable std::mutex essential_mutex_;

    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;

    // Callbacks
    PowerLimitCallback power_limit_callback_;
    PowerWarningCallback power_warning_callback_;
    EmergencyShutdownCallback emergency_shutdown_callback_;
    std::mutex callback_mutex_;

    // =========================================================================
    // Internal Methods
    // =========================================================================

    auto calculateSwitchPower(uint32_t index) -> double;
    auto updateTotalPowerConsumption() -> void;
    auto updateEnergyConsumption() -> void;
    auto addPowerHistoryEntry(double power) -> void;

    auto validatePowerData(const PowerData& data) -> bool;
    auto validatePowerLimit(double limit) -> bool;
    auto setLastError(const std::string& error) const -> void;

    // Safety checks
    auto checkPowerThresholds() -> void;
    auto executePowerLimitActions() -> void;
    auto executeEmergencyShutdown(const std::string& reason) -> void;

    // Notification helpers
    auto notifyPowerLimitExceeded(double currentPower, double limit) -> void;
    auto notifyPowerWarning(double currentPower, double threshold) -> void;
    auto notifyEmergencyShutdown(const std::string& reason) -> void;

    // Utility methods
    auto findPowerDataByName(const std::string& name) -> std::optional<uint32_t>;
    auto ensurePowerDataExists(uint32_t index) -> bool;
};

} // namespace lithium::device::ascom::sw::components
