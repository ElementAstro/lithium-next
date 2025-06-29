/*
 * switch_power.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Power - Power Management Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_SWITCH_POWER_HPP
#define LITHIUM_DEVICE_INDI_SWITCH_POWER_HPP

#include <functional>
#include <mutex>
#include <optional>
#include <string>

// Forward declarations
class INDISwitchClient;

/**
 * @brief Switch power management component
 *
 * Handles power monitoring, consumption tracking, and power limits
 */
class SwitchPower {
public:
    explicit SwitchPower(INDISwitchClient* client);
    ~SwitchPower() = default;

    // Power monitoring
    auto getSwitchPowerConsumption(uint32_t index) -> std::optional<double>;
    auto getSwitchPowerConsumption(const std::string& name) -> std::optional<double>;
    auto getTotalPowerConsumption() -> double;

    // Power limits
    auto setPowerLimit(double maxWatts) -> bool;
    auto getPowerLimit() -> double;
    auto isPowerLimitExceeded() -> bool;

    // Power management
    void updatePowerConsumption();
    void checkPowerLimits();

    // Power callback registration
    using PowerCallback = std::function<void(double totalPower, bool limitExceeded)>;
    void setPowerCallback(PowerCallback callback);

private:
    INDISwitchClient* client_;
    mutable std::mutex power_mutex_;

    // Power tracking
    double total_power_consumption_{0.0};
    double power_limit_{1000.0}; // Default 1000W limit

    // Power callback
    PowerCallback power_callback_;

    // Internal methods
    void notifyPowerEvent(double totalPower, bool limitExceeded);
};

#endif // LITHIUM_DEVICE_INDI_SWITCH_POWER_HPP
