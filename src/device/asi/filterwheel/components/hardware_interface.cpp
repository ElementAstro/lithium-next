/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Filter Wheel Hardware Interface Component Implementation

*************************************************/

#include "hardware_interface.hpp"

#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

#include <spdlog/spdlog.h>

#include <libasi/EFW_filter.h>

namespace lithium::device::asi::filterwheel::components {

HardwareInterface::HardwareInterface()
    : initialized_(false), connected_(false), deviceId_(-1) {
    spdlog::info("Created ASI Filter Wheel Hardware Interface");
}

HardwareInterface::~HardwareInterface() {
    destroy();
    spdlog::info("Destroyed ASI Filter Wheel Hardware Interface");
}

bool HardwareInterface::initialize() {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (initialized_) {
        return true;
    }

    spdlog::info("Initializing ASI Filter Wheel Hardware Interface");

    // Clear any previous state
    connected_ = false;
    deviceId_ = -1;
    lastError_.clear();

    initialized_ = true;
    spdlog::info("Hardware Interface initialized successfully");
    return true;
}

bool HardwareInterface::destroy() {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!initialized_) {
        return true;
    }

    spdlog::info("Destroying ASI Filter Wheel Hardware Interface");

    if (connected_) {
        disconnect();
    }

    initialized_ = false;
    return true;
}

std::vector<HardwareInterface::DeviceInfo> HardwareInterface::scanDevices() {
    std::lock_guard<std::mutex> lock(hwMutex_);
    std::vector<DeviceInfo> devices;

    if (!initialized_) {
        setError("Hardware interface not initialized");
        return devices;
    }

    spdlog::info("Scanning for ASI Filter Wheel devices");

    try {
        int deviceCount = EFWGetNum();
        spdlog::info("Found {} EFW device(s)", deviceCount);

        for (int i = 0; i < deviceCount; ++i) {
            int id;
            if (EFWGetID(i, &id) == EFW_SUCCESS) {
                EFW_INFO info;
                if (EFWGetProperty(id, &info) == EFW_SUCCESS) {
                    DeviceInfo device;
                    device.id = info.ID;
                    device.name = info.Name;
                    device.slotCount = info.slotNum;

                    // Get firmware version using the proper API
                    unsigned char major, minor, build;
                    if (EFWGetFirmwareVersion(info.ID, &major, &minor,
                                              &build) == EFW_SUCCESS) {
                        std::ostringstream fwStream;
                        fwStream << static_cast<int>(major) << "."
                                 << static_cast<int>(minor) << "."
                                 << static_cast<int>(build);
                        device.firmwareVersion = fwStream.str();
                    } else {
                        device.firmwareVersion = "Unknown";
                    }

                    // SDK version as driver version
                    device.driverVersion =
                        EFWGetSDKVersion() ? EFWGetSDKVersion() : "Unknown";

                    devices.push_back(device);
                    spdlog::info("Found device: {} (ID: {}, Slots: {})",
                          device.name, device.id, device.slotCount);
                }
            }
        }

    } catch (const std::exception& e) {
        setError("Device scan failed: " + std::string(e.what()));
        spdlog::error("Device scan failed: {}", e.what());
    }

    return devices;
}

bool HardwareInterface::connectToDevice(const std::string& deviceName) {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!initialized_) {
        setError("Hardware interface not initialized");
        return false;
    }

    if (connected_) {
        return true;
    }

    spdlog::info("Connecting to ASI Filter Wheel: '{}'", deviceName);

    try {
        // Scan for devices
        int deviceCount = EFWGetNum();
        if (deviceCount <= 0) {
            setError("No ASI Filter Wheel devices found");
            return false;
        }

        int targetId = -1;
        bool found = false;

        // Find the specified device or use the first one
        for (int i = 0; i < deviceCount; ++i) {
            int id;
            if (EFWGetID(i, &id) == EFW_SUCCESS) {
                EFW_INFO info;
                if (EFWGetProperty(id, &info) == EFW_SUCCESS) {
                    std::string deviceString = std::string(info.Name) + " (#" +
                                               std::to_string(info.ID) + ")";
                    if (deviceName.empty() ||
                        deviceString.find(deviceName) != std::string::npos) {
                        targetId = id;
                        found = true;
                        break;
                    }
                }
            }
        }

        if (!found && !deviceName.empty()) {
            spdlog::warn("Device '{}' not found, using first available device",
                  deviceName);
            if (EFWGetID(0, &targetId) != EFW_SUCCESS) {
                setError("Failed to get device ID");
                return false;
            }
        }

        // Open the device
        EFW_ERROR_CODE result = EFWOpen(targetId);
        if (result != EFW_SUCCESS) {
            setError("Failed to open device with ID " +
                     std::to_string(targetId));
            return false;
        }

        deviceId_ = targetId;
        connected_ = true;
        updateDeviceInfo();

        spdlog::info("Successfully connected to device: {} (ID: {}, Slots: {})",
              deviceInfo_.name, deviceInfo_.id, deviceInfo_.slotCount);
        return true;

    } catch (const std::exception& e) {
        setError("Connection failed: " + std::string(e.what()));
        spdlog::error("Connection failed: {}", e.what());
        return false;
    }
}

