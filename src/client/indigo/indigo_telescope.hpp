/*
 * indigo_telescope.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Telescope/Mount Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_TELESCOPE_HPP
#define LITHIUM_CLIENT_INDIGO_TELESCOPE_HPP

#include "indigo_device_base.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lithium::client::indigo {

/**
 * @brief Tracking rate type
 */
enum class TrackingRate : uint8_t {
    Off,
    Sidereal,
    Lunar,
    Solar,
    Custom
};

/**
 * @brief Slew rate
 */
enum class SlewRate : uint8_t {
    Guide,
    Center,
    Find,
    Max
};

/**
 * @brief Pier side
 */
enum class PierSide : uint8_t {
    East,
    West,
    Unknown
};

/**
 * @brief Geographic location/site information
 */
struct GeographicLocation {
    double latitude{0.0};    // In degrees, N is positive
    double longitude{0.0};   // In degrees, E is positive
    double elevation{0.0};   // In meters
};

/**
 * @brief Equatorial coordinates (RA/DEC)
 */
struct EquatorialCoordinates {
    double ra{0.0};          // Right ascension in hours (0-24)
    double dec{0.0};         // Declination in degrees (-90 to +90)
};

/**
 * @brief Horizontal coordinates (ALT/AZ)
 */
struct HorizontalCoordinates {
    double altitude{0.0};    // Altitude in degrees
    double azimuth{0.0};     // Azimuth in degrees (0-360)
};

/**
 * @brief Mount status
 */
struct MountStatus {
    bool slewing{false};
    bool tracking{false};
    bool parked{false};
    EquatorialCoordinates position;
    HorizontalCoordinates horizontal;
    TrackingRate trackingRate{TrackingRate::Off};
    SlewRate slewRate{SlewRate::Find};
    PierSide pierSide{PierSide::Unknown};
    PropertyState state{PropertyState::Idle};
    std::string message;
};

/**
 * @brief Mount movement callback (slew/track progress)
 */
using MovementCallback = std::function<void(const MountStatus&)>;

/**
 * @brief INDIGO Telescope/Mount Device
 *
 * Provides mount control functionality for INDIGO-connected telescopes:
 * - Coordinate control (RA/DEC, ALT/AZ)
 * - Goto/sync operations
 * - Tracking control (sidereal, lunar, solar, custom)
 * - Slew rates
 * - Park/unpark
 * - Home position
 * - Pier side monitoring
 * - Geographic location/site settings
 * - Movement callbacks
 */
class INDIGOTelescope : public INDIGODeviceBase {
public:
    /**
     * @brief Constructor
     * @param deviceName INDIGO device name
     */
    explicit INDIGOTelescope(const std::string& deviceName);

    /**
     * @brief Destructor
     */
    ~INDIGOTelescope() override;

    // ==================== Coordinate Control ====================

    /**
     * @brief Get current equatorial coordinates
     */
    [[nodiscard]] auto getCurrentEquatorialCoordinates() const
        -> EquatorialCoordinates;

    /**
     * @brief Get current horizontal coordinates
     */
    [[nodiscard]] auto getCurrentHorizontalCoordinates() const
        -> HorizontalCoordinates;

    /**
     * @brief Slew to equatorial coordinates
     * @param ra Right ascension in hours (0-24)
     * @param dec Declination in degrees (-90 to +90)
     * @return Success or error
     */
    auto slewToEquatorial(double ra, double dec) -> DeviceResult<bool>;

    /**
     * @brief Slew to horizontal coordinates
     * @param altitude Altitude in degrees
     * @param azimuth Azimuth in degrees (0-360)
     * @return Success or error
     */
    auto slewToHorizontal(double altitude, double azimuth)
        -> DeviceResult<bool>;

    /**
     * @brief Synchronize on current position
     * @param ra Right ascension in hours (0-24)
     * @param dec Declination in degrees (-90 to +90)
     * @return Success or error
     */
    auto syncOnEquatorial(double ra, double dec) -> DeviceResult<bool>;

    /**
     * @brief Synchronize on horizontal coordinates
     * @param altitude Altitude in degrees
     * @param azimuth Azimuth in degrees (0-360)
     * @return Success or error
     */
    auto syncOnHorizontal(double altitude, double azimuth)
        -> DeviceResult<bool>;

    /**
     * @brief Abort current slew
     * @return Success or error
     */
    auto abortSlew() -> DeviceResult<bool>;

    /**
     * @brief Check if currently slewing
     */
    [[nodiscard]] auto isSlewing() const -> bool;

    // ==================== Tracking Control ====================

    /**
     * @brief Enable/disable tracking
     * @param on Enable or disable tracking
     * @return Success or error
     */
    auto setTracking(bool on) -> DeviceResult<bool>;

    /**
     * @brief Check if tracking is enabled
     */
    [[nodiscard]] auto isTracking() const -> bool;

    /**
     * @brief Set tracking rate
     * @param rate Tracking rate (Sidereal, Lunar, Solar, Custom)
     * @return Success or error
     */
    auto setTrackingRate(TrackingRate rate) -> DeviceResult<bool>;

    /**
     * @brief Get current tracking rate
     */
    [[nodiscard]] auto getTrackingRate() const -> TrackingRate;

    /**
     * @brief Set custom tracking rate
     * @param raRate RA rate in seconds/sidereal day
     * @param decRate DEC rate in seconds/sidereal day
     * @return Success or error
     */
    auto setCustomTrackingRate(double raRate, double decRate)
        -> DeviceResult<bool>;

