/*
 * indi_rotator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Rotator Device Implementation

**************************************************/

#include "indi_rotator.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDIRotator::INDIRotator(std::string name) : INDIDeviceBase(std::move(name)) {
    LOG_DEBUG("INDIRotator created: {}", name_);
}

INDIRotator::~INDIRotator() {
    if (isRotating()) {
        abortRotation();
    }
    LOG_DEBUG("INDIRotator destroyed: {}", name_);
}

// ==================== Connection ====================

auto INDIRotator::connect(const std::string& deviceName, int timeout,
                          int maxRetry) -> bool {
    if (!INDIDeviceBase::connect(deviceName, timeout, maxRetry)) {
        return false;
    }

    setupPropertyWatchers();
    LOG_INFO("Rotator {} connected", deviceName);
    return true;
}

auto INDIRotator::disconnect() -> bool {
    if (isRotating()) {
        abortRotation();
    }
    return INDIDeviceBase::disconnect();
}

// ==================== Angle Control ====================

auto INDIRotator::setAngle(double angle) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set angle: rotator not connected");
        return false;
    }

    if (isRotating()) {
        LOG_WARN("Rotator already rotating");
        return false;
    }

    // Normalize angle to 0-360
    while (angle < 0) angle += 360.0;
    while (angle >= 360) angle -= 360.0;

    LOG_INFO("Setting rotator angle to: {:.2f}°", angle);

    rotatorState_.store(RotatorState::Rotating);
    isRotating_.store(true);

    if (!setNumberProperty("ABS_ROTATOR_ANGLE", "ANGLE", angle)) {
        LOG_ERROR("Failed to set rotator angle");
        rotatorState_.store(RotatorState::Error);
        isRotating_.store(false);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        positionInfo_.targetAngle = angle;
    }

    return true;
}

auto INDIRotator::getAngle() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_.angle;
}

auto INDIRotator::getPositionInfo() const -> RotatorPosition {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_;
}

auto INDIRotator::abortRotation() -> bool {
    if (!isRotating()) {
        return true;
    }

    LOG_INFO("Aborting rotator rotation");

    if (!setSwitchProperty("ROTATOR_ABORT_MOTION", "ABORT", true)) {
        LOG_ERROR("Failed to abort rotation");
        return false;
    }

    rotatorState_.store(RotatorState::Idle);
    isRotating_.store(false);
    rotationCondition_.notify_all();

    return true;
}

auto INDIRotator::isRotating() const -> bool { return isRotating_.load(); }

auto INDIRotator::waitForRotation(std::chrono::milliseconds timeout) -> bool {
    if (!isRotating()) {
        return true;
    }

    std::unique_lock<std::mutex> lock(positionMutex_);
    return rotationCondition_.wait_for(lock, timeout, [this] {
        return !isRotating_.load();
    });
}

// ==================== Synchronization ====================

auto INDIRotator::syncAngle(double angle) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot sync: rotator not connected");
        return false;
    }

    LOG_INFO("Syncing rotator to angle: {:.2f}°", angle);

    if (!setNumberProperty("SYNC_ROTATOR_ANGLE", "ANGLE", angle)) {
        LOG_ERROR("Failed to sync rotator angle");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        positionInfo_.angle = angle;
    }

    return true;
}

// ==================== Reverse ====================

auto INDIRotator::setReversed(bool reversed) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName = reversed ? "ENABLED" : "DISABLED";

    if (!setSwitchProperty("ROTATOR_REVERSE", elemName, true)) {
        LOG_ERROR("Failed to set reverse motion");
        return false;
    }

    isReversed_.store(reversed);
    return true;
}

auto INDIRotator::isReversed() const -> std::optional<bool> {
    return isReversed_.load();
}

// ==================== Status ====================

auto INDIRotator::getRotatorState() const -> RotatorState {
    return rotatorState_.load();
}

auto INDIRotator::getStatus() const -> json {
    json status = INDIDeviceBase::getStatus();

    status["rotatorState"] = static_cast<int>(rotatorState_.load());
    status["isRotating"] = isRotating();
    status["isReversed"] = isReversed_.load();
    status["position"] = getPositionInfo().toJson();

    return status;
}

// ==================== Property Handlers ====================

void INDIRotator::onPropertyDefined(const INDIProperty& property) {
    INDIDeviceBase::onPropertyDefined(property);

    if (property.name == "ABS_ROTATOR_ANGLE") {
        handleAngleProperty(property);
    } else if (property.name == "ROTATOR_REVERSE") {
        handleReverseProperty(property);
    }
}

void INDIRotator::onPropertyUpdated(const INDIProperty& property) {
    INDIDeviceBase::onPropertyUpdated(property);

    if (property.name == "ABS_ROTATOR_ANGLE") {
        handleAngleProperty(property);

        // Check if rotation completed
        if (property.state == PropertyState::Ok && isRotating()) {
            rotatorState_.store(RotatorState::Idle);
            isRotating_.store(false);
            rotationCondition_.notify_all();
        } else if (property.state == PropertyState::Alert) {
            rotatorState_.store(RotatorState::Error);
            isRotating_.store(false);
            rotationCondition_.notify_all();
        }
    } else if (property.name == "ROTATOR_REVERSE") {
        handleReverseProperty(property);
    }
}

// ==================== Internal Methods ====================

void INDIRotator::handleAngleProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "ANGLE") {
            positionInfo_.angle = elem.value;
            positionInfo_.minAngle = elem.min;
            positionInfo_.maxAngle = elem.max;
        }
    }
}

void INDIRotator::handleReverseProperty(const INDIProperty& property) {
    if (auto enabled = property.getSwitch("ENABLED")) {
        isReversed_.store(*enabled);
    }
}

void INDIRotator::setupPropertyWatchers() {
    // Watch angle property
    watchProperty("ABS_ROTATOR_ANGLE", [this](const INDIProperty& prop) {
        handleAngleProperty(prop);
    });
}

}  // namespace lithium::client::indi