bool HardwareInterface::connectToDevice(int deviceId) {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!initialized_) {
        setError("Hardware interface not initialized");
        return false;
    }

    if (connected_) {
        return true;
    }

    if (!validateDeviceId(deviceId)) {
        setError("Invalid device ID: " + std::to_string(deviceId));
        return false;
    }

    spdlog::info("Connecting to ASI Filter Wheel with ID: {}", deviceId);

    try {
        EFW_ERROR_CODE result = EFWOpen(deviceId);
        if (result != EFW_SUCCESS) {
            setError("Failed to open device with ID " +
                     std::to_string(deviceId));
            return false;
        }

        deviceId_ = deviceId;
        connected_ = true;
        updateDeviceInfo();

        spdlog::info("Successfully connected to device ID: {}", deviceId);
        return true;

    } catch (const std::exception& e) {
        setError("Connection failed: " + std::string(e.what()));
        spdlog::error("Connection failed: {}", e.what());
        return false;
    }
}

bool HardwareInterface::disconnect() {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        return true;
    }

    spdlog::info("Disconnecting from ASI Filter Wheel");

    try {
        EFW_ERROR_CODE result = EFWClose(deviceId_);
        if (result != EFW_SUCCESS) {
            spdlog::warn("Warning during disconnect: EFW error code {}",
                  static_cast<int>(result));
        }

        connected_ = false;
        deviceId_ = -1;

        spdlog::info("Disconnected from ASI Filter Wheel");
        return true;

    } catch (const std::exception& e) {
        setError("Disconnect failed: " + std::string(e.what()));
        spdlog::error("Disconnect failed: {}", e.what());
        return false;
    }
}

bool HardwareInterface::isConnected() const {
    std::lock_guard<std::mutex> lock(hwMutex_);
    return connected_;
}

std::optional<HardwareInterface::DeviceInfo> HardwareInterface::getDeviceInfo()
    const {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        return std::nullopt;
    }

    return deviceInfo_;
}

std::string HardwareInterface::getLastError() const {
    std::lock_guard<std::mutex> lock(hwMutex_);
    return lastError_;
}

bool HardwareInterface::setPosition(int position) {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        setError("Device not connected");
        return false;
    }

    if (!validatePosition(position)) {
        setError("Invalid position: " + std::to_string(position));
        return false;
    }

    spdlog::info("Setting filter position to: {}", position);

    try {
        EFW_ERROR_CODE result = EFWSetPosition(deviceId_, position);
        if (result != EFW_SUCCESS) {
            setError("Failed to set position: " + std::to_string(position));
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        setError("Set position failed: " + std::string(e.what()));
        spdlog::error("Set position failed: {}", e.what());
        return false;
    }
}

int HardwareInterface::getCurrentPosition() {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        return 0;  // Default position (0-based)
    }

    try {
        int position = 0;
        EFW_ERROR_CODE result = EFWGetPosition(deviceId_, &position);
        if (result == EFW_SUCCESS) {
            return position;
        } else {
            setError("Failed to get current position");
            return 0;
        }

    } catch (const std::exception& e) {
        setError("Get position failed: " + std::string(e.what()));
        spdlog::error("Get position failed: {}", e.what());
        return 0;
    }
}

