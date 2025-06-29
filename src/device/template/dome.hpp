/*
 * dome.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomDome device following INDI architecture

*************************************************/

#pragma once

#include "device.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <optional>

enum class DomeState {
    IDLE,
    MOVING,
    PARKING,
    PARKED,
    ERROR
};

enum class DomeMotion {
    CLOCKWISE,
    COUNTER_CLOCKWISE,
    STOP
};

enum class ShutterState {
    OPEN,
    CLOSED,
    OPENING,
    CLOSING,
    ERROR,
    UNKNOWN
};

// Dome capabilities
struct DomeCapabilities {
    bool canPark{true};
    bool canSync{false};
    bool canAbort{true};
    bool hasShutter{true};
    bool hasVariable{false};
    bool canSetAzimuth{true};
    bool canSetParkPosition{true};
    bool hasBacklash{false};
    double minAzimuth{0.0};
    double maxAzimuth{360.0};
} ATOM_ALIGNAS(32);

// Dome parameters
struct DomeParameters {
    double diameter{0.0};       // meters
    double height{0.0};         // meters
    double slitWidth{0.0};      // meters
    double slitHeight{0.0};     // meters
    double telescopeRadius{0.0}; // meters from dome center
} ATOM_ALIGNAS(32);

class AtomDome : public AtomDriver {
public:
    explicit AtomDome(std::string name) : AtomDriver(std::move(name)) {
        setType("Dome");
    }

    ~AtomDome() override = default;

    // Capabilities
    const DomeCapabilities& getDomeCapabilities() const { return dome_capabilities_; }
    void setDomeCapabilities(const DomeCapabilities& caps) { dome_capabilities_ = caps; }

    // Parameters
    const DomeParameters& getDomeParameters() const { return dome_parameters_; }
    void setDomeParameters(const DomeParameters& params) { dome_parameters_ = params; }

    // State
    DomeState getDomeState() const { return dome_state_; }
    virtual bool isMoving() const = 0;
    virtual bool isParked() const = 0;

    // Azimuth control
    virtual auto getAzimuth() -> std::optional<double> = 0;
    virtual auto setAzimuth(double azimuth) -> bool = 0;
    virtual auto moveToAzimuth(double azimuth) -> bool = 0;
    virtual auto rotateClockwise() -> bool = 0;
    virtual auto rotateCounterClockwise() -> bool = 0;
    virtual auto stopRotation() -> bool = 0;
    virtual auto abortMotion() -> bool = 0;
    virtual auto syncAzimuth(double azimuth) -> bool = 0;

    // Parking
    virtual auto park() -> bool = 0;
    virtual auto unpark() -> bool = 0;
    virtual auto getParkPosition() -> std::optional<double> = 0;
    virtual auto setParkPosition(double azimuth) -> bool = 0;
    virtual auto canPark() -> bool = 0;

    // Shutter control
    virtual auto openShutter() -> bool = 0;
    virtual auto closeShutter() -> bool = 0;
    virtual auto abortShutter() -> bool = 0;
    virtual auto getShutterState() -> ShutterState = 0;
    virtual auto hasShutter() -> bool = 0;

    // Speed control
    virtual auto getRotationSpeed() -> std::optional<double> = 0;
    virtual auto setRotationSpeed(double speed) -> bool = 0;
    virtual auto getMaxSpeed() -> double = 0;
    virtual auto getMinSpeed() -> double = 0;

    // Telescope coordination
    virtual auto followTelescope(bool enable) -> bool = 0;
    virtual auto isFollowingTelescope() -> bool = 0;
    virtual auto calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double = 0;
    virtual auto setTelescopePosition(double az, double alt) -> bool = 0;

    // Home position
    virtual auto findHome() -> bool = 0;
    virtual auto setHome() -> bool = 0;
    virtual auto gotoHome() -> bool = 0;
    virtual auto getHomePosition() -> std::optional<double> = 0;

