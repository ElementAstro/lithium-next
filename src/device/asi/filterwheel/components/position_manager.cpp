#include "position_manager.hpp"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

namespace lithium::device::asi::filterwheel {

PositionManager::PositionManager(std::shared_ptr<HardwareInterface> hw)
    : hardware_(std::move(hw))
    , current_position_(0)
    , target_position_(0)
    , is_moving_(false)
    , move_timeout_ms_(30000)  // 30 seconds default timeout
    , position_threshold_(0.1) {
    spdlog::info("PositionManager initialized");
}

PositionManager::~PositionManager() {
    spdlog::info("PositionManager destroyed");
}

bool PositionManager::moveToPosition(int position) {
    if (!hardware_) {
        spdlog::error( "Hardware interface not available");
        return false;
    }

    // Validate position range
    if (position < 0 || position >= getSlotCount()) {
        spdlog::error( "Invalid position: {} (valid range: 0-{})", position, getSlotCount() - 1);
        return false;
    }

    if (is_moving_) {
        spdlog::warn( "Already moving, canceling current move");
        stopMovement();
    }

    spdlog::info( "Moving to position {}", position);
    target_position_ = position;
    is_moving_ = true;

    bool success = hardware_->setPosition(position);
    if (!success) {
        spdlog::error( "Failed to initiate move to position {}", position);
        is_moving_ = false;
        return false;
    }

    // Start monitoring thread
    std::thread([this]() {
        monitorMovement();
    }).detach();

    return true;
}

bool PositionManager::isMoving() const {
    return is_moving_;
}

int PositionManager::getCurrentPosition() const {
    if (hardware_) {
        current_position_ = hardware_->getCurrentPosition();
    }
    return current_position_;
}

int PositionManager::getTargetPosition() const {
    return target_position_;
}

void PositionManager::stopMovement() {
    if (!is_moving_) {
        return;
    }

    spdlog::info( "Stopping movement");
    is_moving_ = false;
    
    // Note: Most filterwheel controllers don't support stopping mid-movement
    // The movement will complete to the nearest stable position
}

bool PositionManager::waitForMovement(int timeout_ms) {
    if (!is_moving_) {
        return true;
    }

    spdlog::info( "Waiting for movement to complete (timeout: {} ms)", timeout_ms);
    
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(timeout_ms);

    while (is_moving_) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout_duration) {
            spdlog::error( "Movement timeout after {} ms", timeout_ms);
            is_moving_ = false;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::info( "Movement completed successfully");
    return true;
}

void PositionManager::setMoveTimeout(int timeout_ms) {
    move_timeout_ms_ = timeout_ms;
    spdlog::info( "Move timeout set to {} ms", timeout_ms);
}

int PositionManager::getMoveTimeout() const {
    return move_timeout_ms_;
}

int PositionManager::getSlotCount() const {
    if (hardware_) {
        return hardware_->getSlotCount();
    }
    return 0;
}

bool PositionManager::isPositionValid(int position) const {
    return position >= 0 && position < getSlotCount();
}

double PositionManager::getPositionAccuracy() const {
    return position_threshold_;
}

void PositionManager::setPositionAccuracy(double threshold) {
    position_threshold_ = threshold;
    spdlog::info( "Position accuracy threshold set to {:.2f}", threshold);
}

std::vector<int> PositionManager::getAvailablePositions() const {
    std::vector<int> positions;
    int slot_count = getSlotCount();
    
    for (int i = 0; i < slot_count; ++i) {
        positions.push_back(i);
    }
    
    return positions;
}

bool PositionManager::calibratePosition(int position) {
    if (!isPositionValid(position)) {
        spdlog::error( "Invalid position for calibration: {}", position);
        return false;
    }

    spdlog::info( "Calibrating position {}", position);
    
    // Move to position and verify
    if (!moveToPosition(position)) {
        spdlog::error( "Failed to move to calibration position {}", position);
        return false;
    }

    if (!waitForMovement(move_timeout_ms_)) {
        spdlog::error( "Calibration move timeout for position {}", position);
        return false;
    }

    // Verify we're at the correct position
    int actual_position = getCurrentPosition();
    if (actual_position != position) {
        spdlog::error( "Calibration failed: expected {}, got {}", position, actual_position);
        return false;
    }

    spdlog::info( "Position {} calibrated successfully", position);
    return true;
}

void PositionManager::monitorMovement() {
    auto start_time = std::chrono::steady_clock::now();
    auto timeout_duration = std::chrono::milliseconds(move_timeout_ms_);

    while (is_moving_) {
        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout_duration) {
            spdlog::error( "Movement timeout after {} ms", move_timeout_ms_);
            is_moving_ = false;
            break;
        }

        // Check if movement is complete
        if (hardware_) {
            current_position_ = hardware_->getCurrentPosition();
            bool movement_complete = !hardware_->isMoving();
            
            if (movement_complete) {
                is_moving_ = false;
                spdlog::info( "Movement completed, current position: {}", current_position_);
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace lithium::device::asi::filterwheel