HardwareInterface::MovementStatus HardwareInterface::getMovementStatus() {
    std::lock_guard<std::mutex> lock(hwMutex_);

    MovementStatus status;
    status.currentPosition = getCurrentPosition();
    status.targetPosition = status.currentPosition;

    if (!connected_) {
        status.isMoving = false;
        return status;
    }

    // EFW API doesn't provide direct movement status
    // We can check if position is -1, which indicates movement
    status.isMoving = (status.currentPosition == -1);

    return status;
}

bool HardwareInterface::waitForMovement(int timeoutMs) {
    auto start = std::chrono::steady_clock::now();

    while (isMoving()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();
        if (elapsed > timeoutMs) {
            setError("Movement timeout after " + std::to_string(timeoutMs) +
                     "ms");
            return false;
        }
    }

    return true;
}

bool HardwareInterface::setUnidirectionalMode(bool enable) {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        setError("Device not connected");
        return false;
    }

    spdlog::info("Setting {} mode", enable ? "unidirectional" : "bidirectional");

    try {
        EFW_ERROR_CODE result = EFWSetDirection(deviceId_, enable);
        if (result != EFW_SUCCESS) {
            setError("Failed to set direction mode");
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        setError("Set direction failed: " + std::string(e.what()));
        spdlog::error("Set direction failed: {}", e.what());
        return false;
    }
}

bool HardwareInterface::isUnidirectionalMode() {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        return false;
    }

    try {
        bool unidirection = false;
        EFW_ERROR_CODE result = EFWGetDirection(deviceId_, &unidirection);
        if (result == EFW_SUCCESS) {
            return unidirection;
        } else {
            setError("Failed to get direction mode");
            return false;
        }

    } catch (const std::exception& e) {
        setError("Get direction failed: " + std::string(e.what()));
        spdlog::error("Get direction failed: {}", e.what());
        return false;
    }
}

bool HardwareInterface::calibrate() {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        setError("Device not connected");
        return false;
    }

    spdlog::info("Calibrating filter wheel");

    try {
        EFW_ERROR_CODE result = EFWCalibrate(deviceId_);
        if (result != EFW_SUCCESS) {
            setError("Calibration failed");
            return false;
        }

        spdlog::info("Filter wheel calibration completed");
        return true;

    } catch (const std::exception& e) {
        setError("Calibration failed: " + std::string(e.what()));
        spdlog::error("Calibration failed: {}", e.what());
        return false;
    }
}

bool HardwareInterface::isMoving() const {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        return false;
    }

    // EFW API doesn't provide direct movement status
    // We can check if position is -1, which indicates movement
    int position = 0;
    EFW_ERROR_CODE result = EFWGetPosition(deviceId_, &position);
    return (result == EFW_SUCCESS && position == -1);
}

int HardwareInterface::getFilterCount() const {
    std::lock_guard<std::mutex> lock(hwMutex_);

    if (!connected_) {
        return 5;  // Default reasonable number of slots
    }

    return deviceInfo_.slotCount;
}

// Private methods

void HardwareInterface::setError(const std::string& error) {
    lastError_ = error;
    spdlog::error("Hardware Interface Error: {}", error);
}

bool HardwareInterface::validateDeviceId(int id) const {
    return id >= 0;  // Simple validation
}

bool HardwareInterface::validatePosition(int position) const {
    return position >= 0 && position < getFilterCount();
}

void HardwareInterface::updateDeviceInfo() {
    if (!connected_) {
        return;
    }

    try {
        EFW_INFO info;
        if (EFWGetProperty(deviceId_, &info) == EFW_SUCCESS) {
            deviceInfo_.id = info.ID;
            deviceInfo_.name = info.Name;
            deviceInfo_.slotCount = info.slotNum;

            // Get firmware version using the proper API
            unsigned char major, minor, build;
            if (EFWGetFirmwareVersion(deviceId_, &major, &minor, &build) ==
                EFW_SUCCESS) {
                std::ostringstream fwStream;
                fwStream << static_cast<int>(major) << "."
                         << static_cast<int>(minor) << "."
                         << static_cast<int>(build);
                deviceInfo_.firmwareVersion = fwStream.str();
            } else {
                deviceInfo_.firmwareVersion = "Unknown";
            }

            // SDK version as driver version
            deviceInfo_.driverVersion =
                EFWGetSDKVersion() ? EFWGetSDKVersion() : "Unknown";
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to update device info: {}", e.what());
    }
}

}  // namespace lithium::device::asi::filterwheel::components
