/*
 * ascom_rotator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Rotator Device Implementation

**************************************************/

#include "ascom_rotator.hpp"

#include <spdlog/spdlog.h>
#include <thread>

namespace lithium::client::ascom {

ASCOMRotator::ASCOMRotator(std::string name, int deviceNumber)
    : ASCOMDeviceBase(std::move(name), ASCOMDeviceType::Rotator, deviceNumber) {
    spdlog::debug("ASCOMRotator created: {}", name_);
}

ASCOMRotator::~ASCOMRotator() {
    spdlog::debug("ASCOMRotator destroyed: {}", name_);
}

auto ASCOMRotator::connect(int timeout) -> bool {
    if (!ASCOMDeviceBase::connect(timeout)) {
        return false;
    }
    canReverse_ = getBoolProperty("canreverse").value_or(false);
    spdlog::info("Rotator {} connected", name_);
    return true;
}

auto ASCOMRotator::moveTo(double position) -> bool {
    if (!isConnected()) return false;

    auto response = setProperty("move", {{"Position", std::to_string(position)}});
    if (response.isSuccess()) {
        rotatorState_.store(RotatorState::Moving);
    }
    return response.isSuccess();
}

auto ASCOMRotator::moveRelative(double offset) -> bool {
    if (!isConnected()) return false;

    auto response = setProperty("moverelative", {{"Offset", std::to_string(offset)}});
    if (response.isSuccess()) {
        rotatorState_.store(RotatorState::Moving);
    }
    return response.isSuccess();
}

auto ASCOMRotator::halt() -> bool {
    if (!isConnected()) return false;

    auto response = setProperty("halt", {});
    if (response.isSuccess()) {
        rotatorState_.store(RotatorState::Idle);
    }
    return response.isSuccess();
}

auto ASCOMRotator::getPosition() -> double {
    return getDoubleProperty("position").value_or(0.0);
}

auto ASCOMRotator::getMechanicalPosition() -> double {
    return getDoubleProperty("mechanicalposition").value_or(0.0);
}

auto ASCOMRotator::getTargetPosition() -> double {
    return getDoubleProperty("targetposition").value_or(0.0);
}

auto ASCOMRotator::isMoving() -> bool {
    return getBoolProperty("ismoving").value_or(false);
}

auto ASCOMRotator::waitForMove(std::chrono::milliseconds timeout) -> bool {
    auto start = std::chrono::steady_clock::now();

    while (isMoving()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > timeout) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    rotatorState_.store(RotatorState::Idle);
    return true;
}

auto ASCOMRotator::sync(double position) -> bool {
    if (!isConnected()) return false;
    return setProperty("sync", {{"Position", std::to_string(position)}}).isSuccess();
}

auto ASCOMRotator::setReverse(bool reverse) -> bool {
    if (!canReverse_) return false;
    return setBoolProperty("reverse", reverse);
}

auto ASCOMRotator::isReversed() -> bool {
    return getBoolProperty("reverse").value_or(false);
}

auto ASCOMRotator::getStepSize() -> double {
    return getDoubleProperty("stepsize").value_or(1.0);
}

auto ASCOMRotator::canReverse() -> bool {
    return canReverse_;
}

auto ASCOMRotator::getStatus() const -> json {
    json status = ASCOMDeviceBase::getStatus();
    status["rotatorState"] = static_cast<int>(rotatorState_.load());
    status["canReverse"] = canReverse_;
    return status;
}

}  // namespace lithium::client::ascom
