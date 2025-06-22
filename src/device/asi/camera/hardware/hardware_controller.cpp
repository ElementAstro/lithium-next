/*
 * hardware_controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI camera hardware accessories controller implementation

*************************************************/

#include "hardware_controller.hpp"
#include "../core/asi_camera_core.hpp"
#include "atom/log/loguru.hpp"

#include <algorithm>
#include <thread>
#include <chrono>

// Include EAF and EFW SDK stubs
#include "../asi_eaf_sdk_stub.hpp"
#include "../asi_efw_sdk_stub.hpp"

namespace lithium::device::asi::camera {

HardwareController::HardwareController(ASICameraCore* core)
    : ComponentBase(core) {
    LOG_F(INFO, "Created ASI hardware controller");
}

HardwareController::~HardwareController() {
    if (eafFocuserConnected_) {
        disconnectEAFFocuser();
    }
    if (efwFilterWheelConnected_) {
        disconnectEFWFilterWheel();
    }
    LOG_F(INFO, "Destroyed ASI hardware controller");
}

auto HardwareController::initialize() -> bool {
    LOG_F(INFO, "Initializing ASI hardware controller");
    
    // Detect available hardware
    detectEAFFocuser();
    detectEFWFilterWheel();
    
    // Enable movement monitoring by default
    movementMonitoringEnabled_ = true;
    
    return true;
}

auto HardwareController::destroy() -> bool {
    LOG_F(INFO, "Destroying ASI hardware controller");
    
    // Disconnect all hardware
    if (eafFocuserConnected_) {
        disconnectEAFFocuser();
    }
    if (efwFilterWheelConnected_) {
        disconnectEFWFilterWheel();
    }
    
    return true;
}

auto HardwareController::getComponentName() const -> std::string {
    return "ASI Hardware Controller";
}

auto HardwareController::onCameraStateChanged(CameraState state) -> void {
    LOG_F(INFO, "ASI hardware controller: Camera state changed to {}", static_cast<int>(state));
    
    // Coordinate hardware during exposures
    if (state == CameraState::EXPOSING && hardwareCoordinationEnabled_) {
        // Ensure hardware is stable during exposure
        if (eafFocuserMoving_ || efwFilterWheelMoving_) {
            LOG_F(WARNING, "Hardware movement detected during exposure start");
        }
    }
}

// EAF (Electronic Auto Focuser) methods
auto HardwareController::hasEAFFocuser() -> bool {
    return hasEAFFocuser_;
}

auto HardwareController::connectEAFFocuser() -> bool {
    std::lock_guard<std::mutex> lock(hardwareMutex_);
    
    if (eafFocuserConnected_) {
        return true;
    }
    
    if (!hasEAFFocuser_) {
        LOG_F(ERROR, "No EAF focuser detected");
        return false;
    }
    
    if (!initializeEAFFocuser()) {
        LOG_F(ERROR, "Failed to initialize EAF focuser");
        return false;
    }
    
    eafFocuserConnected_ = true;
    LOG_F(INFO, "Connected to EAF focuser ID: {}", eafFocuserId_);
    return true;
}

auto HardwareController::disconnectEAFFocuser() -> bool {
    std::lock_guard<std::mutex> lock(hardwareMutex_);
    
    if (!eafFocuserConnected_) {
        return true;
    }
    
    shutdownEAFFocuser();
    eafFocuserConnected_ = false;
    LOG_F(INFO, "Disconnected from EAF focuser");
    return true;
}

auto HardwareController::isEAFFocuserConnected() -> bool {
    return eafFocuserConnected_;
}

auto HardwareController::setEAFFocuserPosition(int position) -> bool {
    std::lock_guard<std::mutex> lock(hardwareMutex_);
    
    if (!eafFocuserConnected_) {
        LOG_F(ERROR, "EAF focuser not connected");
        return false;
    }
    
    if (!validateEAFPosition(position)) {
        LOG_F(ERROR, "Invalid EAF position: {}", position);
        return false;
    }
    
#ifdef LITHIUM_ASI_EAF_ENABLED
    EAF_ERROR_CODE result = EAFMove(eafFocuserId_, position);
    if (result != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to move EAF focuser: {}", result);
        return false;
    }
#else
    // Stub implementation
    eafFocuserMoving_ = true;
    notifyMovementChange("EAF", true);
    
    // Simulate movement time
    std::thread([this, position]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        eafFocuserPosition_ = position;
        eafFocuserMoving_ = false;
        notifyMovementChange("EAF", false);
    }).detach();
#endif
    
    LOG_F(INFO, "Moving EAF focuser to position: {}", position);
    return true;
}

