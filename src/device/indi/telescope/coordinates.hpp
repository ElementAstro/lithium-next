#pragma once

#include <optional>
#include <atomic>
#include <spdlog/spdlog.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "device/template/telescope.hpp"

/**
 * @brief Coordinate system component for INDI telescopes
 * 
 * Handles coordinate transformations, current position tracking, and coordinate systems
 */
class TelescopeCoordinates {
public:
    explicit TelescopeCoordinates(const std::string& name);
    ~TelescopeCoordinates() = default;

    /**
     * @brief Initialize coordinate system component
     */
    auto initialize(INDI::BaseDevice device) -> bool;

    /**
     * @brief Destroy coordinate system component
     */
    auto destroy() -> bool;

    // J2000 coordinates
    /**
     * @brief Get current RA/DEC in J2000 epoch
     */
    auto getRADECJ2000() -> std::optional<EquatorialCoordinates>;

    /**
     * @brief Set target RA/DEC in J2000 epoch
     */
    auto setRADECJ2000(double raHours, double decDegrees) -> bool;

    // JNow (current epoch) coordinates
    /**
     * @brief Get current RA/DEC in current epoch
     */
    auto getRADECJNow() -> std::optional<EquatorialCoordinates>;

    /**
     * @brief Set target RA/DEC in current epoch
     */
    auto setRADECJNow(double raHours, double decDegrees) -> bool;

    /**
     * @brief Get target RA/DEC in current epoch
     */
    auto getTargetRADECJNow() -> std::optional<EquatorialCoordinates>;

    /**
     * @brief Set target RA/DEC in current epoch
     */
    auto setTargetRADECJNow(double raHours, double decDegrees) -> bool;

    // Horizontal coordinates
    /**
     * @brief Get current AZ/ALT coordinates
     */
    auto getAZALT() -> std::optional<HorizontalCoordinates>;

    /**
     * @brief Set target AZ/ALT coordinates
     */
    auto setAZALT(double azDegrees, double altDegrees) -> bool;

    // Location and time
    /**
     * @brief Get geographic location
     */
    auto getLocation() -> std::optional<GeographicLocation>;

    /**
     * @brief Set geographic location
     */
    auto setLocation(const GeographicLocation& location) -> bool;

    /**
     * @brief Get UTC time
     */
    auto getUTCTime() -> std::optional<std::chrono::system_clock::time_point>;

    /**
     * @brief Set UTC time
     */
    auto setUTCTime(const std::chrono::system_clock::time_point& time) -> bool;

    /**
     * @brief Get local time
     */
    auto getLocalTime() -> std::optional<std::chrono::system_clock::time_point>;

    // Coordinate utilities
    /**
     * @brief Convert degrees to hours
     */
    auto degreesToHours(double degrees) -> double;

    /**
     * @brief Convert hours to degrees
     */
    auto hoursToDegrees(double hours) -> double;

    /**
     * @brief Convert degrees to DMS format
     */
    auto degreesToDMS(double degrees) -> std::tuple<int, int, double>;

    /**
     * @brief Convert degrees to HMS format
     */
    auto degreesToHMS(double degrees) -> std::tuple<int, int, double>;

    /**
     * @brief Convert J2000 to JNow coordinates
     */
    auto j2000ToJNow(const EquatorialCoordinates& j2000) -> EquatorialCoordinates;

    /**
     * @brief Convert JNow to J2000 coordinates
     */
    auto jNowToJ2000(const EquatorialCoordinates& jnow) -> EquatorialCoordinates;

    /**
     * @brief Convert equatorial to horizontal coordinates
     */
    auto equatorialToHorizontal(const EquatorialCoordinates& eq, 
                               const GeographicLocation& location,
                               const std::chrono::system_clock::time_point& time) -> HorizontalCoordinates;

    /**
     * @brief Convert horizontal to equatorial coordinates
     */
    auto horizontalToEquatorial(const HorizontalCoordinates& hz,
                               const GeographicLocation& location,
                               const std::chrono::system_clock::time_point& time) -> EquatorialCoordinates;

private:
    std::string name_;
    INDI::BaseDevice device_;
    
    // Current coordinates
    EquatorialCoordinates currentRADECJ2000_;
    EquatorialCoordinates currentRADECJNow_;
    EquatorialCoordinates targetRADECJNow_;
    HorizontalCoordinates currentAZALT_;
    
    // Location and time
    GeographicLocation location_;
    std::chrono::system_clock::time_point utcTime_;
    
    // Helper methods
    auto watchCoordinateProperties() -> void;
    auto watchLocationProperties() -> void;
    auto watchTimeProperties() -> void;
    auto updateCurrentCoordinates() -> void;
};
