/*
 * motion_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_DEVICE_INDI_DOME_MOTION_CONTROLLER_HPP
#define LITHIUM_DEVICE_INDI_DOME_MOTION_CONTROLLER_HPP

#include "component_base.hpp"
#include "device/template/dome.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>

namespace lithium::device::indi {

// Forward declaration
class PropertyManager;

/**
 * @brief Controls dome motion including azimuth movement, speed control, and motion coordination.
 *        Provides precise movement control with backlash compensation and motion profiling.
 */
class MotionController : public DomeComponentBase {
public:
    explicit MotionController(std::shared_ptr<INDIDomeCore> core);
    ~MotionController() override = default;

    // Component interface
    auto initialize() -> bool override;
    auto cleanup() -> bool override;
    void handlePropertyUpdate(const INDI::Property& property) override;

    // Core motion commands
    auto moveToAzimuth(double azimuth) -> bool;
    auto rotateClockwise() -> bool;
    auto rotateCounterClockwise() -> bool;
    auto stopRotation() -> bool;
    auto abortMotion() -> bool;
    auto syncAzimuth(double azimuth) -> bool;

    // Speed control
    auto getRotationSpeed() -> std::optional<double>;
    auto setRotationSpeed(double speed) -> bool;
    auto getMaxSpeed() const -> double { return max_speed_; }
    auto getMinSpeed() const -> double { return min_speed_; }

    // State queries
    [[nodiscard]] auto getCurrentAzimuth() const -> double { return current_azimuth_.load(); }
    [[nodiscard]] auto getTargetAzimuth() const -> double { return target_azimuth_.load(); }
    [[nodiscard]] auto isMoving() const -> bool { return is_moving_.load(); }
    [[nodiscard]] auto getMotionDirection() const -> DomeMotion;
    [[nodiscard]] auto getRemainingDistance() const -> double;
    [[nodiscard]] auto getEstimatedTimeToTarget() const -> std::chrono::seconds;

    // Backlash compensation
    auto getBacklash() -> double;
    auto setBacklash(double backlash) -> bool;
    auto enableBacklashCompensation(bool enable) -> bool;
    [[nodiscard]] auto isBacklashCompensationEnabled() const -> bool { return backlash_enabled_.load(); }

    // Motion planning
    auto calculateOptimalPath(double from, double to) -> std::pair<double, DomeMotion>;
    auto normalizeAzimuth(double azimuth) const -> double;
    auto getAzimuthalDistance(double from, double to) const -> double;
    auto getShortestPath(double from, double to) const -> std::pair<double, DomeMotion>;

    // Motion limits and safety
    auto setSpeedLimits(double minSpeed, double maxSpeed) -> bool;
    auto setAzimuthLimits(double minAz, double maxAz) -> bool;
    auto setSafetyLimits(double maxAcceleration, double maxJerk) -> bool;
    [[nodiscard]] auto isPositionSafe(double azimuth) const -> bool;
    [[nodiscard]] auto isSpeedSafe(double speed) const -> bool;

    // Motion profiling
    auto enableMotionProfiling(bool enable) -> bool;
    [[nodiscard]] auto isMotionProfilingEnabled() const -> bool { return motion_profiling_enabled_.load(); }
    auto setAccelerationProfile(double acceleration, double deceleration) -> bool;
    auto getMotionProfile() const -> std::string;

    // Callbacks for motion events
    using MotionStartCallback = std::function<void(double targetAzimuth)>;
    using MotionCompleteCallback = std::function<void(bool success, const std::string& message)>;
    using PositionUpdateCallback = std::function<void(double currentAzimuth, double targetAzimuth)>;

    void setMotionStartCallback(MotionStartCallback callback) { motion_start_callback_ = std::move(callback); }
    void setMotionCompleteCallback(MotionCompleteCallback callback) { motion_complete_callback_ = std::move(callback); }
    void setPositionUpdateCallback(PositionUpdateCallback callback) { position_update_callback_ = std::move(callback); }

