/*
 * indigo_telescope.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Telescope/Mount Device Implementation

**************************************************/

#include "indigo_telescope.hpp"

#include <cmath>
#include <mutex>

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// Enum type conversions
// ============================================================================

auto trackingRateFromString(std::string_view str) -> TrackingRate {
    if (str == "Sidereal" || str == "SIDEREAL") return TrackingRate::Sidereal;
    if (str == "Lunar" || str == "LUNAR") return TrackingRate::Lunar;
    if (str == "Solar" || str == "SOLAR") return TrackingRate::Solar;
    if (str == "Custom" || str == "CUSTOM") return TrackingRate::Custom;
    return TrackingRate::Off;
}

auto slewRateFromString(std::string_view str) -> SlewRate {
    if (str == "Guide" || str == "GUIDE") return SlewRate::Guide;
    if (str == "Center" || str == "CENTER") return SlewRate::Center;
    if (str == "Find" || str == "FIND") return SlewRate::Find;
    if (str == "Max" || str == "MAX") return SlewRate::Max;
    return SlewRate::Guide;
}

auto pierSideFromString(std::string_view str) -> PierSide {
    if (str == "East" || str == "EAST") return PierSide::East;
    if (str == "West" || str == "WEST") return PierSide::West;
    return PierSide::Unknown;
}

// ============================================================================
// INDIGOTelescope Implementation
// ============================================================================

class INDIGOTelescope::Impl {
public:
    explicit Impl(INDIGOTelescope* parent) : parent_(parent) {}

    void onConnected() {
        // Load current mount state
        updateMountStatus();

        LOG_F(INFO, "INDIGO Telescope[{}]: Connected and initialized",
              parent_->getINDIGODeviceName());
    }

    void onDisconnected() {
        status_.slewing = false;
        status_.tracking = false;
        LOG_F(INFO, "INDIGO Telescope[{}]: Disconnected",
              parent_->getINDIGODeviceName());
    }

    void onPropertyUpdated(const Property& property) {
        const auto& name = property.name;

        if (name == "MOUNT_EQUATORIAL_COORDINATES") {
            handleEquatorialUpdate(property);
        } else if (name == "MOUNT_HORIZONTAL_COORDINATES") {
            handleHorizontalUpdate(property);
        } else if (name == "MOUNT_ON_COORDINATES_SET") {
            handleCoordinatesSetUpdate(property);
        } else if (name == "MOUNT_TRACKING") {
            handleTrackingUpdate(property);
        } else if (name == "MOUNT_TRACK_RATE") {
            handleTrackingRateUpdate(property);
        } else if (name == "MOUNT_SLEW_RATE") {
            handleSlewRateUpdate(property);
        } else if (name == "MOUNT_MOTION_NS" || name == "MOUNT_MOTION_WE") {
            updateMountStatus();
        } else if (name == "MOUNT_PARK") {
            handleParkUpdate(property);
        } else if (name == "MOUNT_SIDE_OF_PIER") {
            handlePierSideUpdate(property);
        }
    }

    [[nodiscard]] auto getCurrentEquatorialCoordinates() const
        -> EquatorialCoordinates {
        return status_.position;
    }

    [[nodiscard]] auto getCurrentHorizontalCoordinates() const
        -> HorizontalCoordinates {
        return status_.horizontal;
    }

    auto slewToEquatorial(double ra, double dec) -> DeviceResult<bool> {
        // Set coordinates first
        auto result = parent_->setNumberProperty("MOUNT_EQUATORIAL_COORDINATES",
            {
                {"RA", ra},
                {"DEC", dec}
            });
        if (!result.has_value()) {
            return result;
        }

        // Then command the slew
        result = parent_->setSwitchProperty("MOUNT_ON_COORDINATES_SET",
                                            "SLEW", true);
        if (result.has_value()) {
            status_.slewing = true;
            status_.state = PropertyState::Busy;
            LOG_F(INFO, "INDIGO Telescope[{}]: Slewing to RA={:.4f}h DEC={:.4f}°",
                  parent_->getINDIGODeviceName(), ra, dec);
        }

        return result;
    }

