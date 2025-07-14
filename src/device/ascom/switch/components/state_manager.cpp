/*
 * state_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch State Manager Component Implementation

This component manages state persistence, configuration saving/loading,
and device state restoration for ASCOM switch devices.

*************************************************/

#include "state_manager.hpp"
#include "switch_manager.hpp"
#include "group_manager.hpp"
#include "power_manager.hpp"

#include <spdlog/spdlog.h>
#include <atom/type/json.hpp>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include "device/template/switch.hpp"

namespace lithium::device::ascom::sw::components {

StateManager::StateManager(std::shared_ptr<SwitchManager> switch_manager,
                          std::shared_ptr<GroupManager> group_manager,
                          std::shared_ptr<PowerManager> power_manager)
    : switch_manager_(std::move(switch_manager)),
      group_manager_(std::move(group_manager)),
      power_manager_(std::move(power_manager)) {
    spdlog::debug("StateManager component created");
}

auto StateManager::initialize() -> bool {
    spdlog::info("Initializing State Manager");

    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    // Ensure directories exist
    if (!ensureDirectoryExists(config_directory_)) {
        setLastError("Failed to create config directory");
        return false;
    }

    if (!ensureDirectoryExists(backup_directory_)) {
        spdlog::warn("Failed to create backup directory, backup functionality will be limited");
    }

    // Load existing configuration if available
    loadConfiguration();

    return true;
}

auto StateManager::destroy() -> bool {
    spdlog::info("Destroying State Manager");

    // Stop auto-save thread
    stopAutoSaveThread();

    // Save current state before shutdown if auto-save is enabled
    if (auto_save_enabled_.load() && state_modified_.load()) {
        saveConfiguration();
    }

    std::lock_guard<std::mutex> config_lock(config_mutex_);
    std::lock_guard<std::mutex> settings_lock(settings_mutex_);

    current_config_ = DeviceConfiguration{};
    custom_settings_.clear();

    return true;
}

auto StateManager::reset() -> bool {
    if (!destroy()) {
        return false;
    }
    return initialize();
}

auto StateManager::saveState() -> bool {
    return saveConfiguration();
}

auto StateManager::loadState() -> bool {
    return loadConfiguration();
}

auto StateManager::resetToDefaults() -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    spdlog::info("Resetting to default state");

    // Turn off all switches
    auto switchCount = switch_manager_->getSwitchCount();
    for (uint32_t i = 0; i < switchCount; ++i) {
        switch_manager_->setSwitchState(i, SwitchState::OFF);
    }

    // Clear settings
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        custom_settings_.clear();
    }

    // Reset configuration
    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        current_config_ = DeviceConfiguration{};
        current_config_.config_version = "1.0";
        current_config_.saved_at = std::chrono::steady_clock::now();
    }

    state_modified_ = true;
    return saveConfiguration();
}

auto StateManager::saveStateToFile(const std::string& filename) -> bool {
    auto config = collectCurrentState();
    bool success = writeConfigurationFile(getFullPath(filename), config);

    if (success) {
        last_save_time_ = std::chrono::steady_clock::now();
        state_modified_ = false;
        notifyStateChange(true, filename);
    }

    logOperation("Save state to " + filename, success);
    return success;
}

auto StateManager::loadStateFromFile(const std::string& filename) -> bool {
    auto config = parseConfigurationFile(getFullPath(filename));
    if (!config) {
        return false;
    }

    bool success = applyConfiguration(*config);
    if (success) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        current_config_ = *config;
        last_load_time_ = std::chrono::steady_clock::now();
        state_modified_ = false;
        notifyStateChange(false, filename);
    }

    logOperation("Load state from " + filename, success);
    return success;
}

auto StateManager::saveConfiguration() -> bool {
    return saveStateToFile(config_filename_);
}

auto StateManager::loadConfiguration() -> bool {
    std::string configPath = getFullPath(config_filename_);
    if (!std::filesystem::exists(configPath)) {
        spdlog::debug("Configuration file not found, using defaults");
        return resetToDefaults();
    }

    return loadStateFromFile(config_filename_);
}

