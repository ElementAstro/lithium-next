/*
 * controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Rotator Controller

This modular controller orchestrates the rotator components to provide
a clean, maintainable, and testable interface for ASCOM rotator control.

*************************************************/

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "./components/hardware_interface.hpp"
#include "./components/position_manager.hpp"
#include "./components/property_manager.hpp"
#include "./components/preset_manager.hpp"
#include "device/template/rotator.hpp"

namespace lithium::device::ascom::rotator {

// Forward declarations
namespace components {
class HardwareInterface;
class PositionManager;
class PropertyManager;
class PresetManager;
}

/**
 * @brief Configuration structure for the ASCOM Rotator Controller
 */
struct RotatorConfig {
    std::string device_name{"ASCOM Rotator"};
    std::string client_id{"Lithium-Next"};
    components::ConnectionType connection_type{components::ConnectionType::ALPACA_REST};

    // Alpaca configuration
    std::string alpaca_host{"localhost"};
    int alpaca_port{11111};
    int alpaca_device_number{0};

    // COM configuration (Windows only)
    std::string com_prog_id;

    // Monitoring configuration
    bool enable_position_monitoring{true};
    int position_monitor_interval_ms{500};
    bool enable_property_monitoring{true};
    int property_monitor_interval_ms{1000};

    // Safety configuration
    bool enable_position_limits{false};
    double min_position{0.0};
    double max_position{360.0};
    bool enable_emergency_stop{true};

    // Movement configuration
    double default_speed{10.0}; // degrees per second
    double default_acceleration{5.0}; // degrees per second squared
    double position_tolerance{0.1}; // degrees
    int movement_timeout_ms{30000}; // 30 seconds

    // Backlash compensation
    bool enable_backlash_compensation{false};
    double backlash_amount{0.0}; // degrees

    // Preset configuration
    bool enable_presets{true};
    int max_presets{100};
    std::string preset_directory;
    bool auto_save_presets{true};
};

/**
 * @brief Status information for the rotator controller
 */
struct RotatorStatus {
    bool connected{false};
    bool moving{false};
    double current_position{0.0};
    double target_position{0.0};
    double mechanical_position{0.0};
    components::MovementState movement_state{components::MovementState::IDLE};
    bool emergency_stop_active{false};
    std::optional<double> temperature;
    std::string last_error;
    std::chrono::steady_clock::time_point last_update;
};

/**
 * @brief Modular ASCOM Rotator Controller
 *
 * This controller provides a comprehensive interface to ASCOM rotator functionality
 * by coordinating specialized components for hardware communication, position control,
 * property management, and preset handling.
 */
class ASCOMRotatorController : public AtomRotator {
public:
    explicit ASCOMRotatorController(std::string name, const RotatorConfig& config = {});
    ~ASCOMRotatorController() override;

    // Basic device operations (AtomDriver interface)
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout = 5000, int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Rotator state (AtomRotator interface)
    auto isMoving() const -> bool override;

    // Position control (AtomRotator interface)
    auto getPosition() -> std::optional<double> override;
    auto setPosition(double angle) -> bool override;
    auto moveToAngle(double angle) -> bool override;
    auto rotateByAngle(double angle) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(double angle) -> bool override;

    // Direction control (AtomRotator interface)
    auto getDirection() -> std::optional<RotatorDirection> override;
    auto setDirection(RotatorDirection direction) -> bool override;
    auto isReversed() -> bool override;
    auto setReversed(bool reversed) -> bool override;

    // Speed control (AtomRotator interface)
    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> double override;
    auto getMinSpeed() -> double override;

    // Limits (AtomRotator interface)
    auto getMinPosition() -> double override;
    auto getMaxPosition() -> double override;
    auto setLimits(double min, double max) -> bool override;

