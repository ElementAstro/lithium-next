#include "backlash_compensator.hpp"
#include "hardware_interface.hpp"
#include "movement_controller.hpp"
#include <algorithm>
#include <cmath>

namespace lithium::device::ascom::focuser::components {

BacklashCompensator::BacklashCompensator(std::shared_ptr<HardwareInterface> hardware,
                                        std::shared_ptr<MovementController> movement)
    : hardware_(hardware)
    , movement_(movement)
    , config_{}
    , compensation_enabled_(false)
    , last_direction_(MovementDirection::NONE)
    , backlash_position_(0)
    , compensation_active_(false)
    , stats_{}
{
}

BacklashCompensator::~BacklashCompensator() = default;

auto BacklashCompensator::initialize() -> bool {
    try {
        // Initialize backlash settings
        config_.enabled = false;
        config_.backlashSteps = 0;
        config_.direction = MovementDirection::NONE;
        config_.algorithm = BacklashAlgorithm::SIMPLE;
        
        // Reset statistics
        resetBacklashStats();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto BacklashCompensator::destroy() -> bool {
    compensation_enabled_ = false;
    compensation_active_ = false;
    return true;
}

auto BacklashCompensator::getBacklashConfig() -> BacklashConfig {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

auto BacklashCompensator::setBacklashConfig(const BacklashConfig& config) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    // Validate configuration
    if (config.backlashSteps < 0 || config.backlashSteps > 10000) {
        return false;
    }
    
    config_ = config;
    compensation_enabled_ = config.enabled;
    
    return true;
}

auto BacklashCompensator::enableBacklashCompensation(bool enable) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.enabled = enable;
    compensation_enabled_ = enable;
    
    return true;
}

auto BacklashCompensator::isBacklashCompensationEnabled() -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.enabled;
}

auto BacklashCompensator::setBacklashSteps(int steps) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (steps < 0 || steps > 10000) {
        return false;
    }
    
    config_.backlashSteps = steps;
    
    return true;
}

auto BacklashCompensator::getBacklashSteps() -> int {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.backlashSteps;
}

auto BacklashCompensator::setBacklashDirection(MovementDirection direction) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.direction = direction;
    
    return true;
}

auto BacklashCompensator::getBacklashDirection() -> MovementDirection {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.direction;
}

auto BacklashCompensator::calculateBacklashCompensation(int targetPosition, MovementDirection direction) -> int {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!config_.enabled || config_.backlashSteps == 0) {
        return 0;
    }
    
    // Check if direction change requires compensation
    if (last_direction_ != MovementDirection::NONE && 
        last_direction_ != direction && 
        direction != MovementDirection::NONE) {
        
        // Direction change detected, apply compensation
        switch (config_.algorithm) {
            case BacklashAlgorithm::SIMPLE:
                return calculateSimpleCompensation(direction);
            case BacklashAlgorithm::ADAPTIVE:
                return calculateAdaptiveCompensation(direction);
            case BacklashAlgorithm::DYNAMIC:
                return calculateDynamicCompensation(direction);
        }
    }
    
    return 0;
}

auto BacklashCompensator::applyBacklashCompensation(int steps, MovementDirection direction) -> bool {
    if (!compensation_enabled_ || steps == 0) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(compensation_mutex_);
    compensation_active_ = true;
    
    try {
        // Apply compensation movement
        bool success = movement_->moveRelative(steps);
        
        if (success) {
            // Update statistics
            updateBacklashStats(steps, direction);
            
            // Update backlash position
            backlash_position_ += steps;
            
            // Record compensation
            recordCompensation(steps, direction, success);
            
            // Notify callback
            if (compensation_callback_) {
                compensation_callback_(steps, direction, success);
            }
        }
        
        compensation_active_ = false;
        return success;
    } catch (const std::exception& e) {
        compensation_active_ = false;
        return false;
    }
}

auto BacklashCompensator::isCompensationActive() -> bool {
    std::lock_guard<std::mutex> lock(compensation_mutex_);
    return compensation_active_;
}

auto BacklashCompensator::getBacklashStats() -> BacklashStats {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

auto BacklashCompensator::resetBacklashStats() -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = BacklashStats{};
    stats_.startTime = std::chrono::steady_clock::now();
}

auto BacklashCompensator::calibrateBacklash(int testRange) -> bool {
    try {
        // Perform backlash calibration
        return performBacklashCalibration(testRange);
    } catch (const std::exception& e) {
        return false;
    }
}

auto BacklashCompensator::autoDetectBacklash() -> bool {
    try {
        // Perform automatic backlash detection
        return performAutoDetection();
    } catch (const std::exception& e) {
        return false;
    }
}

auto BacklashCompensator::getCompensationHistory() -> std::vector<BacklashCompensation> {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return compensation_history_;
}

auto BacklashCompensator::getCompensationHistory(std::chrono::seconds duration) -> std::vector<BacklashCompensation> {
    std::lock_guard<std::mutex> lock(history_mutex_);
    std::vector<BacklashCompensation> recent_history;
    
    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    
    for (const auto& compensation : compensation_history_) {
        if (compensation.timestamp >= cutoff_time) {
            recent_history.push_back(compensation);
        }
    }
    
    return recent_history;
}