auto HardwareController::getEAFFocuserPosition() -> int {
    if (!eafFocuserConnected_) {
        return 0;
    }
    
#ifdef LITHIUM_ASI_EAF_ENABLED
    int position = 0;
    if (EAFGetPosition(eafFocuserId_, &position) == EAF_SUCCESS) {
        eafFocuserPosition_ = position;
        return position;
    }
    return 0;
#else
    return eafFocuserPosition_;
#endif
}

auto HardwareController::getEAFFocuserMaxPosition() -> int {
    return eafFocuserMaxPosition_;
}

auto HardwareController::isEAFFocuserMoving() -> bool {
#ifdef LITHIUM_ASI_EAF_ENABLED
    if (!eafFocuserConnected_) {
        return false;
    }
    
    bool moving = false;
    if (EAFIsMoving(eafFocuserId_, &moving) == EAF_SUCCESS) {
        eafFocuserMoving_ = moving;
        return moving;
    }
    return false;
#else
    return eafFocuserMoving_;
#endif
}

auto HardwareController::stopEAFFocuser() -> bool {
    if (!eafFocuserConnected_) {
        return false;
    }
    
#ifdef LITHIUM_ASI_EAF_ENABLED
    EAF_ERROR_CODE result = EAFStop(eafFocuserId_);
    if (result != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to stop EAF focuser: {}", result);
        return false;
    }
#else
    eafFocuserMoving_ = false;
    notifyMovementChange("EAF", false);
#endif
    
    LOG_F(INFO, "Stopped EAF focuser movement");
    return true;
}

auto HardwareController::setEAFFocuserStepSize(int stepSize) -> bool {
    if (stepSize < 1 || stepSize > 100) {
        LOG_F(ERROR, "Invalid EAF step size: {}", stepSize);
        return false;
    }
    
    eafFocuserStepSize_ = stepSize;
    LOG_F(INFO, "Set EAF focuser step size to: {}", stepSize);
    return true;
}

auto HardwareController::getEAFFocuserStepSize() -> int {
    return eafFocuserStepSize_;
}

auto HardwareController::homeEAFFocuser() -> bool {
    if (!eafFocuserConnected_) {
        LOG_F(ERROR, "EAF focuser not connected");
        return false;
    }
    
#ifdef LITHIUM_ASI_EAF_ENABLED
    EAF_ERROR_CODE result = EAFResetToZero(eafFocuserId_);
    if (result != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to home EAF focuser: {}", result);
        return false;
    }
#else
    // Simulate homing
    eafFocuserMoving_ = true;
    notifyMovementChange("EAF", true);
    
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        eafFocuserPosition_ = 0;
        eafFocuserMoving_ = false;
        notifyMovementChange("EAF", false);
    }).detach();
#endif
    
    LOG_F(INFO, "Homing EAF focuser");
    return true;
}

auto HardwareController::calibrateEAFFocuser() -> bool {
    if (!eafFocuserConnected_) {
        LOG_F(ERROR, "EAF focuser not connected");
        return false;
    }
    
    LOG_F(INFO, "Calibrating EAF focuser");
    
    // Perform calibration sequence
    if (!homeEAFFocuser()) {
        return false;
    }
    
    // Wait for homing to complete
    if (!waitForEAFMovement(10000)) {
        LOG_F(ERROR, "Timeout waiting for EAF homing");
        return false;
    }
    
    // Move to maximum position to determine range
    if (!setEAFFocuserPosition(eafFocuserMaxPosition_)) {
        return false;
    }
    
    LOG_F(INFO, "EAF focuser calibration completed");
    return true;
}

auto HardwareController::getEAFFocuserTemperature() -> double {
#ifdef LITHIUM_ASI_EAF_ENABLED
    if (!eafFocuserConnected_) {
        return 0.0;
    }
    
    float temperature = 0.0f;
    if (EAFGetTemp(eafFocuserId_, &temperature) == EAF_SUCCESS) {
        eafFocuserTemperature_ = static_cast<double>(temperature);
        return eafFocuserTemperature_;
    }
    return 0.0;
#else
    // Simulate temperature reading
    return 25.0 + (std::rand() % 10 - 5) * 0.1;  // 25°C ±0.5°C
#endif
}

auto HardwareController::enableEAFFocuserBacklashCompensation(bool enable) -> bool {
    eafBacklashCompensation_ = enable;
    LOG_F(INFO, "{} EAF backlash compensation", enable ? "Enabled" : "Disabled");
    return true;
}

