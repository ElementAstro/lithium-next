/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Hardware Interface Component

This component provides a clean interface to INDI telescope devices,
handling low-level INDI communication, device management,
and property updates.

*************************************************/

#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <memory>

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "device/template/telescope.hpp"

namespace lithium::device::indi::telescope::components {

/**
 * @brief Hardware Interface for INDI Telescope communication
 *
 * This component encapsulates all direct interaction with INDI devices,
 * providing a clean C++ interface for hardware operations while managing
 * device lifecycle, property management, and low-level telescope control.
 */
class HardwareInterface : public INDI::BaseClient {
public:
    struct TelescopeInfo {
        std::string deviceName;
        std::string driverExec;
        std::string driverVersion;
        std::string driverInterface;
        uint32_t capabilities = 0;
        bool isConnected = false;
    };

    struct PropertyInfo {
        std::string propertyName;
        std::string deviceName;
        std::string label;
        std::string group;
        IPState state = IPS_IDLE;
        IPerm permission = IP_RW;
        double timeout = 0.0;
    };

    // Callback types
    using ConnectionCallback = std::function<void(bool connected)>;
    using PropertyUpdateCallback = std::function<void(const std::string& propertyName, const INDI::Property& property)>;
    using MessageCallback = std::function<void(const std::string& message, int messageID)>;

public:
    HardwareInterface();
    ~HardwareInterface() override;

    // Non-copyable and non-movable
    HardwareInterface(const HardwareInterface&) = delete;
    HardwareInterface& operator=(const HardwareInterface&) = delete;
    HardwareInterface(HardwareInterface&&) = delete;
    HardwareInterface& operator=(HardwareInterface&&) = delete;

    // Connection Management
    bool initialize();
    bool shutdown();
    bool connectToDevice(const std::string& deviceName, int timeout = 30000);
    bool disconnectFromDevice();
    bool isConnected() const { return connected_; }
    bool isInitialized() const { return initialized_; }

    // Device Discovery
    std::vector<std::string> scanDevices();
    std::optional<TelescopeInfo> getTelescopeInfo() const;
    std::string getCurrentDeviceName() const { return deviceName_; }

    // Property Management
    bool waitForProperty(const std::string& propertyName, int timeout = 5000);
    std::vector<PropertyInfo> getAvailableProperties() const;
    
    // Number Properties
    bool setNumberProperty(const std::string& propertyName, const std::string& elementName, double value);
    bool setNumberProperty(const std::string& propertyName, const std::vector<std::pair<std::string, double>>& values);
    std::optional<double> getNumberProperty(const std::string& propertyName, const std::string& elementName) const;
    std::optional<std::vector<double>> getNumberProperty(const std::string& propertyName) const;

    // Switch Properties
    bool setSwitchProperty(const std::string& propertyName, const std::string& elementName, bool value);
    bool setSwitchProperty(const std::string& propertyName, const std::vector<std::pair<std::string, bool>>& values);
    std::optional<bool> getSwitchProperty(const std::string& propertyName, const std::string& elementName) const;
    std::optional<std::vector<bool>> getSwitchProperty(const std::string& propertyName) const;

    // Text Properties
    bool setTextProperty(const std::string& propertyName, const std::string& elementName, const std::string& value);
    bool setTextProperty(const std::string& propertyName, const std::vector<std::pair<std::string, std::string>>& values);
    std::optional<std::string> getTextProperty(const std::string& propertyName, const std::string& elementName) const;

    // Convenience Methods for Common Properties
    bool setTargetCoordinates(double ra, double dec);
    bool setTelescopeAction(const std::string& action); // "SLEW", "TRACK", "SYNC", "ABORT"
    bool setMotionDirection(const std::string& direction, bool enable); // "MOTION_NORTH", "MOTION_SOUTH", etc.
    bool setParkAction(bool park);
    bool setTrackingState(bool enabled);
    bool setTrackingMode(const std::string& mode);

    std::optional<std::pair<double, double>> getCurrentCoordinates() const;
    std::optional<std::pair<double, double>> getTargetCoordinates() const;
    std::optional<std::string> getTelescopeState() const;
    bool isTracking() const;
    bool isParked() const;
    bool isSlewing() const;

    // Callback Registration
    void setConnectionCallback(ConnectionCallback callback) { connectionCallback_ = std::move(callback); }
    void setPropertyUpdateCallback(PropertyUpdateCallback callback) { propertyUpdateCallback_ = std::move(callback); }
    void setMessageCallback(MessageCallback callback) { messageCallback_ = std::move(callback); }

protected:
    // INDI BaseClient virtual methods
    void newDevice(INDI::BaseDevice baseDevice) override;
    void removeDevice(INDI::BaseDevice baseDevice) override;
    void newProperty(INDI::Property property) override;
    void updateProperty(INDI::Property property) override;
    void removeProperty(INDI::Property property) override;
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;
    void serverConnected() override;
    void serverDisconnected(int exit_code) override;

private:
    // Internal state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> serverConnected_{false};
    
    std::string deviceName_;
    INDI::BaseDevice device_;
    
    // Thread safety
    mutable std::recursive_mutex propertyMutex_;
    mutable std::recursive_mutex deviceMutex_;
    
    // Callbacks
    ConnectionCallback connectionCallback_;
    PropertyUpdateCallback propertyUpdateCallback_;
    MessageCallback messageCallback_;
    
    // Internal methods
    bool waitForConnection(int timeout);
    void updateDeviceInfo();
    void handlePropertyUpdate(const INDI::Property& property);
    
    // Property helpers
    INDI::PropertyNumber getNumberPropertyHandle(const std::string& propertyName) const;
    INDI::PropertySwitch getSwitchPropertyHandle(const std::string& propertyName) const;
    INDI::PropertyText getTextPropertyHandle(const std::string& propertyName) const;
    
    // Utility methods
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);
};

} // namespace lithium::device::indi::telescope::components
