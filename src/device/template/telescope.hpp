/*
 * telescope.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced AtomTelescope following INDI architecture

*************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <string_view>
#include <tuple>
#include "device.hpp"

enum class ConnectionMode { SERIAL, TCP, NONE };
enum class T_BAUD_RATE {
    B9600,
    B19200,
    B38400,
    B57600,
    B115200,
    B230400,
    NONE
};

enum class TrackMode { SIDEREAL, SOLAR, LUNAR, CUSTOM, NONE };

enum class PierSide { EAST, WEST, UNKNOWN, NONE };

enum class ParkOptions { CURRENT, DEFAULT, WRITE_DATA, PURGE_DATA, NONE };

enum class SlewRate { GUIDE, CENTERING, FIND, MAX, NONE };

enum class MotionEW { WEST, EAST, NONE };

enum class MotionNS { NORTH, SOUTH, NONE };

enum class DomePolicy { IGNORED, LOCKED, NONE };

enum class TelescopeState { IDLE, SLEWING, TRACKING, PARKING, PARKED, ERROR };

enum class AlignmentMode {
    EQ_NORTH_POLE,
    EQ_SOUTH_POLE,
    ALTAZ,
    GERMAN_POLAR,
    FORK
};

// Forward declarations
struct ln_date;

// Telescope capabilities
struct TelescopeCapabilities {
    bool canPark{true};
    bool canSync{true};
    bool canGoto{true};
    bool canAbort{true};
    bool hasTrackMode{true};
    bool hasPierSide{false};
    bool hasGuideRate{true};
    bool hasParkPosition{true};
    bool hasUnpark{true};
    bool hasTrackRate{true};
    bool hasLocation{false};
    bool hasTime{false};
    bool canControlTrack{true};
} ATOM_ALIGNAS(8);

// Location information
struct GeographicLocation {
    double latitude{0.0};   // degrees
    double longitude{0.0};  // degrees
    double elevation{0.0};  // meters
    std::string timezone;
} ATOM_ALIGNAS(32);

// Telescope parameters
struct TelescopeParameters {
    double aperture{0.0};           // mm
    double focalLength{0.0};        // mm
    double guiderAperture{0.0};     // mm
    double guiderFocalLength{0.0};  // mm
} ATOM_ALIGNAS(32);

// Motion rates
struct MotionRates {
    double guideRateNS{0.5};  // arcsec/sec
    double guideRateEW{0.5};  // arcsec/sec
    double slewRateRA{3.0};   // degrees/sec
    double slewRateDEC{3.0};  // degrees/sec
} ATOM_ALIGNAS(32);

// Coordinates
struct EquatorialCoordinates {
    double ra{0.0};   // hours
    double dec{0.0};  // degrees
} ATOM_ALIGNAS(16);

struct HorizontalCoordinates {
    double az{0.0};   // degrees
    double alt{0.0};  // degrees
} ATOM_ALIGNAS(16);

class AtomTelescope : public AtomDriver {
public:
    explicit AtomTelescope(std::string name) : AtomDriver(std::move(name)) {
        setType("Telescope");
    }

    ~AtomTelescope() override = default;

    // Capabilities
    const TelescopeCapabilities &getTelescopeCapabilities() const {
        return telescope_capabilities_;
    }
    void setTelescopeCapabilities(const TelescopeCapabilities &caps) {
        telescope_capabilities_ = caps;
    }

    // Telescope state
    TelescopeState getTelescopeState() const { return telescope_state_; }

    // Pure virtual methods that must be implemented by derived classes
    virtual auto getTelescopeInfo() -> std::optional<TelescopeParameters> = 0;
    virtual auto setTelescopeInfo(double aperture, double focalLength,
                                  double guiderAperture,
                                  double guiderFocalLength) -> bool = 0;

    // Pier side
    virtual auto getPierSide() -> std::optional<PierSide> = 0;
    virtual auto setPierSide(PierSide side) -> bool = 0;

    // Tracking
    virtual auto getTrackRate() -> std::optional<TrackMode> = 0;
    virtual auto setTrackRate(TrackMode rate) -> bool = 0;
    virtual auto isTrackingEnabled() -> bool = 0;
    virtual auto enableTracking(bool enable) -> bool = 0;
    virtual auto getTrackRates() -> MotionRates = 0;
    virtual auto setTrackRates(const MotionRates &rates) -> bool = 0;

    // Motion control
    virtual auto abortMotion() -> bool = 0;
    virtual auto getStatus() -> std::optional<std::string> = 0;
    virtual auto emergencyStop() -> bool = 0;
    virtual auto isMoving() -> bool = 0;

    // Parking
    virtual auto setParkOption(ParkOptions option) -> bool = 0;
    virtual auto getParkPosition() -> std::optional<EquatorialCoordinates> = 0;
    virtual auto setParkPosition(double ra, double dec) -> bool = 0;
    virtual auto isParked() -> bool = 0;
    virtual auto park() -> bool = 0;
    virtual auto unpark() -> bool = 0;
    virtual auto canPark() -> bool = 0;

    // Home position
    virtual auto initializeHome(std::string_view command = "") -> bool = 0;
    virtual auto findHome() -> bool = 0;
    virtual auto setHome() -> bool = 0;
    virtual auto gotoHome() -> bool = 0;

    // Slew rates
    virtual auto getSlewRate() -> std::optional<double> = 0;
    virtual auto setSlewRate(double speed) -> bool = 0;
    virtual auto getSlewRates() -> std::vector<double> = 0;
    virtual auto setSlewRateIndex(int index) -> bool = 0;

    // Directional movement
    virtual auto getMoveDirectionEW() -> std::optional<MotionEW> = 0;
    virtual auto setMoveDirectionEW(MotionEW direction) -> bool = 0;
    virtual auto getMoveDirectionNS() -> std::optional<MotionNS> = 0;
    virtual auto setMoveDirectionNS(MotionNS direction) -> bool = 0;
    virtual auto startMotion(MotionNS ns_direction, MotionEW ew_direction)
        -> bool = 0;
    virtual auto stopMotion(MotionNS ns_direction, MotionEW ew_direction)
        -> bool = 0;

    // Guiding
    virtual auto guideNS(int direction, int duration) -> bool = 0;
    virtual auto guideEW(int direction, int duration) -> bool = 0;
    virtual auto guidePulse(double ra_ms, double dec_ms) -> bool = 0;

    // Coordinate systems
    virtual auto getRADECJ2000() -> std::optional<EquatorialCoordinates> = 0;
    virtual auto setRADECJ2000(double raHours, double decDegrees) -> bool = 0;

    virtual auto getRADECJNow() -> std::optional<EquatorialCoordinates> = 0;
    virtual auto setRADECJNow(double raHours, double decDegrees) -> bool = 0;

    virtual auto getTargetRADECJNow()
        -> std::optional<EquatorialCoordinates> = 0;
    virtual auto setTargetRADECJNow(double raHours, double decDegrees)
        -> bool = 0;

    virtual auto slewToRADECJNow(double raHours, double decDegrees,
                                 bool enableTracking = true) -> bool = 0;
    virtual auto syncToRADECJNow(double raHours, double decDegrees) -> bool = 0;

    virtual auto getAZALT() -> std::optional<HorizontalCoordinates> = 0;
    virtual auto setAZALT(double azDegrees, double altDegrees) -> bool = 0;
    virtual auto slewToAZALT(double azDegrees, double altDegrees) -> bool = 0;

    // Location and time
    virtual auto getLocation() -> std::optional<GeographicLocation> = 0;
    virtual auto setLocation(const GeographicLocation &location) -> bool = 0;
    virtual auto getUTCTime()
        -> std::optional<std::chrono::system_clock::time_point> = 0;
    virtual auto setUTCTime(const std::chrono::system_clock::time_point &time)
        -> bool = 0;
    virtual auto getLocalTime()
        -> std::optional<std::chrono::system_clock::time_point> = 0;

    // Alignment
    virtual auto getAlignmentMode() -> AlignmentMode = 0;
    virtual auto setAlignmentMode(AlignmentMode mode) -> bool = 0;
    virtual auto addAlignmentPoint(const EquatorialCoordinates &measured,
                                   const EquatorialCoordinates &target)
        -> bool = 0;
    virtual auto clearAlignment() -> bool = 0;

    // Event callbacks
    using SlewCallback =
        std::function<void(bool success, const std::string &message)>;
    using TrackingCallback = std::function<void(bool enabled)>;
    using ParkCallback = std::function<void(bool parked)>;
    using CoordinateCallback =
        std::function<void(const EquatorialCoordinates &)>;

    virtual void setSlewCallback(SlewCallback callback) {
        slew_callback_ = std::move(callback);
    }
    virtual void setTrackingCallback(TrackingCallback callback) {
        tracking_callback_ = std::move(callback);
    }
    virtual void setParkCallback(ParkCallback callback) {
        park_callback_ = std::move(callback);
    }
    virtual void setCoordinateCallback(CoordinateCallback callback) {
        coordinate_callback_ = std::move(callback);
    }

    // Utility methods
    virtual auto degreesToHours(double degrees) -> double {
        return degrees / 15.0;
    }
    virtual auto hoursToDegrees(double hours) -> double { return hours * 15.0; }
    virtual auto degreesToDMS(double degrees)
        -> std::tuple<int, int, double> = 0;
    virtual auto degreesToHMS(double degrees)
        -> std::tuple<int, int, double> = 0;

    // Device scanning and connection management
    virtual auto scan() -> std::vector<std::string> override = 0;

protected:
    TelescopeState telescope_state_{TelescopeState::IDLE};
    TelescopeCapabilities telescope_capabilities_;
    TelescopeParameters telescope_parameters_;
    GeographicLocation location_;
    MotionRates motion_rates_;
    AlignmentMode alignment_mode_{AlignmentMode::EQ_NORTH_POLE};

    // Current coordinates
    EquatorialCoordinates current_radec_;
    EquatorialCoordinates target_radec_;
    HorizontalCoordinates current_azalt_;

    // State tracking
    bool is_tracking_{false};
    bool is_parked_{false};
    bool is_slewing_{false};
    PierSide pier_side_{PierSide::UNKNOWN};

    // Callbacks
    SlewCallback slew_callback_;
    TrackingCallback tracking_callback_;
    ParkCallback park_callback_;
    CoordinateCallback coordinate_callback_;

    // Utility methods
    virtual void updateTelescopeState(TelescopeState state) {
        telescope_state_ = state;
    }
    virtual void notifySlewComplete(bool success,
                                    const std::string &message = "");
    virtual void notifyTrackingChange(bool enabled);
    virtual void notifyParkChange(bool parked);
    virtual void notifyCoordinateUpdate(const EquatorialCoordinates &coords);
};
