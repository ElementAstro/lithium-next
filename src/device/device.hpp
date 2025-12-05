/**
 * @file device.hpp
 * @brief Unified facade header for the lithium device module.
 *
 * This header provides access to all device module components:
 * - DeviceManager: Core device lifecycle management
 * - Device Templates: Base device interfaces
 * - Device Services: Backend-specific implementations
 *
 * @par Usage Example:
 * @code
 * #include "device/device.hpp"
 *
 * using namespace lithium;
 *
 * // Create device manager
 * auto manager = DeviceManager::createShared();
 *
 * // Connect to INDI backend
 * manager->connectBackend("INDI", "localhost", 7624);
 *
 * // Discover devices
 * auto devices = manager->discoverDevices("INDI");
 *
 * // Subscribe to events
 * manager->subscribeToEvents([](const DeviceEvent& event) {
 *     std::cout << "Event: " << event.message << std::endl;
 * });
 * @endcode
 *
 * @date 2024
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_DEVICE_HPP
#define LITHIUM_DEVICE_HPP

#include <memory>
#include <string>

// Manager module
#include "manager/exceptions.hpp"
#include "manager/manager.hpp"
#include "manager/types.hpp"

// Template module (device interfaces)
#include "template/camera.hpp"
#include "template/device.hpp"
#include "template/dome.hpp"
#include "template/filterwheel.hpp"
#include "template/focuser.hpp"
#include "template/telescope.hpp"

// Service module
#include "service/backend_registry.hpp"
#include "service/device_factory.hpp"
#include "service/device_registry.hpp"
#include "service/device_types.hpp"

namespace lithium {

// ============================================================================
// Module Version
// ============================================================================

/**
 * @brief Device module version.
 */
inline constexpr const char* DEVICE_VERSION = "1.1.0";

/**
 * @brief Get device module version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getDeviceVersion() noexcept {
    return DEVICE_VERSION;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Shared pointer to DeviceManager
using DeviceManagerPtr = std::shared_ptr<DeviceManager>;

/// Shared pointer to AtomDriver (base device)
using DevicePtr = std::shared_ptr<AtomDriver>;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new DeviceManager instance.
 * @return Shared pointer to the new DeviceManager.
 */
[[nodiscard]] inline DeviceManagerPtr createDeviceManager() {
    return DeviceManager::createShared();
}

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Create default DeviceRetryConfig.
 * @return Default DeviceRetryConfig.
 */
[[nodiscard]] inline DeviceRetryConfig createDefaultRetryConfig() {
    return DeviceRetryConfig{};
}

/**
 * @brief Create DeviceRetryConfig with linear strategy.
 * @param maxRetries Maximum number of retries.
 * @param delayMs Delay between retries in milliseconds.
 * @return DeviceRetryConfig with linear strategy.
 */
[[nodiscard]] inline DeviceRetryConfig createLinearRetryConfig(int maxRetries,
                                                               int delayMs) {
    DeviceRetryConfig config;
    config.strategy = DeviceRetryConfig::Strategy::Linear;
    config.maxRetries = maxRetries;
    config.initialDelay = std::chrono::milliseconds(delayMs);
    return config;
}

/**
 * @brief Create DeviceRetryConfig with exponential backoff.
 * @param maxRetries Maximum number of retries.
 * @param initialDelayMs Initial delay in milliseconds.
 * @param multiplier Backoff multiplier.
 * @return DeviceRetryConfig with exponential backoff.
 */
[[nodiscard]] inline DeviceRetryConfig createExponentialRetryConfig(
    int maxRetries, int initialDelayMs, float multiplier = 2.0f) {
    DeviceRetryConfig config;
    config.strategy = DeviceRetryConfig::Strategy::Exponential;
    config.maxRetries = maxRetries;
    config.initialDelay = std::chrono::milliseconds(initialDelayMs);
    config.multiplier = multiplier;
    return config;
}

/**
 * @brief Convert DeviceEventType to string.
 * @param type The event type.
 * @return String representation of the event type.
 */
[[nodiscard]] inline std::string deviceEventTypeToString(DeviceEventType type) {
    switch (type) {
        case DeviceEventType::Connected:
            return "Connected";
        case DeviceEventType::Disconnected:
            return "Disconnected";
        case DeviceEventType::StateChanged:
            return "StateChanged";
        case DeviceEventType::PropertyChanged:
            return "PropertyChanged";
        case DeviceEventType::Error:
            return "Error";
        case DeviceEventType::HealthChanged:
            return "HealthChanged";
        case DeviceEventType::OperationStarted:
            return "OperationStarted";
        case DeviceEventType::OperationCompleted:
            return "OperationCompleted";
        case DeviceEventType::OperationFailed:
            return "OperationFailed";
        default:
            return "Unknown";
    }
}

/**
 * @brief Check if device state indicates connection.
 * @param state The device state.
 * @return True if the device is connected.
 */
[[nodiscard]] inline bool isDeviceConnected(const DeviceState& state) {
    return state.isConnected;
}

/**
 * @brief Check if device state indicates healthy status.
 * @param state The device state.
 * @param threshold Health threshold (default 0.5).
 * @return True if the device is healthy.
 */
[[nodiscard]] inline bool isDeviceHealthy(const DeviceState& state,
                                          float threshold = 0.5f) {
    return state.healthScore >= threshold;
}

/**
 * @brief Check if device state indicates busy status.
 * @param state The device state.
 * @return True if the device is busy.
 */
[[nodiscard]] inline bool isDeviceBusy(const DeviceState& state) {
    return state.isBusy;
}

}  // namespace lithium

#endif  // LITHIUM_DEVICE_HPP
