/*
 * device_config_adapter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: Adapter to convert unified DeviceConfig to legacy component configs

**************************************************/

#ifndef LITHIUM_CONFIG_ADAPTERS_DEVICE_CONFIG_ADAPTER_HPP
#define LITHIUM_CONFIG_ADAPTERS_DEVICE_CONFIG_ADAPTER_HPP

#include "../sections/device_config.hpp"
#include "device/manager.hpp"

namespace lithium::config {

/**
 * @brief Convert unified RetryConfig to DeviceManager's DeviceRetryConfig
 */
[[nodiscard]] inline DeviceRetryConfig toDeviceRetryConfig(
    const config::RetryConfig& unified) {
    DeviceRetryConfig config;

    // Convert strategy string to enum
    if (unified.strategy == "none") {
        config.strategy = DeviceRetryConfig::Strategy::None;
    } else if (unified.strategy == "linear") {
        config.strategy = DeviceRetryConfig::Strategy::Linear;
    } else {
        config.strategy = DeviceRetryConfig::Strategy::Exponential;
    }

    config.maxRetries = unified.maxRetries;
    config.initialDelay = std::chrono::milliseconds(unified.initialDelayMs);
    config.maxDelay = std::chrono::milliseconds(unified.maxDelayMs);
    config.multiplier = unified.multiplier;

    return config;
}

/**
 * @brief Convert DeviceManager's DeviceRetryConfig to unified RetryConfig
 */
[[nodiscard]] inline config::RetryConfig fromDeviceRetryConfig(
    const DeviceRetryConfig& legacy) {
    config::RetryConfig config;

    // Convert enum to string
    switch (legacy.strategy) {
        case DeviceRetryConfig::Strategy::None:
            config.strategy = "none";
            break;
        case DeviceRetryConfig::Strategy::Linear:
            config.strategy = "linear";
            break;
        case DeviceRetryConfig::Strategy::Exponential:
            config.strategy = "exponential";
            break;
    }

    config.maxRetries = legacy.maxRetries;
    config.initialDelayMs = legacy.initialDelay.count();
    config.maxDelayMs = legacy.maxDelay.count();
    config.multiplier = legacy.multiplier;

    return config;
}

/**
 * @brief Helper class to apply DeviceConfig to DeviceManager
 */
class DeviceConfigApplier {
public:
    /**
     * @brief Apply unified DeviceConfig to a DeviceManager instance
     */
    static void apply(DeviceManager& manager, const DeviceConfig& config) {
        // Apply default retry config for new devices
        // The DeviceManager will use this for devices that don't have
        // a specific retry config set
        defaultRetryConfig_ = toDeviceRetryConfig(config.retry);

        // Start health monitoring if enabled
        if (config.health.enabled) {
            manager.startHealthMonitor(
                std::chrono::seconds(config.health.checkIntervalSeconds));
        }
    }

    /**
     * @brief Get the default retry config
     */
    static const DeviceRetryConfig& getDefaultRetryConfig() {
        return defaultRetryConfig_;
    }

private:
    static inline DeviceRetryConfig defaultRetryConfig_;
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_ADAPTERS_DEVICE_CONFIG_ADAPTER_HPP
