/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Hardware Interface Component Implementation

*************************************************/

#include "hardware_interface.hpp"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <thread>

#include <spdlog/spdlog.h>

#include <libasi/EAF_focuser.h>

namespace lithium::device::asi::focuser::components {

HardwareInterface::HardwareInterface() {
    spdlog::info("Created ASI Focuser Hardware Interface");
}

HardwareInterface::~HardwareInterface() {
    destroy();
    spdlog::info("Destroyed ASI Focuser Hardware Interface");
}

bool HardwareInterface::initialize() {
    spdlog::info("Initializing ASI Focuser Hardware Interface");

    if (initialized_) {
        return true;
    }

    initialized_ = true;
    spdlog::info("ASI Focuser Hardware Interface initialized successfully");
    return true;
}

bool HardwareInterface::destroy() {
    spdlog::info("Destroying ASI Focuser Hardware Interface");

    if (connected_) {
        disconnect();
    }

    initialized_ = false;
    return true;
}

bool HardwareInterface::connect(const std::string& deviceName, int timeout,
                                int maxRetry) {
    std::lock_guard<std::mutex> lock(deviceMutex_);

    if (connected_) {
        return true;
    }

    spdlog::info("Connecting to ASI Focuser: {}", deviceName);

    for (int retry = 0; retry < maxRetry; ++retry) {
        try {
            spdlog::info("Connection attempt {} of {}", retry + 1, maxRetry);

            // Get available devices
            int deviceCount = EAFGetNum();
            if (deviceCount <= 0) {
                spdlog::warn("No ASI Focuser devices found");
                continue;
            }

            // Find the specified device or use the first one
            int targetId = 0;
            bool found = false;

            for (int i = 0; i < deviceCount; ++i) {
                int id;
                if (EAFGetID(i, &id) == EAF_SUCCESS) {
                    EAF_INFO info;
                    if (EAFGetProperty(id, &info) == EAF_SUCCESS) {
                        if (deviceName.empty() ||
                            std::string(info.Name) == deviceName) {
                            targetId = id;
                            found = true;
                            break;
                        }
                    }
                }
            }

            if (!found && !deviceName.empty()) {
                spdlog::warn(
                    "Device '{}' not found, using first available device",
                    deviceName);
                if (EAFGetID(0, &targetId) != EAF_SUCCESS) {
                    continue;
                }
            }

            // Open the device
            if (EAFOpen(targetId) != EAF_SUCCESS) {
                spdlog::error("Failed to open ASI Focuser with ID {}",
                              targetId);
                continue;
            }

            deviceId_ = targetId;
            updateDeviceInfo();
            connected_ = true;

            spdlog::info(
                "Successfully connected to ASI Focuser: {} (ID: {}, Max "
                "Position: {})",
                modelName_, deviceId_, maxPosition_);
            return true;

        } catch (const std::exception& e) {
            spdlog::error("Connection attempt {} failed: {}", retry + 1,
                          e.what());
            lastError_ = e.what();
        }

        if (retry < maxRetry - 1) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(timeout / maxRetry));
        }
    }

    spdlog::error("Failed to connect to ASI Focuser after {} attempts",
                  maxRetry);
    return false;
}

bool HardwareInterface::disconnect() {
    std::lock_guard<std::mutex> lock(deviceMutex_);

    if (!connected_) {
        return true;
    }

    spdlog::info("Disconnecting ASI Focuser");

    // Stop any movement
    if (isMoving()) {
        stopMovement();
    }

#ifdef LITHIUM_ASI_EAF_ENABLED
    EAFClose(deviceId_);
#else
    EAFClose(deviceId_);
#endif

    connected_ = false;
    deviceId_ = -1;

    spdlog::info("Disconnected from ASI Focuser");
    return true;
}