auto HardwareController::setEAFFocuserBacklashSteps(int steps) -> bool {
    if (steps < 0 || steps > 999) {
        LOG_F(ERROR, "Invalid EAF backlash steps: {}", steps);
        return false;
    }
    
    eafBacklashSteps_ = steps;
    LOG_F(INFO, "Set EAF backlash steps to: {}", steps);
    return true;
}

auto HardwareController::getEAFFocuserFirmware() -> std::string {
    return eafFocuserFirmware_;
}

// EFW (Electronic Filter Wheel) methods
auto HardwareController::hasEFWFilterWheel() -> bool {
    return hasEFWFilterWheel_;
}

auto HardwareController::connectEFWFilterWheel() -> bool {
    std::lock_guard<std::mutex> lock(hardwareMutex_);
    
    if (efwFilterWheelConnected_) {
        return true;
    }
    
    if (!hasEFWFilterWheel_) {
        LOG_F(ERROR, "No EFW filter wheel detected");
        return false;
    }
    
    if (!initializeEFWFilterWheel()) {
        LOG_F(ERROR, "Failed to initialize EFW filter wheel");
        return false;
    }
    
    efwFilterWheelConnected_ = true;
    LOG_F(INFO, "Connected to EFW filter wheel ID: {}", efwFilterWheelId_);
    return true;
}

auto HardwareController::disconnectEFWFilterWheel() -> bool {
    std::lock_guard<std::mutex> lock(hardwareMutex_);
    
    if (!efwFilterWheelConnected_) {
        return true;
    }
    
    shutdownEFWFilterWheel();
    efwFilterWheelConnected_ = false;
    LOG_F(INFO, "Disconnected from EFW filter wheel");
    return true;
}

auto HardwareController::isEFWFilterWheelConnected() -> bool {
    return efwFilterWheelConnected_;
}

auto HardwareController::setEFWFilterPosition(int position) -> bool {
    std::lock_guard<std::mutex> lock(hardwareMutex_);
    
    if (!efwFilterWheelConnected_) {
        LOG_F(ERROR, "EFW filter wheel not connected");
        return false;
    }
    
    if (!validateEFWPosition(position)) {
        LOG_F(ERROR, "Invalid EFW position: {}", position);
        return false;
    }
    
#ifdef LITHIUM_ASI_EFW_ENABLED
    EFW_ERROR_CODE result = EFWSetPosition(efwFilterWheelId_, position);
    if (result != EFW_SUCCESS) {
        LOG_F(ERROR, "Failed to set EFW position: {}", result);
        return false;
    }
#else
    // Stub implementation
    efwFilterWheelMoving_ = true;
    notifyMovementChange("EFW", true);
    
    std::thread([this, position]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        efwCurrentPosition_ = position;
        efwFilterWheelMoving_ = false;
        notifyMovementChange("EFW", false);
    }).detach();
#endif
    
    LOG_F(INFO, "Moving EFW filter wheel to position: {}", position);
    return true;
}

auto HardwareController::getEFWFilterPosition() -> int {
    if (!efwFilterWheelConnected_) {
        return 1;
    }
    
#ifdef LITHIUM_ASI_EFW_ENABLED
    int position = 1;
    if (EFWGetPosition(efwFilterWheelId_, &position) == EFW_SUCCESS) {
        efwCurrentPosition_ = position;
        return position;
    }
    return 1;
#else
    return efwCurrentPosition_;
#endif
}

auto HardwareController::getEFWFilterCount() -> int {
    return efwFilterCount_;
}

auto HardwareController::isEFWFilterWheelMoving() -> bool {
#ifdef LITHIUM_ASI_EFW_ENABLED
    if (!efwFilterWheelConnected_) {
        return false;
    }
    
    bool moving = false;
    if (EFWGetProperty(efwFilterWheelId_, &moving) == EFW_SUCCESS) {
        efwFilterWheelMoving_ = moving;
        return moving;
    }
    return false;
#else
    return efwFilterWheelMoving_;
#endif
}

auto HardwareController::homeEFWFilterWheel() -> bool {
    if (!efwFilterWheelConnected_) {
        LOG_F(ERROR, "EFW filter wheel not connected");
        return false;
    }
    
    LOG_F(INFO, "Homing EFW filter wheel");
    return setEFWFilterPosition(1);  // Home to position 1
}

auto HardwareController::getEFWFilterWheelFirmware() -> std::string {
    return efwFirmware_;
}