    auto slewToHorizontal(double altitude, double azimuth)
        -> DeviceResult<bool> {
        // Set coordinates first
        auto result = parent_->setNumberProperty("MOUNT_HORIZONTAL_COORDINATES",
            {
                {"ALT", altitude},
                {"AZ", azimuth}
            });
        if (!result.has_value()) {
            return result;
        }

        // Then command the slew
        result = parent_->setSwitchProperty("MOUNT_ON_COORDINATES_SET",
                                            "SLEW", true);
        if (result.has_value()) {
            status_.slewing = true;
            status_.state = PropertyState::Busy;
            LOG_F(INFO, "INDIGO Telescope[{}]: Slewing to ALT={:.4f}° AZ={:.4f}°",
                  parent_->getINDIGODeviceName(), altitude, azimuth);
        }

        return result;
    }

    auto syncOnEquatorial(double ra, double dec) -> DeviceResult<bool> {
        // Set coordinates first
        auto result = parent_->setNumberProperty("MOUNT_EQUATORIAL_COORDINATES",
            {
                {"RA", ra},
                {"DEC", dec}
            });
        if (!result.has_value()) {
            return result;
        }

        // Then command the sync
        result = parent_->setSwitchProperty("MOUNT_ON_COORDINATES_SET",
                                            "SYNC", true);
        if (result.has_value()) {
            LOG_F(INFO, "INDIGO Telescope[{}]: Synced on RA={:.4f}h DEC={:.4f}°",
                  parent_->getINDIGODeviceName(), ra, dec);
        }

        return result;
    }

    auto syncOnHorizontal(double altitude, double azimuth)
        -> DeviceResult<bool> {
        // Set coordinates first
        auto result = parent_->setNumberProperty("MOUNT_HORIZONTAL_COORDINATES",
            {
                {"ALT", altitude},
                {"AZ", azimuth}
            });
        if (!result.has_value()) {
            return result;
        }

        // Then command the sync
        result = parent_->setSwitchProperty("MOUNT_ON_COORDINATES_SET",
                                            "SYNC", true);
        if (result.has_value()) {
            LOG_F(INFO, "INDIGO Telescope[{}]: Synced on ALT={:.4f}° AZ={:.4f}°",
                  parent_->getINDIGODeviceName(), altitude, azimuth);
        }

        return result;
    }

    auto abortSlew() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("MOUNT_ABORT_MOTION",
                                                 "ABORT", true);
        if (result.has_value()) {
            status_.slewing = false;
            status_.state = PropertyState::Alert;
            LOG_F(INFO, "INDIGO Telescope[{}]: Slew aborted",
                  parent_->getINDIGODeviceName());
        }

