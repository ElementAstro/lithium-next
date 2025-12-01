/*
 * manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device Manager - Core device management interface

**************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "exceptions.hpp"
#include "types.hpp"

#include "device/template/device.hpp"

namespace lithium {

// Forward declaration
class DeviceManagerImpl;

/**
 * @class DeviceManager
 * @brief Manages the lifecycle and operations of devices in the system.
 *
 * The DeviceManager is responsible for:
 * - Device registration and lifecycle management
 * - Backend connection management (INDI, ASCOM)
 * - Device discovery and auto-registration
 * - Event handling and health monitoring
 * - Async device operations
 */
class DeviceManager {
public:
    /**
     * @brief Constructs a new DeviceManager object.
     */
    DeviceManager();

    /**
     * @brief Destroys the DeviceManager object.
     */
    ~DeviceManager();

    // Disable copy
    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;

    /**
     * @brief Creates a shared pointer to a new DeviceManager.
     * @return A shared pointer to a new DeviceManager.
     */
    static auto createShared() -> std::shared_ptr<DeviceManager>;

    // ==================== Device Registration ====================

    /**
     * @brief Add a device to the manager
     * @param type Device type (e.g., "camera", "telescope")
     * @param device Device instance
     */
    void addDevice(const std::string& type,
                   std::shared_ptr<AtomDriver> device);

    /**
     * @brief Add a device with metadata
     * @param type Device type
     * @param device Device instance
     * @param metadata Device metadata
     */
    void addDeviceWithMetadata(const std::string& type,
                               std::shared_ptr<AtomDriver> device,
                               const DeviceMetadata& metadata);

    /**
     * @brief Remove a device from the manager
     * @param type Device type
     * @param device Device instance
     */
    void removeDevice(const std::string& type,
                      std::shared_ptr<AtomDriver> device);

    /**
     * @brief Remove a device by name
     * @param name Device name
     */
    void removeDeviceByName(const std::string& name);

    /**
     * @brief Remove all devices of a type
     * @param type Device type
     */
    void removeAllDevicesOfType(const std::string& type);

    // ==================== Device Access ====================

    /**
     * @brief Get all devices
     * @return Map of type to device list
     */
    auto getDevices() const
        -> std::unordered_map<std::string,
                              std::vector<std::shared_ptr<AtomDriver>>>;

    /**
     * @brief Get devices of a specific type
     * @param type Device type
     * @return Vector of devices
     */
    auto getDevicesByType(const std::string& type) const
        -> std::vector<std::shared_ptr<AtomDriver>>;

    /**
     * @brief Get device by name
     * @param name Device name
     * @return Device instance or nullptr
     */
    auto getDeviceByName(const std::string& name) const
        -> std::shared_ptr<AtomDriver>;

    /**
     * @brief Get primary device of a type
     * @param type Device type
     * @return Primary device or nullptr
     */
    auto getPrimaryDevice(const std::string& type) const
        -> std::shared_ptr<AtomDriver>;

    /**
     * @brief Set primary device for a type
     * @param type Device type
     * @param device Device to set as primary
     */
    void setPrimaryDevice(const std::string& type,
                          std::shared_ptr<AtomDriver> device);

    /**
     * @brief Get device type by name
     * @param name Device name
     * @return Device type or empty string
     */
    auto getDeviceType(const std::string& name) const -> std::string;

    // ==================== Device Metadata ====================

    /**
     * @brief Get device metadata
     * @param name Device name
     * @return Device metadata or nullopt
     */
    auto getDeviceMetadata(const std::string& name) const
        -> std::optional<DeviceMetadata>;

    /**
     * @brief Update device metadata
     * @param name Device name
     * @param metadata New metadata
     */
    void updateDeviceMetadata(const std::string& name,
                              const DeviceMetadata& metadata);

    /**
     * @brief Get device state
     * @param name Device name
     * @return Device state
     */
    auto getDeviceState(const std::string& name) const -> DeviceState;

    // ==================== Device Connection ====================

