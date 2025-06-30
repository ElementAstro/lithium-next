/*
 * hardware_interface_fixed.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Hardware Interface Component Implementation (Fixed Version)

This component provides a clean interface to ASCOM Camera APIs,
handling low-level hardware communication, device management,
and both COM and Alpaca protocol integration.

*************************************************/

#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>

#ifdef _WIN32
// These headers are only available on Windows
// #include <comdef.h>
// #include <comutil.h>
#endif

namespace lithium::device::ascom::camera::components {

HardwareInterface::HardwareInterface() {
    LOG_F(INFO, "ASCOM Camera Hardware Interface created");
}

HardwareInterface::~HardwareInterface() {
    if (initialized_) {
        shutdown();
    }
}

bool HardwareInterface::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }

    LOG_F(INFO, "Initializing ASCOM Hardware Interface");

#ifdef _WIN32
    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        setError("Failed to initialize COM subsystem");
        return false;
    }
#else
    // For non-Windows platforms, we'll use Alpaca REST API
    LOG_F(INFO, "Non-Windows platform detected, will use Alpaca REST API");
#endif

    initialized_ = true;
    LOG_F(INFO, "ASCOM Hardware Interface initialized successfully");
    return true;
}

bool HardwareInterface::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return true;
    }

    LOG_F(INFO, "Shutting down ASCOM Hardware Interface");
    
    if (connected_) {
        disconnect();
    }

#ifdef _WIN32
    if (comCamera_) {
        comCamera_->Release();
        comCamera_ = nullptr;
    }
    CoUninitialize();
#endif

    initialized_ = false;
    LOG_F(INFO, "ASCOM Hardware Interface shutdown complete");
    return true;
}

std::vector<std::string> HardwareInterface::discoverDevices() {
    std::vector<std::string> devices;
    
    // Add some stub ASCOM devices for testing
    devices.push_back("ASCOM.Simulator.Camera");
    devices.push_back("ASCOM.ASICamera2.Camera");
    devices.push_back("ASCOM.QHYCamera.Camera");
    
    // For Alpaca devices, we could do network discovery here
    auto alpacaDevices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpacaDevices.begin(), alpacaDevices.end());
    
    LOG_F(INFO, "Discovered {} ASCOM camera devices", devices.size());
    return devices;
}

bool HardwareInterface::connect(const ConnectionSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        setError("Hardware interface not initialized");
        return false;
    }
    
    if (connected_) {
        setError("Already connected to a device");
        return false;
    }

    currentSettings_ = settings;
    
    LOG_F(INFO, "Connecting to ASCOM camera: {}", settings.deviceName);

    bool success = false;
    
    // Determine connection type and connect
    if (settings.type == ConnectionType::ALPACA_REST) {
        success = connectAlpaca(settings);
    } else {
#ifdef _WIN32
        success = connectCOM(settings);
#else
        setError("COM drivers not supported on non-Windows platforms");
        return false;
#endif
    }
    
    if (success) {
        connected_ = true;
        connectionType_ = settings.type;
        clearError();
        LOG_F(INFO, "Successfully connected to ASCOM camera");
    }
    
    return success;
}

bool HardwareInterface::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return true;
    }

    LOG_F(INFO, "Disconnecting from ASCOM camera");
    
    bool success = false;
    
    if (connectionType_ == ConnectionType::ALPACA_REST) {
        success = disconnectAlpaca();
    } else {
#ifdef _WIN32
        success = disconnectCOM();
#else
        success = true; // No COM on non-Windows
#endif
    }

    if (success) {
        connected_ = false;
        connectionType_ = ConnectionType::COM_DRIVER; // Reset to default
        cameraInfo_.reset();
        clearError();
        LOG_F(INFO, "Successfully disconnected from ASCOM camera");
    }

    return success;
}

std::optional<HardwareInterface::CameraInfo> HardwareInterface::getCameraInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        return std::nullopt;
    }
    
    // Return cached info if available and recent
    if (cameraInfo_.has_value() && !shouldUpdateInfo()) {
        return cameraInfo_;
    }
    
    // Update camera info
    if (updateCameraInfo()) {
        return cameraInfo_;
    }
    
    return std::nullopt;
}

ASCOMCameraState HardwareInterface::getCameraState() const {
    if (!connected_) {
        return ASCOMCameraState::ERROR;
    }
    
    // Stub implementation - would query actual camera state
    return ASCOMCameraState::IDLE;
}

int HardwareInterface::getInterfaceVersion() const {
    return 3; // ASCOM Camera Interface v3
}

std::string HardwareInterface::getDriverInfo() const {
    if (!connected_) {
        return "Not connected";
    }
    
    return "Lithium-Next ASCOM Camera Driver v1.0";
}

std::string HardwareInterface::getDriverVersion() const {
    return "1.0.0";
}

bool HardwareInterface::startExposure(double duration, bool light) {
    if (!connected_) {
        setError("Not connected to camera");
        return false;
    }
    
    LOG_F(INFO, "Starting exposure: {}s, light={}", duration, light);
    
    // Stub implementation - would send exposure command to camera
    return true;
}

bool HardwareInterface::stopExposure() {
    if (!connected_) {
        setError("Not connected to camera");
        return false;
    }
    
    LOG_F(INFO, "Stopping exposure");
    
    // Stub implementation - would send stop command to camera
    return true;
}

bool HardwareInterface::isExposing() const {
    if (!connected_) {
        return false;
    }
    
    // Stub implementation - would query camera exposure status
    return false;
}

bool HardwareInterface::isImageReady() const {
    if (!connected_) {
        return false;
    }
    
    // Stub implementation - would query camera image ready status
    return true; // For testing, always ready
}

