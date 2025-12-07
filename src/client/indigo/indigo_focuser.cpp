/*
 * indigo_focuser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Focuser Device Implementation

**************************************************/

#include "indigo_focuser.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// INDIGOFocuser Implementation
// ============================================================================

class INDIGOFocuser::Impl {
public:
    explicit Impl(INDIGOFocuser* parent) : parent_(parent) {}

    void onConnected() {
        LOG_F(INFO, "INDIGO Focuser[{}]: Connected and initialized",
              parent_->getINDIGODeviceName());
    }

    void onDisconnected() {
        isMoving_ = false;
        LOG_F(INFO, "INDIGO Focuser[{}]: Disconnected",
              parent_->getINDIGODeviceName());
    }

    void onPropertyUpdated(const Property& property) {
        const auto& name = property.name;

        if (name == "FOCUSER_POSITION") {
            handlePositionUpdate(property);
        } else if (name == "FOCUSER_SPEED") {
            handleSpeedUpdate(property);
        } else if (name == "FOCUSER_REVERSE_MOTION") {
            handleReverseUpdate(property);
        } else if (name == "FOCUSER_LIMITS") {
            handleLimitsUpdate(property);
        } else if (name == "FOCUSER_TEMPERATURE") {
            handleTemperatureUpdate(property);
        } else if (name == "FOCUSER_BACKLASH") {
            handleBacklashUpdate(property);
        } else if (name == "FOCUSER_ABORT_MOTION") {
            handleAbortUpdate(property);
        }
    }

    // ==================== Position Control ====================

    auto moveToPosition(int position) -> DeviceResult<bool> {
        // Validate position within limits
        if (position < positionInfo_.minPosition ||
            position > positionInfo_.maxPosition) {
            return DeviceResult<bool>::Error(
                "Position " + std::to_string(position) + " out of range [" +
                std::to_string(positionInfo_.minPosition) + ", " +
                std::to_string(positionInfo_.maxPosition) + "]");
        }

        positionInfo_.targetPosition = position;
        auto result = parent_->setNumberProperty(
            "FOCUSER_POSITION", "FOCUSER_POSITION_VALUE",
            static_cast<double>(position));

        if (result.has_value()) {
            isMoving_ = true;
            movementStatus_ = FocuserMovementStatus::Moving;
            LOG_F(INFO, "INDIGO Focuser[{}]: Moving to position {}",
                  parent_->getINDIGODeviceName(), position);
        }

        return result;
    }

    auto moveRelative(int steps) -> DeviceResult<bool> {
        // Calculate absolute position from relative movement
        int newPosition = positionInfo_.currentPosition + steps;

        // Validate bounds
        if (newPosition < positionInfo_.minPosition ||
            newPosition > positionInfo_.maxPosition) {
            return DeviceResult<bool>::Error(
                "Relative move would exceed limits");
        }

        positionInfo_.targetPosition = newPosition;
        auto result = parent_->setNumberProperty(
            "FOCUSER_STEPS", "FOCUSER_STEPS_VALUE",
            static_cast<double>(steps));

        if (result.has_value()) {
            isMoving_ = true;
            movementStatus_ = FocuserMovementStatus::Moving;
            LOG_F(INFO, "INDIGO Focuser[{}]: Moving {} steps (from {} to {})",
                  parent_->getINDIGODeviceName(), steps,
                  positionInfo_.currentPosition, newPosition);
        }

        return result;
    }

    [[nodiscard]] auto getCurrentPosition() const -> int {
        return positionInfo_.currentPosition;
    }

    [[nodiscard]] auto getTargetPosition() const -> int {
        return positionInfo_.targetPosition;
    }

    [[nodiscard]] auto getPositionInfo() const -> FocuserPositionInfo {
        return positionInfo_;
    }

    auto syncPosition(int position) -> DeviceResult<bool> {
        if (position < positionInfo_.minPosition ||
            position > positionInfo_.maxPosition) {
            return DeviceResult<bool>::Error("Position out of range");
        }

        // Sync is typically done by setting position directly
        positionInfo_.currentPosition = position;
        return DeviceResult<bool>::Ok(true);
    }

    [[nodiscard]] auto isMoving() const -> bool { return isMoving_; }