        return result;
    }

    [[nodiscard]] auto isSlewing() const -> bool {
        return status_.slewing;
    }

    auto setTracking(bool on) -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("MOUNT_TRACKING",
                                                 on ? "ON" : "OFF", true);
        if (result.has_value()) {
            status_.tracking = on;
            LOG_F(INFO, "INDIGO Telescope[{}]: Tracking {}",
                  parent_->getINDIGODeviceName(),
                  on ? "enabled" : "disabled");
        }

        return result;
    }

    [[nodiscard]] auto isTracking() const -> bool {
        return status_.tracking;
    }

    auto setTrackingRate(TrackingRate rate) -> DeviceResult<bool> {
        std::string rateName;
        switch (rate) {
            case TrackingRate::Sidereal:
                rateName = "SIDEREAL";
                break;
            case TrackingRate::Lunar:
                rateName = "LUNAR";
                break;
            case TrackingRate::Solar:
                rateName = "SOLAR";
                break;
            case TrackingRate::Custom:
                rateName = "CUSTOM";
                break;
            case TrackingRate::Off:
            default:
                rateName = "OFF";
                break;
        }

        auto result = parent_->setSwitchProperty("MOUNT_TRACK_RATE",
                                                 rateName, true);
        if (result.has_value()) {
            status_.trackingRate = rate;
            LOG_F(INFO, "INDIGO Telescope[{}]: Tracking rate set to {}",
                  parent_->getINDIGODeviceName(),
                  trackingRateToString(rate));
        }

        return result;
    }

    [[nodiscard]] auto getTrackingRate() const -> TrackingRate {
        return status_.trackingRate;
    }

    auto setCustomTrackingRate(double raRate, double decRate)
        -> DeviceResult<bool> {
        // First, set the rate
        auto result = parent_->setNumberProperty("MOUNT_TRACK_RATE_CUSTOM",
            {
                {"RA_RATE", raRate},
                {"DEC_RATE", decRate}
            });

        if (result.has_value()) {
            // Then select custom rate
            result = parent_->setSwitchProperty("MOUNT_TRACK_RATE",
                                                "CUSTOM", true);
            if (result.has_value()) {
                status_.trackingRate = TrackingRate::Custom;
                LOG_F(INFO, "INDIGO Telescope[{}]: Custom tracking rate set "
                      "(RA={:.6f}, DEC={:.6f})",
                      parent_->getINDIGODeviceName(), raRate, decRate);
            }
        }

        return result;
    }

    [[nodiscard]] auto getGuideRate() const -> double {
        auto result = parent_->getNumberValue("MOUNT_GUIDE_RATE",
                                              "GUIDE_RATE");
        return result.value_or(0.5);
    }

    auto setGuideRate(double rate) -> DeviceResult<bool> {
        return parent_->setNumberProperty("MOUNT_GUIDE_RATE",
                                          "GUIDE_RATE", rate);
    }

    auto setSlewRate(SlewRate rate) -> DeviceResult<bool> {
        std::string rateName;
        switch (rate) {
            case SlewRate::Guide:
                rateName = "GUIDE";
                break;
            case SlewRate::Center:
                rateName = "CENTER";
                break;
            case SlewRate::Find:
                rateName = "FIND";
                break;
            case SlewRate::Max:
                rateName = "MAX";
                break;
        }

        auto result = parent_->setSwitchProperty("MOUNT_SLEW_RATE",
                                                 rateName, true);
        if (result.has_value()) {
            status_.slewRate = rate;
            LOG_F(INFO, "INDIGO Telescope[{}]: Slew rate set to {}",
                  parent_->getINDIGODeviceName(),
                  slewRateToString(rate));
        }

        return result;
    }

    [[nodiscard]] auto getSlewRate() const -> SlewRate {
        return status_.slewRate;
    }

    auto moveNorth() -> DeviceResult<bool> {
        return parent_->setSwitchProperty("MOUNT_MOTION_NS", "NORTH", true);
    }

    auto moveSouth() -> DeviceResult<bool> {
        return parent_->setSwitchProperty("MOUNT_MOTION_NS", "SOUTH", true);
    }

    auto moveEast() -> DeviceResult<bool> {
        return parent_->setSwitchProperty("MOUNT_MOTION_WE", "EAST", true);
    }

    auto moveWest() -> DeviceResult<bool> {
        return parent_->setSwitchProperty("MOUNT_MOTION_WE", "WEST", true);
    }

    auto stopMotion() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("MOUNT_MOTION_NS",
                                                 "NORTH", false);
        return parent_->setSwitchProperty("MOUNT_MOTION_WE",
                                          "WEST", false);
    }

    [[nodiscard]] auto isMoving() const -> bool {
        auto nsMotion = parent_->getSwitchValue("MOUNT_MOTION_NS", "NORTH");
        auto weMotion = parent_->getSwitchValue("MOUNT_MOTION_WE", "WEST");
        return (nsMotion.value_or(false) || weMotion.value_or(false));
    }

    auto park() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("MOUNT_PARK", "PARK", true);
        if (result.has_value()) {
            status_.parked = true;
            LOG_F(INFO, "INDIGO Telescope[{}]: Park command issued",
                  parent_->getINDIGODeviceName());
        }

        return result;
    }

    auto unpark() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("MOUNT_PARK",
                                                 "UNPARK", true);
        if (result.has_value()) {
            status_.parked = false;
            LOG_F(INFO, "INDIGO Telescope[{}]: Unpark command issued",
                  parent_->getINDIGODeviceName());
        }

        return result;
    }

    [[nodiscard]] auto isParked() const -> bool {
        return status_.parked;
    }

    auto setHome() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("MOUNT_HOME", "SET", true);
        if (result.has_value()) {
            LOG_F(INFO, "INDIGO Telescope[{}]: Home position set",
                  parent_->getINDIGODeviceName());
        }

        return result;
    }

    auto goHome() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("MOUNT_HOME", "GO", true);
        if (result.has_value()) {
            status_.slewing = true;
            status_.state = PropertyState::Busy;
            LOG_F(INFO, "INDIGO Telescope[{}]: Going to home position",
                  parent_->getINDIGODeviceName());
        }

        return result;
    }

    [[nodiscard]] auto getPierSide() const -> PierSide {
        return status_.pierSide;
    }

    auto setPierSide(PierSide side) -> DeviceResult<bool> {
        std::string sideName;
        switch (side) {
            case PierSide::East:
                sideName = "EAST";
                break;
            case PierSide::West:
                sideName = "WEST";
                break;
            case PierSide::Unknown:
            default:
                return DeviceResult<bool>::error("Unknown pier side");
        }

        auto result = parent_->setSwitchProperty("MOUNT_SIDE_OF_PIER",
                                                 sideName, true);
        if (result.has_value()) {
            status_.pierSide = side;
            LOG_F(INFO, "INDIGO Telescope[{}]: Pier side set to {}",
                  parent_->getINDIGODeviceName(), pierSideToString(side));
        }

        return result;
    }

    [[nodiscard]] auto getGeographicLocation() const -> GeographicLocation {
        GeographicLocation location;
        location.latitude = parent_->getNumberValue("MOUNT_GEOGRAPHIC_COORDINATES",
                                                    "LAT").value_or(0.0);
        location.longitude = parent_->getNumberValue("MOUNT_GEOGRAPHIC_COORDINATES",
                                                     "LONG").value_or(0.0);
        location.elevation = parent_->getNumberValue("MOUNT_GEOGRAPHIC_COORDINATES",
                                                     "ELEV").value_or(0.0);
        return location;
    }

    auto setGeographicLocation(const GeographicLocation& location)
        -> DeviceResult<bool> {
        auto result = parent_->setNumberProperty("MOUNT_GEOGRAPHIC_COORDINATES",
            {
                {"LAT", location.latitude},
                {"LONG", location.longitude},
                {"ELEV", location.elevation}
            });

        if (result.has_value()) {
            LOG_F(INFO,
                  "INDIGO Telescope[{}]: Geographic location set to "
                  "Lat={:.4f}° Long={:.4f}° Elev={:.1f}m",
                  parent_->getINDIGODeviceName(),
                  location.latitude, location.longitude, location.elevation);
        }

        return result;
    }

    auto setLatitude(double latitude) -> DeviceResult<bool> {
        return parent_->setNumberProperty("MOUNT_GEOGRAPHIC_COORDINATES",
                                          "LAT", latitude);
    }

    auto setLongitude(double longitude) -> DeviceResult<bool> {
        return parent_->setNumberProperty("MOUNT_GEOGRAPHIC_COORDINATES",
                                          "LONG", longitude);
    }

    auto setElevation(double elevation) -> DeviceResult<bool> {
        return parent_->setNumberProperty("MOUNT_GEOGRAPHIC_COORDINATES",
                                          "ELEV", elevation);
    }

    void setMovementCallback(MovementCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        movementCallback_ = std::move(callback);
    }

    [[nodiscard]] auto getMountStatus() const -> MountStatus {
        return status_;
    }

    [[nodiscard]] auto getCapabilities() const -> json {
        json caps;
        caps["hasEquatorial"] = true;
        caps["hasHorizontal"] = true;
        caps["hasTracking"] = true;
        caps["hasGuiding"] = true;
        caps["hasPark"] = true;
        caps["hasHome"] = true;
        caps["hasPierSide"] = true;
        caps["hasGeographicCoordinates"] = true;

        // Slew rates
        json slewRates = json::array();
        slewRates.push_back("Guide");
        slewRates.push_back("Center");
        slewRates.push_back("Find");
        slewRates.push_back("Max");
        caps["slewRates"] = slewRates;

        // Tracking rates
        json trackRates = json::array();
        trackRates.push_back("Sidereal");
        trackRates.push_back("Lunar");
        trackRates.push_back("Solar");
        trackRates.push_back("Custom");
        caps["trackingRates"] = trackRates;

        return caps;
    }

    [[nodiscard]] auto getStatus() const -> json {
        json status;
        status["connected"] = parent_->isConnected();

        status["position"] = {
            {"ra", status_.position.ra},
            {"dec", status_.position.dec}
        };

        status["horizontal"] = {
            {"altitude", status_.horizontal.altitude},
            {"azimuth", status_.horizontal.azimuth}
        };

        status["slewing"] = status_.slewing;
        status["tracking"] = status_.tracking;
        status["trackingRate"] = std::string(trackingRateToString(
            status_.trackingRate));
        status["slewRate"] = std::string(slewRateToString(
            status_.slewRate));

        status["parked"] = status_.parked;
        status["pierSide"] = std::string(pierSideToString(
            status_.pierSide));

        status["guideRate"] = getGuideRate();

        return status;
    }

