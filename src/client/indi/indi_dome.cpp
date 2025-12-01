/*
 * indi_dome.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Dome Device Implementation

**************************************************/

#include "indi_dome.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDIDome::INDIDome(std::string name) : INDIDeviceBase(std::move(name)) {
    LOG_DEBUG("INDIDome created: {}", name_);
}

INDIDome::~INDIDome() {
    if (isMoving()) {
        abortMotion();
    }
    LOG_DEBUG("INDIDome destroyed: {}", name_);
}

// ==================== Connection ====================

auto INDIDome::connect(const std::string& deviceName, int timeout,
                       int maxRetry) -> bool {
    if (!INDIDeviceBase::connect(deviceName, timeout, maxRetry)) {
        return false;
    }

    setupPropertyWatchers();
    LOG_INFO("Dome {} connected", deviceName);
    return true;
}

auto INDIDome::disconnect() -> bool {
    if (isMoving()) {
        abortMotion();
    }
    return INDIDeviceBase::disconnect();
}

// ==================== Azimuth Control ====================

auto INDIDome::setAzimuth(double azimuth) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set azimuth: dome not connected");
        return false;
    }

    if (isParked()) {
        LOG_ERROR("Cannot move: dome is parked");
        return false;
    }

    // Normalize azimuth to 0-360
    while (azimuth < 0) azimuth += 360.0;
    while (azimuth >= 360) azimuth -= 360.0;

    LOG_INFO("Setting dome azimuth to: {:.2f}°", azimuth);

    domeState_.store(DomeState::Moving);
    isMoving_.store(true);

    if (!setNumberProperty("ABS_DOME_POSITION", "DOME_ABSOLUTE_POSITION",
                           azimuth)) {
        LOG_ERROR("Failed to set dome azimuth");
        domeState_.store(DomeState::Error);
        isMoving_.store(false);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        positionInfo_.targetAzimuth = azimuth;
    }

    return true;
}

auto INDIDome::getAzimuth() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_.azimuth;
}

auto INDIDome::getPositionInfo() const -> DomePosition {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_;
}

auto INDIDome::abortMotion() -> bool {
    LOG_INFO("Aborting dome motion");

    if (!setSwitchProperty("DOME_ABORT_MOTION", "ABORT", true)) {
        LOG_ERROR("Failed to abort motion");
        return false;
    }

    domeState_.store(DomeState::Idle);
    isMoving_.store(false);
    motionCondition_.notify_all();

    return true;
}

auto INDIDome::isMoving() const -> bool { return isMoving_.load(); }

auto INDIDome::waitForMotion(std::chrono::milliseconds timeout) -> bool {
    if (!isMoving()) {
        return true;
    }

    std::unique_lock<std::mutex> lock(positionMutex_);
    return motionCondition_.wait_for(lock, timeout, [this] {
        return !isMoving_.load();
    });
}

auto INDIDome::move(DomeMotion direction) -> bool {
    if (!isConnected()) {
        return false;
    }

    if (isParked()) {
        LOG_ERROR("Cannot move: dome is parked");
        return false;
    }

    std::string elemName = (direction == DomeMotion::Clockwise)
                               ? "DOME_CW"
                               : "DOME_CCW";

    if (!setSwitchProperty("DOME_MOTION", elemName, true)) {
        LOG_ERROR("Failed to start dome motion");
        return false;
    }

    currentMotion_.store(direction);
    domeState_.store(DomeState::Moving);
    isMoving_.store(true);

    return true;
}

auto INDIDome::stop() -> bool {
    setSwitchProperty("DOME_MOTION", "DOME_CW", false);
    setSwitchProperty("DOME_MOTION", "DOME_CCW", false);
    currentMotion_.store(DomeMotion::None);
    return true;
}

// ==================== Shutter Control ====================

auto INDIDome::openShutter() -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot open shutter: dome not connected");
        return false;
    }

    if (!hasShutter()) {
        LOG_ERROR("Dome does not have a shutter");
        return false;
    }

    LOG_INFO("Opening dome shutter");

    domeState_.store(DomeState::Opening);

    if (!setSwitchProperty("DOME_SHUTTER", "SHUTTER_OPEN", true)) {
        LOG_ERROR("Failed to open shutter");
        domeState_.store(DomeState::Error);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(shutterMutex_);
        shutterInfo_.state = ShutterState::Opening;
    }

    return true;
}