    auto waitForMovement(std::chrono::milliseconds timeout)
        -> DeviceResult<bool> {
        auto deadline = std::chrono::steady_clock::now() + timeout;

        std::unique_lock<std::mutex> lock(moveMutex_);
        while (isMoving_) {
            auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now());

            if (remaining.count() <= 0) {
                return DeviceResult<bool>::Error("Movement timeout");
            }

            moveCondition_.wait_for(lock, remaining);
        }

        return DeviceResult<bool>::Ok(true);
    }

    auto abortMovement() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty(
            "FOCUSER_ABORT_MOTION", "ABORT_MOTION", true);

        if (result.has_value()) {
            isMoving_ = false;
            movementStatus_ = FocuserMovementStatus::Stopped;
            moveCondition_.notify_all();
            LOG_F(INFO, "INDIGO Focuser[{}]: Movement aborted",
                  parent_->getINDIGODeviceName());
        }

        return result;
    }

    // ==================== Speed Control ====================

    auto setSpeed(double speed) -> DeviceResult<bool> {
        // Validate speed within range
        if (speed < speedInfo_.minSpeed || speed > speedInfo_.maxSpeed) {
            return DeviceResult<bool>::Error(
                "Speed " + std::to_string(speed) + " out of range");
        }

        auto result = parent_->setNumberProperty(
            "FOCUSER_SPEED", "FOCUSER_SPEED_VALUE", speed);

        if (result.has_value()) {
            speedInfo_.currentSpeed = speed;
            LOG_F(INFO, "INDIGO Focuser[{}]: Speed set to {:.2f}",
                  parent_->getINDIGODeviceName(), speed);
        }

        return result;
    }

    [[nodiscard]] auto getSpeed() const -> double {
        return speedInfo_.currentSpeed;
    }

    [[nodiscard]] auto getSpeedInfo() const -> FocuserSpeedInfo {
        return speedInfo_;
    }

    // ==================== Direction Control ====================

    auto setDirection(FocuserDirection direction) -> DeviceResult<bool> {
        std::string elementName;
        switch (direction) {
            case FocuserDirection::In:
                elementName = "IN";
                break;
            case FocuserDirection::Out:
                elementName = "OUT";
                break;
            case FocuserDirection::None:
                return DeviceResult<bool>::Error("Invalid direction");
        }

        auto result = parent_->setSwitchProperty(
            "FOCUSER_DIRECTION", elementName, true);

        if (result.has_value()) {
            direction_ = direction;
            LOG_F(INFO, "INDIGO Focuser[{}]: Direction set to {}",
                  parent_->getINDIGODeviceName(),
                  direction == FocuserDirection::In ? "In" : "Out");
        }

        return result;
    }

    [[nodiscard]] auto getDirection() const -> FocuserDirection {
        return direction_;
    }

    auto setReverse(bool reversed) -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty(
            "FOCUSER_REVERSE_MOTION", reversed ? "ON" : "OFF", true);

        if (result.has_value()) {
            isReversed_ = reversed;
            LOG_F(INFO, "INDIGO Focuser[{}]: Reverse motion set to {}",
                  parent_->getINDIGODeviceName(), reversed ? "ON" : "OFF");
        }

        return result;
    }

    [[nodiscard]] auto isReversed() const -> bool { return isReversed_; }

    // ==================== Limits ====================

    auto setMinLimit(int minPos) -> DeviceResult<bool> {
        if (minPos >= positionInfo_.maxPosition) {
            return DeviceResult<bool>::Error(
                "Minimum limit must be less than maximum");
        }

        auto result = parent_->setNumberProperty(
            "FOCUSER_LIMITS", "MIN_POSITION", static_cast<double>(minPos));

        if (result.has_value()) {
            positionInfo_.minPosition = minPos;
            LOG_F(INFO, "INDIGO Focuser[{}]: Min limit set to {}",
                  parent_->getINDIGODeviceName(), minPos);
        }

        return result;
    }

    auto setMaxLimit(int maxPos) -> DeviceResult<bool> {
        if (maxPos <= positionInfo_.minPosition) {
            return DeviceResult<bool>::Error(
                "Maximum limit must be greater than minimum");
        }

        auto result = parent_->setNumberProperty(
            "FOCUSER_LIMITS", "MAX_POSITION", static_cast<double>(maxPos));

        if (result.has_value()) {
            positionInfo_.maxPosition = maxPos;
            LOG_F(INFO, "INDIGO Focuser[{}]: Max limit set to {}",
                  parent_->getINDIGODeviceName(), maxPos);
        }

        return result;
    }

    [[nodiscard]] auto getMinLimit() const -> int {
        return positionInfo_.minPosition;
    }

    [[nodiscard]] auto getMaxLimit() const -> int {
        return positionInfo_.maxPosition;
    }

    // ==================== Temperature Compensation ====================

    auto setTemperatureCompensation(bool enabled) -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty(
            "FOCUSER_TEMPERATURE_COMPENSATION",
            enabled ? "ON" : "OFF", true);

        if (result.has_value()) {
            tempCompInfo_.enabled = enabled;
            LOG_F(INFO, "INDIGO Focuser[{}]: Temperature compensation {}",
                  parent_->getINDIGODeviceName(), enabled ? "ON" : "OFF");
        }

        return result;
    }

    auto setTemperatureCoefficient(double coefficient) -> DeviceResult<bool> {
        auto result = parent_->setNumberProperty(
            "FOCUSER_TEMPERATURE_COEFFICIENT",
            "TEMPERATURE_COEFFICIENT", coefficient);

        if (result.has_value()) {
            tempCompInfo_.coefficient = coefficient;
            LOG_F(INFO,
                  "INDIGO Focuser[{}]: Temperature coefficient set to {:.4f}",
                  parent_->getINDIGODeviceName(), coefficient);
        }

        return result;
    }

    [[nodiscard]] auto isTemperatureCompensationEnabled() const -> bool {
        return tempCompInfo_.enabled;
    }

    [[nodiscard]] auto getTemperatureCompensationInfo() const
        -> TemperatureCompensationInfo {
        return tempCompInfo_;
    }

    // ==================== Backlash Compensation ====================

    auto setBacklashCompensation(bool enabled) -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty(
            "FOCUSER_BACKLASH_COMPENSATION",
            enabled ? "ON" : "OFF", true);

        if (result.has_value()) {
            backlashInfo_.enabled = enabled;
            LOG_F(INFO, "INDIGO Focuser[{}]: Backlash compensation {}",
                  parent_->getINDIGODeviceName(), enabled ? "ON" : "OFF");
        }

        return result;
    }

    auto setBacklashSteps(int steps) -> DeviceResult<bool> {
        if (steps < 0) {
            return DeviceResult<bool>::Error("Backlash steps must be >= 0");
        }

        auto result = parent_->setNumberProperty(
            "FOCUSER_BACKLASH", "BACKLASH_STEPS", static_cast<double>(steps));

        if (result.has_value()) {
            backlashInfo_.steps = steps;
            LOG_F(INFO, "INDIGO Focuser[{}]: Backlash steps set to {}",
                  parent_->getINDIGODeviceName(), steps);
        }

        return result;
    }

    [[nodiscard]] auto getBacklashCompensationInfo() const
        -> BacklashCompensationInfo {
        return backlashInfo_;
    }

    // ==================== Movement Callbacks ====================

    void setMovementCallback(FocuserMovementCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        movementCallback_ = std::move(callback);
    }

    // ==================== Status ====================

    [[nodiscard]] auto getMovementStatus() const -> FocuserMovementStatus {
        return movementStatus_;
    }

    [[nodiscard]] auto getStatus() const -> json {
        json status;
        status["connected"] = parent_->isConnected();

        status["position"] = positionInfo_.toJson();
        status["speed"] = speedInfo_.toJson();

        status["direction"] =
            direction_ == FocuserDirection::In ? "In" : "Out";
        status["reversed"] = isReversed_;
        status["isMoving"] = isMoving_;

        status["temperatureCompensation"] = tempCompInfo_.toJson();
        status["backlashCompensation"] = backlashInfo_.toJson();

        status["movementStatus"] = static_cast<int>(movementStatus_);

        return status;
    }

