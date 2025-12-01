/*
 * indi_telescope.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Telescope/Mount Device Implementation

**************************************************/

#include "indi_telescope.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDITelescope::INDITelescope(std::string name)
    : INDIDeviceBase(std::move(name)) {
    LOG_DEBUG("INDITelescope created: {}", name_);
}

INDITelescope::~INDITelescope() {
    if (isSlewing()) {
        abortMotion();
    }
    LOG_DEBUG("INDITelescope destroyed: {}", name_);
}

// ==================== Connection ====================

auto INDITelescope::connect(const std::string& deviceName, int timeout,
                            int maxRetry) -> bool {
    if (!INDIDeviceBase::connect(deviceName, timeout, maxRetry)) {
        return false;
    }

    setupPropertyWatchers();
    LOG_INFO("Telescope {} connected", deviceName);
    return true;
}

auto INDITelescope::disconnect() -> bool {
    if (isSlewing()) {
        abortMotion();
    }
    return INDIDeviceBase::disconnect();
}

// ==================== Coordinates ====================

auto INDITelescope::getRADECJ2000() const -> std::optional<EquatorialCoords> {
    auto prop = getProperty("EQUATORIAL_COORD");
    if (!prop) {
        return std::nullopt;
    }

    EquatorialCoords coords;
    if (auto ra = prop->getNumber("RA")) {
        coords.ra = *ra;
    }
    if (auto dec = prop->getNumber("DEC")) {
        coords.dec = *dec;
    }
    return coords;
}

auto INDITelescope::setRADECJ2000(double ra, double dec) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set coordinates: telescope not connected");
        return false;
    }

    bool success = true;
    success &= setNumberProperty("EQUATORIAL_COORD", "RA", ra);
    success &= setNumberProperty("EQUATORIAL_COORD", "DEC", dec);
    return success;
}

auto INDITelescope::getRADECJNow() const -> std::optional<EquatorialCoords> {
    std::lock_guard<std::mutex> lock(coordsMutex_);
    return currentRADEC_;
}

auto INDITelescope::setRADECJNow(double ra, double dec) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set coordinates: telescope not connected");
        return false;
    }

    bool success = true;
    success &= setNumberProperty("EQUATORIAL_EOD_COORD", "RA", ra);
    success &= setNumberProperty("EQUATORIAL_EOD_COORD", "DEC", dec);
    return success;
}

auto INDITelescope::getTargetRADEC() const -> std::optional<EquatorialCoords> {
    std::lock_guard<std::mutex> lock(coordsMutex_);
    return targetRADEC_;
}

auto INDITelescope::setTargetRADEC(double ra, double dec) -> bool {
    if (!isConnected()) {
        return false;
    }

    bool success = true;
    success &= setNumberProperty("TARGET_EOD_COORD", "RA", ra);
    success &= setNumberProperty("TARGET_EOD_COORD", "DEC", dec);

    if (success) {
        std::lock_guard<std::mutex> lock(coordsMutex_);
        targetRADEC_.ra = ra;
        targetRADEC_.dec = dec;
    }

    return success;
}

auto INDITelescope::getAzAlt() const -> std::optional<HorizontalCoords> {
    std::lock_guard<std::mutex> lock(coordsMutex_);
    return currentAzAlt_;
}

auto INDITelescope::setAzAlt(double az, double alt) -> bool {
    if (!isConnected()) {
        return false;
    }

    bool success = true;
    success &= setNumberProperty("HORIZONTAL_COORD", "AZ", az);
    success &= setNumberProperty("HORIZONTAL_COORD", "ALT", alt);
    return success;
}

// ==================== Slewing ====================

