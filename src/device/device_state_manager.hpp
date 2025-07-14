/*
 * device_state_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Device State Management System with optimized state tracking and transitions

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>

namespace lithium {

// Forward declaration
class AtomDriver;

// Device states with extended information
enum class DeviceState {
    UNKNOWN = 0,
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    INITIALIZING,
    IDLE,
    BUSY,
    ERROR,
    MAINTENANCE,
    SUSPENDED,
    SHUTDOWN
};

// State transition types
enum class TransitionType {
    AUTOMATIC,
    MANUAL,
    FORCED,
    TIMEOUT,
    ERROR_RECOVERY
};

// State change reasons
enum class StateChangeReason {
    USER_REQUEST,
    DEVICE_EVENT,
    TIMEOUT,
    ERROR,
    SYSTEM_SHUTDOWN,
    MAINTENANCE,
    AUTO_RECOVERY,
    EXTERNAL_TRIGGER
};

// Device state information
struct DeviceStateInfo {
    DeviceState current_state{DeviceState::UNKNOWN};
    DeviceState previous_state{DeviceState::UNKNOWN};
    std::chrono::system_clock::time_point state_changed_at;
    std::chrono::milliseconds time_in_current_state{0};
    StateChangeReason reason{StateChangeReason::USER_REQUEST};
    std::string description;
    std::string error_message;

    // State statistics
    size_t state_change_count{0};
    std::chrono::milliseconds total_uptime{0};
    std::chrono::milliseconds total_error_time{0};
    double availability_percentage{100.0};

    // State metadata
    std::unordered_map<std::string, std::string> metadata;
    bool is_stable{true};
    bool requires_attention{false};
    int stability_score{100}; // 0-100
};

// State transition rule
struct StateTransitionRule {
    DeviceState from_state;
    DeviceState to_state;
    TransitionType type;
    std::vector<StateChangeReason> allowed_reasons;
    std::function<bool(const std::string&)> condition_check;
    std::function<void(const std::string&)> pre_transition_action;
    std::function<void(const std::string&)> post_transition_action;
    std::chrono::milliseconds min_time_in_state{0};
    int priority{0};
    bool is_reversible{true};
};

// State validation result
struct StateValidationResult {
    bool is_valid{true};
    std::string error_message;
    std::vector<std::string> warnings;
    std::vector<std::string> suggested_actions;
    DeviceState suggested_state{DeviceState::UNKNOWN};
};

// State monitoring configuration
struct StateMonitoringConfig {
    std::chrono::seconds monitoring_interval{10};
    std::chrono::seconds state_timeout{300};
    std::chrono::seconds error_recovery_timeout{60};
    bool enable_auto_recovery{true};
    bool enable_state_logging{true};
    bool enable_state_persistence{true};
    size_t max_state_history{1000};
    double stability_threshold{0.8};
};

// State history entry
struct StateHistoryEntry {
    DeviceState from_state;
    DeviceState to_state;
    StateChangeReason reason;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::milliseconds duration_in_previous_state{0};
    std::string description;
    std::string triggered_by;
    bool was_successful{true};
};

class DeviceStateManager {
public:
    DeviceStateManager();
    explicit DeviceStateManager(const StateMonitoringConfig& config);
    ~DeviceStateManager();

    // Configuration
    void setConfiguration(const StateMonitoringConfig& config);
    StateMonitoringConfig getConfiguration() const;

    // Device registration
    void registerDevice(const std::string& device_name, std::shared_ptr<AtomDriver> device);
    void unregisterDevice(const std::string& device_name);
    bool isDeviceRegistered(const std::string& device_name) const;
    std::vector<std::string> getRegisteredDevices() const;

    // State management
    bool setState(const std::string& device_name, DeviceState new_state,
                 StateChangeReason reason = StateChangeReason::USER_REQUEST,
                 const std::string& description = "");

    DeviceState getState(const std::string& device_name) const;
    DeviceStateInfo getStateInfo(const std::string& device_name) const;

    bool canTransitionTo(const std::string& device_name, DeviceState target_state) const;
    std::vector<DeviceState> getValidTransitions(const std::string& device_name) const;

    // State validation
    StateValidationResult validateState(const std::string& device_name) const;
    StateValidationResult validateTransition(const std::string& device_name,
                                           DeviceState target_state) const;

    // State history
    std::vector<StateHistoryEntry> getStateHistory(const std::string& device_name,
                                                  size_t max_entries = 100) const;
    void clearStateHistory(const std::string& device_name);

    // State transition rules
    void addTransitionRule(const StateTransitionRule& rule);
    void removeTransitionRule(DeviceState from_state, DeviceState to_state);
    std::vector<StateTransitionRule> getTransitionRules() const;
    void resetTransitionRules();

    // State monitoring
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;

    void startDeviceMonitoring(const std::string& device_name);
    void stopDeviceMonitoring(const std::string& device_name);
    bool isDeviceMonitoring(const std::string& device_name) const;

    // Auto recovery
    void enableAutoRecovery(bool enable);
    bool isAutoRecoveryEnabled() const;
    void triggerRecovery(const std::string& device_name);
    bool attemptStateRecovery(const std::string& device_name);

    // State callbacks
    using StateChangeCallback = std::function<void(const std::string&, DeviceState, DeviceState, StateChangeReason)>;
    using StateErrorCallback = std::function<void(const std::string&, const std::string&)>;
    using StateValidationCallback = std::function<void(const std::string&, const StateValidationResult&)>;

    void setStateChangeCallback(StateChangeCallback callback);
    void setStateErrorCallback(StateErrorCallback callback);
    void setStateValidationCallback(StateValidationCallback callback);

    // Batch operations
    std::unordered_map<std::string, bool> setStateForMultipleDevices(
        const std::vector<std::string>& device_names,
        DeviceState new_state,
        StateChangeReason reason = StateChangeReason::USER_REQUEST);

    std::unordered_map<std::string, DeviceState> getStateForMultipleDevices(
        const std::vector<std::string>& device_names) const;

    // State queries
    std::vector<std::string> getDevicesInState(DeviceState state) const;
    std::vector<std::string> getErrorDevices() const;
    std::vector<std::string> getUnstableDevices() const;
    size_t getDeviceCountInState(DeviceState state) const;

    // State statistics
    struct StateStatistics {
        size_t total_devices{0};
        size_t stable_devices{0};
        size_t error_devices{0};
        size_t busy_devices{0};
        double average_uptime{0.0};
        double average_stability_score{0.0};
        size_t total_state_changes{0};
        std::chrono::milliseconds average_state_duration{0};
        std::unordered_map<DeviceState, size_t> device_count_by_state;
        std::unordered_map<StateChangeReason, size_t> transition_count_by_reason;
    };

    StateStatistics getStatistics() const;
    StateStatistics getDeviceStatistics(const std::string& device_name) const;
    void resetStatistics();

    // State persistence
    bool saveState(const std::string& file_path);
    bool loadState(const std::string& file_path);
    void enableStatePersistence(bool enable);
    bool isStatePersistenceEnabled() const;

    // State export/import
    std::string exportStateConfiguration() const;
    bool importStateConfiguration(const std::string& config_json);

    // Advanced features

    // State prediction
    DeviceState predictNextState(const std::string& device_name) const;
    std::chrono::milliseconds predictTimeToStateChange(const std::string& device_name) const;

    // State correlation
    std::vector<std::string> findCorrelatedDevices(const std::string& device_name) const;
    void addStateCorrelation(const std::string& device1, const std::string& device2, double correlation);

    // State templates
    void createStateTemplate(const std::string& template_name,
                           const std::vector<StateTransitionRule>& rules);
    void applyStateTemplate(const std::string& device_name, const std::string& template_name);
    std::vector<std::string> getAvailableTemplates() const;

    // State workflows
    struct StateWorkflow {
        std::string name;
        std::vector<std::pair<DeviceState, std::chrono::milliseconds>> steps;
        bool allow_interruption{true};
        std::function<void(const std::string&, bool)> completion_callback;
    };

    void executeStateWorkflow(const std::string& device_name, const StateWorkflow& workflow);
    void cancelStateWorkflow(const std::string& device_name);
    bool isWorkflowRunning(const std::string& device_name) const;

    // Debugging and diagnostics
    std::string getStateManagerStatus() const;
    std::string getDeviceStateInfo(const std::string& device_name) const;
    void dumpStateManagerData(const std::string& output_path) const;

    // Maintenance
    void runMaintenance();
    void cleanupOldHistory(std::chrono::seconds age_threshold);
    void validateAllDeviceStates();
    void repairInconsistentStates();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;

    // Internal methods
    void monitoringLoop();
    void updateDeviceState(const std::string& device_name);
    void checkStateTimeouts();
    void performStateValidation(const std::string& device_name);

    // State transition logic
    bool executeStateTransition(const std::string& device_name,
                               DeviceState from_state,
                               DeviceState to_state,
                               StateChangeReason reason);

    void recordStateChange(const std::string& device_name,
                          DeviceState from_state,
                          DeviceState to_state,
                          StateChangeReason reason,
                          const std::string& description);

    // Recovery logic
    void attemptErrorRecovery(const std::string& device_name);
    void handleStateTimeout(const std::string& device_name);

    // Validation logic
    bool isTransitionAllowed(DeviceState from_state, DeviceState to_state, StateChangeReason reason) const;
    bool checkTransitionConditions(const std::string& device_name, DeviceState target_state) const;

    // State analysis
    void updateStabilityScore(const std::string& device_name);
    void analyzeStatePatterns(const std::string& device_name);
    void detectAnomalies(const std::string& device_name);
};

// Utility functions
namespace state_utils {
    std::string stateToString(DeviceState state);
    DeviceState stringToState(const std::string& state_str);

    std::string reasonToString(StateChangeReason reason);
    StateChangeReason stringToReason(const std::string& reason_str);

    bool isErrorState(DeviceState state);
    bool isActiveState(DeviceState state);
    bool isStableState(DeviceState state);

    double calculateUptime(const std::vector<StateHistoryEntry>& history);
    double calculateStabilityScore(const std::vector<StateHistoryEntry>& history);

    std::string formatStateInfo(const DeviceStateInfo& info);
    std::string formatStateHistory(const std::vector<StateHistoryEntry>& history);

    // State transition utilities
    std::vector<DeviceState> getDefaultTransitionPath(DeviceState from, DeviceState to);
    bool isValidTransitionPath(const std::vector<DeviceState>& path);

    // State analysis utilities
    std::vector<DeviceState> findMostCommonStates(const std::vector<StateHistoryEntry>& history, size_t count = 5);
    std::chrono::milliseconds getAverageTimeInState(const std::vector<StateHistoryEntry>& history, DeviceState state);

    // State pattern detection
    bool detectCyclicPattern(const std::vector<StateHistoryEntry>& history);
    bool detectRapidChanges(const std::vector<StateHistoryEntry>& history, std::chrono::seconds threshold);
    std::vector<std::string> identifyProblematicPatterns(const std::vector<StateHistoryEntry>& history);
}

} // namespace lithium