private:
    void handleEquatorialUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "RA") {
                status_.position.ra = elem.value;
            } else if (elem.name == "DEC") {
                status_.position.dec = elem.value;
            }
        }

        if (property.state == PropertyState::Ok) {
            status_.slewing = false;
        } else if (property.state == PropertyState::Busy) {
            status_.slewing = true;
        }

        updateMountStatusCallback();
    }

    void handleHorizontalUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "ALT") {
                status_.horizontal.altitude = elem.value;
            } else if (elem.name == "AZ") {
                status_.horizontal.azimuth = elem.value;
            }
        }

        updateMountStatusCallback();
    }

    void handleCoordinatesSetUpdate(const Property& property) {
        // Check what command was executed
        for (const auto& elem : property.switchElements) {
            if (elem.name == "SLEW" && elem.value) {
                status_.slewing = true;
                status_.state = PropertyState::Busy;
            } else if (elem.name == "SYNC" && elem.value) {
                status_.slewing = false;
                status_.state = PropertyState::Ok;
            }
        }

        updateMountStatusCallback();
    }

    void handleTrackingUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.name == "ON") {
                status_.tracking = elem.value;
                break;
            }
        }

        updateMountStatusCallback();
    }

    void handleTrackingRateUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.value) {
                status_.trackingRate = trackingRateFromString(elem.name);
                break;
            }
        }

        updateMountStatusCallback();
    }

    void handleSlewRateUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.value) {
                status_.slewRate = slewRateFromString(elem.name);
                break;
            }
        }

        updateMountStatusCallback();
    }

    void handleParkUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.name == "PARK") {
                status_.parked = elem.value;
                break;
            }
        }

        updateMountStatusCallback();
    }

    void handlePierSideUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.value) {
                status_.pierSide = pierSideFromString(elem.name);
                break;
            }
        }

        updateMountStatusCallback();
    }

    void updateMountStatus() {
        status_.position = getCurrentEquatorialCoordinates();
        status_.horizontal = getCurrentHorizontalCoordinates();
        status_.tracking = parent_->getSwitchValue("MOUNT_TRACKING", "ON")
            .value_or(false);
        status_.parked = parent_->getSwitchValue("MOUNT_PARK", "PARK")
            .value_or(false);

        auto trackRateActive = parent_->getActiveSwitchName("MOUNT_TRACK_RATE");
        if (trackRateActive.has_value()) {
            status_.trackingRate = trackingRateFromString(trackRateActive.value());
        }

        auto slewRateActive = parent_->getActiveSwitchName("MOUNT_SLEW_RATE");
        if (slewRateActive.has_value()) {
            status_.slewRate = slewRateFromString(slewRateActive.value());
        }

        auto pierSideActive = parent_->getActiveSwitchName("MOUNT_SIDE_OF_PIER");
        if (pierSideActive.has_value()) {
            status_.pierSide = pierSideFromString(pierSideActive.value());
        }
    }

    void updateMountStatusCallback() {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (movementCallback_) {
            movementCallback_(status_);
        }
    }

    INDIGOTelescope* parent_;

    MountStatus status_;

    std::mutex callbackMutex_;
    MovementCallback movementCallback_;
};