auto StateManager::exportConfiguration(const std::string& filename) -> bool {
    auto config = collectCurrentState();
    bool success = writeConfigurationFile(filename, config);

    logOperation("Export configuration to " + filename, success);
    return success;
}

auto StateManager::importConfiguration(const std::string& filename) -> bool {
    if (!validateConfiguration(filename)) {
        return false;
    }

    auto config = parseConfigurationFile(filename);
    if (!config) {
        return false;
    }

    bool success = applyConfiguration(*config);
    if (success) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        current_config_ = *config;
        state_modified_ = true;
        saveConfiguration();
    }

    logOperation("Import configuration from " + filename, success);
    return success;
}

auto StateManager::validateConfiguration(const std::string& filename) -> bool {
    auto config = parseConfigurationFile(filename);
    if (!config) {
        return false;
    }

    return validateConfigurationData(*config);
}

auto StateManager::enableAutoSave(bool enable) -> bool {
    bool wasEnabled = auto_save_enabled_.load();
    auto_save_enabled_ = enable;

    if (enable && !wasEnabled) {
        return startAutoSaveThread();
    } else if (!enable && wasEnabled) {
        stopAutoSaveThread();
    }

    spdlog::debug("Auto-save {}", enable ? "enabled" : "disabled");
    return true;
}

auto StateManager::isAutoSaveEnabled() -> bool {
    return auto_save_enabled_.load();
}

auto StateManager::setAutoSaveInterval(uint32_t intervalSeconds) -> bool {
    if (intervalSeconds < 10) {
        setLastError("Auto-save interval must be at least 10 seconds");
        return false;
    }

    auto_save_interval_ = intervalSeconds;
    spdlog::debug("Auto-save interval set to {} seconds", intervalSeconds);
    return true;
}

auto StateManager::getAutoSaveInterval() -> uint32_t {
    return auto_save_interval_.load();
}

auto StateManager::createBackup() -> bool {
    std::string backupName = generateBackupName();
    std::string backupPath = getBackupPath(backupName);

    auto config = collectCurrentState();
    bool success = writeConfigurationFile(backupPath, config);

    if (success) {
        cleanupOldBackups();
        notifyBackup(backupName, true);
    } else {
        notifyBackup(backupName, false);
    }

    logOperation("Create backup " + backupName, success);
    return success;
}

auto StateManager::restoreFromBackup(const std::string& backupName) -> bool {
    std::string backupPath = getBackupPath(backupName);

    if (!std::filesystem::exists(backupPath)) {
        setLastError("Backup not found: " + backupName);
        return false;
    }

    auto config = parseConfigurationFile(backupPath);
    if (!config) {
        return false;
    }

    bool success = applyConfiguration(*config);
    if (success) {
        std::lock_guard<std::mutex> lock(config_mutex_);
        current_config_ = *config;
        state_modified_ = true;
        saveConfiguration();
    }

    logOperation("Restore from backup " + backupName, success);
    return success;
}

auto StateManager::listBackups() -> std::vector<std::string> {
    std::vector<std::string> backups;

    try {
        if (std::filesystem::exists(backup_directory_)) {
            for (const auto& entry : std::filesystem::directory_iterator(backup_directory_)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    backups.push_back(entry.path().stem().string());
                }
            }
        }
    } catch (const std::exception& e) {
        setLastError("Failed to list backups: " + std::string(e.what()));
    }

    std::sort(backups.begin(), backups.end(), std::greater<std::string>());
    return backups;
}

auto StateManager::enableSafetyMode(bool enable) -> bool {
    safety_mode_enabled_ = enable;
    spdlog::debug("Safety mode {}", enable ? "enabled" : "disabled");
    return true;
}

auto StateManager::isSafetyModeEnabled() -> bool {
    return safety_mode_enabled_.load();
}

