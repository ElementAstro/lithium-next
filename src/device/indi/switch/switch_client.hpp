/*
 * switch_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Client - Main Client Interface

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_SWITCH_CLIENT_HPP
#define LITHIUM_DEVICE_INDI_SWITCH_CLIENT_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "device/template/switch.hpp"
#include "switch_manager.hpp"
#include "switch_persistence.hpp"
#include "switch_power.hpp"
#include "switch_safety.hpp"
#include "switch_stats.hpp"
#include "switch_timer.hpp"

/**
 * @class INDISwitchClient
 * @brief Main client interface for INDI switch devices.
 *
 * This class manages the connection to INDI devices, handles device and
 * property events, and provides access to various switch-related managers
 * (timing, power, safety, stats, persistence). It is thread-safe and designed
 * for robust astrophotography automation.
 */
class INDISwitchClient : public INDI::BaseClient, public AtomSwitch {
public:
    /**
     * @brief Construct a new INDISwitchClient object.
     * @param name The name of the client/device.
     */
    explicit INDISwitchClient(std::string name);

    /**
     * @brief Destroy the INDISwitchClient object and release resources.
     */
    ~INDISwitchClient() override;

    // Non-copyable, non-movable
    INDISwitchClient(const INDISwitchClient& other) = delete;
    INDISwitchClient& operator=(const INDISwitchClient& other) = delete;
    INDISwitchClient(INDISwitchClient&& other) = delete;
    INDISwitchClient& operator=(INDISwitchClient&& other) = delete;

    // Base device interface

    /**
     * @brief Initialize the client and connect to the INDI server.
     * @return true if initialization succeeded, false otherwise.
     */
    auto initialize() -> bool override;

    /**
     * @brief Destroy the client and disconnect from the INDI server.
     * @return true if destruction succeeded, false otherwise.
     */
    auto destroy() -> bool override;

    /**
     * @brief Connect to a specific INDI device.
     * @param deviceName Name of the device to connect.
     * @param timeout Timeout in seconds for connection.
     * @param maxRetry Maximum number of connection retries.
     * @return true if connected, false otherwise.
     */
    auto connect(const std::string& deviceName, int timeout, int maxRetry)
        -> bool override;

    /**
     * @brief Disconnect from the current INDI device.
     * @return true if disconnected, false otherwise.
     */
    auto disconnect() -> bool override;

    /**
     * @brief Reconnect to the INDI device with retries.
     * @param timeout Timeout in seconds for each attempt.
     * @param maxRetry Maximum number of retries.
     * @return true if reconnected, false otherwise.
     */
    auto reconnect(int timeout, int maxRetry) -> bool;

    /**
     * @brief Scan for available INDI devices.
     * @return Vector of device names found.
     */
    auto scan() -> std::vector<std::string> override;

    /**
     * @brief Check if the client is connected to the INDI server.
     * @return true if connected, false otherwise.
     */
    [[nodiscard]] auto isConnected() const -> bool override;

    // INDI Client interface

    /**
     * @brief Handle a new device detected by the INDI server.
     * @param device The new INDI device.
     */
    void newDevice(INDI::BaseDevice device) override;

    /**
     * @brief Handle removal of a device from the INDI server.
     * @param device The removed INDI device.
     */
    void removeDevice(INDI::BaseDevice device) override;

    /**
     * @brief Handle a new property reported by the INDI server.
     * @param property The new INDI property.
     */
    void newProperty(INDI::Property property) override;

    /**
     * @brief Handle an updated property from the INDI server.
     * @param property The updated INDI property.
     */
    void updateProperty(INDI::Property property) override;

    /**
     * @brief Handle removal of a property from the INDI server.
     * @param property The removed INDI property.
     */
    void removeProperty(INDI::Property property) override;

    /**
     * @brief Handle a new message from the INDI server.
     * @param device The device associated with the message.
     * @param messageID The message identifier.
     */
    void newMessage(INDI::BaseDevice device, int messageID) override;

    /**
     * @brief Called when the client successfully connects to the INDI server.
     */
    void serverConnected() override;