auto INDITelescope::slewToRADEC(double ra, double dec, bool enableTracking)
    -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot slew: telescope not connected");
        return false;
    }

    if (isParked()) {
        LOG_ERROR("Cannot slew: telescope is parked");
        return false;
    }

    LOG_INFO("Slewing to RA={:.4f}h, DEC={:.4f}°", ra, dec);

    // Set on-coord action to slew
    if (!setSwitchProperty("ON_COORD_SET", "TRACK", true)) {
        LOG_WARN("Could not set ON_COORD_SET to TRACK");
    }

    telescopeState_.store(TelescopeState::Slewing);
    isSlewing_.store(true);

    // Set target coordinates
    bool success = setRADECJNow(ra, dec);

    if (!success) {
        LOG_ERROR("Failed to set slew coordinates");
        telescopeState_.store(TelescopeState::Error);
        isSlewing_.store(false);
        return false;
    }

    return true;
}

auto INDITelescope::slewToAzAlt(double az, double alt) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot slew: telescope not connected");
        return false;
    }

    if (isParked()) {
        LOG_ERROR("Cannot slew: telescope is parked");
        return false;
    }

    LOG_INFO("Slewing to AZ={:.4f}°, ALT={:.4f}°", az, alt);

    telescopeState_.store(TelescopeState::Slewing);
    isSlewing_.store(true);

    if (!setAzAlt(az, alt)) {
        LOG_ERROR("Failed to set slew coordinates");
        telescopeState_.store(TelescopeState::Error);
        isSlewing_.store(false);
        return false;
    }

    return true;
}

auto INDITelescope::syncToRADEC(double ra, double dec) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot sync: telescope not connected");
        return false;
    }

    LOG_INFO("Syncing to RA={:.4f}h, DEC={:.4f}°", ra, dec);

    // Set on-coord action to sync
    if (!setSwitchProperty("ON_COORD_SET", "SYNC", true)) {
        LOG_ERROR("Failed to set ON_COORD_SET to SYNC");
        return false;
    }

    return setRADECJNow(ra, dec);
}

auto INDITelescope::abortMotion() -> bool {
    LOG_INFO("Aborting telescope motion");

    if (!setSwitchProperty("TELESCOPE_ABORT_MOTION", "ABORT", true)) {
        LOG_ERROR("Failed to abort motion");
        return false;
    }

    telescopeState_.store(TelescopeState::Idle);
    isSlewing_.store(false);
    slewCondition_.notify_all();

    return true;
}

auto INDITelescope::isSlewing() const -> bool { return isSlewing_.load(); }

auto INDITelescope::waitForSlew(std::chrono::milliseconds timeout) -> bool {
    if (!isSlewing()) {
        return true;
    }

    std::unique_lock<std::mutex> lock(coordsMutex_);
    return slewCondition_.wait_for(lock, timeout, [this] {
        return !isSlewing_.load();
    });
}

// ==================== Tracking ====================

auto INDITelescope::enableTracking(bool enable) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName = enable ? "TRACK_ON" : "TRACK_OFF";

    if (!setSwitchProperty("TELESCOPE_TRACK_STATE", elemName, true)) {
        LOG_ERROR("Failed to {} tracking", enable ? "enable" : "disable");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(trackMutex_);
        trackInfo_.enabled = enable;
    }

    return true;
}

auto INDITelescope::isTrackingEnabled() const -> bool {
    std::lock_guard<std::mutex> lock(trackMutex_);
    return trackInfo_.enabled;
}

auto INDITelescope::setTrackMode(TrackMode mode) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName;
    switch (mode) {
        case TrackMode::Sidereal:
            elemName = "TRACK_SIDEREAL";
            break;
        case TrackMode::Solar:
            elemName = "TRACK_SOLAR";
            break;
        case TrackMode::Lunar:
            elemName = "TRACK_LUNAR";
            break;
        case TrackMode::Custom:
            elemName = "TRACK_CUSTOM";
            break;
        default:
            return false;
    }

    if (!setSwitchProperty("TELESCOPE_TRACK_MODE", elemName, true)) {
        LOG_ERROR("Failed to set track mode");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(trackMutex_);
        trackInfo_.mode = mode;
    }

    return true;
}

auto INDITelescope::getTrackMode() const -> TrackMode {
    std::lock_guard<std::mutex> lock(trackMutex_);
    return trackInfo_.mode;
}