auto StateManager::setEmergencyState() -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    spdlog::warn("Setting emergency state");

    // Save current state before emergency shutdown
    saveEmergencyState();

    // Turn off all non-essential switches
    if (power_manager_) {
        power_manager_->powerOffNonEssentialSwitches();
    } else {
        // Fallback: turn off all switches
        auto switchCount = switch_manager_->getSwitchCount();
        for (uint32_t i = 0; i < switchCount; ++i) {
            switch_manager_->setSwitchState(i, SwitchState::OFF);
        }
    }

    emergency_state_active_ = true;
    notifyEmergency(true);

    return true;
}

auto StateManager::clearEmergencyState() -> bool {
    if (!emergency_state_active_.load()) {
        return true;
    }

    spdlog::info("Clearing emergency state");
    emergency_state_active_ = false;
    notifyEmergency(false);

    return true;
}

auto StateManager::isEmergencyStateActive() -> bool {
    return emergency_state_active_.load();
}

auto StateManager::saveEmergencyState() -> bool {
    auto config = collectCurrentState();
    std::string emergencyPath = getFullPath(emergency_filename_);

    bool success = writeConfigurationFile(emergencyPath, config);
    logOperation("Save emergency state", success);

    return success;
}

auto StateManager::restoreEmergencyState() -> bool {
    std::string emergencyPath = getFullPath(emergency_filename_);

    if (!std::filesystem::exists(emergencyPath)) {
        setLastError("Emergency state file not found");
        return false;
    }

    auto config = parseConfigurationFile(emergencyPath);
    if (!config) {
        return false;
    }

    bool success = applyConfiguration(*config);
    if (success) {
        clearEmergencyState();
    }

    logOperation("Restore emergency state", success);
    return success;
}

auto StateManager::getLastSaveTime() -> std::optional<std::chrono::steady_clock::time_point> {
    return last_save_time_;
}

auto StateManager::getLastLoadTime() -> std::optional<std::chrono::steady_clock::time_point> {
    return last_load_time_;
}

auto StateManager::getStateFileSize() -> std::optional<size_t> {
    try {
        std::string configPath = getFullPath(config_filename_);
        if (std::filesystem::exists(configPath)) {
            return std::filesystem::file_size(configPath);
        }
    } catch (const std::exception& e) {
        setLastError("Failed to get file size: " + std::string(e.what()));
    }

    return std::nullopt;
}

auto StateManager::getConfigurationVersion() -> std::string {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return current_config_.config_version;
}

auto StateManager::isStateModified() -> bool {
    return state_modified_.load();
}

auto StateManager::setSetting(const std::string& key, const std::string& value) -> bool {
    if (key.empty()) {
        setLastError("Setting key cannot be empty");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        custom_settings_[key] = value;
    }

    state_modified_ = true;
    spdlog::debug("Setting '{}' = '{}'", key, value);

    return true;
}

auto StateManager::getSetting(const std::string& key) -> std::optional<std::string> {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    auto it = custom_settings_.find(key);
    return (it != custom_settings_.end()) ? std::make_optional(it->second) : std::nullopt;
}

auto StateManager::removeSetting(const std::string& key) -> bool {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    auto erased = custom_settings_.erase(key);

    if (erased > 0) {
        state_modified_ = true;
        spdlog::debug("Removed setting '{}'", key);
    }

    return erased > 0;
}

auto StateManager::getAllSettings() -> std::unordered_map<std::string, std::string> {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    return custom_settings_;
}

auto StateManager::clearAllSettings() -> bool {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    bool hadSettings = !custom_settings_.empty();
    custom_settings_.clear();

    if (hadSettings) {
        state_modified_ = true;
        spdlog::debug("Cleared all settings");
    }

    return true;
}

void StateManager::setStateChangeCallback(StateChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    state_change_callback_ = std::move(callback);
}

void StateManager::setBackupCallback(BackupCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    backup_callback_ = std::move(callback);
}

void StateManager::setEmergencyCallback(EmergencyCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    emergency_callback_ = std::move(callback);
}

auto StateManager::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto StateManager::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// =========================================================================
// Internal Methods
// =========================================================================

auto StateManager::startAutoSaveThread() -> bool {
    std::lock_guard<std::mutex> lock(auto_save_mutex_);

    if (auto_save_running_.load()) {
        return true;
    }

    auto_save_running_ = true;
    auto_save_thread_ = std::make_unique<std::thread>(&StateManager::autoSaveLoop, this);

    spdlog::debug("Auto-save thread started");
    return true;
}

