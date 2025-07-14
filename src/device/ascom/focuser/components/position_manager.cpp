#include "position_manager.hpp"
#include "hardware_interface.hpp"
#include <algorithm>
#include <cmath>

namespace lithium::device::ascom::focuser::components {

PositionManager::PositionManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware)
    , current_position_(0)
    , target_position_(0)
    , position_valid_(false)
    , position_offset_(0)
    , position_limits_{}
    , position_stats_{}
{
}

PositionManager::~PositionManager() = default;

auto PositionManager::initialize() -> bool {
    try {
        // Read current position from hardware
        if (!syncPositionFromHardware()) {
            return false;
        }

        // Initialize position limits
        position_limits_.minPosition = hardware_->getMinPosition();
        position_limits_.maxPosition = hardware_->getMaxPosition();
        position_limits_.enforceHardLimits = true;
        position_limits_.enforceStepLimits = true;

        // Reset statistics
        resetPositionStats();

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto PositionManager::destroy() -> bool {
    position_valid_ = false;
    return true;
}

auto PositionManager::getCurrentPosition() -> int {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return current_position_;
}

auto PositionManager::getTargetPosition() -> int {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return target_position_;
}

auto PositionManager::isPositionValid() -> bool {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return position_valid_;
}

auto PositionManager::setCurrentPosition(int position) -> bool {
    std::lock_guard<std::mutex> lock(position_mutex_);

    if (!isPositionInLimits(position)) {
        return false;
    }

    current_position_ = position;
    position_valid_ = true;

    updatePositionStats(position);
    notifyPositionChanged(position);

    return true;
}

auto PositionManager::setTargetPosition(int position) -> bool {
    std::lock_guard<std::mutex> lock(position_mutex_);

    if (!isPositionInLimits(position)) {
        return false;
    }

    target_position_ = position;

    return true;
}

auto PositionManager::syncPositionFromHardware() -> bool {
    try {
        auto position = hardware_->getCurrentPosition();
        if (position.has_value()) {
            return setCurrentPosition(position.value());
        }
        return false;
    } catch (const std::exception& e) {
        return false;
    }
}

auto PositionManager::getPositionLimits() -> PositionLimits {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return position_limits_;
}

auto PositionManager::setPositionLimits(const PositionLimits& limits) -> bool {
    std::lock_guard<std::mutex> lock(position_mutex_);

    // Validate limits
    if (limits.minPosition >= limits.maxPosition) {
        return false;
    }

    if (limits.maxStepSize <= 0) {
        return false;
    }

    position_limits_ = limits;

    // Check if current position is still valid
    if (!isPositionInLimits(current_position_)) {
        // Clamp to limits
        current_position_ = std::clamp(current_position_, limits.minPosition, limits.maxPosition);
        notifyPositionChanged(current_position_);
    }

    return true;
}

auto PositionManager::getPositionOffset() -> int {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return position_offset_;
}

auto PositionManager::setPositionOffset(int offset) -> bool {
    std::lock_guard<std::mutex> lock(position_mutex_);
    position_offset_ = offset;

    // Recalculate effective position
    int effective_position = current_position_ + offset;

    // Validate the effective position is within limits
    if (!isPositionInLimits(effective_position)) {
        return false;
    }

    notifyPositionChanged(effective_position);

    return true;
}

auto PositionManager::getEffectivePosition() -> int {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return current_position_ + position_offset_;
}

auto PositionManager::validatePosition(int position) -> bool {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return isPositionInLimits(position);
}

auto PositionManager::clampPosition(int position) -> int {
    std::lock_guard<std::mutex> lock(position_mutex_);
    return std::clamp(position, position_limits_.minPosition, position_limits_.maxPosition);
}

auto PositionManager::calculateDistance(int from, int to) -> int {
    return std::abs(to - from);
}

auto PositionManager::calculateSteps(int from, int to) -> int {
    return to - from;
}

auto PositionManager::getPositionStats() -> PositionStats {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return position_stats_;
}

auto PositionManager::resetPositionStats() -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    position_stats_ = PositionStats{};
    position_stats_.startTime = std::chrono::steady_clock::now();
}

auto PositionManager::getPositionHistory() -> std::vector<PositionReading> {
    std::lock_guard<std::mutex> lock(history_mutex_);
    return position_history_;
}

auto PositionManager::getPositionHistory(std::chrono::seconds duration) -> std::vector<PositionReading> {
    std::lock_guard<std::mutex> lock(history_mutex_);
    std::vector<PositionReading> recent_history;

    auto cutoff_time = std::chrono::steady_clock::now() - duration;

    for (const auto& reading : position_history_) {
        if (reading.timestamp >= cutoff_time) {
            recent_history.push_back(reading);
        }
    }

    return recent_history;
}

auto PositionManager::clearPositionHistory() -> void {
    std::lock_guard<std::mutex> lock(history_mutex_);
    position_history_.clear();
}

auto PositionManager::exportPositionData(const std::string& filename) -> bool {
    // Implementation for exporting position data
    return false; // Placeholder
}

auto PositionManager::importPositionData(const std::string& filename) -> bool {
    // Implementation for importing position data
    return false; // Placeholder
}

auto PositionManager::setPositionCallback(PositionCallback callback) -> void {
    position_callback_ = std::move(callback);
}

auto PositionManager::setLimitCallback(LimitCallback callback) -> void {
    limit_callback_ = std::move(callback);
}

auto PositionManager::setPositionAlertCallback(PositionAlertCallback callback) -> void {
    position_alert_callback_ = std::move(callback);
}

auto PositionManager::enablePositionTracking(bool enable) -> bool {
    position_tracking_enabled_ = enable;
    return true;
}

auto PositionManager::isPositionTrackingEnabled() -> bool {
    return position_tracking_enabled_;
}

auto PositionManager::getPositionAccuracy() -> double {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return position_stats_.accuracy;
}

auto PositionManager::getPositionStability() -> double {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return position_stats_.stability;
}

auto PositionManager::calibratePosition() -> bool {
    // Implementation for position calibration
    return false; // Placeholder
}

auto PositionManager::autoDetectLimits() -> bool {
    // Implementation for auto-detecting position limits
    return false; // Placeholder
}

// Private methods

auto PositionManager::isPositionInLimits(int position) -> bool {
    if (!position_limits_.enforceHardLimits) {
        return true;
    }

    return position >= position_limits_.minPosition &&
           position <= position_limits_.maxPosition;
}

auto PositionManager::updatePositionStats(int position) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    position_stats_.totalMoves++;
    position_stats_.currentPosition = position;
    position_stats_.lastUpdateTime = std::chrono::steady_clock::now();

    // Update min/max positions
    if (position_stats_.totalMoves == 1) {
        position_stats_.minPosition = position;
        position_stats_.maxPosition = position;
    } else {
        position_stats_.minPosition = std::min(position_stats_.minPosition, position);
        position_stats_.maxPosition = std::max(position_stats_.maxPosition, position);
    }

    // Calculate average position
    position_stats_.averagePosition =
        (position_stats_.averagePosition * (position_stats_.totalMoves - 1) + position) /
        position_stats_.totalMoves;

    // Update position range
    position_stats_.positionRange = position_stats_.maxPosition - position_stats_.minPosition;

    // Calculate drift from target
    if (target_position_ != 0) {
        position_stats_.drift = position - target_position_;
    }
}