auto INDITelescope::setTrackRate(double raRate, double decRate) -> bool {
    if (!isConnected()) {
        return false;
    }

    bool success = true;
    success &= setNumberProperty("TELESCOPE_TRACK_RATE", "TRACK_RATE_RA", raRate);
    success &=
        setNumberProperty("TELESCOPE_TRACK_RATE", "TRACK_RATE_DE", decRate);

    if (success) {
        std::lock_guard<std::mutex> lock(trackMutex_);
        trackInfo_.raRate = raRate;
        trackInfo_.decRate = decRate;
    }

    return success;
}

auto INDITelescope::getTrackRateInfo() const -> TrackRateInfo {
    std::lock_guard<std::mutex> lock(trackMutex_);
    return trackInfo_;
}

// ==================== Parking ====================

auto INDITelescope::park() -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot park: telescope not connected");
        return false;
    }

    if (isParked()) {
        LOG_WARN("Telescope already parked");
        return true;
    }

    LOG_INFO("Parking telescope");

    telescopeState_.store(TelescopeState::Parking);

    if (!setSwitchProperty("TELESCOPE_PARK", "PARK", true)) {
        LOG_ERROR("Failed to park telescope");
        telescopeState_.store(TelescopeState::Error);
        return false;
    }

    return true;
}

auto INDITelescope::unpark() -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot unpark: telescope not connected");
        return false;
    }

    if (!isParked()) {
        return true;
    }

    LOG_INFO("Unparking telescope");

    if (!setSwitchProperty("TELESCOPE_PARK", "UNPARK", true)) {
        LOG_ERROR("Failed to unpark telescope");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(parkMutex_);
        parkInfo_.parked = false;
    }

    telescopeState_.store(TelescopeState::Idle);
    return true;
}

auto INDITelescope::isParked() const -> bool {
    std::lock_guard<std::mutex> lock(parkMutex_);
    return parkInfo_.parked;
}

auto INDITelescope::setParkPosition(double ra, double dec) -> bool {
    if (!isConnected()) {
        return false;
    }

    bool success = true;
    success &=
        setNumberProperty("TELESCOPE_PARK_POSITION", "PARK_RA", ra);
    success &=
        setNumberProperty("TELESCOPE_PARK_POSITION", "PARK_DEC", dec);

    if (success) {
        std::lock_guard<std::mutex> lock(parkMutex_);
        parkInfo_.parkRA = ra;
        parkInfo_.parkDEC = dec;
    }

    return success;
}

auto INDITelescope::getParkPosition() const -> std::optional<EquatorialCoords> {
    std::lock_guard<std::mutex> lock(parkMutex_);
    if (!parkInfo_.parkEnabled) {
        return std::nullopt;
    }
    return EquatorialCoords{parkInfo_.parkRA, parkInfo_.parkDEC};
}

auto INDITelescope::setParkOption(ParkOption option) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName;
    switch (option) {
        case ParkOption::Current:
            elemName = "PARK_CURRENT";
            break;
        case ParkOption::Default:
            elemName = "PARK_DEFAULT";
            break;
        case ParkOption::WriteData:
            elemName = "PARK_WRITE_DATA";
            break;
        case ParkOption::PurgeData:
            elemName = "PARK_PURGE_DATA";
            break;
        default:
            return false;
    }

    if (!setSwitchProperty("TELESCOPE_PARK_OPTION", elemName, true)) {
        LOG_ERROR("Failed to set park option");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(parkMutex_);
        parkInfo_.option = option;
    }

    return true;
}

auto INDITelescope::getParkInfo() const -> ParkInfo {
    std::lock_guard<std::mutex> lock(parkMutex_);
    return parkInfo_;
}

// ==================== Motion Control ====================

auto INDITelescope::setSlewRate(SlewRate rate) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName;
    switch (rate) {
        case SlewRate::Guide:
            elemName = "SLEW_GUIDE";
            break;
        case SlewRate::Centering:
            elemName = "SLEW_CENTERING";
            break;
        case SlewRate::Find:
            elemName = "SLEW_FIND";
            break;
        case SlewRate::Max:
            elemName = "SLEW_MAX";
            break;
        default:
            return false;
    }

    if (!setSwitchProperty("TELESCOPE_SLEW_RATE", elemName, true)) {
        LOG_ERROR("Failed to set slew rate");
        return false;
    }

    slewRate_.store(rate);
    return true;
}