auto HardwareController::setEFWFilterNames(const std::vector<std::string>& names) -> bool {
    if (names.size() > static_cast<size_t>(efwFilterCount_)) {
        LOG_F(ERROR, "Too many filter names: {} (max: {})", names.size(), efwFilterCount_);
        return false;
    }
    
    efwFilterNames_ = names;
    
    // Pad with default names if needed
    while (efwFilterNames_.size() < static_cast<size_t>(efwFilterCount_)) {
        efwFilterNames_.push_back("Filter " + std::to_string(efwFilterNames_.size() + 1));
    }
    
    LOG_F(INFO, "Set EFW filter names");
    return true;
}

auto HardwareController::getEFWFilterNames() -> std::vector<std::string> {
    return efwFilterNames_;
}

auto HardwareController::getEFWUnidirectionalMode() -> bool {
    return efwUnidirectionalMode_;
}

auto HardwareController::setEFWUnidirectionalMode(bool enable) -> bool {
#ifdef LITHIUM_ASI_EFW_ENABLED
    if (!efwFilterWheelConnected_) {
        return false;
    }
    
    EFW_ERROR_CODE result = EFWSetDirection(efwFilterWheelId_, enable ? EFW_UNIDIRECTION : EFW_BIDIRECTION);
    if (result != EFW_SUCCESS) {
        LOG_F(ERROR, "Failed to set EFW direction mode: {}", result);
        return false;
    }
#endif
    
    efwUnidirectionalMode_ = enable;
    LOG_F(INFO, "Set EFW to {} mode", enable ? "unidirectional" : "bidirectional");
    return true;
}

auto HardwareController::calibrateEFWFilterWheel() -> bool {
    if (!efwFilterWheelConnected_) {
        LOG_F(ERROR, "EFW filter wheel not connected");
        return false;
    }
    
    LOG_F(INFO, "Calibrating EFW filter wheel");
    
    // Perform calibration by cycling through all positions
    for (int pos = 1; pos <= efwFilterCount_; ++pos) {
        if (!setEFWFilterPosition(pos)) {
            LOG_F(ERROR, "Failed to move to position {} during calibration", pos);
            return false;
        }
        
        if (!waitForEFWMovement(10000)) {
            LOG_F(ERROR, "Timeout waiting for EFW movement to position {}", pos);
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Return to position 1
    setEFWFilterPosition(1);
    
    LOG_F(INFO, "EFW filter wheel calibration completed");
    return true;
}

// Hardware coordination methods
auto HardwareController::performFocusSequence(const std::vector<int>& positions, 
                                             std::function<void(int, bool)> callback) -> bool {
    return performSequenceWithCallback(
        positions,
        [this](int pos) { return setEAFFocuserPosition(pos); },
        [this]() { return !isEAFFocuserMoving(); },
        callback
    );
}

auto HardwareController::performFilterSequence(const std::vector<int>& positions,
                                              std::function<void(int, bool)> callback) -> bool {
    return performSequenceWithCallback(
        positions,
        [this](int pos) { return setEFWFilterPosition(pos); },
        [this]() { return !isEFWFilterWheelMoving(); },
        callback
    );
}

auto HardwareController::enableHardwareCoordination(bool enable) -> bool {
    hardwareCoordinationEnabled_ = enable;
    LOG_F(INFO, "{} hardware coordination", enable ? "Enabled" : "Disabled");
    return true;
}

auto HardwareController::isHardwareCoordinationEnabled() const -> bool {
    return hardwareCoordinationEnabled_;
}

auto HardwareController::setMovementCallback(std::function<void(const std::string&, bool)> callback) -> void {
    std::lock_guard<std::mutex> lock(hardwareMutex_);
    movementCallback_ = callback;
}

auto HardwareController::enableMovementMonitoring(bool enable) -> bool {
    movementMonitoringEnabled_ = enable;
    LOG_F(INFO, "{} movement monitoring", enable ? "Enabled" : "Disabled");
    return true;
}

auto HardwareController::isMovementMonitoringEnabled() const -> bool {
    return movementMonitoringEnabled_;
}

// Private helper methods
auto HardwareController::detectEAFFocuser() -> bool {
#ifdef LITHIUM_ASI_EAF_ENABLED
    int count = EAFGetNum();
    if (count > 0) {
        hasEAFFocuser_ = true;
        eafFocuserId_ = 0;  // Use first available
        return true;
    }
    return false;
#else
    // Stub implementation - simulate detection
    hasEAFFocuser_ = true;
    eafFocuserId_ = 0;
    eafFocuserMaxPosition_ = 31000;
    eafFocuserFirmware_ = "EAF v2.1 (Stub)";
    LOG_F(INFO, "Detected EAF focuser (stub)");
    return true;
#endif
}

auto HardwareController::detectEFWFilterWheel() -> bool {
#ifdef LITHIUM_ASI_EFW_ENABLED
    int count = EFWGetNum();
    if (count > 0) {
        hasEFWFilterWheel_ = true;
        efwFilterWheelId_ = 0;  // Use first available
        return true;
    }
    return false;
#else
    // Stub implementation - simulate detection
    hasEFWFilterWheel_ = true;
    efwFilterWheelId_ = 0;
    efwFilterCount_ = 7;  // 7-position wheel
    efwFirmware_ = "EFW v1.3 (Stub)";
    setEFWFilterNames({"L", "R", "G", "B", "Ha", "OIII", "SII"});
    LOG_F(INFO, "Detected EFW filter wheel (stub)");
    return true;
#endif
}

auto HardwareController::initializeEAFFocuser() -> bool {
#ifdef LITHIUM_ASI_EAF_ENABLED
    EAF_ERROR_CODE result = EAFOpen(eafFocuserId_);
    if (result != EAF_SUCCESS) {
        LOG_F(ERROR, "Failed to open EAF focuser: {}", result);
        return false;
    }
    
    // Get focuser properties
    EAF_INFO info;
    if (EAFGetProperty(eafFocuserId_, &info) == EAF_SUCCESS) {
        eafFocuserMaxPosition_ = info.MaxStep;
        eafFocuserFirmware_ = std::string(info.Name) + " v" + std::to_string(info.FirmwareVersion);
    }
    
    return true;
#else
    LOG_F(INFO, "Initialized EAF focuser (stub)");
    return true;
#endif
}

auto HardwareController::initializeEFWFilterWheel() -> bool {
#ifdef LITHIUM_ASI_EFW_ENABLED
    EFW_ERROR_CODE result = EFWOpen(efwFilterWheelId_);
    if (result != EFW_SUCCESS) {
        LOG_F(ERROR, "Failed to open EFW filter wheel: {}", result);
        return false;
    }
    
    // Get filter wheel properties
    EFW_INFO info;
    if (EFWGetProperty(efwFilterWheelId_, &info) == EFW_SUCCESS) {
        efwFilterCount_ = info.slotNum;
        efwFirmware_ = std::string(info.Name) + " v" + std::to_string(info.FirmwareVersion);
    }
    
    return true;
#else
    LOG_F(INFO, "Initialized EFW filter wheel (stub)");
    return true;
#endif
}

auto HardwareController::shutdownEAFFocuser() -> bool {
#ifdef LITHIUM_ASI_EAF_ENABLED
    EAFClose(eafFocuserId_);
#endif
    return true;
}

auto HardwareController::shutdownEFWFilterWheel() -> bool {
#ifdef LITHIUM_ASI_EFW_ENABLED
    EFWClose(efwFilterWheelId_);
#endif
    return true;
}

auto HardwareController::waitForEAFMovement(int timeoutMs) -> bool {
    auto start = std::chrono::steady_clock::now();
    while (isEAFFocuserMoving()) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutMs) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
}

auto HardwareController::waitForEFWMovement(int timeoutMs) -> bool {
    auto start = std::chrono::steady_clock::now();
    while (isEFWFilterWheelMoving()) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() > timeoutMs) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
}