private:
    void handlePositionUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "FOCUSER_POSITION_VALUE") {
                int newPosition = static_cast<int>(elem.value);
                positionInfo_.currentPosition = newPosition;

                // Check if reached target
                if (newPosition == positionInfo_.targetPosition &&
                    property.state == PropertyState::Ok) {
                    isMoving_ = false;
                    movementStatus_ = FocuserMovementStatus::Idle;
                    moveCondition_.notify_all();
                    LOG_F(INFO, "INDIGO Focuser[{}]: Movement complete, at {}",
                          parent_->getINDIGODeviceName(), newPosition);
                } else if (property.state == PropertyState::Alert) {
                    isMoving_ = false;
                    movementStatus_ = FocuserMovementStatus::Error;
                    moveCondition_.notify_all();
                    LOG_F(WARNING,
                          "INDIGO Focuser[{}]: Movement error at position {}",
                          parent_->getINDIGODeviceName(), newPosition);
                }

                // Call movement callback
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (movementCallback_) {
                    movementCallback_(positionInfo_.currentPosition,
                                      positionInfo_.targetPosition);
                }
                break;
            }
        }
    }

    void handleSpeedUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "FOCUSER_SPEED_VALUE") {
                speedInfo_.currentSpeed = elem.value;
                speedInfo_.minSpeed = elem.min;
                speedInfo_.maxSpeed = elem.max;
                break;
            }
        }
    }

    void handleReverseUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.name == "ON") {
                isReversed_ = elem.value;
                break;
            }
        }
    }

    void handleLimitsUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "MIN_POSITION") {
                positionInfo_.minPosition = static_cast<int>(elem.value);
            } else if (elem.name == "MAX_POSITION") {
                positionInfo_.maxPosition = static_cast<int>(elem.value);
            }
        }
    }

    void handleTemperatureUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "TEMPERATURE_VALUE") {
                tempCompInfo_.lastTemperature = elem.value;
                break;
            }
        }
    }

    void handleBacklashUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "BACKLASH_STEPS") {
                backlashInfo_.steps = static_cast<int>(elem.value);
                break;
            }
        }
    }

    void handleAbortUpdate(const Property& property) {
        if (property.state == PropertyState::Ok) {
            isMoving_ = false;
            movementStatus_ = FocuserMovementStatus::Stopped;
            moveCondition_.notify_all();
        }
    }

    INDIGOFocuser* parent_;

    // Position tracking
    FocuserPositionInfo positionInfo_;
    mutable std::mutex positionMutex_;

    // Speed tracking
    FocuserSpeedInfo speedInfo_;
    mutable std::mutex speedMutex_;

    // Direction
    FocuserDirection direction_{FocuserDirection::None};
    std::atomic<bool> isReversed_{false};

    // Movement state
    std::atomic<bool> isMoving_{false};
    FocuserMovementStatus movementStatus_{FocuserMovementStatus::Idle};
    std::mutex moveMutex_;
    std::condition_variable moveCondition_;

    // Temperature compensation
    TemperatureCompensationInfo tempCompInfo_;
    mutable std::mutex tempCompMutex_;

    // Backlash compensation
    BacklashCompensationInfo backlashInfo_;
    mutable std::mutex backlashMutex_;

    // Callbacks
    std::mutex callbackMutex_;
    FocuserMovementCallback movementCallback_;
};

