/*
 * mock_telescope.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Mock Telescope Implementation for testing

*************************************************/

#pragma once

#include "../template/telescope.hpp"

#include <random>
#include <thread>

class MockTelescope : public AtomTelescope {
public:
    explicit MockTelescope(const std::string& name = "MockTelescope");
    ~MockTelescope() override = default;

    // AtomDriver interface
    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& port = "", int timeout = 5000, int maxRetry = 3) override;
    bool disconnect() override;
    std::vector<std::string> scan() override;

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
    // Mock configuration
    static constexpr double MOCK_APERTURE = 203.0;        // mm
    static constexpr double MOCK_FOCAL_LENGTH = 1000.0;   // mm
    static constexpr double MOCK_LATITUDE = 40.0;         // degrees
    static constexpr double MOCK_LONGITUDE = -74.0;       // degrees

    // Current state
    bool is_slewing_{false};
    bool is_moving_ns_{false};
    bool is_moving_ew_{false};

    // Motion parameters
    MotionNS current_ns_motion_{MotionNS::NONE};
    MotionEW current_ew_motion_{MotionEW::NONE};

    // Slew rates
    std::vector<double> slew_rates_{1.0, 2.0, 8.0, 32.0, 128.0}; // degrees/sec
    int current_slew_rate_index_{2};

    // Park position
    EquatorialCoordinates park_position_{0.0, 90.0}; // NCP

    // Home position
    EquatorialCoordinates home_position_{0.0, 90.0}; // NCP

    // Current time offset for simulation
    std::chrono::system_clock::time_point utc_offset_;

    // Random number generation for simulation
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;

    // Helper methods
    void simulateSlew(const EquatorialCoordinates& target, bool enableTracking);
    void simulateMotion(std::chrono::milliseconds duration);
    void updateCoordinates();
    EquatorialCoordinates equatorialToLocal(const EquatorialCoordinates& coords);
    HorizontalCoordinates equatorialToHorizontal(const EquatorialCoordinates& coords);
    EquatorialCoordinates horizontalToEquatorial(const HorizontalCoordinates& coords);
    double calculateSiderealTime();
};