auto INDIDome::closeShutter() -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot close shutter: dome not connected");
        return false;
    }

    if (!hasShutter()) {
        LOG_ERROR("Dome does not have a shutter");
        return false;
    }

    LOG_INFO("Closing dome shutter");

    domeState_.store(DomeState::Closing);

    if (!setSwitchProperty("DOME_SHUTTER", "SHUTTER_CLOSE", true)) {
        LOG_ERROR("Failed to close shutter");
        domeState_.store(DomeState::Error);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(shutterMutex_);
        shutterInfo_.state = ShutterState::Closing;
    }

    return true;
}

auto INDIDome::getShutterState() const -> ShutterState {
    std::lock_guard<std::mutex> lock(shutterMutex_);
    return shutterInfo_.state;
}

auto INDIDome::getShutterInfo() const -> ShutterInfo {
    std::lock_guard<std::mutex> lock(shutterMutex_);
    return shutterInfo_;
}

auto INDIDome::hasShutter() const -> bool {
    std::lock_guard<std::mutex> lock(shutterMutex_);
    return shutterInfo_.hasShutter;
}

// ==================== Parking ====================

auto INDIDome::park() -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot park: dome not connected");
        return false;
    }

    if (isParked()) {
        LOG_WARN("Dome already parked");
        return true;
    }

    LOG_INFO("Parking dome");

    domeState_.store(DomeState::Moving);

    if (!setSwitchProperty("DOME_PARK", "PARK", true)) {
        LOG_ERROR("Failed to park dome");
        domeState_.store(DomeState::Error);
        return false;
    }

    return true;
}

auto INDIDome::unpark() -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot unpark: dome not connected");
        return false;
    }

    if (!isParked()) {
        return true;
    }

    LOG_INFO("Unparking dome");

    if (!setSwitchProperty("DOME_PARK", "UNPARK", true)) {
        LOG_ERROR("Failed to unpark dome");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(parkMutex_);
        parkInfo_.parked = false;
    }

    domeState_.store(DomeState::Idle);
    return true;
}

auto INDIDome::isParked() const -> bool {
    std::lock_guard<std::mutex> lock(parkMutex_);
    return parkInfo_.parked;
}

auto INDIDome::setParkPosition(double azimuth) -> bool {
    if (!isConnected()) {
        return false;
    }

    if (!setNumberProperty("DOME_PARK_POSITION", "PARK_AZ", azimuth)) {
        LOG_ERROR("Failed to set park position");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(parkMutex_);
        parkInfo_.parkAzimuth = azimuth;
    }

    return true;
}

auto INDIDome::getDomeParkInfo() const -> DomeParkInfo {
    std::lock_guard<std::mutex> lock(parkMutex_);
    return parkInfo_;
}

// ==================== Telescope Sync ====================

auto INDIDome::enableTelescopeSync(bool enable) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName = enable ? "DOME_AUTOSYNC_ENABLE" : "DOME_AUTOSYNC_DISABLE";

    if (!setSwitchProperty("DOME_AUTOSYNC", elemName, true)) {
        LOG_ERROR("Failed to set telescope sync");
        return false;
    }

    telescopeSyncEnabled_.store(enable);
    return true;
}

auto INDIDome::isTelescopeSyncEnabled() const -> bool {
    return telescopeSyncEnabled_.load();
}

auto INDIDome::syncToTelescope() -> bool {
    if (!isConnected()) {
        return false;
    }

    if (!setSwitchProperty("DOME_GOTO", "DOME_SYNC", true)) {
        LOG_ERROR("Failed to sync to telescope");
        return false;
    }

    return true;
}

// ==================== Status ====================

auto INDIDome::getDomeState() const -> DomeState {
    return domeState_.load();
}