auto INDITelescope::getSlewRate() const -> SlewRate { return slewRate_.load(); }

auto INDITelescope::moveNS(MotionNS direction) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName = (direction == MotionNS::North) ? "MOTION_NORTH"
                                                          : "MOTION_SOUTH";

    if (!setSwitchProperty("TELESCOPE_MOTION_NS", elemName, true)) {
        LOG_ERROR("Failed to start NS motion");
        return false;
    }

    motionNS_.store(direction);
    return true;
}

auto INDITelescope::moveEW(MotionEW direction) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName =
        (direction == MotionEW::East) ? "MOTION_EAST" : "MOTION_WEST";

    if (!setSwitchProperty("TELESCOPE_MOTION_WE", elemName, true)) {
        LOG_ERROR("Failed to start EW motion");
        return false;
    }

    motionEW_.store(direction);
    return true;
}

auto INDITelescope::stopNS() -> bool {
    // Turn off both directions
    setSwitchProperty("TELESCOPE_MOTION_NS", "MOTION_NORTH", false);
    setSwitchProperty("TELESCOPE_MOTION_NS", "MOTION_SOUTH", false);
    motionNS_.store(MotionNS::None);
    return true;
}

auto INDITelescope::stopEW() -> bool {
    // Turn off both directions
    setSwitchProperty("TELESCOPE_MOTION_WE", "MOTION_EAST", false);
    setSwitchProperty("TELESCOPE_MOTION_WE", "MOTION_WEST", false);
    motionEW_.store(MotionEW::None);
    return true;
}

// ==================== Guiding ====================

auto INDITelescope::guideNS(int direction, int durationMs) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string propName =
        (direction > 0) ? "TELESCOPE_TIMED_GUIDE_NS" : "TELESCOPE_TIMED_GUIDE_NS";
    std::string elemName = (direction > 0) ? "TIMED_GUIDE_N" : "TIMED_GUIDE_S";

    return setNumberProperty(propName, elemName, static_cast<double>(durationMs));
}

auto INDITelescope::guideEW(int direction, int durationMs) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string propName = "TELESCOPE_TIMED_GUIDE_WE";
    std::string elemName = (direction > 0) ? "TIMED_GUIDE_E" : "TIMED_GUIDE_W";

    return setNumberProperty(propName, elemName, static_cast<double>(durationMs));
}

// ==================== Telescope Info ====================

auto INDITelescope::getTelescopeInfo() const -> TelescopeInfo {
    std::lock_guard<std::mutex> lock(infoMutex_);
    return telescopeInfo_;
}

auto INDITelescope::setTelescopeInfo(const TelescopeInfo& info) -> bool {
    if (!isConnected()) {
        return false;
    }

    bool success = true;
    success &= setNumberProperty("TELESCOPE_INFO", "TELESCOPE_APERTURE",
                                 info.aperture);
    success &= setNumberProperty("TELESCOPE_INFO", "TELESCOPE_FOCAL_LENGTH",
                                 info.focalLength);
    success &= setNumberProperty("TELESCOPE_INFO", "GUIDER_APERTURE",
                                 info.guiderAperture);
    success &= setNumberProperty("TELESCOPE_INFO", "GUIDER_FOCAL_LENGTH",
                                 info.guiderFocalLength);

    if (success) {
        std::lock_guard<std::mutex> lock(infoMutex_);
        telescopeInfo_ = info;
    }

    return success;
}

auto INDITelescope::getPierSide() const -> PierSide { return pierSide_.load(); }

// ==================== Status ====================

auto INDITelescope::getTelescopeState() const -> TelescopeState {
    return telescopeState_.load();
}