// ============================================================================
// INDIGOTelescope public interface
// ============================================================================

INDIGOTelescope::INDIGOTelescope(const std::string& deviceName)
    : INDIGODeviceBase(deviceName, "Telescope"),
      telescopeImpl_(std::make_unique<Impl>(this)) {}

INDIGOTelescope::~INDIGOTelescope() = default;

auto INDIGOTelescope::getCurrentEquatorialCoordinates() const
    -> EquatorialCoordinates {
    return telescopeImpl_->getCurrentEquatorialCoordinates();
}

auto INDIGOTelescope::getCurrentHorizontalCoordinates() const
    -> HorizontalCoordinates {
    return telescopeImpl_->getCurrentHorizontalCoordinates();
}

auto INDIGOTelescope::slewToEquatorial(double ra, double dec)
    -> DeviceResult<bool> {
    return telescopeImpl_->slewToEquatorial(ra, dec);
}

auto INDIGOTelescope::slewToHorizontal(double altitude, double azimuth)
    -> DeviceResult<bool> {
    return telescopeImpl_->slewToHorizontal(altitude, azimuth);
}

auto INDIGOTelescope::syncOnEquatorial(double ra, double dec)
    -> DeviceResult<bool> {
    return telescopeImpl_->syncOnEquatorial(ra, dec);
}

