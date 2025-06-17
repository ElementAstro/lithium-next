/*
 * dome_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Client - Main Client Interface

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_DOME_CLIENT_HPP
#define LITHIUM_DEVICE_INDI_DOME_CLIENT_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "components/dome_home.hpp"
#include "components/dome_motion.hpp"
#include "components/dome_parking.hpp"
#include "components/dome_shutter.hpp"
#include "components/dome_telescope.hpp"
#include "components/dome_weather.hpp"
#include "device/template/dome.hpp"

/**
 * @brief INDI Dome Client main class
 *
 * Provides the main interface for dome control, device connection, and
 * component management. Handles INDI client events, device synchronization, and
 * component routing.
 */
class INDIDomeClient : public INDI::BaseClient, public AtomDome {
public:
    /**
     * @brief Construct an INDI Dome Client with a given name.
     * @param name Dome client name.
     */
    explicit INDIDomeClient(std::string name);
    ~INDIDomeClient() override;

    // Non-copyable, non-movable
    INDIDomeClient(const INDIDomeClient& other) = delete;
    INDIDomeClient& operator=(const INDIDomeClient& other) = delete;
    INDIDomeClient(INDIDomeClient&& other) = delete;
    INDIDomeClient& operator=(INDIDomeClient&& other) = delete;

    /**
     * @brief Initialize the dome client and components.
     * @return True if initialized successfully.
     */
    auto initialize() -> bool override;

    /**
     * @brief Destroy the dome client and clean up resources.
     * @return True if destroyed successfully.
     */
    auto destroy() -> bool override;

    /**
     * @brief Connect to the INDI server and device.
     * @param deviceName Name of the device to connect.
     * @param timeout Connection timeout in seconds.
     * @param maxRetry Maximum number of connection attempts.
     * @return True if connected successfully.
     */
    auto connect(const std::string& deviceName, int timeout, int maxRetry)
        -> bool override;

    /**
     * @brief Disconnect from the INDI server and device.
     * @return True if disconnected successfully.
     */
    auto disconnect() -> bool override;

    /**
     * @brief Reconnect to the INDI server and device.
     * @param timeout Connection timeout in seconds.
     * @param maxRetry Maximum number of connection attempts.
     * @return True if reconnected successfully.
     */
    auto reconnect(int timeout, int maxRetry) -> bool;

    /**
     * @brief Scan for available INDI dome devices.
     * @return Vector of device names.
     */
    auto scan() -> std::vector<std::string> override;

    /**
     * @brief Check if the client is connected to the server and device.
     * @return True if connected, false otherwise.
     */
    [[nodiscard]] auto isConnected() const -> bool override;

    // INDI Client interface
    /** @name INDI Client Event Handlers */
    ///@{
    void newDevice(INDI::BaseDevice device) override;
    void removeDevice(INDI::BaseDevice device) override;
    void newProperty(INDI::Property property) override;
    void updateProperty(INDI::Property property) override;
    void removeProperty(INDI::Property property) override;
    void newMessage(INDI::BaseDevice device, int messageID) override;
    void serverConnected() override;
    void serverDisconnected(int exit_code) override;
    ///@}

    // Component access
    /**
     * @brief Get the dome motion manager.
     * @return Shared pointer to DomeMotionManager.
     */
    auto getMotionManager() -> std::shared_ptr<DomeMotionManager> {
        return motion_manager_;
    }
    /**
     * @brief Get the dome shutter manager.
     * @return Shared pointer to DomeShutterManager.
     */
    auto getShutterManager() -> std::shared_ptr<DomeShutterManager> {
        return shutter_manager_;
    }
    /**
     * @brief Get the dome parking manager.
     * @return Shared pointer to DomeParkingManager.
     */
    auto getParkingManager() -> std::shared_ptr<DomeParkingManager> {
        return parking_manager_;
    }
    /**
     * @brief Get the dome weather manager.
     * @return Shared pointer to DomeWeatherManager.
     */
    auto getWeatherManager() -> std::shared_ptr<DomeWeatherManager> {
        return weather_manager_;
    }
    /**
     * @brief Get the dome telescope manager.
     * @return Shared pointer to DomeTelescopeManager.
     */
    auto getTelescopeManager() -> std::shared_ptr<DomeTelescopeManager> {
        return telescope_manager_;
    }
    /**
     * @brief Get the dome home manager.
     * @return Shared pointer to DomeHomeManager.
     */
    auto getHomeManager() -> std::shared_ptr<DomeHomeManager> {
        return home_manager_;
    }

    /**
     * @brief Get the underlying INDI base device.
     * @return Reference to INDI::BaseDevice.
     */
    INDI::BaseDevice& getBaseDevice() { return base_device_; }
    /**
     * @brief Get the current device name.
     * @return Device name string.
     */
    const std::string& getDeviceName() const { return device_name_; }

protected:
    // Component managers
    std::shared_ptr<DomeMotionManager>
        motion_manager_;  ///< Dome motion manager
    std::shared_ptr<DomeShutterManager>
        shutter_manager_;  ///< Dome shutter manager
    std::shared_ptr<DomeParkingManager>
        parking_manager_;  ///< Dome parking manager
    std::shared_ptr<DomeWeatherManager>
        weather_manager_;  ///< Dome weather manager
    std::shared_ptr<DomeTelescopeManager>
        telescope_manager_;                          ///< Dome telescope manager
    std::shared_ptr<DomeHomeManager> home_manager_;  ///< Dome home manager

    // INDI device
    INDI::BaseDevice base_device_;          ///< INDI base device
    std::string device_name_;               ///< Device name
    std::string server_host_{"localhost"};  ///< INDI server host
    int server_port_{7624};                 ///< INDI server port

    // Connection state
    std::atomic<bool> connected_{false};         ///< Server connection state
    std::atomic<bool> device_connected_{false};  ///< Device connection state

    // Threading
    std::mutex state_mutex_;         ///< Mutex for connection state
    std::thread monitoring_thread_;  ///< Monitoring thread
    std::atomic<bool> monitoring_active_{
        false};  ///< Monitoring thread active flag

    // Internal methods
    /**
     * @brief Initialize all component managers and callbacks.
     */
    void initializeComponents();
    /**
     * @brief Monitoring thread function for periodic updates.
     */
    void monitoringThreadFunction();
    /**
     * @brief Wait for server connection with timeout.
     * @param timeout Timeout in seconds.
     * @return True if connected, false otherwise.
     */
    auto waitForConnection(int timeout) -> bool;
    /**
     * @brief Wait for a property to appear with timeout.
     * @param propertyName Property name.
     * @param timeout Timeout in seconds.
     * @return True if property found, false otherwise.
     */
    auto waitForProperty(const std::string& propertyName, int timeout) -> bool;
    /**
     * @brief Update all components from device properties.
     */
    void updateFromDevice();
    /**
     * @brief Route INDI property updates to component managers.
     * @param property The INDI property to process.
     */
    void handleDomeProperty(const INDI::Property& property);
};

#endif  // LITHIUM_DEVICE_INDI_DOME_CLIENT_HPP
