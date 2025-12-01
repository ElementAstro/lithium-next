/*
 * ascom_focuser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Focuser Device Implementation

**************************************************/

#include "ascom_focuser.hpp"

#include <spdlog/spdlog.h>
#include <thread>

namespace lithium::client::ascom {

// ==================== Constructor/Destructor ====================

ASCOMFocuser::ASCOMFocuser(std::string name, int deviceNumber)
    : ASCOMDeviceBase(std::move(name), ASCOMDeviceType::Focuser, deviceNumber) {
    spdlog::debug("ASCOMFocuser created: {}", name_);
}

ASCOMFocuser::~ASCOMFocuser() {
    spdlog::debug("ASCOMFocuser destroyed: {}", name_);
}

// ==================== Connection ====================

auto ASCOMFocuser::connect(int timeout) -> bool {
    if (!ASCOMDeviceBase::connect(timeout)) {
        return false;
    }

    refreshCapabilities();
    spdlog::info("Focuser {} connected", name_);
    return true;
}

// ==================== Capabilities ====================

auto ASCOMFocuser::getCapabilities() -> FocuserCapabilities {
    refreshCapabilities();
    return capabilities_;
}

// ==================== Position Control ====================

auto ASCOMFocuser::moveTo(int position) -> bool {
    if (!isConnected()) {
        setError("Focuser not connected");
        return false;
    }

    if (!capabilities_.absolute) {
        setError("Focuser does not support absolute positioning");
        return false;
    }

    auto response = setProperty("move", {{"Position", std::to_string(position)}});
    if (!response.isSuccess()) {
        setError("Failed to move focuser: " + response.errorMessage);
        return false;
    }

    focuserState_.store(FocuserState::Moving);
    spdlog::info("Focuser {} moving to position {}", name_, position);
    return true;
}

auto ASCOMFocuser::moveRelative(int steps) -> bool {
    if (!isConnected()) {
        setError("Focuser not connected");
        return false;
    }

    int currentPos = getPosition();
    int targetPos = currentPos + steps;

    return moveTo(targetPos);
}

auto ASCOMFocuser::halt() -> bool {
    if (!isConnected()) return false;

    if (!capabilities_.canHalt) {
        return false;
    }

    auto response = setProperty("halt", {});
    focuserState_.store(FocuserState::Idle);
    moveCV_.notify_all();

    return response.isSuccess();
}

auto ASCOMFocuser::isMoving() -> bool {
    return getBoolProperty("ismoving").value_or(false);
}

auto ASCOMFocuser::waitForMove(std::chrono::milliseconds timeout) -> bool {
    auto start = std::chrono::steady_clock::now();

    while (isMoving()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > timeout) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    focuserState_.store(FocuserState::Idle);
    return true;
}

auto ASCOMFocuser::getPosition() -> int {
    return getIntProperty("position").value_or(0);
}

auto ASCOMFocuser::getMaxStep() -> int {
    return getIntProperty("maxstep").value_or(100000);
}

auto ASCOMFocuser::getMaxIncrement() -> int {
    return getIntProperty("maxincrement").value_or(10000);
}

auto ASCOMFocuser::getStepSize() -> double {
    return getDoubleProperty("stepsize").value_or(1.0);
}

// ==================== Temperature ====================

auto ASCOMFocuser::getTemperature() -> std::optional<double> {
    return getDoubleProperty("temperature");
}

auto ASCOMFocuser::setTempComp(bool enable) -> bool {
    if (!capabilities_.tempCompAvailable) {
        return false;
    }
    return setBoolProperty("tempcomp", enable);
}

auto ASCOMFocuser::isTempCompEnabled() -> bool {
    return getBoolProperty("tempcomp").value_or(false);
}

// ==================== Status ====================

auto ASCOMFocuser::getStatus() const -> json {
    json status = ASCOMDeviceBase::getStatus();

    status["focuserState"] = static_cast<int>(focuserState_.load());
    status["capabilities"] = capabilities_.toJson();

    return status;
}

// ==================== Internal Methods ====================

void ASCOMFocuser::refreshCapabilities() {
    capabilities_.absolute = getBoolProperty("absolute").value_or(false);
    capabilities_.canHalt = getBoolProperty("canhalt").value_or(false);
    capabilities_.tempComp = getBoolProperty("tempcomp").value_or(false);
    capabilities_.tempCompAvailable = getBoolProperty("tempcompavailable").value_or(false);

    positionInfo_.maxStep = getMaxStep();
    positionInfo_.maxIncrement = getMaxIncrement();
    positionInfo_.stepSize = getStepSize();
}

}  // namespace lithium::client::ascom