bool HardwareInterface::scan(std::vector<std::string>& devices) {
    devices.clear();

    int count = EAFGetNum();
    for (int i = 0; i < count; ++i) {
        int id;
        if (EAFGetID(i, &id) == EAF_SUCCESS) {
            EAF_INFO info;
            if (EAFGetProperty(id, &info) == EAF_SUCCESS) {
                std::string deviceString = std::string(info.Name) + " (#" +
                                           std::to_string(info.ID) + ")";
                devices.push_back(deviceString);
            }
        }
    }

    spdlog::info("Found {} ASI Focuser device(s)", devices.size());
    return !devices.empty();
}

bool HardwareInterface::moveToPosition(int position) {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot move to position {} - device not connected",
                      position);
        return false;
    }

    // Validate position range
    if (position < 0 || position > maxPosition_) {
        lastError_ = "Position out of range";
        spdlog::error("Position {} is out of range [0, {}]", position,
                      maxPosition_);
        return false;
    }

    spdlog::info("Moving focuser to position: {}", position);

    EAF_ERROR_CODE result = EAFMove(deviceId_, position);
    if (result != EAF_SUCCESS) {
        switch (result) {
            case EAF_ERROR_MOVING:
                lastError_ = "Focuser is already moving";
                spdlog::warn(
                    "Cannot move to position {} - focuser is already moving",
                    position);
                break;
            case EAF_ERROR_ERROR_STATE:
                lastError_ = "Focuser is in error state";
                spdlog::error(
                    "Cannot move to position {} - focuser is in error state",
                    position);
                break;
            case EAF_ERROR_REMOVED:
                lastError_ = "Focuser has been removed";
                spdlog::error(
                    "Cannot move to position {} - focuser has been removed",
                    position);
                break;
            default:
                lastError_ = "Failed to move to position";
                spdlog::error("Failed to move to position {}, error code: {}",
                              position, static_cast<int>(result));
                break;
        }
        return false;
    }

    spdlog::debug("Move command sent successfully to position: {}", position);
    return true;
}

int HardwareInterface::getCurrentPosition() {
    if (!checkConnection()) {
        spdlog::error("Cannot get current position - device not connected");
        return -1;
    }

    int position = 0;
    EAF_ERROR_CODE result = EAFGetPosition(deviceId_, &position);
    if (result == EAF_SUCCESS) {
        spdlog::debug("Current position: {}", position);
        return position;
    } else {
        spdlog::error("Failed to get current position, error code: {}",
                      static_cast<int>(result));
        lastError_ = "Failed to get current position";
        return -1;
    }
}

bool HardwareInterface::stopMovement() {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot stop movement - device not connected");
        return false;
    }

    spdlog::info("Stopping focuser movement");

    EAF_ERROR_CODE result = EAFStop(deviceId_);
    if (result != EAF_SUCCESS) {
        switch (result) {
            case EAF_ERROR_ERROR_STATE:
                lastError_ = "Focuser is in error state";
                spdlog::error(
                    "Cannot stop movement - focuser is in error state");
                break;
            case EAF_ERROR_REMOVED:
                lastError_ = "Focuser has been removed";
                spdlog::error(
                    "Cannot stop movement - focuser has been removed");
                break;
            default:
                lastError_ = "Failed to stop movement";
                spdlog::error("Failed to stop movement, error code: {}",
                              static_cast<int>(result));
                break;
        }
        return false;
    }

    spdlog::info("Focuser movement stopped successfully");
    return true;
}

bool HardwareInterface::isMoving() const {
    if (!checkConnection()) {
        return false;
    }

    bool moving = false;
    bool handControl = false;
    EAF_ERROR_CODE result = EAFIsMoving(deviceId_, &moving, &handControl);

    if (result == EAF_SUCCESS) {
        if (handControl) {
            spdlog::debug("Focuser is being moved by hand control");
        }
        spdlog::debug("Focuser movement status - Moving: {}, Hand Control: {}",
                      moving, handControl);
        return moving;
    } else {
        spdlog::error("Failed to check movement status, error code: {}",
                      static_cast<int>(result));
        return false;
    }
}