auto INDIGOTelescope::syncOnHorizontal(double altitude, double azimuth)
    -> DeviceResult<bool> {
    return telescopeImpl_->syncOnHorizontal(altitude, azimuth);
}

auto INDIGOTelescope::abortSlew() -> DeviceResult<bool> {
    return telescopeImpl_->abortSlew();
}

auto INDIGOTelescope::isSlewing() const -> bool {
    return telescopeImpl_->isSlewing();
}

auto INDIGOTelescope::setTracking(bool on) -> DeviceResult<bool> {
    return telescopeImpl_->setTracking(on);
}

auto INDIGOTelescope::isTracking() const -> bool {
    return telescopeImpl_->isTracking();
}

auto INDIGOTelescope::setTrackingRate(TrackingRate rate) -> DeviceResult<bool> {
    return telescopeImpl_->setTrackingRate(rate);
}

auto INDIGOTelescope::getTrackingRate() const -> TrackingRate {
    return telescopeImpl_->getTrackingRate();
}

auto INDIGOTelescope::setCustomTrackingRate(double raRate, double decRate)
    -> DeviceResult<bool> {
    return telescopeImpl_->setCustomTrackingRate(raRate, decRate);
}

auto INDIGOTelescope::getGuideRate() const -> double {
    return telescopeImpl_->getGuideRate();
}