    // Backlash compensation
    virtual auto getBacklash() -> double = 0;
    virtual auto setBacklash(double backlash) -> bool = 0;
    virtual auto enableBacklashCompensation(bool enable) -> bool = 0;
    virtual auto isBacklashCompensationEnabled() -> bool = 0;

    // Weather monitoring
    virtual auto canOpenShutter() -> bool = 0;
    virtual auto isSafeToOperate() -> bool = 0;
    virtual auto getWeatherStatus() -> std::string = 0;

    // Statistics
    virtual auto getTotalRotation() -> double = 0;
    virtual auto resetTotalRotation() -> bool = 0;
    virtual auto getShutterOperations() -> uint64_t = 0;
    virtual auto resetShutterOperations() -> bool = 0;

    // Presets
    virtual auto savePreset(int slot, double azimuth) -> bool = 0;
    virtual auto loadPreset(int slot) -> bool = 0;
    virtual auto getPreset(int slot) -> std::optional<double> = 0;
    virtual auto deletePreset(int slot) -> bool = 0;

    // Event callbacks
    using AzimuthCallback = std::function<void(double azimuth)>;
    using ShutterCallback = std::function<void(ShutterState state)>;
    using ParkCallback = std::function<void(bool parked)>;
    using MoveCompleteCallback = std::function<void(bool success, const std::string& message)>;

    virtual void setAzimuthCallback(AzimuthCallback callback) { azimuth_callback_ = std::move(callback); }
    virtual void setShutterCallback(ShutterCallback callback) { shutter_callback_ = std::move(callback); }
    virtual void setParkCallback(ParkCallback callback) { park_callback_ = std::move(callback); }
    virtual void setMoveCompleteCallback(MoveCompleteCallback callback) { move_complete_callback_ = std::move(callback); }

    // Utility methods
    virtual auto normalizeAzimuth(double azimuth) -> double;
    virtual auto getAzimuthalDistance(double from, double to) -> double;
    virtual auto getShortestPath(double from, double to) -> std::pair<double, DomeMotion>;

protected:
    DomeState dome_state_{DomeState::IDLE};
    DomeCapabilities dome_capabilities_;
    DomeParameters dome_parameters_;
    ShutterState shutter_state_{ShutterState::UNKNOWN};

    // Current state
    double current_azimuth_{0.0};
    double target_azimuth_{0.0};
    double park_position_{0.0};
    double home_position_{0.0};
    bool is_parked_{false};
    bool is_following_telescope_{false};

    // Telescope position for following
    double telescope_azimuth_{0.0};
    double telescope_altitude_{0.0};

    // Statistics
    double total_rotation_{0.0};
    uint64_t shutter_operations_{0};

    // Presets
    std::array<std::optional<double>, 10> presets_;

    // Callbacks
    AzimuthCallback azimuth_callback_;
    ShutterCallback shutter_callback_;
    ParkCallback park_callback_;
    MoveCompleteCallback move_complete_callback_;

    // Utility methods
    virtual void updateDomeState(DomeState state) { dome_state_ = state; }
    virtual void updateShutterState(ShutterState state) { shutter_state_ = state; }
    virtual void notifyAzimuthChange(double azimuth);
    virtual void notifyShutterChange(ShutterState state);
    virtual void notifyParkChange(bool parked);
    virtual void notifyMoveComplete(bool success, const std::string& message = "");
};

// Inline implementations
inline auto AtomDome::normalizeAzimuth(double azimuth) -> double {
    while (azimuth < 0.0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;
    return azimuth;
}

inline auto AtomDome::getAzimuthalDistance(double from, double to) -> double {
    double diff = normalizeAzimuth(to - from);
    return std::min(diff, 360.0 - diff);
}

inline auto AtomDome::getShortestPath(double from, double to) -> std::pair<double, DomeMotion> {
    double clockwise = normalizeAzimuth(to - from);
    double counter_clockwise = 360.0 - clockwise;

    if (clockwise <= counter_clockwise) {
        return {clockwise, DomeMotion::CLOCKWISE};
    } else {
        return {counter_clockwise, DomeMotion::COUNTER_CLOCKWISE};
    }
}