    /**
     * @brief Connect to a device by name
     * @param name Device name
     * @param timeout Connection timeout in ms
     * @return true if connected
     */
    bool connectDeviceByName(const std::string& name, int timeout = 5000);

    /**
     * @brief Disconnect from a device by name
     * @param name Device name
     * @return true if disconnected
     */
    bool disconnectDeviceByName(const std::string& name);

    /**
     * @brief Connect to a device asynchronously
     * @param name Device name
     * @param timeout Connection timeout in ms
     * @return Future with connection result
     */
    auto connectDeviceAsync(const std::string& name, int timeout = 5000)
        -> std::future<bool>;

    /**
     * @brief Disconnect from a device asynchronously
     * @param name Device name
     * @return Future with disconnection result
     */
    auto disconnectDeviceAsync(const std::string& name) -> std::future<bool>;

    // ==================== Event System ====================

    /**
     * @brief Subscribe to device events
     * @param callback Event callback
     * @return Callback ID for unsubscription
     */
    auto subscribeToEvents(DeviceEventCallback callback) -> EventCallbackId;

    /**
     * @brief Subscribe to specific event types
     * @param callback Event callback
     * @param eventTypes Event types to subscribe to
     * @return Callback ID
     */
    auto subscribeToEvents(DeviceEventCallback callback,
                           const std::vector<DeviceEventType>& eventTypes)
        -> EventCallbackId;

    /**
     * @brief Unsubscribe from events
     * @param callbackId Callback ID
     */
    void unsubscribeFromEvents(EventCallbackId callbackId);

    /**
     * @brief Set legacy event callback (deprecated)
     * @param callback Event callback
     */
    void setEventCallback(DeviceEventCallback callback);

    // ==================== Backend Management ====================

    /**
     * @brief Connect to a backend server
     * @param backend Backend name ("INDI", "ASCOM")
     * @param host Server host
     * @param port Server port (0 for default)
     * @return true if connected
     */
    bool connectBackend(const std::string& backend,
                        const std::string& host = "localhost", int port = 0);

    /**
     * @brief Disconnect from a backend server
     * @param backend Backend name
     * @return true if disconnected
     */
    bool disconnectBackend(const std::string& backend);

    /**
     * @brief Check if backend is connected
     * @param backend Backend name
     * @return true if connected
     */
    bool isBackendConnected(const std::string& backend) const;

    /**
     * @brief Get backend status
     * @return JSON with all backend status
     */
    json getBackendStatus() const;

    // ==================== Device Discovery ====================

    /**
     * @brief Discover available devices
     * @param backend Backend name ("INDI", "ASCOM", "ALL")
     * @return Vector of discovered device metadata
     */
    auto discoverDevices(const std::string& backend = "ALL")
        -> std::vector<DeviceMetadata>;

    /**
     * @brief Discover and register devices
     * @param backend Backend name
     * @param autoConnect Whether to auto-connect
     * @return Number of devices registered
     */
    int discoverAndRegisterDevices(const std::string& backend = "ALL",
                                   bool autoConnect = false);

    /**
     * @brief Refresh device list from backends
     */
    void refreshDevices();

    // ==================== Health Monitoring ====================

    /**
     * @brief Start health monitor
     * @param interval Check interval
     */
    void startHealthMonitor(
        std::chrono::seconds interval = std::chrono::seconds(30));

    /**
     * @brief Stop health monitor
     */
    void stopHealthMonitor();

    /**
     * @brief Check health of all devices
     * @return JSON with health report
     */
    json checkAllDevicesHealth();

    /**
     * @brief Get device health score
     * @param name Device name
     * @return Health score (0.0 to 1.0)
     */
    float getDeviceHealth(const std::string& name) const;

    /**
     * @brief Get unhealthy devices
     * @param threshold Health threshold
     * @return Vector of unhealthy device names
     */
    auto getUnhealthyDevices(float threshold = 0.5f) const
        -> std::vector<std::string>;

    // ==================== Device Validation ====================

    /**
     * @brief Check if device is valid
     * @param name Device name
     * @return true if valid
     */
    bool isDeviceValid(const std::string& name) const;

