/*
 * position_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Position Manager Implementation

*************************************************/

#include "position_manager.hpp"

#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cmath>

namespace lithium::device::asi::focuser::components {

PositionManager::PositionManager(HardwareInterface* hardware)
    : hardware_(hardware) {
    spdlog::info("Created ASI Focuser Position Manager");
}

PositionManager::~PositionManager() {
    spdlog::info("Destroyed ASI Focuser Position Manager");
}

bool PositionManager::moveToPosition(int position) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    if (!hardware_ || !hardware_->isConnected()) {
        lastError_ = "Hardware not connected";
        return false;
    }

    if (!validatePosition(position)) {
        lastError_ = "Invalid position: " + std::to_string(position);
        return false;
    }

    updatePosition();  // Get current position from hardware

    if (position == currentPosition_) {
        spdlog::info("Already at position {}", position);
        return true;
    }

    spdlog::info("Moving to position: {}", position);

    auto startTime = std::chrono::steady_clock::now();

    if (!hardware_->moveToPosition(position)) {
        lastError_ = hardware_->getLastError();
        spdlog::error("Failed to move to position {}", position);
        return false;
    }

    // Update statistics
    int steps = std::abs(position - currentPosition_);
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();

    updateMoveStatistics(steps, static_cast<int>(duration));

    // Update current position
    currentPosition_ = position;
    notifyPositionChange(position);

    return true;
}

bool PositionManager::moveSteps(int steps) {
    updatePosition();
    int targetPos = currentPosition_ + steps;

    if (directionReversed_) {
        targetPos = currentPosition_ - steps;
    }

    return moveToPosition(targetPos);
}

int PositionManager::getCurrentPosition() {
    updatePosition();
    return currentPosition_;
}

bool PositionManager::syncPosition(int position) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    if (!hardware_ || !hardware_->isConnected()) {
        lastError_ = "Hardware not connected";
        return false;
    }

    currentPosition_ = position;
    spdlog::info("Synced position to: {}", position);
    notifyPositionChange(position);
    return true;
}

bool PositionManager::abortMove() {
    if (!hardware_ || !hardware_->isConnected()) {
        lastError_ = "Hardware not connected";
        return false;
    }

    spdlog::info("Aborting focuser movement");

    if (!hardware_->stopMovement()) {
        lastError_ = hardware_->getLastError();
        return false;
    }

    notifyMoveComplete(false);
    return true;
}

bool PositionManager::setMaxLimit(int limit) {
    if (limit <= minPosition_ || limit < 0) {
        return false;
    }

    maxPosition_ = limit;
    spdlog::info("Set max limit to: {}", limit);
    return true;
}

bool PositionManager::setMinLimit(int limit) {
    if (limit >= maxPosition_ || limit < 0) {
        return false;
    }

    minPosition_ = limit;
    spdlog::info("Set min limit to: {}", limit);
    return true;
}

bool PositionManager::validatePosition(int position) const {
    return position >= minPosition_ && position <= maxPosition_;
}

bool PositionManager::setSpeed(double speed) {
    if (speed < 1 || speed > maxSpeed_) {
        return false;
    }

    currentSpeed_ = speed;
    spdlog::info("Set speed to: {:.1f}", speed);
    return true;
}

bool PositionManager::setDirection(bool inward) {
    directionReversed_ = inward;

    if (hardware_ && hardware_->isConnected()) {
        if (!hardware_->setReverse(inward)) {
            return false;
        }
    }

    spdlog::info("Set direction reversed: {}", inward);
    return true;
}

bool PositionManager::setHomePosition() {
    homePosition_ = getCurrentPosition();
    spdlog::info("Set home position to: {}", homePosition_);
    return true;
}

bool PositionManager::goToHome() { return moveToPosition(homePosition_); }

bool PositionManager::isMoving() const {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    return hardware_->isMoving();
}

void PositionManager::updatePosition() {
    if (hardware_ && hardware_->isConnected()) {
        int position = hardware_->getCurrentPosition();
        if (position >= 0) {
            currentPosition_ = position;
        }
    }
}

void PositionManager::notifyPositionChange(int position) {
    if (positionCallback_) {
        positionCallback_(position);
    }
}

void PositionManager::notifyMoveComplete(bool success) {
    if (moveCompleteCallback_) {
        moveCompleteCallback_(success);
    }
}

void PositionManager::updateMoveStatistics(int steps, int duration) {
    lastMoveSteps_ = steps;
    lastMoveDuration_ = duration;
    totalSteps_ += steps;
    movementCount_++;
}

}  // namespace lithium::device::asi::focuser::components
