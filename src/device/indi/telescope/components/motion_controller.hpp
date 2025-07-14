/*
 * motion_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Motion Controller Component

This component manages all telescope motion operations including
slewing, directional movement, speed control, and motion state tracking.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "device/template/telescope.hpp"

namespace lithium::device::indi::telescope::components {

class HardwareInterface;

/**
 * @brief Motion Controller for INDI Telescope
 *
 * Manages all telescope motion operations including slewing, directional
 * movement, speed control, abort operations, and motion state tracking.
 */
class MotionController {
public:
    enum class MotionState {
        IDLE,
        SLEWING,
        TRACKING,
        MOVING_NORTH,
        MOVING_SOUTH,
        MOVING_EAST,
        MOVING_WEST,
        ABORTING,
        ERROR
    };

    struct SlewCommand {
        double targetRA = 0.0;      // hours
        double targetDEC = 0.0;     // degrees
        bool enableTracking = true;
        bool isSync = false;        // true for sync, false for slew
        std::chrono::steady_clock::time_point timestamp;
    };

    struct MotionStatus {
        MotionState state = MotionState::IDLE;
        double currentRA = 0.0;     // hours
        double currentDEC = 0.0;    // degrees
        double targetRA = 0.0;      // hours
        double targetDEC = 0.0;     // degrees
        double slewProgress = 0.0;  // 0.0 to 1.0
        std::chrono::steady_clock::time_point lastUpdate;
        std::string errorMessage;
    };

    using MotionCompleteCallback = std::function<void(bool success, const std::string& message)>;
    using MotionProgressCallback = std::function<void(const MotionStatus& status)>;

public:
    explicit MotionController(std::shared_ptr<HardwareInterface> hardware);
    ~MotionController();

    // Non-copyable and non-movable
    MotionController(const MotionController&) = delete;
    MotionController& operator=(const MotionController&) = delete;
    MotionController(MotionController&&) = delete;
    MotionController& operator=(MotionController&&) = delete;

    // Initialization
    bool initialize();
    bool shutdown();
    bool isInitialized() const { return initialized_; }

    // Slewing Operations
    bool slewToCoordinates(double ra, double dec, bool enableTracking = true);
    bool slewToAltAz(double azimuth, double altitude);
    bool syncToCoordinates(double ra, double dec);
    bool abortSlew();
    bool isSlewing() const;

    // Directional Movement
    bool startDirectionalMove(MotionNS nsDirection, MotionEW ewDirection);
    bool stopDirectionalMove(MotionNS nsDirection, MotionEW ewDirection);
    bool stopAllMotion();

    // Speed Control
    bool setSlewRate(SlewRate rate);
    bool setSlewRate(double degreesPerSecond);
    std::optional<SlewRate> getCurrentSlewRate() const;
    std::optional<double> getCurrentSlewSpeed() const;
    std::vector<double> getAvailableSlewRates() const;

    // Motion State
    MotionState getMotionState() const { return currentState_; }
    std::string getMotionStateString() const;
    MotionStatus getMotionStatus() const;
    bool isMoving() const;
    bool canMove() const;

    // Progress Tracking
    double getSlewProgress() const;
    std::chrono::seconds getEstimatedSlewTime() const;
    std::chrono::seconds getElapsedSlewTime() const;

    // Target Management
    bool setTargetCoordinates(double ra, double dec);
    std::optional<std::pair<double, double>> getTargetCoordinates() const;
    std::optional<std::pair<double, double>> getCurrentCoordinates() const;

    // Callback Registration
    void setMotionCompleteCallback(MotionCompleteCallback callback) { motionCompleteCallback_ = std::move(callback); }
    void setMotionProgressCallback(MotionProgressCallback callback) { motionProgressCallback_ = std::move(callback); }

    // Emergency Operations
    bool emergencyStop();
    bool recoverFromError();

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<MotionState> currentState_{MotionState::IDLE};
    mutable std::recursive_mutex stateMutex_;

    // Motion tracking
    SlewCommand currentSlewCommand_;
    MotionStatus currentStatus_;
    std::chrono::steady_clock::time_point slewStartTime_;

    // Speed control
    SlewRate currentSlewRate_{SlewRate::CENTERING};
    double customSlewSpeed_{1.0}; // degrees per second
    std::vector<double> availableSlewRates_;

    // Callbacks
    MotionCompleteCallback motionCompleteCallback_;
    MotionProgressCallback motionProgressCallback_;

    // Internal methods
    void updateMotionStatus();
    void handlePropertyUpdate(const std::string& propertyName);
    double calculateSlewProgress() const;
    double calculateAngularDistance(double ra1, double dec1, double ra2, double dec2) const;
    std::string stateToString(MotionState state) const;

    // Property update handlers
    void onCoordinateUpdate();
    void onSlewStateUpdate();
    void onMotionStateUpdate();

    // Validation methods
    bool validateCoordinates(double ra, double dec) const;
    bool validateAltAz(double azimuth, double altitude) const;

    // Utility methods
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);
};

} // namespace lithium::device::indi::telescope::components