// ============================================================================
// INDIGOFocuser public interface
// ============================================================================

INDIGOFocuser::INDIGOFocuser(const std::string& deviceName)
    : INDIGODeviceBase(deviceName, "Focuser"),
      focuserImpl_(std::make_unique<Impl>(this)) {}

INDIGOFocuser::~INDIGOFocuser() = default;

// ==================== Position Control ====================

auto INDIGOFocuser::moveToPosition(int position) -> DeviceResult<bool> {
    return focuserImpl_->moveToPosition(position);
}

auto INDIGOFocuser::moveRelative(int steps) -> DeviceResult<bool> {
    return focuserImpl_->moveRelative(steps);
}

auto INDIGOFocuser::getCurrentPosition() const -> int {
    return focuserImpl_->getCurrentPosition();
}

auto INDIGOFocuser::getTargetPosition() const -> int {
    return focuserImpl_->getTargetPosition();
}

auto INDIGOFocuser::getPositionInfo() const -> FocuserPositionInfo {
    return focuserImpl_->getPositionInfo();
}

auto INDIGOFocuser::syncPosition(int position) -> DeviceResult<bool> {
    return focuserImpl_->syncPosition(position);
}

auto INDIGOFocuser::isMoving() const -> bool {
    return focuserImpl_->isMoving();
}