auto StateManager::stopAutoSaveThread() -> void {
    {
        std::lock_guard<std::mutex> lock(auto_save_mutex_);
        if (!auto_save_running_.load()) {
            return;
        }
        auto_save_running_ = false;
    }

    auto_save_cv_.notify_all();

    if (auto_save_thread_ && auto_save_thread_->joinable()) {
        auto_save_thread_->join();
    }

    auto_save_thread_.reset();
    spdlog::debug("Auto-save thread stopped");
}

auto StateManager::autoSaveLoop() -> void {
    spdlog::debug("Auto-save loop started");

    while (auto_save_running_.load()) {
        std::unique_lock<std::mutex> lock(auto_save_mutex_);
        auto interval = std::chrono::seconds(auto_save_interval_.load());

        auto_save_cv_.wait_for(lock, interval, [this] {
            return !auto_save_running_.load();
        });

        if (!auto_save_running_.load()) {
            break;
        }

        if (state_modified_.load()) {
            lock.unlock();
            saveConfiguration();
            lock.lock();
        }
    }

    spdlog::debug("Auto-save loop stopped");
}

auto StateManager::collectCurrentState() -> DeviceConfiguration {
    DeviceConfiguration config;
    config.config_version = "1.0";
    config.saved_at = std::chrono::steady_clock::now();

    if (switch_manager_) {
        auto switchCount = switch_manager_->getSwitchCount();
        for (uint32_t i = 0; i < switchCount; ++i) {
            SavedSwitchState savedState;
            savedState.index = i;

            auto switchInfo = switch_manager_->getSwitchInfo(i);
            savedState.name = switchInfo ? switchInfo->name : ("Switch " + std::to_string(i));
            savedState.state = switch_manager_->getSwitchState(i).value_or(SwitchState::OFF);
            savedState.enabled = true;
            savedState.timestamp = std::chrono::steady_clock::now();

            config.switch_states.push_back(savedState);
        }
    }

    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        config.settings = custom_settings_;
    }

    return config;
}

auto StateManager::applyConfiguration(const DeviceConfiguration& config) -> bool {
    if (!validateConfigurationData(config)) {
        return false;
    }

    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    spdlog::info("Applying configuration with {} switch states", config.switch_states.size());

    // Apply switch states
    for (const auto& savedState : config.switch_states) {
        if (savedState.enabled && savedState.index < switch_manager_->getSwitchCount()) {
            if (!switch_manager_->setSwitchState(savedState.index, savedState.state)) {
                spdlog::warn("Failed to set state for switch {}", savedState.index);
            }
        }
    }

    // Apply settings
    {
        std::lock_guard<std::mutex> lock(settings_mutex_);
        custom_settings_ = config.settings;
    }

    return true;
}

auto StateManager::validateConfigurationData(const DeviceConfiguration& config) -> bool {
    if (config.config_version.empty()) {
        setLastError("Configuration version cannot be empty");
        return false;
    }

    if (!switch_manager_) {
        return true; // Can't validate switch states without manager
    }

    auto switchCount = switch_manager_->getSwitchCount();
    for (const auto& savedState : config.switch_states) {
        if (savedState.index >= switchCount) {
            setLastError("Invalid switch index in configuration: " + std::to_string(savedState.index));
            return false;
        }
    }

    return true;
}

auto StateManager::ensureDirectoryExists(const std::string& directory) -> bool {
    try {
        if (!std::filesystem::exists(directory)) {
            std::filesystem::create_directories(directory);
        }
        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to create directory: " + std::string(e.what()));
        return false;
    }
}

auto StateManager::generateBackupName() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << "backup_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

