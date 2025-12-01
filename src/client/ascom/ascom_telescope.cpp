/*
 * ascom_telescope.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Telescope Device Implementation

**************************************************/

#include "ascom_telescope.hpp"

#include <spdlog/spdlog.h>
#include <thread>

namespace lithium::client::ascom {

ASCOMTelescope::ASCOMTelescope(std::string name, int deviceNumber)
    : ASCOMDeviceBase(std::move(name), ASCOMDeviceType::Telescope, deviceNumber) {
    spdlog::debug("ASCOMTelescope created: {}", name_);
}

ASCOMTelescope::~ASCOMTelescope() {
    spdlog::debug("ASCOMTelescope destroyed: {}", name_);
}

auto ASCOMTelescope::connect(int timeout) -> bool {
    if (!ASCOMDeviceBase::connect(timeout)) {
        return false;
    }
    refreshCapabilities();
    spdlog::info("Telescope {} connected", name_);
    return true;
}

auto ASCOMTelescope::getCapabilities() -> TelescopeCapabilities {
    refreshCapabilities();
    return capabilities_;
}

// ==================== Coordinates ====================

auto ASCOMTelescope::getRightAscension() -> double {
    return getDoubleProperty("rightascension").value_or(0.0);
}

auto ASCOMTelescope::getDeclination() -> double {
    return getDoubleProperty("declination").value_or(0.0);
}

auto ASCOMTelescope::getEquatorialCoords() -> EquatorialCoords {
    return {getRightAscension(), getDeclination()};
}

auto ASCOMTelescope::getAltitude() -> double {
    return getDoubleProperty("altitude").value_or(0.0);
}

auto ASCOMTelescope::getAzimuth() -> double {
    return getDoubleProperty("azimuth").value_or(0.0);
}

auto ASCOMTelescope::getHorizontalCoords() -> HorizontalCoords {
    return {getAltitude(), getAzimuth()};
}

auto ASCOMTelescope::getSiderealTime() -> double {
    return getDoubleProperty("siderealtime").value_or(0.0);
}

// ==================== Slewing ====================

auto ASCOMTelescope::slewToCoordinates(double ra, double dec) -> bool {
    if (!isConnected() || !capabilities_.canSlew) return false;

    auto response = setProperty("slewtocoordinates",
                                {{"RightAscension", std::to_string(ra)},
                                 {"Declination", std::to_string(dec)}});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Slewing);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::slewToCoordinatesAsync(double ra, double dec) -> bool {
    if (!isConnected() || !capabilities_.canSlewAsync) return false;

    auto response = setProperty("slewtocoordinatesasync",
                                {{"RightAscension", std::to_string(ra)},
                                 {"Declination", std::to_string(dec)}});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Slewing);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::slewToAltAz(double alt, double az) -> bool {
    if (!isConnected() || !capabilities_.canSlewAltAz) return false;

    auto response = setProperty("slewtoaltaz",
                                {{"Altitude", std::to_string(alt)},
                                 {"Azimuth", std::to_string(az)}});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Slewing);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::slewToAltAzAsync(double alt, double az) -> bool {
    if (!isConnected() || !capabilities_.canSlewAltAzAsync) return false;

    auto response = setProperty("slewtoaltazasync",
                                {{"Altitude", std::to_string(alt)},
                                 {"Azimuth", std::to_string(az)}});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Slewing);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::slewToTarget() -> bool {
    if (!isConnected() || !capabilities_.canSlew) return false;

    auto response = setProperty("slewtotarget", {});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Slewing);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::slewToTargetAsync() -> bool {
    if (!isConnected() || !capabilities_.canSlewAsync) return false;

    auto response = setProperty("slewtotargetasync", {});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Slewing);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::abortSlew() -> bool {
    if (!isConnected()) return false;

    auto response = setProperty("abortslew", {});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Idle);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::isSlewing() -> bool {
    return getBoolProperty("slewing").value_or(false);
}

auto ASCOMTelescope::waitForSlew(std::chrono::milliseconds timeout) -> bool {
    auto start = std::chrono::steady_clock::now();

    while (isSlewing()) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed > timeout) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    telescopeState_.store(TelescopeState::Idle);
    return true;
}

