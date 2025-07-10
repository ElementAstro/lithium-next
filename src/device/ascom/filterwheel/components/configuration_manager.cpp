/*
 * configuration_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Filter Wheel Configuration Manager Implementation
Note: Refactored to use existing ConfigManager infrastructure

*************************************************/

#include "configuration_manager.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <sstream>

namespace lithium::device::ascom::filterwheel::components {

ConfigurationManager::ConfigurationManager() {
    spdlog::debug("ConfigurationManager constructor called");
}

ConfigurationManager::~ConfigurationManager() {
    spdlog::debug("ConfigurationManager destructor called");
}

auto ConfigurationManager::initialize(const std::string& config_path) -> bool {
    spdlog::info("Initializing ASCOM FilterWheel Configuration Manager");
    
    try {
        config_path_ = config_path.empty() ? "/device/ascom/filterwheel" : config_path;
        profiles_path_ = config_path_ + "/profiles";
        settings_path_ = config_path_ + "/settings";
        backups_path_ = config_path_ + "/backups";
        
        // Initialize default configuration and profile
        createDefaultConfiguration();
        
        spdlog::info("ASCOM FilterWheel Configuration Manager initialized successfully");
        return true;
        
    } catch (const std::exception& e) {
        setError("Configuration initialization failed: " + std::string(e.what()));
        spdlog::error("Configuration initialization failed: {}", e.what());
        return false;
    }
}

auto ConfigurationManager::shutdown() -> void {
    spdlog::info("Shutting down Configuration Manager");
    
    std::lock_guard<std::mutex> lock1(config_mutex_);
    std::lock_guard<std::mutex> lock2(profiles_mutex_);
    std::lock_guard<std::mutex> lock3(settings_mutex_);
    
    filter_configs_.clear();
    profiles_.clear();
    settings_.clear();
    current_profile_name_.clear();
}

auto ConfigurationManager::getFilterConfiguration(int slot) -> std::optional<FilterConfiguration> {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!validateSlot(slot)) {
        setError("Invalid filter slot: " + std::to_string(slot));
        return std::nullopt;
    }
    
    auto it = filter_configs_.find(slot);
    if (it != filter_configs_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

auto ConfigurationManager::setFilterConfiguration(int slot, const FilterConfiguration& config) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!validateSlot(slot)) {
        setError("Invalid filter slot: " + std::to_string(slot));
        return false;
    }
    
    auto validation = validateFilterConfiguration(config);
    if (!validation.is_valid) {
        setError("Invalid filter configuration: " + (validation.errors.empty() ? "Unknown error" : validation.errors[0]));
        return false;
    }
    
    filter_configs_[slot] = config;
    notifyConfigurationChange(slot, config);
    
    spdlog::debug("Filter configuration set for slot {}: {}", slot, config.name);
    return true;
}

auto ConfigurationManager::getAllFilterConfigurations() -> std::vector<FilterConfiguration> {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    std::vector<FilterConfiguration> configs;
    configs.reserve(filter_configs_.size());
    
    for (const auto& [slot, config] : filter_configs_) {
        configs.push_back(config);
    }
    
    return configs;
}

auto ConfigurationManager::validateFilterConfiguration(const FilterConfiguration& config) -> ConfigValidation {
    ConfigValidation result;
    result.is_valid = true;
    
    // Basic validation
    if (config.name.empty()) {
        result.errors.push_back("Filter name cannot be empty");
        result.is_valid = false;
    }
    
    if (config.slot < 0 || config.slot > 255) {
        result.errors.push_back("Filter slot must be between 0 and 255");
        result.is_valid = false;
    }
    
    // Wavelength validation
    if (config.wavelength < 0) {
        result.warnings.push_back("Negative wavelength specified");
    }
    
    // Bandwidth validation
    if (config.bandwidth < 0) {
        result.warnings.push_back("Negative bandwidth specified");
    }
    
    return result;
}

auto ConfigurationManager::getFilterName(int slot) -> std::optional<std::string> {
    auto config = getFilterConfiguration(slot);
    return config ? std::optional{config->name} : std::nullopt;
}

auto ConfigurationManager::setFilterName(int slot, const std::string& name) -> bool {
    return updateFilterField(slot, [&name](FilterConfiguration& config) {
        config.name = name;
    });
}

auto ConfigurationManager::getFilterType(int slot) -> std::optional<std::string> {
    auto config = getFilterConfiguration(slot);
    return config ? std::optional{config->type} : std::nullopt;
}