auto StateManager::parseConfigurationFile(const std::string& filename) -> std::optional<DeviceConfiguration> {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            setLastError("Failed to open file: " + filename);
            return std::nullopt;
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

        return jsonToConfig(content);
    } catch (const std::exception& e) {
        setLastError("Failed to parse configuration file: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto StateManager::writeConfigurationFile(const std::string& filename, const DeviceConfiguration& config) -> bool {
    try {
        std::string json = configToJson(config);

        std::ofstream file(filename);
        if (!file.is_open()) {
            setLastError("Failed to create file: " + filename);
            return false;
        }

        file << json;
        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to write configuration file: " + std::string(e.what()));
        return false;
    }
}

auto StateManager::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("StateManager Error: {}", error);
}

auto StateManager::logOperation(const std::string& operation, bool success) -> void {
    if (success) {
        spdlog::debug("StateManager operation succeeded: {}", operation);
    } else {
        spdlog::warn("StateManager operation failed: {}", operation);
    }
}

auto StateManager::configToJson(const DeviceConfiguration& config) -> std::string {
    nlohmann::json j;

    j["device_name"] = config.device_name;
    j["config_version"] = config.config_version;
    j["saved_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        config.saved_at.time_since_epoch()).count();

    j["switch_states"] = nlohmann::json::array();
    for (const auto& state : config.switch_states) {
        nlohmann::json stateJson;
        stateJson["index"] = state.index;
        stateJson["name"] = state.name;
        stateJson["state"] = static_cast<int>(state.state);
        stateJson["enabled"] = state.enabled;
        stateJson["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            state.timestamp.time_since_epoch()).count();

        j["switch_states"].push_back(stateJson);
    }

    j["settings"] = config.settings;

    return j.dump(2);
}

auto StateManager::jsonToConfig(const std::string& json) -> std::optional<DeviceConfiguration> {
    try {
        nlohmann::json j = nlohmann::json::parse(json);

        DeviceConfiguration config;
        config.device_name = j.value("device_name", "");
        config.config_version = j.value("config_version", "1.0");

        if (j.contains("saved_at")) {
            auto ms = j["saved_at"].get<uint64_t>();
            config.saved_at = std::chrono::steady_clock::time_point(std::chrono::milliseconds(ms));
        }

        if (j.contains("switch_states")) {
            for (const auto& stateJson : j["switch_states"]) {
                SavedSwitchState state;
                state.index = stateJson.value("index", 0);
                state.name = stateJson.value("name", "");
                state.state = static_cast<SwitchState>(stateJson.value("state", 0));
                state.enabled = stateJson.value("enabled", true);

                if (stateJson.contains("timestamp")) {
                    auto ms = stateJson["timestamp"].get<uint64_t>();
                    state.timestamp = std::chrono::steady_clock::time_point(std::chrono::milliseconds(ms));
                }

                config.switch_states.push_back(state);
            }
        }

        if (j.contains("settings")) {
            config.settings = j["settings"].get<std::unordered_map<std::string, std::string>>();
        }

        return config;
    } catch (const std::exception& e) {
        setLastError("Failed to parse JSON configuration: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto StateManager::notifyStateChange(bool saved, const std::string& filename) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (state_change_callback_) {
        state_change_callback_(saved, filename);
    }
}

auto StateManager::notifyBackup(const std::string& backupName, bool success) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (backup_callback_) {
        backup_callback_(backupName, success);
    }
}

auto StateManager::notifyEmergency(bool active) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (emergency_callback_) {
        emergency_callback_(active);
    }
}

auto StateManager::getFullPath(const std::string& filename) -> std::string {
    return config_directory_ + "/" + filename;
}

auto StateManager::getBackupPath(const std::string& backupName) -> std::string {
    return backup_directory_ + "/" + backupName + ".json";
}

auto StateManager::cleanupOldBackups(uint32_t maxBackups) -> void {
    try {
        auto backups = listBackups();
        if (backups.size() > maxBackups) {
            // Sort by name (which includes timestamp), keep newest
            std::sort(backups.begin(), backups.end(), std::greater<std::string>());

            for (size_t i = maxBackups; i < backups.size(); ++i) {
                std::string backupPath = getBackupPath(backups[i]);
                std::filesystem::remove(backupPath);
                spdlog::debug("Removed old backup: {}", backups[i]);
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to cleanup old backups: {}", e.what());
    }
}

} // namespace lithium::device::ascom::sw::components