auto PositionManager::addPositionReading(int position, bool isTarget) -> void {
    std::lock_guard<std::mutex> lock(history_mutex_);

    PositionReading reading{
        .timestamp = std::chrono::steady_clock::now(),
        .position = position,
        .isTargetPosition = isTarget,
        .accuracy = calculateAccuracy(position),
        .drift = position - target_position_
    };

    position_history_.push_back(reading);

    // Limit history size
    if (position_history_.size() > MAX_HISTORY_SIZE) {
        position_history_.erase(position_history_.begin());
    }
}

auto PositionManager::calculateAccuracy(int position) -> double {
    if (target_position_ == 0) {
        return 100.0; // Perfect accuracy if no target set
    }

    int error = std::abs(position - target_position_);
    double accuracy = 100.0 - (static_cast<double>(error) / std::max(1, target_position_)) * 100.0;

    return std::max(0.0, accuracy);
}

auto PositionManager::notifyPositionChanged(int position) -> void {
    if (position_callback_) {
        try {
            position_callback_(position);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }

    // Add to history
    addPositionReading(position, false);
}

auto PositionManager::notifyLimitReached(int position, const std::string& limitType) -> void {
    if (limit_callback_) {
        try {
            limit_callback_(position, limitType);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto PositionManager::notifyPositionAlert(int position, const std::string& message) -> void {
    if (position_alert_callback_) {
        try {
            position_alert_callback_(position, message);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto PositionManager::validatePositionLimits(const PositionLimits& limits) -> bool {
    return limits.minPosition < limits.maxPosition &&
           limits.maxStepSize > 0 &&
           limits.minStepSize >= 0;
}

auto PositionManager::enforcePositionLimits(int& position) -> bool {
    if (!position_limits_.enforceHardLimits) {
        return true;
    }

    if (position < position_limits_.minPosition) {
        position = position_limits_.minPosition;
        notifyLimitReached(position, "minimum");
        return false;
    }

    if (position > position_limits_.maxPosition) {
        position = position_limits_.maxPosition;
        notifyLimitReached(position, "maximum");
        return false;
    }

    return true;
}

auto PositionManager::formatPosition(int position) -> std::string {
    return std::to_string(position) + " steps";
}

} // namespace lithium::device::ascom::focuser::components