// ==================== Sync ====================

auto ASCOMTelescope::syncToCoordinates(double ra, double dec) -> bool {
    if (!isConnected() || !capabilities_.canSync) return false;

    return setProperty("synctocoordinates",
                       {{"RightAscension", std::to_string(ra)},
                        {"Declination", std::to_string(dec)}})
        .isSuccess();
}

auto ASCOMTelescope::syncToAltAz(double alt, double az) -> bool {
    if (!isConnected() || !capabilities_.canSyncAltAz) return false;

    return setProperty("synctoaltaz",
                       {{"Altitude", std::to_string(alt)},
                        {"Azimuth", std::to_string(az)}})
        .isSuccess();
}

auto ASCOMTelescope::syncToTarget() -> bool {
    if (!isConnected() || !capabilities_.canSync) return false;
    return setProperty("synctotarget", {}).isSuccess();
}

// ==================== Target ====================

auto ASCOMTelescope::setTargetRightAscension(double ra) -> bool {
    return setDoubleProperty("targetrightascension", ra);
}

auto ASCOMTelescope::setTargetDeclination(double dec) -> bool {
    return setDoubleProperty("targetdeclination", dec);
}

auto ASCOMTelescope::getTargetRightAscension() -> double {
    return getDoubleProperty("targetrightascension").value_or(0.0);
}

auto ASCOMTelescope::getTargetDeclination() -> double {
    return getDoubleProperty("targetdeclination").value_or(0.0);
}

// ==================== Tracking ====================

auto ASCOMTelescope::setTracking(bool enable) -> bool {
    if (!capabilities_.canSetTracking) return false;
    bool result = setBoolProperty("tracking", enable);
    if (result) {
        telescopeState_.store(enable ? TelescopeState::Tracking : TelescopeState::Idle);
    }
    return result;
}

auto ASCOMTelescope::isTracking() -> bool {
    return getBoolProperty("tracking").value_or(false);
}

auto ASCOMTelescope::setTrackingRate(DriveRate rate) -> bool {
    return setIntProperty("trackingrate", static_cast<int>(rate));
}

auto ASCOMTelescope::getTrackingRate() -> DriveRate {
    return static_cast<DriveRate>(getIntProperty("trackingrate").value_or(0));
}

auto ASCOMTelescope::setRightAscensionRate(double rate) -> bool {
    if (!capabilities_.canSetRightAscensionRate) return false;
    return setDoubleProperty("rightascensionrate", rate);
}

auto ASCOMTelescope::setDeclinationRate(double rate) -> bool {
    if (!capabilities_.canSetDeclinationRate) return false;
    return setDoubleProperty("declinationrate", rate);
}

auto ASCOMTelescope::getRightAscensionRate() -> double {
    return getDoubleProperty("rightascensionrate").value_or(0.0);
}

auto ASCOMTelescope::getDeclinationRate() -> double {
    return getDoubleProperty("declinationrate").value_or(0.0);
}

// ==================== Parking ====================

auto ASCOMTelescope::park() -> bool {
    if (!isConnected() || !capabilities_.canPark) return false;

    auto response = setProperty("park", {});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Parked);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::unpark() -> bool {
    if (!isConnected() || !capabilities_.canUnpark) return false;

    auto response = setProperty("unpark", {});
    if (response.isSuccess()) {
        telescopeState_.store(TelescopeState::Idle);
    }
    return response.isSuccess();
}

auto ASCOMTelescope::isParked() -> bool {
    return getBoolProperty("atpark").value_or(false);
}

auto ASCOMTelescope::findHome() -> bool {
    if (!isConnected() || !capabilities_.canFindHome) return false;
    return setProperty("findhome", {}).isSuccess();
}

auto ASCOMTelescope::isAtHome() -> bool {
    return getBoolProperty("athome").value_or(false);
}

auto ASCOMTelescope::setParkedPosition() -> bool {
    if (!capabilities_.canSetPark) return false;
    return setProperty("setpark", {}).isSuccess();
}

// ==================== Guiding ====================