auto BacklashCompensator::clearCompensationHistory() -> void {
    std::lock_guard<std::mutex> lock(history_mutex_);
    compensation_history_.clear();
}

auto BacklashCompensator::exportBacklashData(const std::string& filename) -> bool {
    // Implementation for exporting backlash data
    return false; // Placeholder
}

auto BacklashCompensator::importBacklashData(const std::string& filename) -> bool {
    // Implementation for importing backlash data
    return false; // Placeholder
}

auto BacklashCompensator::setCompensationCallback(CompensationCallback callback) -> void {
    compensation_callback_ = std::move(callback);
}

auto BacklashCompensator::setBacklashAlertCallback(BacklashAlertCallback callback) -> void {
    backlash_alert_callback_ = std::move(callback);
}

auto BacklashCompensator::predictBacklashCompensation(int targetPosition, MovementDirection direction) -> int {
    return calculateBacklashCompensation(targetPosition, direction);
}

auto BacklashCompensator::validateBacklashSettings() -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.backlashSteps >= 0 && config_.backlashSteps <= 10000;
}

auto BacklashCompensator::optimizeBacklashSettings() -> bool {
    // Implementation for optimizing backlash settings
    return false; // Placeholder
}

auto BacklashCompensator::updateLastDirection(MovementDirection direction) -> void {
    last_direction_ = direction;
}

// Private methods

auto BacklashCompensator::calculateSimpleCompensation(MovementDirection direction) -> int {
    // Simple compensation: always apply fixed backlash steps
    if (config_.direction == MovementDirection::NONE || config_.direction == direction) {
        return config_.backlashSteps;
    }
    
    return 0;
}

auto BacklashCompensator::calculateAdaptiveCompensation(MovementDirection direction) -> int {
    // Adaptive compensation based on historical data
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (stats_.totalCompensations == 0) {
        return config_.backlashSteps;
    }
    
    // Use success rate to adjust compensation
    double success_rate = static_cast<double>(stats_.successfulCompensations) / stats_.totalCompensations;
    
    if (success_rate > 0.95) {
        // High success rate, might be over-compensating
        return static_cast<int>(config_.backlashSteps * 0.9);
    } else if (success_rate < 0.85) {
        // Low success rate, might be under-compensating
        return static_cast<int>(config_.backlashSteps * 1.1);
    }
    
    return config_.backlashSteps;
}

auto BacklashCompensator::calculateDynamicCompensation(MovementDirection direction) -> int {
    // Dynamic compensation based on movement distance and speed
    // This is a placeholder implementation
    return config_.backlashSteps;
}

auto BacklashCompensator::updateBacklashStats(int steps, MovementDirection direction) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.totalCompensations++;
    stats_.totalCompensationSteps += std::abs(steps);
    stats_.lastCompensationTime = std::chrono::steady_clock::now();
    
    if (direction == MovementDirection::INWARD) {
        stats_.inwardCompensations++;
    } else if (direction == MovementDirection::OUTWARD) {
        stats_.outwardCompensations++;
    }
    
    // Calculate average compensation
    stats_.averageCompensation = 
        static_cast<double>(stats_.totalCompensationSteps) / stats_.totalCompensations;
}

auto BacklashCompensator::recordCompensation(int steps, MovementDirection direction, bool success) -> void {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    BacklashCompensation compensation{
        .timestamp = std::chrono::steady_clock::now(),
        .steps = steps,
        .direction = direction,
        .success = success,
        .position = backlash_position_
    };
    
    compensation_history_.push_back(compensation);
    
    // Limit history size
    if (compensation_history_.size() > MAX_HISTORY_SIZE) {
        compensation_history_.erase(compensation_history_.begin());
    }
    
    // Update success count
    if (success) {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.successfulCompensations++;
    }
}

auto BacklashCompensator::performBacklashCalibration(int testRange) -> bool {
    // Implementation for backlash calibration
    // This would involve moving back and forth to measure backlash
    return false; // Placeholder
}

auto BacklashCompensator::performAutoDetection() -> bool {
    // Implementation for automatic backlash detection
    // This would analyze movement patterns to detect backlash
    return false; // Placeholder
}

auto BacklashCompensator::notifyBacklashAlert(const std::string& message) -> void {
    if (backlash_alert_callback_) {
        try {
            backlash_alert_callback_(message);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto BacklashCompensator::validateCompensationSteps(int steps) -> int {
    return std::clamp(steps, 0, 10000);
}

auto BacklashCompensator::isDirectionChangeRequired(MovementDirection newDirection) -> bool {
    return last_direction_ != MovementDirection::NONE && 
           last_direction_ != newDirection && 
           newDirection != MovementDirection::NONE;
}

auto BacklashCompensator::calculateOptimalBacklash() -> int {
    // Calculate optimal backlash based on historical data
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (stats_.totalCompensations == 0) {
        return config_.backlashSteps;
    }
    
    // Simple optimization: use average compensation
    return static_cast<int>(stats_.averageCompensation);
}

} // namespace lithium::device::ascom::focuser::components
