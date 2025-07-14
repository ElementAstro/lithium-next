/*
 * azimuth_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Azimuth Management Component

*************************************************/

#pragma once

#include <memory>
#include <optional>
#include <atomic>
#include <functional>
#include <chrono>

namespace lithium::ascom::dome::components {

class HardwareInterface;

/**
 * @brief Azimuth Management Component for ASCOM Dome
 *
 * This component manages dome azimuth positioning, rotation, and movement
 * operations with support for speed control, backlash compensation, and
 * precise positioning.
 */
class AzimuthManager {
public:
    struct AzimuthSettings {
        double min_speed{1.0};
        double max_speed{10.0};
        double default_speed{5.0};
        double backlash_compensation{0.0};
        bool backlash_enabled{false};
        double position_tolerance{0.5};  // degrees
        int movement_timeout{300};  // seconds
    };

    explicit AzimuthManager(std::shared_ptr<HardwareInterface> hardware);
    virtual ~AzimuthManager();

    // === Azimuth Control ===
    auto getCurrentAzimuth() -> std::optional<double>;
    auto setTargetAzimuth(double azimuth) -> bool;
    auto moveToAzimuth(double azimuth) -> bool;
    auto syncAzimuth(double azimuth) -> bool;
    auto isMoving() const -> bool;
    auto abortMovement() -> bool;

    // === Rotation Control ===
    auto rotateClockwise() -> bool;
    auto rotateCounterClockwise() -> bool;
    auto stopRotation() -> bool;
    auto continuousRotation(bool clockwise) -> bool;

    // === Speed Control ===
    auto getRotationSpeed() -> std::optional<double>;
    auto setRotationSpeed(double speed) -> bool;
    auto getMaxSpeed() const -> double;
    auto getMinSpeed() const -> double;
    auto validateSpeed(double speed) const -> bool;

    // === Backlash Compensation ===
    auto getBacklash() const -> double;
    auto setBacklash(double backlash) -> bool;
    auto enableBacklashCompensation(bool enable) -> bool;
    auto isBacklashCompensationEnabled() const -> bool;

    // === Position Validation ===
    auto normalizeAzimuth(double azimuth) -> double;
    auto isValidAzimuth(double azimuth) const -> bool;
    auto getPositionTolerance() const -> double;
    auto setPositionTolerance(double tolerance) -> bool;

    // === Movement Monitoring ===
    auto isAtPosition(double azimuth) const -> bool;
    auto waitForPosition(double azimuth, int timeout_ms = 30000) -> bool;
    auto getMovementProgress() -> std::optional<double>;
    auto getEstimatedTimeToTarget() -> std::optional<int>;

    // === Statistics ===
    auto getTotalRotation() const -> double;
    auto resetTotalRotation() -> bool;
    auto getMovementCount() const -> uint64_t;
    auto resetMovementCount() -> bool;

    // === Configuration ===
    auto getSettings() const -> AzimuthSettings;
    auto updateSettings(const AzimuthSettings& settings) -> bool;
    auto resetToDefaults() -> bool;

    // === Callback Support ===
    using PositionCallback = std::function<void(double)>;
    using MovementCallback = std::function<void(bool)>;

    auto setPositionCallback(PositionCallback callback) -> void;
    auto setMovementCallback(MovementCallback callback) -> void;

private:
    // === Component Dependencies ===
    std::shared_ptr<HardwareInterface> hardware_;

    // === State Variables ===
    std::atomic<double> current_azimuth_{0.0};
    std::atomic<double> target_azimuth_{0.0};
    std::atomic<double> rotation_speed_{5.0};
    std::atomic<bool> is_moving_{false};
    std::atomic<bool> movement_aborted_{false};

    // === Settings ===
    AzimuthSettings settings_;

    // === Statistics ===
    std::atomic<double> total_rotation_{0.0};
    std::atomic<uint64_t> movement_count_{0};

    // === Callbacks ===
    PositionCallback position_callback_;
    MovementCallback movement_callback_;

    // === Movement Control ===
    auto startMovement(double target_azimuth) -> bool;
    auto stopMovement() -> bool;
    auto updatePosition() -> bool;
    auto calculateRotationDirection(double current, double target) -> bool;  // true = clockwise
    auto calculateRotationAmount(double current, double target) -> double;

    // === Backlash Compensation ===
    auto applyBacklashCompensation(double target_azimuth) -> double;
    auto needsBacklashCompensation(double current, double target) -> bool;

    // === Error Handling ===
    auto validateHardwareConnection() const -> bool;
    auto handleMovementError(const std::string& error) -> void;

    // === Callback Execution ===
    auto notifyPositionChange(double azimuth) -> void;
    auto notifyMovementStateChange(bool is_moving) -> void;
};

} // namespace lithium::ascom::dome::components