double HardwareInterface::getExposureProgress() const {
    if (!connected_) {
        return 0.0;
    }
    
    // Stub implementation - would calculate actual progress
    return 1.0; // Always complete for testing
}

double HardwareInterface::getRemainingExposureTime() const {
    if (!connected_) {
        return 0.0;
    }
    
    // Stub implementation - would calculate remaining time
    return 0.0;
}

std::optional<std::vector<uint16_t>> HardwareInterface::getImageArray() {
    if (!connected_) {
        setError("Not connected to camera");
        return std::nullopt;
    }
    
    // Stub implementation - return a small test image
    std::vector<uint16_t> testImage(1920 * 1080, 1000); // 1920x1080 with value 1000
    
    LOG_F(INFO, "Retrieved image array: {} pixels", testImage.size());
    return testImage;
}

std::pair<int, int> HardwareInterface::getImageDimensions() const {
    if (!connected_) {
        return {0, 0};
    }
    
    // Stub implementation - return default dimensions
    return {1920, 1080};
}

bool HardwareInterface::setCCDTemperature(double temperature) {
    if (!connected_) {
        setError("Not connected to camera");
        return false;
    }
    
    LOG_F(INFO, "Setting CCD temperature to {:.1f}°C", temperature);
    
    // Stub implementation - would send temperature command to camera
    return true;
}

double HardwareInterface::getCCDTemperature() const {
    if (!connected_) {
        return -999.0;
    }
    
    // Stub implementation - return simulated temperature
    return 20.0; // Room temperature
}

bool HardwareInterface::setCoolerOn(bool enable) {
    if (!connected_) {
        setError("Not connected to camera");
        return false;
    }
    
    LOG_F(INFO, "Setting cooler: {}", enable ? "ON" : "OFF");
    
    // Stub implementation - would send cooler command to camera
    return true;
}

bool HardwareInterface::isCoolerOn() const {
    if (!connected_) {
        return false;
    }
    
    // Stub implementation - return cooler status
    return false;
}

double HardwareInterface::getCoolerPower() const {
    if (!connected_) {
        return 0.0;
    }
    
    // Stub implementation - return cooler power percentage
    return 50.0;
}

bool HardwareInterface::setGain(int gain) {
    if (!connected_) {
        setError("Not connected to camera");
        return false;
    }
    
    LOG_F(INFO, "Setting gain to {}", gain);
    
    // Stub implementation - would send gain command to camera
    return true;
}

int HardwareInterface::getGain() const {
    if (!connected_) {
        return 0;
    }
    
    // Stub implementation - return current gain
    return 100;
}

std::pair<int, int> HardwareInterface::getGainRange() const {
    // Stub implementation - return typical gain range
    return {0, 300};
}

bool HardwareInterface::setOffset(int offset) {
    if (!connected_) {
        setError("Not connected to camera");
        return false;
    }
    
    LOG_F(INFO, "Setting offset to {}", offset);
    
    // Stub implementation - would send offset command to camera
    return true;
}

int HardwareInterface::getOffset() const {
    if (!connected_) {
        return 0;
    }
    
    // Stub implementation - return current offset
    return 10;
}

std::pair<int, int> HardwareInterface::getOffsetRange() const {
    // Stub implementation - return typical offset range
    return {0, 255};
}

std::string HardwareInterface::getLastError() const {
    return lastError_;
}

// Private helper methods
bool HardwareInterface::connectCOM(const ConnectionSettings& settings) {
#ifdef _WIN32
    // Stub COM connection implementation
    LOG_F(INFO, "Connecting via COM to: {}", settings.progId);
    return true;
#else
    setError("COM not supported on this platform");
    return false;
#endif
}

bool HardwareInterface::connectAlpaca(const ConnectionSettings& settings) {
    // Stub Alpaca connection implementation
    LOG_F(INFO, "Connecting via Alpaca to: {}:{}", settings.host, settings.port);
    return true;
}

bool HardwareInterface::disconnectCOM() {
#ifdef _WIN32
    // Stub COM disconnection implementation
    LOG_F(INFO, "Disconnecting COM interface");
    return true;
#else
    return true;
#endif
}

bool HardwareInterface::disconnectAlpaca() {
    // Stub Alpaca disconnection implementation
    LOG_F(INFO, "Disconnecting Alpaca interface");
    return true;
}

std::vector<std::string> HardwareInterface::discoverAlpacaDevices() {
    std::vector<std::string> devices;
    
    // Stub Alpaca discovery implementation
    devices.push_back("http://localhost:11111/api/v1/camera/0");
    
    return devices;
}

bool HardwareInterface::updateCameraInfo() const {
    // Stub implementation - create default camera info
    CameraInfo info;
    info.name = "ASCOM Test Camera";
    info.serialNumber = "TEST-001";
    info.driverInfo = getDriverInfo();
    info.driverVersion = getDriverVersion();
    info.cameraXSize = 1920;
    info.cameraYSize = 1080;
    info.pixelSizeX = 5.86;
    info.pixelSizeY = 5.86;
    info.maxBinX = 4;
    info.maxBinY = 4;
    info.canAbortExposure = true;
    info.canStopExposure = true;
    info.canSubFrame = true;
    info.hasShutter = true;
    info.sensorType = ASCOMSensorType::MONOCHROME;
    info.electronsPerADU = 0.37;
    info.fullWellCapacity = 25000.0;
    info.maxADU = 65535;
    info.hasCooler = true;
    
    cameraInfo_ = info;
    lastInfoUpdate_ = std::chrono::steady_clock::now();
    
    return true;
}

bool HardwareInterface::shouldUpdateInfo() const {
    // Update info every 30 seconds
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastInfoUpdate_);
    return elapsed.count() > 30;
}

} // namespace lithium::device::ascom::camera::components