bool HardwareInterface::setReverse(bool reverse) {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot set reverse direction - device not connected");
        return false;
    }

    spdlog::info("Setting reverse direction: {}", reverse ? "true" : "false");

    EAF_ERROR_CODE result = EAFSetReverse(deviceId_, reverse);
    if (result != EAF_SUCCESS) {
        lastError_ = "Failed to set reverse direction";
        spdlog::error("Failed to set reverse direction, error code: {}",
                      static_cast<int>(result));
        return false;
    }

    spdlog::debug("Reverse direction set successfully");
    return true;
}

bool HardwareInterface::getReverse(bool& reverse) {
    if (!checkConnection()) {
        spdlog::error("Cannot get reverse direction - device not connected");
        return false;
    }

    EAF_ERROR_CODE result = EAFGetReverse(deviceId_, &reverse);
    if (result == EAF_SUCCESS) {
        spdlog::debug("Current reverse direction: {}",
                      reverse ? "true" : "false");
        return true;
    } else {
        spdlog::error("Failed to get reverse direction, error code: {}",
                      static_cast<int>(result));
        return false;
    }
}

bool HardwareInterface::setBacklash(int backlash) {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot set backlash - device not connected");
        return false;
    }

    // Validate backlash range (0-255 according to API)
    if (backlash < 0 || backlash > 255) {
        lastError_ = "Backlash value out of range (0-255)";
        spdlog::error("Backlash value {} is out of range [0, 255]", backlash);
        return false;
    }

    spdlog::info("Setting backlash compensation: {}", backlash);

    EAF_ERROR_CODE result = EAFSetBacklash(deviceId_, backlash);
    if (result != EAF_SUCCESS) {
        if (result == EAF_ERROR_INVALID_VALUE) {
            lastError_ = "Invalid backlash value";
            spdlog::error("Invalid backlash value: {}", backlash);
        } else {
            lastError_ = "Failed to set backlash";
            spdlog::error("Failed to set backlash, error code: {}",
                          static_cast<int>(result));
        }
        return false;
    }

    spdlog::debug("Backlash compensation set successfully");
    return true;
}

bool HardwareInterface::getBacklash(int& backlash) {
    if (!checkConnection()) {
        spdlog::error("Cannot get backlash - device not connected");
        return false;
    }

    EAF_ERROR_CODE result = EAFGetBacklash(deviceId_, &backlash);
    if (result == EAF_SUCCESS) {
        spdlog::debug("Current backlash compensation: {}", backlash);
        return true;
    } else {
        spdlog::error("Failed to get backlash, error code: {}",
                      static_cast<int>(result));
        return false;
    }
}

bool HardwareInterface::getTemperature(float& temperature) {
    if (!checkConnection() || !hasTemperatureSensor_) {
        return false;
    }

    spdlog::debug("Getting temperature from device ID: {}", deviceId_);
    EAF_ERROR_CODE result = EAFGetTemp(deviceId_, &temperature);

    if (result == EAF_SUCCESS) {
        spdlog::debug("Temperature reading: {:.2f}°C", temperature);
        return true;
    } else if (result == EAF_ERROR_GENERAL_ERROR) {
        spdlog::warn(
            "Temperature value is unusable (device may be moved by hand)");
        lastError_ = "Temperature value is unusable";
        return false;
    } else {
        spdlog::error("Failed to get temperature, error code: {}",
                      static_cast<int>(result));
        lastError_ = "Failed to get temperature";
        return false;
    }
}

bool HardwareInterface::resetToZero() {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot reset to zero - device not connected");
        return false;
    }

    spdlog::info("Resetting focuser to zero position");

    EAF_ERROR_CODE result = EAFResetPostion(deviceId_, 0);
    if (result != EAF_SUCCESS) {
        lastError_ = "Failed to reset to zero position";
        spdlog::error("Failed to reset to zero position, error code: {}",
                      static_cast<int>(result));
        return false;
    }

    spdlog::info("Successfully reset focuser to zero position");
    return true;
}

