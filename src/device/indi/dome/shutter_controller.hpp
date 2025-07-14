/*
 * shutter_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_DEVICE_INDI_DOME_SHUTTER_CONTROLLER_HPP
#define LITHIUM_DEVICE_INDI_DOME_SHUTTER_CONTROLLER_HPP

#include "component_base.hpp"
#include "device/template/dome.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>

namespace lithium::device::indi {

// Forward declaration
class PropertyManager;

/**
 * @brief Controls dome shutter operations including open/close commands,
 *        safety interlocks, and automatic weather response.
 */
class ShutterController : public DomeComponentBase {
public:
    explicit ShutterController(std::shared_ptr<INDIDomeCore> core);
    ~ShutterController() override = default;

    // Component interface
    auto initialize() -> bool override;
    auto cleanup() -> bool override;
    void handlePropertyUpdate(const INDI::Property& property) override;

    // Shutter commands
    auto openShutter() -> bool;
    auto closeShutter() -> bool;
    auto abortShutter() -> bool;
    auto toggleShutter() -> bool;

    // State queries
    [[nodiscard]] auto getShutterState() const -> ShutterState;
    [[nodiscard]] auto isShutterOpen() const -> bool { return getShutterState() == ShutterState::OPEN; }
    [[nodiscard]] auto isShutterClosed() const -> bool { return getShutterState() == ShutterState::CLOSED; }
    [[nodiscard]] auto isShutterMoving() const -> bool;
    [[nodiscard]] auto hasShutter() const -> bool { return has_shutter_.load(); }

    // Safety features
    auto enableSafetyInterlock(bool enable) -> bool;
    [[nodiscard]] auto isSafetyInterlockEnabled() const -> bool { return safety_interlock_enabled_.load(); }
    auto setSafetyCallback(std::function<bool()> callback) -> bool;
    [[nodiscard]] auto isSafeToOperate() const -> bool;

    // Weather response
    auto enableWeatherResponse(bool enable) -> bool;
    [[nodiscard]] auto isWeatherResponseEnabled() const -> bool { return weather_response_enabled_.load(); }
    auto setWeatherCallback(std::function<bool()> callback) -> bool;
    auto checkWeatherSafety() -> bool;

    // Automatic operations
    auto enableAutoClose(bool enable, std::chrono::minutes timeout = std::chrono::minutes(30)) -> bool;
    [[nodiscard]] auto isAutoCloseEnabled() const -> bool { return auto_close_enabled_.load(); }
    auto resetAutoCloseTimer() -> bool;
    [[nodiscard]] auto getAutoCloseTimeRemaining() const -> std::chrono::minutes;

    // Operation timeouts
    auto setOperationTimeout(std::chrono::seconds timeout) -> bool;
    [[nodiscard]] auto getOperationTimeout() const -> std::chrono::seconds;
    [[nodiscard]] auto isOperationTimedOut() const -> bool;

    // Statistics
    [[nodiscard]] auto getShutterOperations() const -> uint64_t { return shutter_operations_.load(); }
    auto resetShutterOperations() -> bool;
    [[nodiscard]] auto getTotalOpenTime() const -> std::chrono::hours;
    [[nodiscard]] auto getAverageOperationTime() const -> std::chrono::seconds;
    [[nodiscard]] auto getLastOperationDuration() const -> std::chrono::seconds;

    // Event callbacks
    using ShutterStateCallback = std::function<void(ShutterState state)>;
    using ShutterCompleteCallback = std::function<void(bool success, const std::string& message)>;
    using SafetyTriggerCallback = std::function<void(const std::string& reason)>;

    void setShutterStateCallback(ShutterStateCallback callback) { shutter_state_callback_ = std::move(callback); }
    void setShutterCompleteCallback(ShutterCompleteCallback callback) { shutter_complete_callback_ = std::move(callback); }
    void setSafetyTriggerCallback(SafetyTriggerCallback callback) { safety_trigger_callback_ = std::move(callback); }

    // Component dependencies
    void setPropertyManager(std::shared_ptr<PropertyManager> manager) { property_manager_ = manager; }

    // Emergency operations
    auto emergencyClose() -> bool;
    [[nodiscard]] auto isEmergencyCloseActive() const -> bool { return emergency_close_active_.load(); }
    auto clearEmergencyClose() -> bool;

    // Maintenance operations
    auto performShutterTest() -> bool;
    auto calibrateShutter() -> bool;
    [[nodiscard]] auto getShutterHealth() const -> std::string;

    // Validation helpers
    [[nodiscard]] auto canOpenShutter() const -> bool;
    [[nodiscard]] auto canCloseShutter() const -> bool;
    [[nodiscard]] auto canPerformOperation() const -> bool;

private:
    // Component dependencies
    std::weak_ptr<PropertyManager> property_manager_;

    // Shutter state (atomic for thread safety)
    std::atomic<int> shutter_state_{static_cast<int>(ShutterState::UNKNOWN)};
    std::atomic<bool> has_shutter_{false};
    std::atomic<bool> is_moving_{false};

    // Safety features
    std::atomic<bool> safety_interlock_enabled_{true};
    std::atomic<bool> weather_response_enabled_{true};
    std::atomic<bool> emergency_close_active_{false};
    std::function<bool()> safety_callback_;
    std::function<bool()> weather_callback_;

    // Automatic operations
    std::atomic<bool> auto_close_enabled_{false};
    std::chrono::minutes auto_close_timeout_{30};
    std::chrono::steady_clock::time_point last_activity_time_;

    // Operation timeouts
    std::chrono::seconds operation_timeout_{30};
    std::chrono::steady_clock::time_point operation_start_time_;

    // Statistics
    std::atomic<uint64_t> shutter_operations_{0};
    std::chrono::steady_clock::time_point open_time_start_;
    std::atomic<int64_t> total_open_time_ms_{0};
    std::atomic<int64_t> total_operation_time_ms_{0};
    std::atomic<int64_t> last_operation_duration_ms_{0};

    // Thread safety
    mutable std::recursive_mutex shutter_mutex_;

    // Callbacks
    ShutterStateCallback shutter_state_callback_;
    ShutterCompleteCallback shutter_complete_callback_;
    SafetyTriggerCallback safety_trigger_callback_;

    // Internal methods
    void updateShutterState(ShutterState state);
    void updateMovingState(bool moving);
    auto performSafetyChecks() const -> bool;
    auto checkOperationTimeout() -> bool;
    void recordOperation(std::chrono::milliseconds duration);
    void updateOpenTime();

    // Safety check methods
    auto checkSafetyInterlock() -> bool;
    auto checkWeatherConditions() -> bool;
    auto checkSystemHealth() -> bool;

    // Event notification
    void notifyStateChange(ShutterState state);
    void notifyOperationComplete(bool success, const std::string& message = "");
    void notifySafetyTrigger(const std::string& reason);

    // Property update handlers
    void handleShutterPropertyUpdate(const INDI::Property& property);

    // Timer helpers
    void startOperationTimer();
    void stopOperationTimer();
    [[nodiscard]] auto getOperationDuration() const -> std::chrono::milliseconds;
};

} // namespace lithium::device::indi

#endif // LITHIUM_DEVICE_INDI_DOME_SHUTTER_CONTROLLER_HPP
