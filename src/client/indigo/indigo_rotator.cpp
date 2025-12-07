/*
 * indigo_rotator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "indigo_rotator.hpp"
#include <mutex>
#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

class INDIGORotator::Impl {
public:
    explicit Impl(INDIGORotator* parent) : parent_(parent) {}

    void onConnected() {
        LOG_F(INFO, "INDIGO Rotator[{}]: Connected", parent_->getINDIGODeviceName());
    }

    void onDisconnected() {
        moving_ = false;
        LOG_F(INFO, "INDIGO Rotator[{}]: Disconnected", parent_->getINDIGODeviceName());
    }

    void onPropertyUpdated(const Property& property) {
        if (property.name == "ROTATOR_POSITION") {
            for (const auto& elem : property.numberElements) {
                if (elem.name == "POSITION") {
                    status_.position = elem.value;
                    status_.targetPosition = elem.target;
                }
            }
            status_.state = property.state;
            status_.moving = (property.state == PropertyState::Busy);
            moving_ = status_.moving;

            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (movementCallback_ && moving_) {
                movementCallback_(status_.position, status_.targetPosition);
            }
        } else if (property.name == "ROTATOR_DIRECTION") {
            for (const auto& elem : property.switchElements) {
                if (elem.name == "REVERSED" && elem.value) {
                    status_.reversed = true;
                } else if (elem.name == "NORMAL" && elem.value) {
                    status_.reversed = false;
                }
            }
        }
    }

    auto moveToAngle(double angle) -> DeviceResult<bool> {
        auto result = parent_->setNumberProperty("ROTATOR_POSITION", "POSITION", angle);
        if (result.has_value()) {
            moving_ = true;
            status_.targetPosition = angle;
            LOG_F(INFO, "INDIGO Rotator[{}]: Moving to {:.2f}°",
                  parent_->getINDIGODeviceName(), angle);
        }
        return result;
    }

    auto moveRelative(double degrees) -> DeviceResult<bool> {
        return parent_->setNumberProperty("ROTATOR_RELATIVE_MOVE", "RELATIVE_MOVE", degrees);
    }

    auto abortMove() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("ROTATOR_ABORT_MOTION", "ABORT", true);
        if (result.has_value()) {
            moving_ = false;
        }
        return result;
    }

    auto syncAngle(double angle) -> DeviceResult<bool> {
        return parent_->setNumberProperty("ROTATOR_SYNC", "SYNC", angle);
    }

    [[nodiscard]] auto isMoving() const -> bool { return moving_; }
    [[nodiscard]] auto getAngle() const -> double { return status_.position; }
    [[nodiscard]] auto getTargetAngle() const -> double { return status_.targetPosition; }

    auto setReverse(bool reverse) -> DeviceResult<bool> {
        return parent_->setSwitchProperty("ROTATOR_DIRECTION",
                                          reverse ? "REVERSED" : "NORMAL", true);
    }

    [[nodiscard]] auto isReversed() const -> bool { return status_.reversed; }

    auto setBacklash(double degrees) -> DeviceResult<bool> {
        return parent_->setNumberProperty("ROTATOR_BACKLASH", "BACKLASH", degrees);
    }

    [[nodiscard]] auto getBacklash() const -> double {
        return parent_->getNumberValue("ROTATOR_BACKLASH", "BACKLASH").value_or(0.0);
    }

    void setMovementCallback(RotatorMovementCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        movementCallback_ = std::move(callback);
    }

    [[nodiscard]] auto getRotatorStatus() const -> RotatorStatus { return status_; }

    [[nodiscard]] auto getStatus() const -> json {
        return {
            {"connected", parent_->isConnected()},
            {"position", status_.position},
            {"targetPosition", status_.targetPosition},
            {"moving", moving_.load()},
            {"reversed", status_.reversed}
        };
    }

private:
    INDIGORotator* parent_;
    std::atomic<bool> moving_{false};
    RotatorStatus status_;
    std::mutex callbackMutex_;
    RotatorMovementCallback movementCallback_;
};

INDIGORotator::INDIGORotator(const std::string& deviceName)
    : INDIGODeviceBase(deviceName, "Rotator"),
      rotatorImpl_(std::make_unique<Impl>(this)) {}

INDIGORotator::~INDIGORotator() = default;

auto INDIGORotator::moveToAngle(double angle) -> DeviceResult<bool> {
    return rotatorImpl_->moveToAngle(angle);
}

auto INDIGORotator::moveRelative(double degrees) -> DeviceResult<bool> {
    return rotatorImpl_->moveRelative(degrees);
}

auto INDIGORotator::abortMove() -> DeviceResult<bool> {
    return rotatorImpl_->abortMove();
}

auto INDIGORotator::syncAngle(double angle) -> DeviceResult<bool> {
    return rotatorImpl_->syncAngle(angle);
}

auto INDIGORotator::isMoving() const -> bool {
    return rotatorImpl_->isMoving();
}

auto INDIGORotator::getAngle() const -> double {
    return rotatorImpl_->getAngle();
}

auto INDIGORotator::getTargetAngle() const -> double {
    return rotatorImpl_->getTargetAngle();
}

auto INDIGORotator::setReverse(bool reverse) -> DeviceResult<bool> {
    return rotatorImpl_->setReverse(reverse);
}

auto INDIGORotator::isReversed() const -> bool {
    return rotatorImpl_->isReversed();
}

auto INDIGORotator::setBacklash(double degrees) -> DeviceResult<bool> {
    return rotatorImpl_->setBacklash(degrees);
}

auto INDIGORotator::getBacklash() const -> double {
    return rotatorImpl_->getBacklash();
}

void INDIGORotator::setMovementCallback(RotatorMovementCallback callback) {
    rotatorImpl_->setMovementCallback(std::move(callback));
}

auto INDIGORotator::getRotatorStatus() const -> RotatorStatus {
    return rotatorImpl_->getRotatorStatus();
}

auto INDIGORotator::getStatus() const -> json {
    return rotatorImpl_->getStatus();
}

void INDIGORotator::onConnected() {
    INDIGODeviceBase::onConnected();
    rotatorImpl_->onConnected();
}

void INDIGORotator::onDisconnected() {
    INDIGODeviceBase::onDisconnected();
    rotatorImpl_->onDisconnected();
}

void INDIGORotator::onPropertyUpdated(const Property& property) {
    INDIGODeviceBase::onPropertyUpdated(property);
    rotatorImpl_->onPropertyUpdated(property);
}

}  // namespace lithium::client::indigo
