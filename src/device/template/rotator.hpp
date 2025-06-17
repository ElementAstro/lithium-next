/*
 * rotator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomRotator device following INDI architecture

*************************************************/

#pragma once

#include "device.hpp"

#include <array>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>

enum class RotatorState {
    IDLE,
    MOVING,
    ERROR
};

enum class RotatorDirection {
    CLOCKWISE,
    COUNTER_CLOCKWISE
};

// Rotator capabilities
struct RotatorCapabilities {
    bool canAbsoluteMove{true};
    bool canRelativeMove{true};
    bool canAbort{true};
    bool canReverse{false};
    bool canSync{false};
    bool hasTemperature{false};
    bool hasBacklash{false};
    double minAngle{0.0};
    double maxAngle{360.0};
    double stepSize{0.1};
} ATOM_ALIGNAS(32);

class AtomRotator : public AtomDriver {
public:
    explicit AtomRotator(std::string name) : AtomDriver(std::move(name)) {
        setType("Rotator");
    }
    
    ~AtomRotator() override = default;

    // Capabilities
    const RotatorCapabilities& getRotatorCapabilities() const { return rotator_capabilities_; }
    void setRotatorCapabilities(const RotatorCapabilities& caps) { rotator_capabilities_ = caps; }

    // State
    RotatorState getRotatorState() const { return rotator_state_; }
    virtual bool isMoving() const = 0;

    // Position control
    virtual auto getPosition() -> std::optional<double> = 0;
    virtual auto setPosition(double angle) -> bool = 0;
    virtual auto moveToAngle(double angle) -> bool = 0;
    virtual auto rotateByAngle(double angle) -> bool = 0;
    virtual auto abortMove() -> bool = 0;
    virtual auto syncPosition(double angle) -> bool = 0;

    // Direction control
    virtual auto getDirection() -> std::optional<RotatorDirection> = 0;
    virtual auto setDirection(RotatorDirection direction) -> bool = 0;
    virtual auto isReversed() -> bool = 0;
    virtual auto setReversed(bool reversed) -> bool = 0;

    // Speed control
    virtual auto getSpeed() -> std::optional<double> = 0;
    virtual auto setSpeed(double speed) -> bool = 0;
    virtual auto getMaxSpeed() -> double = 0;
    virtual auto getMinSpeed() -> double = 0;

    // Limits
    virtual auto getMinPosition() -> double = 0;
    virtual auto getMaxPosition() -> double = 0;
    virtual auto setLimits(double min, double max) -> bool = 0;

    // Backlash compensation
    virtual auto getBacklash() -> double = 0;
    virtual auto setBacklash(double backlash) -> bool = 0;
    virtual auto enableBacklashCompensation(bool enable) -> bool = 0;
    virtual auto isBacklashCompensationEnabled() -> bool = 0;

    // Temperature
    virtual auto getTemperature() -> std::optional<double> = 0;
    virtual auto hasTemperatureSensor() -> bool = 0;

    // Presets
    virtual auto savePreset(int slot, double angle) -> bool = 0;
    virtual auto loadPreset(int slot) -> bool = 0;
    virtual auto getPreset(int slot) -> std::optional<double> = 0;
    virtual auto deletePreset(int slot) -> bool = 0;

    // Statistics
    virtual auto getTotalRotation() -> double = 0;
    virtual auto resetTotalRotation() -> bool = 0;
    virtual auto getLastMoveAngle() -> double = 0;
    virtual auto getLastMoveDuration() -> int = 0;

    // Utility methods
    virtual auto normalizeAngle(double angle) -> double;
    virtual auto getAngularDistance(double from, double to) -> double;
    virtual auto getShortestPath(double from, double to) -> std::pair<double, RotatorDirection>;

    // Event callbacks
    using PositionCallback = std::function<void(double position)>;
    using MoveCompleteCallback = std::function<void(bool success, const std::string& message)>;
    using TemperatureCallback = std::function<void(double temperature)>;

    virtual void setPositionCallback(PositionCallback callback) { position_callback_ = std::move(callback); }
    virtual void setMoveCompleteCallback(MoveCompleteCallback callback) { move_complete_callback_ = std::move(callback); }
    virtual void setTemperatureCallback(TemperatureCallback callback) { temperature_callback_ = std::move(callback); }

protected:
    RotatorState rotator_state_{RotatorState::IDLE};
    RotatorCapabilities rotator_capabilities_;
    
    // Current state
    double current_position_{0.0};
    double target_position_{0.0};
    double current_speed_{10.0};
    bool is_reversed_{false};
    double backlash_angle_{0.0};
    
    // Statistics
    double total_rotation_{0.0};
    double last_move_angle_{0.0};
    int last_move_duration_{0};
    
    // Presets
    std::array<std::optional<double>, 10> presets_;
    
    // Callbacks
    PositionCallback position_callback_;
    MoveCompleteCallback move_complete_callback_;
    TemperatureCallback temperature_callback_;
    
    // Utility methods
    virtual void updateRotatorState(RotatorState state) { rotator_state_ = state; }
    virtual void notifyPositionChange(double position);
    virtual void notifyMoveComplete(bool success, const std::string& message = "");
    virtual void notifyTemperatureChange(double temperature);
};

// Inline implementations
inline auto AtomRotator::normalizeAngle(double angle) -> double {
    while (angle < 0.0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    return angle;
}

inline auto AtomRotator::getAngularDistance(double from, double to) -> double {
    double diff = normalizeAngle(to - from);
    return std::min(diff, 360.0 - diff);
}

inline auto AtomRotator::getShortestPath(double from, double to) -> std::pair<double, RotatorDirection> {
    double clockwise = normalizeAngle(to - from);
    double counter_clockwise = 360.0 - clockwise;
    
    if (clockwise <= counter_clockwise) {
        return {clockwise, RotatorDirection::CLOCKWISE};
    } else {
        return {counter_clockwise, RotatorDirection::COUNTER_CLOCKWISE};
    }
}