    /**
     * @brief Set retry configuration for a device
     * @param name Device name
     * @param config Retry configuration
     */
    void setDeviceRetryConfig(const std::string& name,
                              const DeviceRetryConfig& config);

    /**
     * @brief Get retry configuration for a device
     * @param name Device name
     * @return Retry configuration
     */
    auto getDeviceRetryConfig(const std::string& name) const
        -> DeviceRetryConfig;

    /**
     * @brief Abort device operation
     * @param name Device name
     */
    void abortDeviceOperation(const std::string& name);

    // ==================== Additional Methods ====================

    /**
     * @brief Connect all devices
     */
    void connectAllDevices();

    /**
     * @brief Disconnect all devices
     */
    void disconnectAllDevices();

    /**
     * @brief Connect devices by type
     */
    void connectDevicesByType(const std::string& type);

    /**
     * @brief Disconnect devices by type
     */
    void disconnectDevicesByType(const std::string& type);

    /**
     * @brief Find device by name (alias for getDeviceByName)
     */
    auto findDeviceByName(const std::string& name) const
        -> std::shared_ptr<AtomDriver>;

    /**
     * @brief Check if device is connected
     */
    bool isDeviceConnected(const std::string& name) const;

    /**
     * @brief Initialize device
     */
    bool initializeDevice(const std::string& name);

    /**
     * @brief Destroy device
     */
    bool destroyDevice(const std::string& name);

    /**
     * @brief Scan devices of type
     */
    auto scanDevices(const std::string& type) -> std::vector<std::string>;

    /**
     * @brief Reset device state
     */
    void resetDevice(const std::string& name);

    /**
     * @brief Update device health
     */
    void updateDeviceHealth(const std::string& name, bool operationSuccess);

    /**
     * @brief Get devices with state
     */
    auto getDevicesWithState(const std::string& type) const
        -> std::vector<std::pair<std::shared_ptr<AtomDriver>, DeviceState>>;

    /**
     * @brief Find devices by driver name
     */
    auto findDevicesByDriver(const std::string& driverName) const
        -> std::vector<std::shared_ptr<AtomDriver>>;

    /**
     * @brief Get device by ID
     */
    auto getDeviceById(const std::string& deviceId) const
        -> std::shared_ptr<AtomDriver>;

    /**
     * @brief Register event callback (legacy)
     */
    void registerEventCallback(DeviceEventCallback callback);

    /**
     * @brief Unregister event callback (legacy)
     */
    void unregisterEventCallback();

    /**
     * @brief Get pending events
     */
    auto getPendingEvents(size_t maxEvents = 100) -> std::vector<DeviceEvent>;

    /**
     * @brief Clear pending events
     */
    void clearPendingEvents();

    /**
     * @brief Export configuration
     */
    json exportConfiguration() const;

    /**
     * @brief Import configuration
     */
    void importConfiguration(const json& config);

    /**
     * @brief Execute with retry
     */
    auto executeWithRetry(
        const std::string& name,
        std::function<bool(std::shared_ptr<AtomDriver>)> operation,
        const std::string& operationName = "")
        -> std::future<DeviceOperationResult>;

    /**
     * @brief Connect devices batch
     */
    auto connectDevicesBatch(const std::vector<std::string>& names,
                             int timeoutMs = 5000)
        -> std::vector<std::pair<std::string, bool>>;

    /**
     * @brief Disconnect devices batch
     */
    auto disconnectDevicesBatch(const std::vector<std::string>& names)
        -> std::vector<std::pair<std::string, bool>>;

    // ==================== Status & Statistics ====================

    /**
     * @brief Get manager status
     * @return JSON with status
     */
    json getStatus() const;

    /**
     * @brief Get operation statistics
     * @return JSON with statistics
     */
    json getStatistics() const;

    /**
     * @brief Reset statistics
     */
    void resetStatistics();

private:
    std::unique_ptr<DeviceManagerImpl> pimpl_;
};

}  // namespace lithium
