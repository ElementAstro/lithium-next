/*
 * ascom_dome.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Dome Device Implementation

**************************************************/

#include "ascom_dome.hpp"

#include <spdlog/spdlog.h>
#include <thread>

namespace lithium::client::ascom {

ASCOMDome::ASCOMDome(std::string name, int deviceNumber)
    : ASCOMDeviceBase(std::move(name), ASCOMDeviceType::Dome, deviceNumber) {
    spdlog::debug("ASCOMDome created: {}", name_);
}

ASCOMDome::~ASCOMDome() {
    spdlog::debug("ASCOMDome destroyed: {}", name_);
}

auto ASCOMDome::connect(int timeout) -> bool {
    if (!ASCOMDeviceBase::connect(timeout)) {
        return false;
    }
    refreshCapabilities();
    spdlog::info("Dome {} connected", name_);
    return true;
}

auto ASCOMDome::getCapabilities() -> DomeCapabilities {
    refreshCapabilities();
    return capabilities_;
}

// ==================== Azimuth Control ====================

auto ASCOMDome::slewToAzimuth(double azimuth) -> bool {
    if (!isConnected() || !capabilities_.canSetAzimuth) return false;

    auto response = setProperty("slewtoazimuth", {{"Azimuth", std::to_string(azimuth)}});
    if (response.isSuccess()) {
        domeState_.store(DomeState::Moving);
    }
    return response.isSuccess();
}

auto ASCOMDome::syncToAzimuth(double azimuth) -> bool {
    if (!isConnected() || !capabilities_.canSyncAzimuth) return false;
    return setProperty("synctoazimuth", {{"Azimuth", std::to_string(azimuth)}}).isSuccess();
}

auto ASCOMDome::getAzimuth() -> double {
    return getDoubleProperty("azimuth").value_or(0.0);
}

auto ASCOMDome::isSlewing() -> bool {
    return getBoolProperty("slewing").value_or(false);
}

auto ASCOMDome::abortSlew() -> bool {
    if (!isConnected()) return false;

    auto response = setProperty("abortslew", {});
    if (response.isSuccess()) {
        domeState_.store(DomeState::Idle);
    }
    return response.isSuccess();
}

auto ASCOMDome::waitForSlew(std::chrono::milliseconds timeout) -> bool {
    auto start = std::chrono::steady_clock::now();

    while (isSlewing()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > timeout) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    domeState_.store(DomeState::Idle);
    return true;
}

// ==================== Altitude Control ====================

auto ASCOMDome::slewToAltitude(double altitude) -> bool {
    if (!isConnected() || !capabilities_.canSetAltitude) return false;

    auto response = setProperty("slewtoaltitude", {{"Altitude", std::to_string(altitude)}});
    if (response.isSuccess()) {
        domeState_.store(DomeState::Moving);
    }
    return response.isSuccess();
}

auto ASCOMDome::getAltitude() -> double {
    return getDoubleProperty("altitude").value_or(0.0);
}

// ==================== Shutter Control ====================

auto ASCOMDome::openShutter() -> bool {
    if (!isConnected() || !capabilities_.canSetShutter) return false;
    return setProperty("openshutter", {}).isSuccess();
}

auto ASCOMDome::closeShutter() -> bool {
    if (!isConnected() || !capabilities_.canSetShutter) return false;
    return setProperty("closeshutter", {}).isSuccess();
}

auto ASCOMDome::getShutterStatus() -> ShutterState {
    auto status = getIntProperty("shutterstatus");
    if (status.has_value()) {
        return static_cast<ShutterState>(status.value());
    }
    return ShutterState::Error;
}

// ==================== Parking ====================

auto ASCOMDome::park() -> bool {
    if (!isConnected() || !capabilities_.canPark) return false;

    auto response = setProperty("park", {});
    if (response.isSuccess()) {
        domeState_.store(DomeState::Parking);
    }
    return response.isSuccess();
}

auto ASCOMDome::findHome() -> bool {
    if (!isConnected() || !capabilities_.canFindHome) return false;
    return setProperty("findhome", {}).isSuccess();
}

auto ASCOMDome::isParked() -> bool {
    bool parked = getBoolProperty("atpark").value_or(false);
    if (parked) {
        domeState_.store(DomeState::Parked);
    }
    return parked;
}

auto ASCOMDome::isAtHome() -> bool {
    return getBoolProperty("athome").value_or(false);
}

auto ASCOMDome::setParkedPosition() -> bool {
    if (!capabilities_.canSetPark) return false;
    return setProperty("setpark", {}).isSuccess();
}

// ==================== Slaving ====================

auto ASCOMDome::setSlaved(bool slaved) -> bool {
    if (!capabilities_.canSlave) return false;
    return setBoolProperty("slaved", slaved);
}

auto ASCOMDome::isSlaved() -> bool {
    return getBoolProperty("slaved").value_or(false);
}

// ==================== Status ====================

auto ASCOMDome::getStatus() const -> json {
    json status = ASCOMDeviceBase::getStatus();
    status["domeState"] = static_cast<int>(domeState_.load());
    status["capabilities"] = capabilities_.toJson();
    return status;
}

void ASCOMDome::refreshCapabilities() {
    capabilities_.canFindHome = getBoolProperty("canfindhome").value_or(false);
    capabilities_.canPark = getBoolProperty("canpark").value_or(false);
    capabilities_.canSetAltitude = getBoolProperty("cansetaltitude").value_or(false);
    capabilities_.canSetAzimuth = getBoolProperty("cansetazimuth").value_or(false);
    capabilities_.canSetPark = getBoolProperty("cansetpark").value_or(false);
    capabilities_.canSetShutter = getBoolProperty("cansetshutter").value_or(false);
    capabilities_.canSlave = getBoolProperty("canslave").value_or(false);
    capabilities_.canSyncAzimuth = getBoolProperty("cansyncazimuth").value_or(false);
}

}  // namespace lithium::client::ascom