    // Component dependencies
    void setPropertyManager(std::shared_ptr<PropertyManager> manager) { property_manager_ = manager; }

    // Statistics and diagnostics
    [[nodiscard]] auto getTotalRotation() const -> double { return total_rotation_.load(); }
    auto resetTotalRotation() -> bool;
    [[nodiscard]] auto getAverageSpeed() const -> double { return average_speed_.load(); }
    [[nodiscard]] auto getMotionCount() const -> uint64_t { return motion_count_.load(); }
    [[nodiscard]] auto getLastMotionDuration() const -> std::chrono::milliseconds;

    // Emergency functions
    auto emergencyStop() -> bool;
    auto isEmergencyStopActive() const -> bool { return emergency_stop_active_.load(); }
    auto clearEmergencyStop() -> bool;

private:
    // Component dependencies
    std::weak_ptr<PropertyManager> property_manager_;

    // Motion state (atomic for thread safety)
    std::atomic<double> current_azimuth_{0.0};
    std::atomic<double> target_azimuth_{0.0};
    std::atomic<bool> is_moving_{false};
    std::atomic<int> motion_direction_{static_cast<int>(DomeMotion::STOP)};
    std::atomic<double> current_speed_{0.0};

    // Motion limits
    double min_speed_{1.0};
    double max_speed_{10.0};
    double min_azimuth_{0.0};
    double max_azimuth_{360.0};
    double max_acceleration_{5.0};
    double max_jerk_{10.0};

    // Backlash compensation
    std::atomic<double> backlash_value_{0.0};
    std::atomic<bool> backlash_enabled_{false};
    std::atomic<bool> backlash_applied_{false};

    // Motion profiling
    std::atomic<bool> motion_profiling_enabled_{false};
    double acceleration_rate_{2.0};
    double deceleration_rate_{2.0};

    // Safety features
    std::atomic<bool> emergency_stop_active_{false};
    std::atomic<bool> safety_limits_enabled_{true};

    // Statistics
    std::atomic<double> total_rotation_{0.0};
    std::atomic<double> average_speed_{0.0};
    std::atomic<uint64_t> motion_count_{0};
    std::chrono::steady_clock::time_point last_motion_start_;
    std::atomic<int64_t> last_motion_duration_ms_{0};

    // Thread safety
    mutable std::recursive_mutex motion_mutex_;

    // Callbacks
    MotionStartCallback motion_start_callback_;
    MotionCompleteCallback motion_complete_callback_;
    PositionUpdateCallback position_update_callback_;

    // Internal methods
    void updateCurrentAzimuth(double azimuth);
    void updateTargetAzimuth(double azimuth);
    void updateMotionState(bool moving);
    void updateMotionDirection(DomeMotion direction);
    void updateSpeed(double speed);

    // Motion planning helpers
    auto calculateBacklashCompensation(double targetAz) -> double;
    auto applyMotionProfile(double distance, double speed) -> std::pair<double, double>;
    void notifyMotionStart(double targetAzimuth);
    void notifyMotionComplete(bool success, const std::string& message = "");
    void notifyPositionUpdate();

    // Validation helpers
    [[nodiscard]] auto validateAzimuth(double azimuth) const -> bool;
    [[nodiscard]] auto validateSpeed(double speed) const -> bool;
    [[nodiscard]] auto canStartMotion() const -> bool;

    // Statistics helpers
    void updateMotionStatistics(double distance, std::chrono::milliseconds duration);
    void incrementMotionCount();

    // Property update handlers
    void handleAzimuthUpdate(const INDI::Property& property);
    void handleMotionUpdate(const INDI::Property& property);
    void handleSpeedUpdate(const INDI::Property& property);
};

} // namespace lithium::device::indi

#endif // LITHIUM_DEVICE_INDI_DOME_MOTION_CONTROLLER_HPP