bool HardwareInterface::resetPosition(int position) {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot reset position - device not connected");
        return false;
    }

    spdlog::info("Resetting focuser position to: {}", position);

    EAF_ERROR_CODE result = EAFResetPostion(deviceId_, position);
    if (result != EAF_SUCCESS) {
        lastError_ = "Failed to reset position";
        spdlog::error("Failed to reset position to {}, error code: {}",
                      position, static_cast<int>(result));
        return false;
    }

    spdlog::info("Successfully reset focuser position to: {}", position);
    return true;
}

bool HardwareInterface::setBeep(bool enable) {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot set beep - device not connected");
        return false;
    }

    spdlog::info("Setting beep: {}", enable ? "enabled" : "disabled");

    EAF_ERROR_CODE result = EAFSetBeep(deviceId_, enable);
    if (result != EAF_SUCCESS) {
        lastError_ = "Failed to set beep";
        spdlog::error("Failed to set beep, error code: {}",
                      static_cast<int>(result));
        return false;
    }

    spdlog::debug("Beep setting applied successfully");
    return true;
}

bool HardwareInterface::getBeep(bool& enabled) {
    if (!checkConnection()) {
        spdlog::error("Cannot get beep setting - device not connected");
        return false;
    }

    EAF_ERROR_CODE result = EAFGetBeep(deviceId_, &enabled);
    if (result == EAF_SUCCESS) {
        spdlog::debug("Current beep setting: {}",
                      enabled ? "enabled" : "disabled");
        return true;
    } else {
        spdlog::error("Failed to get beep setting, error code: {}",
                      static_cast<int>(result));
        return false;
    }
}

bool HardwareInterface::setMaxStep(int maxStep) {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot set max step - device not connected");
        return false;
    }

    if (isMoving()) {
        lastError_ = "Cannot set max step while moving";
        spdlog::error("Cannot set max step while focuser is moving");
        return false;
    }

    spdlog::info("Setting maximum step position: {}", maxStep);

    EAF_ERROR_CODE result = EAFSetMaxStep(deviceId_, maxStep);
    if (result != EAF_SUCCESS) {
        switch (result) {
            case EAF_ERROR_MOVING:
                lastError_ = "Focuser is moving";
                spdlog::error("Cannot set max step - focuser is moving");
                break;
            default:
                lastError_ = "Failed to set max step";
                spdlog::error("Failed to set max step, error code: {}",
                              static_cast<int>(result));
                break;
        }
        return false;
    }

    maxPosition_ = maxStep;  // Update cached value
    spdlog::debug("Maximum step position set successfully");
    return true;
}

bool HardwareInterface::getMaxStep(int& maxStep) {
    if (!checkConnection()) {
        spdlog::error("Cannot get max step - device not connected");
        return false;
    }

    EAF_ERROR_CODE result = EAFGetMaxStep(deviceId_, &maxStep);
    if (result == EAF_SUCCESS) {
        spdlog::debug("Current maximum step position: {}", maxStep);
        return true;
    } else {
        spdlog::error("Failed to get max step, error code: {}",
                      static_cast<int>(result));
        return false;
    }
}

bool HardwareInterface::getStepRange(int& range) {
    if (!checkConnection()) {
        spdlog::error("Cannot get step range - device not connected");
        return false;
    }

    EAF_ERROR_CODE result = EAFStepRange(deviceId_, &range);
    if (result == EAF_SUCCESS) {
        spdlog::debug("Current step range: {}", range);
        return true;
    } else {
        spdlog::error("Failed to get step range, error code: {}",
                      static_cast<int>(result));
        return false;
    }
}

bool HardwareInterface::getFirmwareVersion(unsigned char& major,
                                           unsigned char& minor,
                                           unsigned char& build) {
    if (!checkConnection()) {
        spdlog::error("Cannot get firmware version - device not connected");
        return false;
    }

    EAF_ERROR_CODE result =
        EAFGetFirmwareVersion(deviceId_, &major, &minor, &build);
    if (result == EAF_SUCCESS) {
        spdlog::debug("Firmware version: {}.{}.{}", major, minor, build);
        return true;
    } else {
        spdlog::error("Failed to get firmware version, error code: {}",
                      static_cast<int>(result));
        return false;
    }
}

