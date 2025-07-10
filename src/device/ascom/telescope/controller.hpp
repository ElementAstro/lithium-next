/*
 * controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Telescope Controller

This modular controller orchestrates the telescope components to provide
a clean, maintainable, and testable interface for ASCOM telescope control.

*************************************************/

#pragma once

#include <memory>
#include <string>

#include "main.hpp"
#include "device/template/telescope.hpp"

namespace lithium::device::ascom::telescope {

/**
 * @brief Modular ASCOM Telescope Controller
 * 
 * This controller implements the AtomTelescope interface using the modular
 * component architecture, providing a clean separation of concerns and
 * improved maintainability.
 */
class ASCOMTelescopeController : public AtomTelescope {
public:
    explicit ASCOMTelescopeController(const std::string& name);
    ~ASCOMTelescopeController() override;

    // Non-copyable and non-movable
    ASCOMTelescopeController(const ASCOMTelescopeController&) = delete;
    ASCOMTelescopeController& operator=(const ASCOMTelescopeController&) = delete;
    ASCOMTelescopeController(ASCOMTelescopeController&&) = delete;
    ASCOMTelescopeController& operator=(ASCOMTelescopeController&&) = delete;

    // =========================================================================
    // AtomTelescope Interface Implementation
    // =========================================================================

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Telescope information
    auto getTelescopeInfo() -> std::optional<TelescopeParameters> override;
    auto setTelescopeInfo(double aperture, double focalLength,
                          double guiderAperture, double guiderFocalLength) -> bool override;

    // Pier side
    auto getPierSide() -> std::optional<PierSide> override;
    auto setPierSide(PierSide side) -> bool override;

    // Tracking
    auto getTrackRate() -> std::optional<TrackMode> override;
    auto setTrackRate(TrackMode rate) -> bool override;
    auto isTrackingEnabled() -> bool override;
    auto enableTracking(bool enable) -> bool override;
    auto getTrackRates() -> MotionRates override;
    auto setTrackRates(const MotionRates& rates) -> bool override;

    // Motion control
    auto abortMotion() -> bool override;
    auto getStatus() -> std::optional<std::string> override;
    auto emergencyStop() -> bool override;
    auto isMoving() -> bool override;

    // Parking
    auto setParkOption(ParkOptions option) -> bool override;
    auto getParkPosition() -> std::optional<EquatorialCoordinates> override;
    auto setParkPosition(double ra, double dec) -> bool override;
    auto isParked() -> bool override;
    auto park() -> bool override;
    auto unpark() -> bool override;
    auto canPark() -> bool override;

    // Home position
    auto initializeHome(std::string_view command = "") -> bool override;
    auto findHome() -> bool override;
    auto setHome() -> bool override;
    auto gotoHome() -> bool override;

    // Slew rates
    auto getSlewRate() -> std::optional<double> override;
    auto setSlewRate(double speed) -> bool override;
    auto getSlewRates() -> std::vector<double> override;
    auto setSlewRateIndex(int index) -> bool override;

    // Directional movement
    auto getMoveDirectionEW() -> std::optional<MotionEW> override;
    auto setMoveDirectionEW(MotionEW direction) -> bool override;
    auto getMoveDirectionNS() -> std::optional<MotionNS> override;
    auto setMoveDirectionNS(MotionNS direction) -> bool override;
    auto startMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool override;
    auto stopMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool override;

    // Guiding
    auto guideNS(int direction, int duration) -> bool override;
    auto guideEW(int direction, int duration) -> bool override;
    auto guidePulse(double ra_ms, double dec_ms) -> bool override;

    // Coordinate systems
    auto getRADECJ2000() -> std::optional<EquatorialCoordinates> override;
    auto setRADECJ2000(double raHours, double decDegrees) -> bool override;

    auto getRADECJNow() -> std::optional<EquatorialCoordinates> override;
    auto setRADECJNow(double raHours, double decDegrees) -> bool override;

    auto getTargetRADECJNow() -> std::optional<EquatorialCoordinates> override;
    auto setTargetRADECJNow(double raHours, double decDegrees) -> bool override;

    auto slewToRADECJNow(double raHours, double decDegrees, bool enableTracking = true) -> bool override;
    auto syncToRADECJNow(double raHours, double decDegrees) -> bool override;

    auto getAZALT() -> std::optional<HorizontalCoordinates> override;
    auto setAZALT(double azDegrees, double altDegrees) -> bool override;
    auto slewToAZALT(double azDegrees, double altDegrees) -> bool override;

    // Location and time
    auto getLocation() -> std::optional<GeographicLocation> override;
    auto setLocation(const GeographicLocation& location) -> bool override;
    auto getUTCTime() -> std::optional<std::chrono::system_clock::time_point> override;
    auto setUTCTime(const std::chrono::system_clock::time_point& time) -> bool override;
    auto getLocalTime() -> std::optional<std::chrono::system_clock::time_point> override;

    // Alignment
    auto getAlignmentMode() -> AlignmentMode override;
    auto setAlignmentMode(AlignmentMode mode) -> bool override;
    auto addAlignmentPoint(const EquatorialCoordinates& measured,
                           const EquatorialCoordinates& target) -> bool override;
    auto clearAlignment() -> bool override;

    // Utility methods
    auto degreesToDMS(double degrees) -> std::tuple<int, int, double> override;
    auto degreesToHMS(double degrees) -> std::tuple<int, int, double> override;

private:
    // Main telescope implementation
    std::unique_ptr<ASCOMTelescopeMain> telescope_;

    // Helper methods
    void logError(const std::string& operation, const std::string& error) const;
    bool validateParameters(const std::string& operation, 
                           std::function<bool()> validator) const;
};

} // namespace lithium::device::ascom::telescope
