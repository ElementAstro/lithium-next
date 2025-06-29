/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Hardware Interface Component
Handles direct communication with EAF SDK

*************************************************/

#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace lithium::device::asi::focuser::components {

/**
 * @brief Hardware interface for ASI EAF devices
 *
 * This component handles low-level communication with the EAF SDK,
 * including device enumeration, connection management, and basic commands.
 */
class HardwareInterface {
public:
    HardwareInterface();
    ~HardwareInterface();

    // Non-copyable and non-movable
    HardwareInterface(const HardwareInterface&) = delete;
    HardwareInterface& operator=(const HardwareInterface&) = delete;
    HardwareInterface(HardwareInterface&&) = delete;
    HardwareInterface& operator=(HardwareInterface&&) = delete;

    // Device management
    bool initialize();
    bool destroy();
    bool connect(const std::string& deviceName, int timeout, int maxRetry);
    bool disconnect();
    bool scan(std::vector<std::string>& devices);

    // Connection status
    bool isConnected() const { return connected_; }
    int getDeviceId() const { return deviceId_; }
    std::string getModelName() const { return modelName_; }
    std::string getFirmwareVersion() const { return firmwareVersion_; }
    std::string getLastError() const { return lastError_; }

    // Basic hardware commands
    bool moveToPosition(int position);
    int getCurrentPosition();
    bool stopMovement();
    bool isMoving() const;

    // Hardware settings
    bool setReverse(bool reverse);
    bool getReverse(bool& reverse);
    bool setBacklash(int backlash);
    bool getBacklash(int& backlash);

    // Temperature (if supported)
    bool getTemperature(float& temperature);
    bool hasTemperatureSensor() const { return hasTemperatureSensor_; }

    // Hardware limits
    int getMaxPosition() const { return maxPosition_; }

    // Reset operations
    bool resetToZero();
    bool resetPosition(int position);

    // Beep control
    bool setBeep(bool enable);
    bool getBeep(bool& enabled);

    // Position limits
    bool setMaxStep(int maxStep);
    bool getMaxStep(int& maxStep);
    bool getStepRange(int& range);

    // Device information
    bool getFirmwareVersion(unsigned char& major, unsigned char& minor,
                            unsigned char& build);
    bool getSerialNumber(std::string& serialNumber);
    bool setDeviceAlias(const std::string& alias);

    // SDK information
    static std::string getSDKVersion();

    // Error handling
    void clearError() { lastError_.clear(); }

private:
    // Connection state
    bool initialized_ = false;
    bool connected_ = false;
    int deviceId_ = -1;

    // Device information
    std::string modelName_ = "Unknown";
    std::string firmwareVersion_ = "Unknown";
    int maxPosition_ = 30000;
    bool hasTemperatureSensor_ = true;

    // Error tracking
    std::string lastError_;

    // Thread safety
    mutable std::mutex deviceMutex_;

    // Helper methods
    bool findDevice(const std::string& deviceName, int& deviceId);
    void updateDeviceInfo();
    bool checkConnection() const;
};

}  // namespace lithium::device::asi::focuser::components