bool HardwareInterface::getSerialNumber(std::string& serialNumber) {
    if (!checkConnection()) {
        spdlog::error("Cannot get serial number - device not connected");
        return false;
    }

    EAF_SN serialNum;
    EAF_ERROR_CODE result = EAFGetSerialNumber(deviceId_, &serialNum);
    if (result == EAF_SUCCESS) {
        // Convert byte array to hex string
        std::stringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << std::setfill('0') << std::setw(2)
               << static_cast<int>(serialNum.id[i]);
        }
        serialNumber = ss.str();
        spdlog::debug("Serial number: {}", serialNumber);
        return true;
    } else if (result == EAF_ERROR_NOT_SUPPORTED) {
        lastError_ = "Serial number not supported by firmware";
        spdlog::warn("Serial number not supported by firmware");
        return false;
    } else {
        spdlog::error("Failed to get serial number, error code: {}",
                      static_cast<int>(result));
        return false;
    }
}

bool HardwareInterface::setDeviceAlias(const std::string& alias) {
    if (!checkConnection()) {
        lastError_ = "Device not connected";
        spdlog::error("Cannot set device alias - device not connected");
        return false;
    }

    if (alias.length() >
        7) {  // EAF_ID has 8 bytes, reserve one for null terminator
        lastError_ = "Alias too long (max 7 characters)";
        spdlog::error("Alias '{}' is too long (max 7 characters)", alias);
        return false;
    }

    spdlog::info("Setting device alias: {}", alias);

    EAF_ID aliasId;
    std::memset(&aliasId, 0, sizeof(aliasId));
    std::strncpy(reinterpret_cast<char*>(aliasId.id), alias.c_str(), 7);

    EAF_ERROR_CODE result = EAFSetID(deviceId_, aliasId);
    if (result != EAF_SUCCESS) {
        if (result == EAF_ERROR_NOT_SUPPORTED) {
            lastError_ = "Setting alias not supported by firmware";
            spdlog::warn("Setting alias not supported by firmware");
        } else {
            lastError_ = "Failed to set device alias";
            spdlog::error("Failed to set device alias, error code: {}",
                          static_cast<int>(result));
        }
        return false;
    }

    spdlog::debug("Device alias set successfully");
    return true;
}

std::string HardwareInterface::getSDKVersion() {
    char* version = EAFGetSDKVersion();
    if (version) {
        std::string versionStr(version);
        spdlog::debug("EAF SDK Version: {}", versionStr);
        return versionStr;
    }
    return "Unknown";
}

void HardwareInterface::updateDeviceInfo() {
    if (!checkConnection()) {
        spdlog::warn("Cannot update device info - device not connected");
        return;
    }

    spdlog::debug("Updating device information for device ID: {}", deviceId_);

    EAF_INFO info;
    EAF_ERROR_CODE result = EAFGetProperty(deviceId_, &info);
    if (result == EAF_SUCCESS) {
        modelName_ = std::string(info.Name);
        maxPosition_ = info.MaxStep;
        spdlog::info("Device info updated - Name: {}, Max Position: {}",
                     modelName_, maxPosition_);

        // Get firmware version separately
        unsigned char major, minor, build;
        result = EAFGetFirmwareVersion(deviceId_, &major, &minor, &build);
        if (result == EAF_SUCCESS) {
            firmwareVersion_ = std::to_string(major) + "." +
                               std::to_string(minor) + "." +
                               std::to_string(build);
            spdlog::info("Firmware version: {}", firmwareVersion_);
        } else {
            firmwareVersion_ = "Unknown";
            spdlog::warn("Failed to get firmware version, error code: {}",
                         static_cast<int>(result));
        }
    } else {
        spdlog::error("Failed to get device property, error code: {}",
                      static_cast<int>(result));
        lastError_ = "Failed to get device properties";
    }
}

bool HardwareInterface::checkConnection() const {
    return connected_ && deviceId_ >= 0;
}

}  // namespace lithium::device::asi::focuser::components
