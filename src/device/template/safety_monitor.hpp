/*
 * safety_monitor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomSafetyMonitor device following INDI architecture

*************************************************/

#pragma once

#include "device.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

enum class SafetyState {
    SAFE,
    UNSAFE,
    WARNING,
    ERROR,
    UNKNOWN
};

enum class SafetyCondition {
    WEATHER,
    POWER,
    TEMPERATURE,
    HUMIDITY,
    WIND,
    RAIN,
    CLOUD_COVER,
    ROOF_OPEN,
    EMERGENCY_STOP,
    USER_DEFINED
};

// Safety parameter
struct SafetyParameter {
    std::string name;
    double value{0.0};
    double min_safe{0.0};
    double max_safe{0.0};
    double warning_threshold{0.0};
    bool enabled{true};
    SafetyCondition condition{SafetyCondition::USER_DEFINED};
    std::string unit;
    std::chrono::system_clock::time_point last_update;
} ATOM_ALIGNAS(64);

// Safety event
struct SafetyEvent {
    SafetyState state;
    SafetyCondition condition;
    std::string description;
    double value{0.0};
    std::chrono::system_clock::time_point timestamp;
    bool acknowledged{false};
} ATOM_ALIGNAS(64);

// Safety configuration
struct SafetyConfiguration {
    // Monitoring intervals
    std::chrono::seconds check_interval{10};
    std::chrono::seconds warning_delay{30};
    std::chrono::seconds unsafe_delay{60};

    // Auto-recovery settings
    bool auto_recovery_enabled{true};
    std::chrono::seconds recovery_delay{300};
    int max_recovery_attempts{3};

    // Notification settings
    bool email_notifications{false};
    bool sound_alerts{true};
    bool log_events{true};

    // Emergency settings
    bool emergency_stop_enabled{true};
    bool auto_park_mount{true};
    bool auto_close_dome{true};
    bool auto_warm_camera{false};
} ATOM_ALIGNAS(64);

class AtomSafetyMonitor : public AtomDriver {
public:
    explicit AtomSafetyMonitor(std::string name) : AtomDriver(std::move(name)) {
        setType("SafetyMonitor");
    }

    ~AtomSafetyMonitor() override = default;

    // Configuration
    const SafetyConfiguration& getSafetyConfiguration() const { return safety_configuration_; }
    void setSafetyConfiguration(const SafetyConfiguration& config) { safety_configuration_ = config; }

    // State management
    SafetyState getSafetyState() const { return safety_state_; }
    virtual bool isSafe() const = 0;
    virtual bool isUnsafe() const = 0;
    virtual bool isWarning() const = 0;

    // Parameter management
    virtual auto addParameter(const SafetyParameter& param) -> bool = 0;
    virtual auto removeParameter(const std::string& name) -> bool = 0;
    virtual auto updateParameter(const std::string& name, double value) -> bool = 0;
    virtual auto getParameter(const std::string& name) -> std::optional<SafetyParameter> = 0;
    virtual auto getAllParameters() -> std::vector<SafetyParameter> = 0;
    virtual auto enableParameter(const std::string& name, bool enabled) -> bool = 0;

    // Safety checks
    virtual auto checkSafety() -> SafetyState = 0;
    virtual auto checkParameter(const SafetyParameter& param) -> SafetyState = 0;
    virtual auto getUnsafeConditions() -> std::vector<SafetyCondition> = 0;
    virtual auto getWarningConditions() -> std::vector<SafetyCondition> = 0;
    virtual auto getSafetyReport() -> std::string = 0;

    // Emergency controls
    virtual auto emergencyStop() -> bool = 0;
    virtual auto acknowledgeAlert(const std::string& event_id) -> bool = 0;
    virtual auto resetSafetySystem() -> bool = 0;
    virtual auto testSafetySystem() -> bool = 0;

    // Event management
    virtual auto getRecentEvents(std::chrono::hours duration = std::chrono::hours(24)) -> std::vector<SafetyEvent> = 0;
    virtual auto getUnacknowledgedEvents() -> std::vector<SafetyEvent> = 0;
    virtual auto clearEventHistory() -> bool = 0;
    virtual auto exportEventLog(const std::string& filename) -> bool = 0;

    // Device monitoring
    virtual auto addMonitoredDevice(const std::string& device_name) -> bool = 0;
    virtual auto removeMonitoredDevice(const std::string& device_name) -> bool = 0;
    virtual auto getMonitoredDevices() -> std::vector<std::string> = 0;
    virtual auto checkDeviceStatus(const std::string& device_name) -> bool = 0;

    // Weather integration
    virtual auto setWeatherStation(const std::string& weather_name) -> bool = 0;
    virtual auto getWeatherStation() -> std::string = 0;
    virtual auto checkWeatherConditions() -> SafetyState = 0;

    // Power monitoring
    virtual auto checkPowerStatus() -> SafetyState = 0;
    virtual auto getPowerVoltage() -> std::optional<double> = 0;
    virtual auto getPowerCurrent() -> std::optional<double> = 0;
    virtual auto isPowerFailure() -> bool = 0;