auto INDIGOTelescope::setGuideRate(double rate) -> DeviceResult<bool> {
    return telescopeImpl_->setGuideRate(rate);
}

auto INDIGOTelescope::setSlewRate(SlewRate rate) -> DeviceResult<bool> {
    return telescopeImpl_->setSlewRate(rate);
}

auto INDIGOTelescope::getSlewRate() const -> SlewRate {
    return telescopeImpl_->getSlewRate();
}

auto INDIGOTelescope::moveNorth() -> DeviceResult<bool> {
    return telescopeImpl_->moveNorth();
}

auto INDIGOTelescope::moveSouth() -> DeviceResult<bool> {
    return telescopeImpl_->moveSouth();
}

auto INDIGOTelescope::moveEast() -> DeviceResult<bool> {
    return telescopeImpl_->moveEast();
}

auto INDIGOTelescope::moveWest() -> DeviceResult<bool> {
    return telescopeImpl_->moveWest();
}

auto INDIGOTelescope::stopMotion() -> DeviceResult<bool> {
    return telescopeImpl_->stopMotion();
}

auto INDIGOTelescope::isMoving() const -> bool {
    return telescopeImpl_->isMoving();
}

auto INDIGOTelescope::park() -> DeviceResult<bool> {
    return telescopeImpl_->park();
}

auto INDIGOTelescope::unpark() -> DeviceResult<bool> {
    return telescopeImpl_->unpark();
}

auto INDIGOTelescope::isParked() const -> bool {
    return telescopeImpl_->isParked();
}

auto INDIGOTelescope::setHome() -> DeviceResult<bool> {
    return telescopeImpl_->setHome();
}

auto INDIGOTelescope::goHome() -> DeviceResult<bool> {
    return telescopeImpl_->goHome();
}

auto INDIGOTelescope::getPierSide() const -> PierSide {
    return telescopeImpl_->getPierSide();
}

auto INDIGOTelescope::setPierSide(PierSide side) -> DeviceResult<bool> {
    return telescopeImpl_->setPierSide(side);
}

auto INDIGOTelescope::getGeographicLocation() const -> GeographicLocation {
    return telescopeImpl_->getGeographicLocation();
}

auto INDIGOTelescope::setGeographicLocation(const GeographicLocation& location)
    -> DeviceResult<bool> {
    return telescopeImpl_->setGeographicLocation(location);
}

auto INDIGOTelescope::setLatitude(double latitude) -> DeviceResult<bool> {
    return telescopeImpl_->setLatitude(latitude);
}

auto INDIGOTelescope::setLongitude(double longitude) -> DeviceResult<bool> {
    return telescopeImpl_->setLongitude(longitude);
}

auto INDIGOTelescope::setElevation(double elevation) -> DeviceResult<bool> {
    return telescopeImpl_->setElevation(elevation);
}

void INDIGOTelescope::setMovementCallback(MovementCallback callback) {
    telescopeImpl_->setMovementCallback(std::move(callback));
}

auto INDIGOTelescope::getMountStatus() const -> MountStatus {
    return telescopeImpl_->getMountStatus();
}

auto INDIGOTelescope::getCapabilities() const -> json {
    return telescopeImpl_->getCapabilities();
}

auto INDIGOTelescope::getStatus() const -> json {
    return telescopeImpl_->getStatus();
}

void INDIGOTelescope::onConnected() {
    INDIGODeviceBase::onConnected();
    telescopeImpl_->onConnected();
}

void INDIGOTelescope::onDisconnected() {
    INDIGODeviceBase::onDisconnected();
    telescopeImpl_->onDisconnected();
}

void INDIGOTelescope::onPropertyUpdated(const Property& property) {
    INDIGODeviceBase::onPropertyUpdated(property);
    telescopeImpl_->onPropertyUpdated(property);
}

}  // namespace lithium::client::indigo