auto ConfigurationManager::setFilterType(int slot, const std::string& type) -> bool {
    return updateFilterField(slot, [&type](FilterConfiguration& config) {
        config.type = type;
    });
}

auto ConfigurationManager::getFocusOffset(int slot) -> double {
    auto config = getFilterConfiguration(slot);
    return config ? config->focus_offset : 0.0;
}

auto ConfigurationManager::setFocusOffset(int slot, double offset) -> bool {
    return updateFilterField(slot, [offset](FilterConfiguration& config) {
        config.focus_offset = offset;
    });
}

auto ConfigurationManager::findFilterByName(const std::string& name) -> std::optional<int> {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    for (const auto& [slot, config] : filter_configs_) {
        if (config.name == name) {
            return slot;
        }
    }
    
    return std::nullopt;
}

auto ConfigurationManager::findFiltersByType(const std::string& type) -> std::vector<int> {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    std::vector<int> slots;
    for (const auto& [slot, config] : filter_configs_) {
        if (config.type == type) {
            slots.push_back(slot);
        }
    }
    
    return slots;
}

auto ConfigurationManager::getFilterInfo(int slot) -> std::optional<FilterInfo> {
    auto config = getFilterConfiguration(slot);
    if (config) {
        FilterInfo info;
        info.name = config->name;
        info.type = config->type;
        info.description = config->description;
        return info;
    }
    return std::nullopt;
}

auto ConfigurationManager::setFilterInfo(int slot, const FilterInfo& info) -> bool {
    return updateFilterField(slot, [&info](FilterConfiguration& config) {
        config.name = info.name;
        config.type = info.type;
        config.description = info.description;
    });
}

auto ConfigurationManager::createProfile(const std::string& name, const std::string& description) -> bool {
    std::lock_guard<std::mutex> lock(profiles_mutex_);
    
    if (!validateProfileName(name)) {
        setError("Invalid profile name: " + name);
        return false;
    }
    
    if (profiles_.find(name) != profiles_.end()) {
        setError("Profile already exists: " + name);
        return false;
    }
    
    FilterProfile profile;
    profile.name = name;
    profile.description = description;
    profile.created = std::chrono::system_clock::now();
    profile.modified = profile.created;
    
    // Copy current filter configurations
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        for (const auto& [slot, config] : filter_configs_) {
            profile.filters.push_back(config);
        }
    }
    
    profiles_[name] = profile;
    spdlog::debug("Created profile: {}", name);
    return true;
}

auto ConfigurationManager::loadProfile(const std::string& name) -> bool {
    std::lock_guard<std::mutex> profiles_lock(profiles_mutex_);
    
    auto it = profiles_.find(name);
    if (it == profiles_.end()) {
        setError("Profile not found: " + name);
        return false;
    }
    
    // Load filters from profile
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        filter_configs_.clear();
        
        for (const auto& config : it->second.filters) {
            filter_configs_[config.slot] = config;
        }
    }
    
    current_profile_name_ = name;
    notifyProfileChange(name);
    
    spdlog::debug("Loaded profile: {}", name);
    return true;
}

auto ConfigurationManager::saveProfile(const std::string& name) -> bool {
    std::lock_guard<std::mutex> profiles_lock(profiles_mutex_);
    
    auto it = profiles_.find(name);
    if (it == profiles_.end()) {
        setError("Profile not found: " + name);
        return false;
    }
    
    // Update profile with current filter configurations
    {
        std::lock_guard<std::mutex> config_lock(config_mutex_);
        it->second.filters.clear();
        
        for (const auto& [slot, config] : filter_configs_) {
            it->second.filters.push_back(config);
        }
    }
    
    it->second.modified = std::chrono::system_clock::now();
    
    spdlog::debug("Saved profile: {}", name);
    return true;
}

auto ConfigurationManager::deleteProfile(const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(profiles_mutex_);
    
    if (name == "Default") {
        setError("Cannot delete default profile");
        return false;
    }
    
    auto erased = profiles_.erase(name);
    if (erased == 0) {
        setError("Profile not found: " + name);
        return false;
    }
    
    if (current_profile_name_ == name) {
        current_profile_name_ = "Default";
    }
    
    spdlog::debug("Deleted profile: {}", name);
    return true;
}

