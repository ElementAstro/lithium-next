/*
 * state_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch State Manager Component

This component manages state persistence, configuration saving/loading,
and device state restoration for ASCOM switch devices.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>

#include "device/template/switch.hpp"

namespace lithium::device::ascom::sw::components {

// Forward declarations
class SwitchManager;
class GroupManager;
class PowerManager;

/**
 * @brief Saved state data for a switch
 */
struct SavedSwitchState {
    uint32_t index;
    std::string name;
    SwitchState state;
    bool enabled{true};
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Configuration data for persistence
 */
struct DeviceConfiguration {
    std::string device_name;
    std::string config_version{"1.0"};
    std::vector<SavedSwitchState> switch_states;
    std::unordered_map<std::string, std::string> settings;
    std::chrono::steady_clock::time_point saved_at;
};

/**
 * @brief State Manager Component
 * 
 * This component handles state persistence, configuration management,
 * and device state restoration functionality.
 */
class StateManager {
public:
    explicit StateManager(std::shared_ptr<SwitchManager> switch_manager,
                         std::shared_ptr<GroupManager> group_manager,
                         std::shared_ptr<PowerManager> power_manager);
    ~StateManager() = default;

    // Non-copyable and non-movable
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;
    StateManager(StateManager&&) = delete;
    StateManager& operator=(StateManager&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    auto initialize() -> bool;
    auto destroy() -> bool;
    auto reset() -> bool;

    // =========================================================================
    // State Persistence
    // =========================================================================

    auto saveState() -> bool;
    auto loadState() -> bool;
    auto resetToDefaults() -> bool;
    auto saveStateToFile(const std::string& filename) -> bool;
    auto loadStateFromFile(const std::string& filename) -> bool;

    // =========================================================================
    // Configuration Management
    // =========================================================================

    auto saveConfiguration() -> bool;
    auto loadConfiguration() -> bool;
    auto exportConfiguration(const std::string& filename) -> bool;
    auto importConfiguration(const std::string& filename) -> bool;
    auto validateConfiguration(const std::string& filename) -> bool;

    // =========================================================================
    // Auto-save and Backup
    // =========================================================================

    auto enableAutoSave(bool enable) -> bool;
    auto isAutoSaveEnabled() -> bool;
    auto setAutoSaveInterval(uint32_t intervalSeconds) -> bool;
    auto getAutoSaveInterval() -> uint32_t;
    auto createBackup() -> bool;
    auto restoreFromBackup(const std::string& backupName) -> bool;
    auto listBackups() -> std::vector<std::string>;

    // =========================================================================
    // Safety Features
    // =========================================================================

    auto enableSafetyMode(bool enable) -> bool;
    auto isSafetyModeEnabled() -> bool;
    auto setEmergencyState() -> bool;
    auto clearEmergencyState() -> bool;
    auto isEmergencyStateActive() -> bool;
    auto saveEmergencyState() -> bool;
    auto restoreEmergencyState() -> bool;

    // =========================================================================
    // State Information
    // =========================================================================

    auto getLastSaveTime() -> std::optional<std::chrono::steady_clock::time_point>;
    auto getLastLoadTime() -> std::optional<std::chrono::steady_clock::time_point>;
    auto getStateFileSize() -> std::optional<size_t>;
    auto getConfigurationVersion() -> std::string;
    auto isStateModified() -> bool;

    // =========================================================================
    // Custom Settings
    // =========================================================================

    auto setSetting(const std::string& key, const std::string& value) -> bool;
    auto getSetting(const std::string& key) -> std::optional<std::string>;
    auto removeSetting(const std::string& key) -> bool;
    auto getAllSettings() -> std::unordered_map<std::string, std::string>;
    auto clearAllSettings() -> bool;

    // =========================================================================
    // Callbacks
    // =========================================================================

    using StateChangeCallback = std::function<void(bool saved, const std::string& filename)>;
    using BackupCallback = std::function<void(const std::string& backupName, bool success)>;
    using EmergencyCallback = std::function<void(bool active)>;

    void setStateChangeCallback(StateChangeCallback callback);
    void setBackupCallback(BackupCallback callback);
    void setEmergencyCallback(EmergencyCallback callback);

    // =========================================================================
    // Error Handling
    // =========================================================================

    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Component references
    std::shared_ptr<SwitchManager> switch_manager_;
    std::shared_ptr<GroupManager> group_manager_;
    std::shared_ptr<PowerManager> power_manager_;

    // Configuration
    DeviceConfiguration current_config_;
    mutable std::mutex config_mutex_;

    // File management
    std::string config_directory_{"./config"};
    std::string config_filename_{"switch_config.json"};
    std::string backup_directory_{"./config/backups"};
    std::string emergency_filename_{"emergency_state.json"};

    // Auto-save
    std::atomic<bool> auto_save_enabled_{false};
    std::atomic<uint32_t> auto_save_interval_{300}; // 5 minutes
    std::unique_ptr<std::thread> auto_save_thread_;
    std::atomic<bool> auto_save_running_{false};
    std::condition_variable auto_save_cv_;
    std::mutex auto_save_mutex_;

    // State tracking
    std::atomic<bool> state_modified_{false};
    std::atomic<bool> safety_mode_enabled_{false};
    std::atomic<bool> emergency_state_active_{false};
    std::optional<std::chrono::steady_clock::time_point> last_save_time_;
    std::optional<std::chrono::steady_clock::time_point> last_load_time_;

    // Settings
    std::unordered_map<std::string, std::string> custom_settings_;
    mutable std::mutex settings_mutex_;

    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;

    // Callbacks
    StateChangeCallback state_change_callback_;
    BackupCallback backup_callback_;
    EmergencyCallback emergency_callback_;
    std::mutex callback_mutex_;

    // =========================================================================
    // Internal Methods
    // =========================================================================

    auto startAutoSaveThread() -> bool;
    auto stopAutoSaveThread() -> void;
    auto autoSaveLoop() -> void;
    
    auto collectCurrentState() -> DeviceConfiguration;
    auto applyConfiguration(const DeviceConfiguration& config) -> bool;
    auto validateConfigurationData(const DeviceConfiguration& config) -> bool;
    
    auto ensureDirectoryExists(const std::string& directory) -> bool;
    auto generateBackupName() -> std::string;
    auto parseConfigurationFile(const std::string& filename) -> std::optional<DeviceConfiguration>;
    auto writeConfigurationFile(const std::string& filename, const DeviceConfiguration& config) -> bool;
    
    auto setLastError(const std::string& error) const -> void;
    auto logOperation(const std::string& operation, bool success) -> void;
    
    // JSON serialization helpers
    auto configToJson(const DeviceConfiguration& config) -> std::string;
    auto jsonToConfig(const std::string& json) -> std::optional<DeviceConfiguration>;
    
    // Notification helpers
    auto notifyStateChange(bool saved, const std::string& filename) -> void;
    auto notifyBackup(const std::string& backupName, bool success) -> void;
    auto notifyEmergency(bool active) -> void;
    
    // Utility methods
    auto getFullPath(const std::string& filename) -> std::string;
    auto getBackupPath(const std::string& backupName) -> std::string;
    auto cleanupOldBackups(uint32_t maxBackups = 10) -> void;
};

} // namespace lithium::device::ascom::sw::components