    // Recovery procedures
    virtual auto startRecoveryProcedure() -> bool = 0;
    virtual auto stopRecoveryProcedure() -> bool = 0;
    virtual auto isRecovering() -> bool = 0;
    virtual auto getRecoveryStatus() -> std::string = 0;

    // Automation responses
    virtual auto enableAutoParkMount(bool enable) -> bool = 0;
    virtual auto enableAutoCloseDome(bool enable) -> bool = 0;
    virtual auto enableAutoWarmCamera(bool enable) -> bool = 0;
    virtual auto executeEmergencyShutdown() -> bool = 0;

    // Configuration management
    virtual auto loadConfiguration(const std::string& filename) -> bool = 0;
    virtual auto saveConfiguration(const std::string& filename) -> bool = 0;
    virtual auto resetToDefaults() -> bool = 0;

    // Monitoring control
    virtual auto startMonitoring() -> bool = 0;
    virtual auto stopMonitoring() -> bool = 0;
    virtual auto isMonitoring() -> bool = 0;
    virtual auto setMonitoringInterval(std::chrono::seconds interval) -> bool = 0;

    // Statistics
    virtual auto getUptime() -> std::chrono::seconds = 0;
    virtual auto getUnsafeTime() -> std::chrono::seconds = 0;
    virtual auto getSafetyRatio() -> double = 0;
    virtual auto getTotalEvents() -> uint64_t = 0;
    virtual auto getAverageRecoveryTime() -> std::chrono::seconds = 0;

    // Event callbacks
    using SafetyCallback = std::function<void(SafetyState state, const std::string& message)>;
    using EventCallback = std::function<void(const SafetyEvent& event)>;
    using ParameterCallback = std::function<void(const SafetyParameter& param)>;
    using EmergencyCallback = std::function<void(const std::string& reason)>;

    virtual void setSafetyCallback(SafetyCallback callback) { safety_callback_ = std::move(callback); }
    virtual void setEventCallback(EventCallback callback) { event_callback_ = std::move(callback); }
    virtual void setParameterCallback(ParameterCallback callback) { parameter_callback_ = std::move(callback); }
    virtual void setEmergencyCallback(EmergencyCallback callback) { emergency_callback_ = std::move(callback); }

    // Utility methods
    virtual auto safetyStateToString(SafetyState state) -> std::string;
    virtual auto safetyConditionToString(SafetyCondition condition) -> std::string;
    virtual auto formatSafetyReport() -> std::string;

protected:
    SafetyState safety_state_{SafetyState::UNKNOWN};
    SafetyConfiguration safety_configuration_;

    // Parameters and events
    std::vector<SafetyParameter> safety_parameters_;
    std::vector<SafetyEvent> event_history_;
    std::vector<std::string> monitored_devices_;

    // State tracking
    bool monitoring_active_{false};
    bool recovery_in_progress_{false};
    std::chrono::system_clock::time_point monitoring_start_time_;
    std::chrono::system_clock::time_point last_unsafe_time_;
    std::chrono::seconds total_unsafe_time_{0};

    // Statistics
    uint64_t total_events_{0};
    std::chrono::seconds total_recovery_time_{0};
    int recovery_attempts_{0};

    // Connected devices
    std::string weather_station_name_;

    // Callbacks
    SafetyCallback safety_callback_;
    EventCallback event_callback_;
    ParameterCallback parameter_callback_;
    EmergencyCallback emergency_callback_;

    // Utility methods
    virtual void updateSafetyState(SafetyState state) { safety_state_ = state; }
    virtual void addEvent(const SafetyEvent& event);
    virtual void cleanupEventHistory();
    virtual void notifySafetyChange(SafetyState state, const std::string& message = "");
    virtual void notifyEvent(const SafetyEvent& event);
    virtual void notifyParameterChange(const SafetyParameter& param);
    virtual void notifyEmergency(const std::string& reason);
};

// Inline utility implementations
inline auto AtomSafetyMonitor::safetyStateToString(SafetyState state) -> std::string {
    switch (state) {
        case SafetyState::SAFE: return "SAFE";
        case SafetyState::UNSAFE: return "UNSAFE";
        case SafetyState::WARNING: return "WARNING";
        case SafetyState::ERROR: return "ERROR";
        case SafetyState::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

inline auto AtomSafetyMonitor::safetyConditionToString(SafetyCondition condition) -> std::string {
    switch (condition) {
        case SafetyCondition::WEATHER: return "WEATHER";
        case SafetyCondition::POWER: return "POWER";
        case SafetyCondition::TEMPERATURE: return "TEMPERATURE";
        case SafetyCondition::HUMIDITY: return "HUMIDITY";
        case SafetyCondition::WIND: return "WIND";
        case SafetyCondition::RAIN: return "RAIN";
        case SafetyCondition::CLOUD_COVER: return "CLOUD_COVER";
        case SafetyCondition::ROOF_OPEN: return "ROOF_OPEN";
        case SafetyCondition::EMERGENCY_STOP: return "EMERGENCY_STOP";
        case SafetyCondition::USER_DEFINED: return "USER_DEFINED";
        default: return "UNKNOWN";
    }
}