    // Backlash compensation (AtomRotator interface)
    auto getBacklash() -> double override;
    auto setBacklash(double backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Temperature (AtomRotator interface)
    auto getTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Presets (AtomRotator interface)
    auto savePreset(int slot, double angle) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<double> override;
    auto deletePreset(int slot) -> bool override;

    // Statistics (AtomRotator interface)
    auto getTotalRotation() -> double override;
    auto resetTotalRotation() -> bool override;
    auto getLastMoveAngle() -> double override;
    auto getLastMoveDuration() -> int override;

    // Enhanced position control (beyond base interface)
    auto moveToAngleAsync(double angle) -> std::shared_ptr<std::future<bool>>;
    auto getMechanicalPosition() -> std::optional<double>;
    auto getPositionInfo() -> components::PositionInfo;
    auto performHoming() -> bool;
    auto calibratePosition(double known_angle) -> bool;

    // Enhanced movement control
    auto setMovementParameters(const components::MovementParams& params) -> bool;
    auto getMovementParameters() -> components::MovementParams;
    auto getOptimalPath(double from_angle, double to_angle) -> std::pair<double, bool>;
    auto snapToNearestPreset(double tolerance = 5.0) -> std::optional<int>;

    // Safety and emergency features
    auto setEmergencyStop(bool enabled) -> void;
    auto isEmergencyStopActive() -> bool;
    auto validatePosition(double position) -> bool;
    auto enforcePositionLimits(double& position) -> bool;

    // Enhanced preset management
    auto saveCurrentPosition(int slot, const std::string& name = "") -> bool;
    auto moveToPreset(int slot) -> bool;
    auto copyPreset(int from_slot, int to_slot) -> bool;
    auto findPresetByName(const std::string& name) -> std::optional<int>;
    auto getFavoritePresets() -> std::vector<int>;
    auto exportPresets(const std::string& filename) -> bool;
    auto importPresets(const std::string& filename) -> bool;

    // Configuration and settings
    auto updateConfiguration(const RotatorConfig& config) -> bool;
    auto getConfiguration() const -> RotatorConfig;
    auto saveConfiguration(const std::string& filename) -> bool;
    auto loadConfiguration(const std::string& filename) -> bool;

    // Status and monitoring
    auto getStatus() -> RotatorStatus;
    auto startMonitoring() -> bool;
    auto stopMonitoring() -> bool;
    auto getDeviceCapabilities() -> components::DeviceCapabilities;

    // Property access
    auto getProperty(const std::string& name) -> std::optional<components::PropertyValue>;
    auto setProperty(const std::string& name, const components::PropertyValue& value) -> bool;
    auto refreshProperties() -> bool;

    // Event callbacks
    auto setPositionCallback(std::function<void(double, double)> callback) -> void;
    auto setMovementStateCallback(std::function<void(components::MovementState)> callback) -> void;
    auto setConnectionCallback(std::function<void(bool)> callback) -> void;
    auto setErrorCallback(std::function<void(const std::string&)> callback) -> void;

    // Component access (for advanced use cases)
    auto getHardwareInterface() -> std::shared_ptr<components::HardwareInterface>;
    auto getPositionManager() -> std::shared_ptr<components::PositionManager>;
    auto getPropertyManager() -> std::shared_ptr<components::PropertyManager>;
    auto getPresetManager() -> std::shared_ptr<components::PresetManager>;

    // Diagnostics and debugging
    auto performDiagnostics() -> std::unordered_map<std::string, std::string>;
    auto getComponentStatuses() -> std::unordered_map<std::string, bool>;
    auto enableDebugLogging(bool enable) -> void;
    auto getDebugInfo() -> std::string;

private:
    // Configuration
    RotatorConfig config_;

    // Component instances
    std::shared_ptr<components::HardwareInterface> hardware_interface_;
    std::shared_ptr<components::PositionManager> position_manager_;
    std::shared_ptr<components::PropertyManager> property_manager_;
    std::shared_ptr<components::PresetManager> preset_manager_;

    // Connection state
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_initialized_{false};

    // Monitoring
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> monitoring_active_{false};
    int monitor_interval_ms_{500};

    // Event callbacks
    std::function<void(double, double)> position_callback_;
    std::function<void(components::MovementState)> movement_state_callback_;
    std::function<void(bool)> connection_callback_;
    std::function<void(const std::string&)> error_callback_;
    mutable std::mutex callback_mutex_;

    // Error handling
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // Helper methods
    auto initializeComponents() -> bool;
    auto destroyComponents() -> bool;
    auto setupComponentCallbacks() -> void;
    auto removeComponentCallbacks() -> void;
    auto validateConfiguration(const RotatorConfig& config) -> bool;
    auto setLastError(const std::string& error) -> void;
    auto notifyPositionChange(double current, double target) -> void;
    auto notifyMovementStateChange(components::MovementState state) -> void;
    auto notifyConnectionChange(bool connected) -> void;
    auto notifyError(const std::string& error) -> void;
    auto monitoringLoop() -> void;
    auto updateStatus() -> void;
    auto checkComponentHealth() -> bool;
};

} // namespace lithium::device::ascom::rotator