auto ASCOMTelescope::pulseGuide(GuideDirection direction, int duration) -> bool {
    if (!isConnected() || !capabilities_.canPulseGuide) return false;

    return setProperty("pulseguide",
                       {{"Direction", std::to_string(static_cast<int>(direction))},
                        {"Duration", std::to_string(duration)}})
        .isSuccess();
}

auto ASCOMTelescope::isPulseGuiding() -> bool {
    return getBoolProperty("ispulseguiding").value_or(false);
}

auto ASCOMTelescope::setGuideRateRightAscension(double rate) -> bool {
    if (!capabilities_.canSetGuideRates) return false;
    return setDoubleProperty("guideraterightascension", rate);
}

auto ASCOMTelescope::setGuideRateDeclination(double rate) -> bool {
    if (!capabilities_.canSetGuideRates) return false;
    return setDoubleProperty("guideratedeclination", rate);
}

auto ASCOMTelescope::getGuideRateRightAscension() -> double {
    return getDoubleProperty("guideraterightascension").value_or(0.0);
}

auto ASCOMTelescope::getGuideRateDeclination() -> double {
    return getDoubleProperty("guideratedeclination").value_or(0.0);
}

// ==================== Motion ====================

auto ASCOMTelescope::moveAxis(int axis, double rate) -> bool {
    if (!isConnected() || !capabilities_.canMoveAxis) return false;

    return setProperty("moveaxis",
                       {{"Axis", std::to_string(axis)},
                        {"Rate", std::to_string(rate)}})
        .isSuccess();
}

// ==================== Info ====================

auto ASCOMTelescope::getAlignmentMode() -> AlignmentMode {
    return static_cast<AlignmentMode>(getIntProperty("alignmentmode").value_or(0));
}

auto ASCOMTelescope::getPierSide() -> PierSide {
    return static_cast<PierSide>(getIntProperty("sideofpier").value_or(-1));
}

auto ASCOMTelescope::getApertureArea() -> double {
    return getDoubleProperty("aperturearea").value_or(0.0);
}

auto ASCOMTelescope::getApertureDiameter() -> double {
    return getDoubleProperty("aperturediameter").value_or(0.0);
}

auto ASCOMTelescope::getFocalLength() -> double {
    return getDoubleProperty("focallength").value_or(0.0);
}

// ==================== Status ====================

auto ASCOMTelescope::getStatus() const -> json {
    json status = ASCOMDeviceBase::getStatus();
    status["telescopeState"] = static_cast<int>(telescopeState_.load());
    status["capabilities"] = capabilities_.toJson();
    return status;
}

void ASCOMTelescope::refreshCapabilities() {
    capabilities_.canFindHome = getBoolProperty("canfindhome").value_or(false);
    capabilities_.canMoveAxis = getBoolProperty("canmoveaxis").value_or(false);
    capabilities_.canPark = getBoolProperty("canpark").value_or(false);
    capabilities_.canPulseGuide = getBoolProperty("canpulseguide").value_or(false);
    capabilities_.canSetDeclinationRate = getBoolProperty("cansetdeclinationrate").value_or(false);
    capabilities_.canSetGuideRates = getBoolProperty("cansetguiderates").value_or(false);
    capabilities_.canSetPark = getBoolProperty("cansetpark").value_or(false);
    capabilities_.canSetPierSide = getBoolProperty("cansetpierside").value_or(false);
    capabilities_.canSetRightAscensionRate = getBoolProperty("cansetrightascensionrate").value_or(false);
    capabilities_.canSetTracking = getBoolProperty("cansettracking").value_or(false);
    capabilities_.canSlew = getBoolProperty("canslew").value_or(false);
    capabilities_.canSlewAltAz = getBoolProperty("canslewaltaz").value_or(false);
    capabilities_.canSlewAltAzAsync = getBoolProperty("canslewaltazasync").value_or(false);
    capabilities_.canSlewAsync = getBoolProperty("canslewasync").value_or(false);
    capabilities_.canSync = getBoolProperty("cansync").value_or(false);
    capabilities_.canSyncAltAz = getBoolProperty("cansyncaltaz").value_or(false);
    capabilities_.canUnpark = getBoolProperty("canunpark").value_or(false);
}

}  // namespace lithium::client::ascom
