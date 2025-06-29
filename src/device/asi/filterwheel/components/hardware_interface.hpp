/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Filter Wheel Hardware Interface Component

This component handles the low-level communication with ASI EFW hardware,
providing an abstraction layer over the EFW SDK.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <optional>

namespace lithium::device::asi::filterwheel::components {

/**
 * @brief Hardware interface for ASI Filter Wheel devices
 *
 * This component provides a high-level interface to the EFW SDK,
 * handling device discovery, connection, and basic hardware operations.
 */
class HardwareInterface {
public:
    /**
     * @brief Device information structure
     */
    struct DeviceInfo {
        int id;
        std::string name;
        int slotCount;
        std::string firmwareVersion;
        std::string driverVersion;
    };

    /**
     * @brief Movement status structure
     */
    struct MovementStatus {
        bool isMoving;
        int currentPosition;
        int targetPosition;
    };

    HardwareInterface();
    ~HardwareInterface();

    // Non-copyable and non-movable
    HardwareInterface(const HardwareInterface&) = delete;
    HardwareInterface& operator=(const HardwareInterface&) = delete;
    HardwareInterface(HardwareInterface&&) = delete;
    HardwareInterface& operator=(HardwareInterface&&) = delete;

    // Initialization and cleanup
    bool initialize();
    bool destroy();

    // Device discovery and connection
    std::vector<DeviceInfo> scanDevices();
    bool connectToDevice(const std::string& deviceName = "");
    bool connectToDevice(int deviceId);
    bool disconnect();
    bool isConnected() const;

    // Device information
    std::optional<DeviceInfo> getDeviceInfo() const;
    std::string getLastError() const;

    // Basic hardware operations
    bool setPosition(int position);
    int getCurrentPosition();
    MovementStatus getMovementStatus();
    bool waitForMovement(int timeoutMs = 10000);

    // Direction control
    bool setUnidirectionalMode(bool enable);
    bool isUnidirectionalMode();

    // Calibration
    bool calibrate();

    // Status queries
    bool isMoving() const;
    int getFilterCount() const;

private:
    mutable std::mutex hwMutex_;
    bool initialized_;
    bool connected_;
    int deviceId_;
    DeviceInfo deviceInfo_;
    std::string lastError_;

    // Helper methods
    void setError(const std::string& error);
    bool validateDeviceId(int id) const;
    bool validatePosition(int position) const;
    void updateDeviceInfo();
};

} // namespace lithium::device::asi::filterwheel::components