auto INDITelescope::getStatus() const -> json {
    json status = INDIDeviceBase::getStatus();

    status["telescopeState"] = static_cast<int>(telescopeState_.load());
    status["isSlewing"] = isSlewing();
    status["pierSide"] = static_cast<int>(pierSide_.load());
    status["slewRate"] = static_cast<int>(slewRate_.load());

    {
        std::lock_guard<std::mutex> lock(coordsMutex_);
        status["currentRADEC"] = currentRADEC_.toJson();
        status["targetRADEC"] = targetRADEC_.toJson();
        status["currentAzAlt"] = currentAzAlt_.toJson();
    }

    status["tracking"] = getTrackRateInfo().toJson();
    status["park"] = getParkInfo().toJson();
    status["telescopeInfo"] = getTelescopeInfo().toJson();

    return status;
}

// ==================== Property Handlers ====================

void INDITelescope::onPropertyDefined(const INDIProperty& property) {
    INDIDeviceBase::onPropertyDefined(property);

    if (property.name == "EQUATORIAL_EOD_COORD" ||
        property.name == "HORIZONTAL_COORD") {
        handleCoordinateProperty(property);
    } else if (property.name == "TELESCOPE_TRACK_STATE" ||
               property.name == "TELESCOPE_TRACK_MODE" ||
               property.name == "TELESCOPE_TRACK_RATE") {
        handleTrackProperty(property);
    } else if (property.name == "TELESCOPE_PARK" ||
               property.name == "TELESCOPE_PARK_POSITION") {
        handleParkProperty(property);
    } else if (property.name == "TELESCOPE_INFO") {
        handleTelescopeInfoProperty(property);
    } else if (property.name == "TELESCOPE_PIER_SIDE") {
        handlePierSideProperty(property);
    }
}

void INDITelescope::onPropertyUpdated(const INDIProperty& property) {
    INDIDeviceBase::onPropertyUpdated(property);

    if (property.name == "EQUATORIAL_EOD_COORD") {
        handleCoordinateProperty(property);

        // Check if slew completed
        if (property.state == PropertyState::Ok && isSlewing()) {
            telescopeState_.store(isTrackingEnabled() ? TelescopeState::Tracking
                                                      : TelescopeState::Idle);
            isSlewing_.store(false);
            slewCondition_.notify_all();
        } else if (property.state == PropertyState::Alert) {
            telescopeState_.store(TelescopeState::Error);
            isSlewing_.store(false);
            slewCondition_.notify_all();
        }
    } else if (property.name == "HORIZONTAL_COORD") {
        handleCoordinateProperty(property);
    } else if (property.name == "TELESCOPE_TRACK_STATE" ||
               property.name == "TELESCOPE_TRACK_MODE" ||
               property.name == "TELESCOPE_TRACK_RATE") {
        handleTrackProperty(property);
    } else if (property.name == "TELESCOPE_PARK") {
        handleParkProperty(property);

        // Check park state
        if (auto parked = property.getSwitch("PARK")) {
            if (*parked) {
                telescopeState_.store(TelescopeState::Parked);
            }
        }
    } else if (property.name == "TELESCOPE_PIER_SIDE") {
        handlePierSideProperty(property);
    } else if (property.name == "TARGET_EOD_COORD") {
        std::lock_guard<std::mutex> lock(coordsMutex_);
        if (auto ra = property.getNumber("RA")) {
            targetRADEC_.ra = *ra;
        }
        if (auto dec = property.getNumber("DEC")) {
            targetRADEC_.dec = *dec;
        }
    }
}

// ==================== Internal Methods ====================

void INDITelescope::handleCoordinateProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(coordsMutex_);

    if (property.name == "EQUATORIAL_EOD_COORD") {
        if (auto ra = property.getNumber("RA")) {
            currentRADEC_.ra = *ra;
        }
        if (auto dec = property.getNumber("DEC")) {
            currentRADEC_.dec = *dec;
        }
    } else if (property.name == "HORIZONTAL_COORD") {
        if (auto az = property.getNumber("AZ")) {
            currentAzAlt_.az = *az;
        }
        if (auto alt = property.getNumber("ALT")) {
            currentAzAlt_.alt = *alt;
        }
    }
}