auto INDIDome::getStatus() const -> json {
    json status = INDIDeviceBase::getStatus();

    status["domeState"] = static_cast<int>(domeState_.load());
    status["isMoving"] = isMoving();
    status["telescopeSyncEnabled"] = telescopeSyncEnabled_.load();
    status["position"] = getPositionInfo().toJson();
    status["shutter"] = getShutterInfo().toJson();
    status["park"] = getDomeParkInfo().toJson();

    return status;
}

// ==================== Property Handlers ====================

void INDIDome::onPropertyDefined(const INDIProperty& property) {
    INDIDeviceBase::onPropertyDefined(property);

    if (property.name == "ABS_DOME_POSITION") {
        handleAzimuthProperty(property);
    } else if (property.name == "DOME_SHUTTER") {
        handleShutterProperty(property);
        std::lock_guard<std::mutex> lock(shutterMutex_);
        shutterInfo_.hasShutter = true;
    } else if (property.name == "DOME_PARK") {
        handleParkProperty(property);
    }
}

void INDIDome::onPropertyUpdated(const INDIProperty& property) {
    INDIDeviceBase::onPropertyUpdated(property);

    if (property.name == "ABS_DOME_POSITION") {
        handleAzimuthProperty(property);

        // Check if motion completed
        if (property.state == PropertyState::Ok && isMoving()) {
            domeState_.store(DomeState::Idle);
            isMoving_.store(false);
            motionCondition_.notify_all();
        } else if (property.state == PropertyState::Alert) {
            domeState_.store(DomeState::Error);
            isMoving_.store(false);
            motionCondition_.notify_all();
        }
    } else if (property.name == "DOME_SHUTTER") {
        handleShutterProperty(property);

        // Check shutter state
        if (property.state == PropertyState::Ok) {
            std::lock_guard<std::mutex> lock(shutterMutex_);
            if (auto open = property.getSwitch("SHUTTER_OPEN")) {
                shutterInfo_.state = *open ? ShutterState::Open : ShutterState::Closed;
            }
            domeState_.store(DomeState::Idle);
        }
    } else if (property.name == "DOME_PARK") {
        handleParkProperty(property);

        if (auto parked = property.getSwitch("PARK")) {
            if (*parked) {
                domeState_.store(DomeState::Parked);
            }
        }
    } else if (property.name == "DOME_MOTION") {
        handleMotionProperty(property);
    }
}

// ==================== Internal Methods ====================

void INDIDome::handleAzimuthProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "DOME_ABSOLUTE_POSITION") {
            positionInfo_.azimuth = elem.value;
            positionInfo_.minAzimuth = elem.min;
            positionInfo_.maxAzimuth = elem.max;
        }
    }
}

void INDIDome::handleShutterProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(shutterMutex_);

    if (auto open = property.getSwitch("SHUTTER_OPEN")) {
        if (*open) {
            shutterInfo_.state = ShutterState::Open;
        }
    }
    if (auto close = property.getSwitch("SHUTTER_CLOSE")) {
        if (*close) {
            shutterInfo_.state = ShutterState::Closed;
        }
    }
}

void INDIDome::handleParkProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(parkMutex_);

    parkInfo_.parkEnabled = true;
    if (auto parked = property.getSwitch("PARK")) {
        parkInfo_.parked = *parked;
    }
}

void INDIDome::handleMotionProperty(const INDIProperty& property) {
    if (auto cw = property.getSwitch("DOME_CW")) {
        if (*cw) {
            currentMotion_.store(DomeMotion::Clockwise);
        }
    }
    if (auto ccw = property.getSwitch("DOME_CCW")) {
        if (*ccw) {
            currentMotion_.store(DomeMotion::CounterClockwise);
        }
    }
}

void INDIDome::setupPropertyWatchers() {
    // Watch azimuth property
    watchProperty("ABS_DOME_POSITION", [this](const INDIProperty& prop) {
        handleAzimuthProperty(prop);
    });

    // Watch shutter property
    watchProperty("DOME_SHUTTER", [this](const INDIProperty& prop) {
        handleShutterProperty(prop);
    });

    // Watch park property
    watchProperty("DOME_PARK", [this](const INDIProperty& prop) {
        handleParkProperty(prop);
    });
}

}  // namespace lithium::client::indi
