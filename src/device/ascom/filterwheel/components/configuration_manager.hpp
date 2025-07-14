/*
 * configuration_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Configuration Manager Component

This component manages filter configurations, profiles, and persistent
settings for the ASCOM filterwheel.

*************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <mutex>

#include "device/template/filterwheel.hpp"

namespace lithium::device::ascom::filterwheel::components {

// Filter configuration structure
struct FilterConfiguration {
    int slot;
    std::string name;
    std::string type;
    double wavelength{0.0};      // nm
    double bandwidth{0.0};       // nm
    double focus_offset{0.0};    // steps
    std::string description;
    std::map<std::string, std::string> custom_properties;
};

// Profile structure for complete filter wheel setups
struct FilterProfile {
    std::string name;
    std::string description;
    std::vector<FilterConfiguration> filters;
    std::map<std::string, std::string> settings;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;
};

// Configuration validation result
struct ConfigValidation {
    bool is_valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

/**
 * @brief Configuration Manager for ASCOM Filter Wheels
 *
 * This component handles filter configurations, profiles, and persistent
 * settings storage and retrieval.
 */
class ConfigurationManager {
public:
    ConfigurationManager();
    ~ConfigurationManager();

    // Initialization
    auto initialize(const std::string& config_path = "") -> bool;
    auto shutdown() -> void;

    // Filter configuration management
    auto getFilterConfiguration(int slot) -> std::optional<FilterConfiguration>;
    auto setFilterConfiguration(int slot, const FilterConfiguration& config) -> bool;
    auto getAllFilterConfigurations() -> std::vector<FilterConfiguration>;
    auto validateFilterConfiguration(const FilterConfiguration& config) -> ConfigValidation;

    // Filter information shortcuts
    auto getFilterName(int slot) -> std::optional<std::string>;
    auto setFilterName(int slot, const std::string& name) -> bool;
    auto getFilterType(int slot) -> std::optional<std::string>;
    auto setFilterType(int slot, const std::string& type) -> bool;
    auto getFocusOffset(int slot) -> double;
    auto setFocusOffset(int slot, double offset) -> bool;

    // Filter search and selection
    auto findFilterByName(const std::string& name) -> std::optional<int>;
    auto findFiltersByType(const std::string& type) -> std::vector<int>;
    auto getFilterInfo(int slot) -> std::optional<FilterInfo>;
    auto setFilterInfo(int slot, const FilterInfo& info) -> bool;

    // Profile management
    auto createProfile(const std::string& name, const std::string& description = "") -> bool;
    auto loadProfile(const std::string& name) -> bool;
    auto saveProfile(const std::string& name) -> bool;
    auto deleteProfile(const std::string& name) -> bool;
    auto getCurrentProfile() -> std::optional<FilterProfile>;
    auto setCurrentProfile(const std::string& name) -> bool;
    auto getAvailableProfiles() -> std::vector<std::string>;
    auto getProfileInfo(const std::string& name) -> std::optional<FilterProfile>;

    // Import/Export
    auto exportProfile(const std::string& name, const std::string& file_path) -> bool;
    auto importProfile(const std::string& file_path) -> std::optional<std::string>;
    auto exportAllProfiles(const std::string& directory) -> bool;
    auto importProfiles(const std::string& directory) -> std::vector<std::string>;

    // Settings management
    auto getSetting(const std::string& key) -> std::optional<std::string>;
    auto setSetting(const std::string& key, const std::string& value) -> bool;
    auto getAllSettings() -> std::map<std::string, std::string>;
    auto resetSettings() -> void;

    // Validation and consistency
    auto validateAllConfigurations() -> ConfigValidation;
    auto repairConfiguration() -> bool;
    auto getConfigurationStatus() -> std::string;

    // Backup and restore
    auto createBackup(const std::string& backup_name = "") -> bool;
    auto restoreBackup(const std::string& backup_name) -> bool;
    auto getAvailableBackups() -> std::vector<std::string>;
    auto deleteBackup(const std::string& backup_name) -> bool;

    // Event handling
    using ConfigurationChangeCallback = std::function<void(int slot, const FilterConfiguration& config)>;
    using ProfileChangeCallback = std::function<void(const std::string& profile_name)>;

    auto setConfigurationChangeCallback(ConfigurationChangeCallback callback) -> void;
    auto setProfileChangeCallback(ProfileChangeCallback callback) -> void;

    // Error handling
    auto getLastError() -> std::string;
    auto clearError() -> void;

private:
    // Configuration storage
    std::map<int, FilterConfiguration> filter_configs_;
    std::map<std::string, FilterProfile> profiles_;
    std::map<std::string, std::string> settings_;
    std::string current_profile_name_;

    // File paths
    std::string config_path_;
    std::string profiles_path_;
    std::string settings_path_;
    std::string backups_path_;

    // Threading and synchronization
    mutable std::mutex config_mutex_;
    mutable std::mutex profiles_mutex_;
    mutable std::mutex settings_mutex_;

    // Callbacks
    ConfigurationChangeCallback config_change_callback_;
    ProfileChangeCallback profile_change_callback_;

    // Error handling
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // File operations
    auto loadConfigurationsFromFile() -> bool;
    auto saveConfigurationsToFile() -> bool;
    auto loadProfilesFromFile() -> bool;
    auto saveProfilesToFile() -> bool;
    auto loadSettingsFromFile() -> bool;
    auto saveSettingsToFile() -> bool;

    // JSON serialization
    auto configurationToJson(const FilterConfiguration& config) -> std::string;
    auto configurationFromJson(const std::string& json) -> std::optional<FilterConfiguration>;
    auto profileToJson(const FilterProfile& profile) -> std::string;
    auto profileFromJson(const std::string& json) -> std::optional<FilterProfile>;

    // Validation helpers
    auto validateSlot(int slot) -> bool;
    auto validateName(const std::string& name) -> bool;
    auto validateProfileName(const std::string& name) -> bool;

    // Utility methods
    auto setError(const std::string& error) -> void;
    auto notifyConfigurationChange(int slot, const FilterConfiguration& config) -> void;
    auto notifyProfileChange(const std::string& profile_name) -> void;
    auto generateBackupName() -> std::string;
    auto ensureDirectoriesExist() -> bool;
    auto createDefaultConfiguration() -> void;
    auto updateFilterField(int slot, std::function<void(FilterConfiguration&)> updater) -> bool;
};

} // namespace lithium::device::ascom::filterwheel::components