auto HardwareController::validateEAFPosition(int position) const -> bool {
    return position >= 0 && position <= eafFocuserMaxPosition_;
}

auto HardwareController::validateEFWPosition(int position) const -> bool {
    return position >= 1 && position <= efwFilterCount_;
}

auto HardwareController::notifyMovementChange(const std::string& device, bool moving) -> void {
    if (movementMonitoringEnabled_) {
        std::lock_guard<std::mutex> lock(hardwareMutex_);
        if (movementCallback_) {
            movementCallback_(device, moving);
        }
    }
}

auto HardwareController::performSequenceWithCallback(const std::vector<int>& positions,
                                                    std::function<bool(int)> mover,
                                                    std::function<bool()> checker,
                                                    std::function<void(int, bool)> callback) -> bool {
    for (size_t i = 0; i < positions.size(); ++i) {
        int position = positions[i];
        
        if (callback) {
            callback(position, false);  // Starting movement
        }
        
        if (!mover(position)) {
            if (callback) {
                callback(position, false);  // Movement failed
            }
            return false;
        }
        
        // Wait for movement to complete
        auto start = std::chrono::steady_clock::now();
        while (!checker()) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 30) {
                LOG_F(ERROR, "Timeout waiting for movement to position {}", position);
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (callback) {
            callback(position, true);  // Movement completed
        }
    }
    
    return true;
}

} // namespace lithium::device::asi::camera