auto ConfigurationManager::getCurrentProfile() -> std::optional<FilterProfile> {
    std::lock_guard<std::mutex> lock(profiles_mutex_);
    
    auto it = profiles_.find(current_profile_name_);
    if (it != profiles_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

auto ConfigurationManager::setCurrentProfile(const std::string& name) -> bool {
    return loadProfile(name);
}

auto ConfigurationManager::getAvailableProfiles() -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(profiles_mutex_);
    
    std::vector<std::string> names;
    names.reserve(profiles_.size());
    
    for (const auto& [name, profile] : profiles_) {
        names.push_back(name);
    }
    
    return names;
}

auto ConfigurationManager::getProfileInfo(const std::string& name) -> std::optional<FilterProfile> {
    std::lock_guard<std::mutex> lock(profiles_mutex_);
    
    auto it = profiles_.find(name);
    if (it != profiles_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// Settings management
auto ConfigurationManager::getSetting(const std::string& key) -> std::optional<std::string> {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    
    auto it = settings_.find(key);
    if (it != settings_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

auto ConfigurationManager::setSetting(const std::string& key, const std::string& value) -> bool {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    
    settings_[key] = value;
    spdlog::debug("Setting '{}' = '{}'", key, value);
    return true;
}

auto ConfigurationManager::getAllSettings() -> std::map<std::string, std::string> {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    return settings_;
}

auto ConfigurationManager::resetSettings() -> void {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    settings_.clear();
    spdlog::debug("All settings reset");
}

// Error handling
auto ConfigurationManager::getLastError() -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto ConfigurationManager::clearError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Callback management
auto ConfigurationManager::setConfigurationChangeCallback(ConfigurationChangeCallback callback) -> void {
    config_change_callback_ = std::move(callback);
}

auto ConfigurationManager::setProfileChangeCallback(ProfileChangeCallback callback) -> void {
    profile_change_callback_ = std::move(callback);
}

// Private helper methods
auto ConfigurationManager::setError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("ConfigurationManager error: {}", error);
}

auto ConfigurationManager::notifyConfigurationChange(int slot, const FilterConfiguration& config) -> void {
    if (config_change_callback_) {
        try {
            config_change_callback_(slot, config);
        } catch (const std::exception& e) {
            spdlog::error("Exception in configuration change callback: {}", e.what());
        }
    }
}

auto ConfigurationManager::notifyProfileChange(const std::string& profile_name) -> void {
    if (profile_change_callback_) {
        try {
            profile_change_callback_(profile_name);
        } catch (const std::exception& e) {
            spdlog::error("Exception in profile change callback: {}", e.what());
        }
    }
}

auto ConfigurationManager::validateSlot(int slot) -> bool {
    return slot >= 0 && slot <= 255;  // Reasonable range for filter slots
}

auto ConfigurationManager::validateName(const std::string& name) -> bool {
    return !name.empty() && name.length() <= 255;
}

auto ConfigurationManager::validateProfileName(const std::string& name) -> bool {
    return validateName(name) && name != "." && name != "..";
}

auto ConfigurationManager::createDefaultConfiguration() -> void {
    spdlog::debug("Creating default filter wheel configuration");
    
    // Create default profile
    FilterProfile default_profile;
    default_profile.name = "Default";
    default_profile.description = "Default filter wheel configuration";
    default_profile.created = std::chrono::system_clock::now();
    default_profile.modified = default_profile.created;
    
    // Create default filter configurations (8 filters)
    for (int i = 0; i < 8; ++i) {
        FilterConfiguration config;
        config.slot = i;
        config.name = "Filter " + std::to_string(i + 1);
        config.type = "Unknown";
        config.wavelength = 0.0;
        config.bandwidth = 0.0;
        config.focus_offset = 0.0;
        config.description = "Default filter slot " + std::to_string(i + 1);
        
        default_profile.filters.push_back(config);
        filter_configs_[i] = config;
    }
    
    profiles_["Default"] = default_profile;
    current_profile_name_ = "Default";
    
    spdlog::debug("Default configuration created with {} filters", default_profile.filters.size());
}

// Stub implementations for remaining methods
auto ConfigurationManager::exportProfile(const std::string& name, const std::string& file_path) -> bool {
    // TODO: Implement JSON export
    setError("Export functionality not yet implemented");
    return false;
}

auto ConfigurationManager::importProfile(const std::string& file_path) -> std::optional<std::string> {
    // TODO: Implement JSON import
    setError("Import functionality not yet implemented");
    return std::nullopt;
}

auto ConfigurationManager::exportAllProfiles(const std::string& directory) -> bool {
    // TODO: Implement export all
    setError("Export all functionality not yet implemented");
    return false;
}

auto ConfigurationManager::importProfiles(const std::string& directory) -> std::vector<std::string> {
    // TODO: Implement import all
    setError("Import all functionality not yet implemented");
    return {};
}

auto ConfigurationManager::validateAllConfigurations() -> ConfigValidation {
    ConfigValidation result;
    result.is_valid = true;
    
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    for (const auto& [slot, config] : filter_configs_) {
        auto validation = validateFilterConfiguration(config);
        if (!validation.is_valid) {
            result.is_valid = false;
            for (const auto& error : validation.errors) {
                result.errors.push_back("Slot " + std::to_string(slot) + ": " + error);
            }
        }
        for (const auto& warning : validation.warnings) {
            result.warnings.push_back("Slot " + std::to_string(slot) + ": " + warning);
        }
    }
    
    return result;
}

auto ConfigurationManager::repairConfiguration() -> bool {
    // TODO: Implement repair logic
    setError("Repair functionality not yet implemented");
    return false;
}

auto ConfigurationManager::getConfigurationStatus() -> std::string {
    std::lock_guard<std::mutex> config_lock(config_mutex_);
    std::lock_guard<std::mutex> profile_lock(profiles_mutex_);
    
    return "Configurations: " + std::to_string(filter_configs_.size()) + 
           ", Profiles: " + std::to_string(profiles_.size()) + 
           ", Current: " + current_profile_name_;
}

auto ConfigurationManager::createBackup(const std::string& backup_name) -> bool {
    // TODO: Implement backup functionality
    setError("Backup functionality not yet implemented");
    return false;
}

auto ConfigurationManager::restoreBackup(const std::string& backup_name) -> bool {
    // TODO: Implement restore functionality
    setError("Restore functionality not yet implemented");
    return false;
}

auto ConfigurationManager::getAvailableBackups() -> std::vector<std::string> {
    // TODO: Implement backup listing
    return {};
}

auto ConfigurationManager::deleteBackup(const std::string& backup_name) -> bool {
    // TODO: Implement backup deletion
    setError("Backup deletion functionality not yet implemented");
    return false;
}

// File operation stubs
auto ConfigurationManager::loadConfigurationsFromFile() -> bool {
    // TODO: Implement file loading
    return true;
}

auto ConfigurationManager::saveConfigurationsToFile() -> bool {
    // TODO: Implement file saving
    return true;
}

auto ConfigurationManager::loadProfilesFromFile() -> bool {
    // TODO: Implement profile loading
    return true;
}

auto ConfigurationManager::saveProfilesToFile() -> bool {
    // TODO: Implement profile saving
    return true;
}

auto ConfigurationManager::loadSettingsFromFile() -> bool {
    // TODO: Implement settings loading
    return true;
}

auto ConfigurationManager::saveSettingsToFile() -> bool {
    // TODO: Implement settings saving
    return true;
}

auto ConfigurationManager::configurationToJson(const FilterConfiguration& config) -> std::string {
    // TODO: Implement JSON serialization
    return "{}";
}

auto ConfigurationManager::configurationFromJson(const std::string& json) -> std::optional<FilterConfiguration> {
    // TODO: Implement JSON deserialization
    return std::nullopt;
}

auto ConfigurationManager::profileToJson(const FilterProfile& profile) -> std::string {
    // TODO: Implement JSON serialization
    return "{}";
}

auto ConfigurationManager::profileFromJson(const std::string& json) -> std::optional<FilterProfile> {
    // TODO: Implement JSON deserialization
    return std::nullopt;
}

auto ConfigurationManager::generateBackupName() -> std::string {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << "backup_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return oss.str();
}

auto ConfigurationManager::ensureDirectoriesExist() -> bool {
    // TODO: Implement directory creation
    return true;
}

auto ConfigurationManager::updateFilterField(int slot, std::function<void(FilterConfiguration&)> updater) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!validateSlot(slot)) {
        setError("Invalid filter slot: " + std::to_string(slot));
        return false;
    }
    
    auto it = filter_configs_.find(slot);
    if (it != filter_configs_.end()) {
        updater(it->second);
        notifyConfigurationChange(slot, it->second);
        return true;
    } else {
        // Create new configuration with the slot set
        FilterConfiguration config;
        config.slot = slot;
        updater(config);
        filter_configs_[slot] = config;
        notifyConfigurationChange(slot, config);
        return true;
    }
}

} // namespace lithium::device::ascom::filterwheel::components