    /**
     * @brief Called when the client disconnects from the INDI server.
     * @param exit_code The exit code for the disconnection.
     */
    void serverDisconnected(int exit_code) override;

    // Component access

    /**
     * @brief Get the switch manager component.
     * @return Shared pointer to SwitchManager.
     */
    auto getSwitchManager() -> std::shared_ptr<SwitchManager> {
        return switch_manager_;
    }

    /**
     * @brief Get the timer manager component.
     * @return Shared pointer to SwitchTimer.
     */
    auto getTimerManager() -> std::shared_ptr<SwitchTimer> {
        return timer_manager_;
    }

    /**
     * @brief Get the power manager component.
     * @return Shared pointer to SwitchPower.
     */
    auto getPowerManager() -> std::shared_ptr<SwitchPower> {
        return power_manager_;
    }

    /**
     * @brief Get the safety manager component.
     * @return Shared pointer to SwitchSafety.
     */
    auto getSafetyManager() -> std::shared_ptr<SwitchSafety> {
        return safety_manager_;
    }

    /**
     * @brief Get the statistics manager component.
     * @return Shared pointer to SwitchStats.
     */
    auto getStatsManager() -> std::shared_ptr<SwitchStats> {
        return stats_manager_;
    }

    /**
     * @brief Get the persistence manager component.
     * @return Shared pointer to SwitchPersistence.
     */
    auto getPersistenceManager() -> std::shared_ptr<SwitchPersistence> {
        return persistence_manager_;
    }

    // Device access for components

    /**
     * @brief Get the underlying INDI base device.
     * @return Reference to INDI::BaseDevice.
     */
    INDI::BaseDevice& getBaseDevice() { return base_device_; }

    /**
     * @brief Get the name of the connected device.
     * @return Device name as a string.
     */
    const std::string& getDeviceName() const { return device_name_; }

protected:
    // Component managers
    std::shared_ptr<SwitchManager>
        switch_manager_;                          ///< Switch manager component.
    std::shared_ptr<SwitchTimer> timer_manager_;  ///< Timer manager component.
    std::shared_ptr<SwitchPower> power_manager_;  ///< Power manager component.
    std::shared_ptr<SwitchSafety>
        safety_manager_;  ///< Safety manager component.
    std::shared_ptr<SwitchStats>
        stats_manager_;  ///< Statistics manager component.
    std::shared_ptr<SwitchPersistence>
        persistence_manager_;  ///< Persistence manager component.

    // INDI device
    INDI::BaseDevice base_device_;          ///< The underlying INDI device.
    std::string device_name_;               ///< Name of the connected device.
    std::string server_host_{"localhost"};  ///< INDI server host.
    int server_port_{7624};                 ///< INDI server port.

    // Connection state
    std::atomic<bool> connected_{false};  ///< True if connected to INDI server.
    std::atomic<bool> device_connected_{
        false};  ///< True if device is connected.

    // Threading
    std::mutex state_mutex_;         ///< Mutex for thread-safe state changes.
    std::thread monitoring_thread_;  ///< Thread for device monitoring.
    std::atomic<bool> monitoring_active_{
        false};  ///< True if monitoring is active.

    // Internal methods

    /**
     * @brief Initialize all component managers.
     */
    void initializeComponents();

    /**
     * @brief Function executed by the monitoring thread.
     */
    void monitoringThreadFunction();

    /**
     * @brief Wait for the client to connect to the INDI server.
     * @param timeout Timeout in seconds.
     * @return true if connection is established, false otherwise.
     */
    auto waitForConnection(int timeout) -> bool;

    /**
     * @brief Wait for a specific property to become available.
     * @param propertyName Name of the property.
     * @param timeout Timeout in seconds.
     * @return true if property is available, false otherwise.
     */
    auto waitForProperty(const std::string& propertyName, int timeout) -> bool;

    /**
     * @brief Update internal state from the connected device.
     */
    void updateFromDevice();

    /**
     * @brief Handle an incoming switch property from the INDI server.
     * @param property The INDI property to handle.
     */
    void handleSwitchProperty(const INDI::Property& property);
};

#endif  // LITHIUM_DEVICE_INDI_SWITCH_CLIENT_HPP