auto INDIGOFocuser::waitForMovement(std::chrono::milliseconds timeout)
    -> DeviceResult<bool> {
    return focuserImpl_->waitForMovement(timeout);
}

auto INDIGOFocuser::abortMovement() -> DeviceResult<bool> {
    return focuserImpl_->abortMovement();
}

// ==================== Speed Control ====================

auto INDIGOFocuser::setSpeed(double speed) -> DeviceResult<bool> {
    return focuserImpl_->setSpeed(speed);
}

auto INDIGOFocuser::getSpeed() const -> double {
    return focuserImpl_->getSpeed();
}

auto INDIGOFocuser::getSpeedInfo() const -> FocuserSpeedInfo {
    return focuserImpl_->getSpeedInfo();
}

// ==================== Direction Control ====================

auto INDIGOFocuser::setDirection(FocuserDirection direction)
    -> DeviceResult<bool> {
    return focuserImpl_->setDirection(direction);
}

auto INDIGOFocuser::getDirection() const -> FocuserDirection {
    return focuserImpl_->getDirection();
}

auto INDIGOFocuser::setReverse(bool reversed) -> DeviceResult<bool> {
    return focuserImpl_->setReverse(reversed);
}

auto INDIGOFocuser::isReversed() const -> bool {
    return focuserImpl_->isReversed();
}

// ==================== Limits ====================

auto INDIGOFocuser::setMinLimit(int minPos) -> DeviceResult<bool> {
    return focuserImpl_->setMinLimit(minPos);
}

auto INDIGOFocuser::setMaxLimit(int maxPos) -> DeviceResult<bool> {
    return focuserImpl_->setMaxLimit(maxPos);
}

auto INDIGOFocuser::getMinLimit() const -> int {
    return focuserImpl_->getMinLimit();
}

auto INDIGOFocuser::getMaxLimit() const -> int {
    return focuserImpl_->getMaxLimit();
}

// ==================== Temperature Compensation ====================

auto INDIGOFocuser::setTemperatureCompensation(bool enabled)
    -> DeviceResult<bool> {
    return focuserImpl_->setTemperatureCompensation(enabled);
}

auto INDIGOFocuser::setTemperatureCoefficient(double coefficient)
    -> DeviceResult<bool> {
    return focuserImpl_->setTemperatureCoefficient(coefficient);
}

auto INDIGOFocuser::isTemperatureCompensationEnabled() const -> bool {
    return focuserImpl_->isTemperatureCompensationEnabled();
}

auto INDIGOFocuser::getTemperatureCompensationInfo() const
    -> TemperatureCompensationInfo {
    return focuserImpl_->getTemperatureCompensationInfo();
}

// ==================== Backlash Compensation ====================

auto INDIGOFocuser::setBacklashCompensation(bool enabled)
    -> DeviceResult<bool> {
    return focuserImpl_->setBacklashCompensation(enabled);
}

auto INDIGOFocuser::setBacklashSteps(int steps) -> DeviceResult<bool> {
    return focuserImpl_->setBacklashSteps(steps);
}

auto INDIGOFocuser::getBacklashCompensationInfo() const
    -> BacklashCompensationInfo {
    return focuserImpl_->getBacklashCompensationInfo();
}

// ==================== Movement Callbacks ====================

void INDIGOFocuser::setMovementCallback(FocuserMovementCallback callback) {
    focuserImpl_->setMovementCallback(std::move(callback));
}

// ==================== Status ====================

auto INDIGOFocuser::getMovementStatus() const -> FocuserMovementStatus {
    return focuserImpl_->getMovementStatus();
}

auto INDIGOFocuser::getStatus() const -> json {
    return focuserImpl_->getStatus();
}

void INDIGOFocuser::onConnected() {
    INDIGODeviceBase::onConnected();
    focuserImpl_->onConnected();
}

void INDIGOFocuser::onDisconnected() {
    INDIGODeviceBase::onDisconnected();
    focuserImpl_->onDisconnected();
}

void INDIGOFocuser::onPropertyUpdated(const Property& property) {
    INDIGODeviceBase::onPropertyUpdated(property);
    focuserImpl_->onPropertyUpdated(property);
}

}  // namespace lithium::client::indigo
