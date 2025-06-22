/*
 * position_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Position Manager Component
Handles position tracking, movement control, and validation

*************************************************/

#pragma once

#include <functional>
#include <mutex>
#include <string>

namespace lithium::device::asi::focuser::components {

// Forward declaration
class HardwareInterface;

/**
 * @brief Position management for ASI Focuser
 *
 * This component handles position tracking, movement validation,
 * step calculations, and movement statistics.
 */
class PositionManager {
public:
    explicit PositionManager(HardwareInterface* hardware);
    ~PositionManager();

    // Non-copyable and non-movable
    PositionManager(const PositionManager&) = delete;
    PositionManager& operator=(const PositionManager&) = delete;
    PositionManager(PositionManager&&) = delete;
    PositionManager& operator=(PositionManager&&) = delete;

    // Position control
    bool moveToPosition(int position);
    bool moveSteps(int steps);
    int getCurrentPosition();
    bool syncPosition(int position);
    bool abortMove();

    // Position limits
    bool setMaxLimit(int limit);
    bool setMinLimit(int limit);
    int getMaxLimit() const { return maxPosition_; }
    int getMinLimit() const { return minPosition_; }
    bool validatePosition(int position) const;

    // Movement settings
    bool setSpeed(double speed);
    double getSpeed() const { return currentSpeed_; }
    int getMaxSpeed() const { return maxSpeed_; }
    std::pair<int, int> getSpeedRange() const { return {1, maxSpeed_}; }

    // Direction control
    bool setDirection(bool inward);
    bool isDirectionReversed() const { return directionReversed_; }

    // Home position
    bool setHomePosition();
    int getHomePosition() const { return homePosition_; }
    bool goToHome();

    // Movement statistics
    uint32_t getMovementCount() const { return movementCount_; }
    uint64_t getTotalSteps() const { return totalSteps_; }
    int getLastMoveSteps() const { return lastMoveSteps_; }
    int getLastMoveDuration() const { return lastMoveDuration_; }

    // Status
    bool isMoving() const;
    std::string getLastError() const { return lastError_; }

    // Callbacks
    void setPositionCallback(std::function<void(int)> callback) {
        positionCallback_ = callback;
    }
    void setMoveCompleteCallback(std::function<void(bool)> callback) {
        moveCompleteCallback_ = callback;
    }

private:
    // Hardware interface
    HardwareInterface* hardware_;

    // Position state
    int currentPosition_ = 15000;
    int maxPosition_ = 30000;
    int minPosition_ = 0;
    int homePosition_ = 15000;

    // Movement settings
    double currentSpeed_ = 300.0;
    int maxSpeed_ = 500;
    bool directionReversed_ = false;

    // Movement statistics
    uint32_t movementCount_ = 0;
    uint64_t totalSteps_ = 0;
    int lastMoveSteps_ = 0;
    int lastMoveDuration_ = 0;

    // Error tracking
    std::string lastError_;

    // Callbacks
    std::function<void(int)> positionCallback_;
    std::function<void(bool)> moveCompleteCallback_;

    // Thread safety
    mutable std::mutex positionMutex_;

    // Helper methods
    void updatePosition();
    void notifyPositionChange(int position);
    void notifyMoveComplete(bool success);
    void updateMoveStatistics(int steps, int duration);
};

}  // namespace lithium::device::asi::focuser::components