void INDITelescope::handleTrackProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(trackMutex_);

    if (property.name == "TELESCOPE_TRACK_STATE") {
        if (auto on = property.getSwitch("TRACK_ON")) {
            trackInfo_.enabled = *on;
        }
    } else if (property.name == "TELESCOPE_TRACK_MODE") {
        for (const auto& sw : property.switches) {
            if (sw.on) {
                if (sw.name == "TRACK_SIDEREAL") {
                    trackInfo_.mode = TrackMode::Sidereal;
                } else if (sw.name == "TRACK_SOLAR") {
                    trackInfo_.mode = TrackMode::Solar;
                } else if (sw.name == "TRACK_LUNAR") {
                    trackInfo_.mode = TrackMode::Lunar;
                } else if (sw.name == "TRACK_CUSTOM") {
                    trackInfo_.mode = TrackMode::Custom;
                }
            }
        }
    } else if (property.name == "TELESCOPE_TRACK_RATE") {
        if (auto ra = property.getNumber("TRACK_RATE_RA")) {
            trackInfo_.raRate = *ra;
        }
        if (auto dec = property.getNumber("TRACK_RATE_DE")) {
            trackInfo_.decRate = *dec;
        }
    }
}

void INDITelescope::handleParkProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(parkMutex_);

    if (property.name == "TELESCOPE_PARK") {
        parkInfo_.parkEnabled = true;
        if (auto parked = property.getSwitch("PARK")) {
            parkInfo_.parked = *parked;
        }
    } else if (property.name == "TELESCOPE_PARK_POSITION") {
        if (auto ra = property.getNumber("PARK_RA")) {
            parkInfo_.parkRA = *ra;
        }
        if (auto dec = property.getNumber("PARK_DEC")) {
            parkInfo_.parkDEC = *dec;
        }
    }
}

void INDITelescope::handleTelescopeInfoProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(infoMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "TELESCOPE_APERTURE") {
            telescopeInfo_.aperture = elem.value;
        } else if (elem.name == "TELESCOPE_FOCAL_LENGTH") {
            telescopeInfo_.focalLength = elem.value;
        } else if (elem.name == "GUIDER_APERTURE") {
            telescopeInfo_.guiderAperture = elem.value;
        } else if (elem.name == "GUIDER_FOCAL_LENGTH") {
            telescopeInfo_.guiderFocalLength = elem.value;
        }
    }
}

void INDITelescope::handlePierSideProperty(const INDIProperty& property) {
    for (const auto& sw : property.switches) {
        if (sw.on) {
            if (sw.name == "PIER_EAST") {
                pierSide_.store(PierSide::East);
            } else if (sw.name == "PIER_WEST") {
                pierSide_.store(PierSide::West);
            }
        }
    }
}

void INDITelescope::handleMotionProperty(const INDIProperty& property) {
    if (property.name == "TELESCOPE_MOTION_NS") {
        for (const auto& sw : property.switches) {
            if (sw.on) {
                if (sw.name == "MOTION_NORTH") {
                    motionNS_.store(MotionNS::North);
                } else if (sw.name == "MOTION_SOUTH") {
                    motionNS_.store(MotionNS::South);
                }
            }
        }
    } else if (property.name == "TELESCOPE_MOTION_WE") {
        for (const auto& sw : property.switches) {
            if (sw.on) {
                if (sw.name == "MOTION_EAST") {
                    motionEW_.store(MotionEW::East);
                } else if (sw.name == "MOTION_WEST") {
                    motionEW_.store(MotionEW::West);
                }
            }
        }
    }
}

void INDITelescope::setupPropertyWatchers() {
    // Watch coordinate property
    watchProperty("EQUATORIAL_EOD_COORD", [this](const INDIProperty& prop) {
        handleCoordinateProperty(prop);
    });

    // Watch tracking property
    watchProperty("TELESCOPE_TRACK_STATE", [this](const INDIProperty& prop) {
        handleTrackProperty(prop);
    });

    // Watch park property
    watchProperty("TELESCOPE_PARK", [this](const INDIProperty& prop) {
        handleParkProperty(prop);
    });
}

}  // namespace lithium::client::indi