    /**
     * @brief Get guide rate
     * @return Guide rate as percentage (0-1)
     */
    [[nodiscard]] auto getGuideRate() const -> double;

    /**
     * @brief Set guide rate
     * @param rate Guide rate as percentage (0-1)
     * @return Success or error
     */
    auto setGuideRate(double rate) -> DeviceResult<bool>;

    // ==================== Slew Rate Control ====================

    /**
     * @brief Set slew rate
     * @param rate Slew rate (Guide, Center, Find, Max)
     * @return Success or error
     */
    auto setSlewRate(SlewRate rate) -> DeviceResult<bool>;

    /**
     * @brief Get current slew rate
     */
    [[nodiscard]] auto getSlewRate() const -> SlewRate;

    // ==================== Manual Motion ====================

    /**
     * @brief Move telescope north
     * @return Success or error
     */
    auto moveNorth() -> DeviceResult<bool>;

    /**
     * @brief Move telescope south
     * @return Success or error
     */
    auto moveSouth() -> DeviceResult<bool>;

    /**
     * @brief Move telescope east
     * @return Success or error
     */
    auto moveEast() -> DeviceResult<bool>;

    /**
     * @brief Move telescope west
     * @return Success or error
     */
    auto moveWest() -> DeviceResult<bool>;

    /**
     * @brief Stop all manual motion
     * @return Success or error
     */
    auto stopMotion() -> DeviceResult<bool>;

    /**
     * @brief Check if currently moving
     */
    [[nodiscard]] auto isMoving() const -> bool;

    // ==================== Park/Home Control ====================

    /**
     * @brief Park the telescope
     * @return Success or error
     */
    auto park() -> DeviceResult<bool>;

    /**
     * @brief Unpark the telescope
     * @return Success or error
     */
    auto unpark() -> DeviceResult<bool>;

    /**
     * @brief Check if parked
     */
    [[nodiscard]] auto isParked() const -> bool;

    /**
     * @brief Set home position
     * @return Success or error
     */
    auto setHome() -> DeviceResult<bool>;

    /**
     * @brief Go to home position
     * @return Success or error
     */
    auto goHome() -> DeviceResult<bool>;

    // ==================== Pier Side ====================

    /**
     * @brief Get current pier side
     */
    [[nodiscard]] auto getPierSide() const -> PierSide;

    /**
     * @brief Force pier side
     * @param side Desired pier side
     * @return Success or error
     */
    auto setPierSide(PierSide side) -> DeviceResult<bool>;

    // ==================== Geographic Location ====================

    /**
     * @brief Get geographic location
     */
    [[nodiscard]] auto getGeographicLocation() const -> GeographicLocation;

    /**
     * @brief Set geographic location
     * @param location Geographic coordinates
     * @return Success or error
     */
    auto setGeographicLocation(const GeographicLocation& location)
        -> DeviceResult<bool>;

    /**
     * @brief Set latitude
     * @param latitude Latitude in degrees (N is positive)
     * @return Success or error
     */
    auto setLatitude(double latitude) -> DeviceResult<bool>;

    /**
     * @brief Set longitude
     * @param longitude Longitude in degrees (E is positive)
     * @return Success or error
     */
    auto setLongitude(double longitude) -> DeviceResult<bool>;

    /**
     * @brief Set elevation
     * @param elevation Elevation in meters
     * @return Success or error
     */
    auto setElevation(double elevation) -> DeviceResult<bool>;

    // ==================== Callbacks ====================

    /**
     * @brief Set movement callback
     * @param callback Called when mount status changes (slew/track progress)
     */
    void setMovementCallback(MovementCallback callback);

    // ==================== Status ====================

    /**
     * @brief Get current mount status
     */
    [[nodiscard]] auto getMountStatus() const -> MountStatus;

    /**
     * @brief Get mount capabilities as JSON
     */
    [[nodiscard]] auto getCapabilities() const -> json;

    /**
     * @brief Get current mount status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json;

protected:
    void onConnected() override;
    void onDisconnected() override;
    void onPropertyUpdated(const Property& property) override;

private:
    class Impl;
    std::unique_ptr<Impl> telescopeImpl_;
};

// ============================================================================
// Tracking rate conversion
// ============================================================================

[[nodiscard]] constexpr auto trackingRateToString(TrackingRate rate) noexcept
    -> std::string_view {
    switch (rate) {
        case TrackingRate::Off: return "Off";
        case TrackingRate::Sidereal: return "Sidereal";
        case TrackingRate::Lunar: return "Lunar";
        case TrackingRate::Solar: return "Solar";
        case TrackingRate::Custom: return "Custom";
        default: return "Off";
    }
}

[[nodiscard]] auto trackingRateFromString(std::string_view str)
    -> TrackingRate;

// ============================================================================
// Slew rate conversion
// ============================================================================

[[nodiscard]] constexpr auto slewRateToString(SlewRate rate) noexcept
    -> std::string_view {
    switch (rate) {
        case SlewRate::Guide: return "Guide";
        case SlewRate::Center: return "Center";
        case SlewRate::Find: return "Find";
        case SlewRate::Max: return "Max";
        default: return "Guide";
    }
}

[[nodiscard]] auto slewRateFromString(std::string_view str) -> SlewRate;

// ============================================================================
// Pier side conversion
// ============================================================================

[[nodiscard]] constexpr auto pierSideToString(PierSide side) noexcept
    -> std::string_view {
    switch (side) {
        case PierSide::East: return "East";
        case PierSide::West: return "West";
        case PierSide::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

[[nodiscard]] auto pierSideFromString(std::string_view str) -> PierSide;

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_TELESCOPE_HPP
