/*
 * configuration_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Configuration Management Component

*************************************************/

#pragma once

#include <string>
#include <map>
#include <optional>
#include <variant>
#include <functional>

namespace lithium::ascom::dome::components {

/**
 * @brief Configuration value type
 */
using ConfigValue = std::variant<bool, int, double, std::string>;

/**
 * @brief Configuration section structure
 */
struct ConfigSection {
    std::map<std::string, ConfigValue> values;
    std::string description;
};

/**
 * @brief Configuration Management Component for ASCOM Dome
 */
class ConfigurationManager {
public:
    explicit ConfigurationManager();
    virtual ~ConfigurationManager();

    // === Configuration File Operations ===
    auto loadConfiguration(const std::string& config_path) -> bool;
    auto saveConfiguration(const std::string& config_path) -> bool;
    auto getDefaultConfigPath() -> std::string;

    // === Value Management ===
    auto setValue(const std::string& section, const std::string& key, const ConfigValue& value) -> bool;
    auto getValue(const std::string& section, const std::string& key) -> std::optional<ConfigValue>;
    auto hasValue(const std::string& section, const std::string& key) -> bool;
    auto removeValue(const std::string& section, const std::string& key) -> bool;

    // === Type-specific Getters ===
    auto getBool(const std::string& section, const std::string& key, bool default_value = false) -> bool;
    auto getInt(const std::string& section, const std::string& key, int default_value = 0) -> int;
    auto getDouble(const std::string& section, const std::string& key, double default_value = 0.0) -> double;
    auto getString(const std::string& section, const std::string& key, const std::string& default_value = "") -> std::string;

    // === Section Management ===
    auto addSection(const std::string& section, const std::string& description = "") -> bool;
    auto removeSection(const std::string& section) -> bool;
    auto hasSection(const std::string& section) -> bool;
    auto getSectionNames() -> std::vector<std::string>;
    auto getSection(const std::string& section) -> std::optional<ConfigSection>;

    // === Preset Management ===
    auto savePreset(const std::string& name, const std::string& description = "") -> bool;
    auto loadPreset(const std::string& name) -> bool;
    auto deletePreset(const std::string& name) -> bool;
    auto getPresetNames() -> std::vector<std::string>;

    // === Validation ===
    auto validateConfiguration() -> std::vector<std::string>;
    auto setValidator(const std::string& section, const std::string& key, 
                     std::function<bool(const ConfigValue&)> validator) -> bool;

    // === Default Configuration ===
    auto loadDefaultConfiguration() -> bool;
    auto resetToDefaults() -> bool;
    auto isDefaultValue(const std::string& section, const std::string& key) -> bool;

    // === Change Tracking ===
    auto hasUnsavedChanges() -> bool;
    auto markAsSaved() -> void;
    auto setChangeCallback(std::function<void(const std::string&, const std::string&, const ConfigValue&)> callback) -> void;

private:
    std::map<std::string, ConfigSection> config_sections_;
    std::map<std::string, std::map<std::string, ConfigValue>> presets_;
    std::map<std::string, std::map<std::string, std::function<bool(const ConfigValue&)>>> validators_;
    std::map<std::string, std::map<std::string, ConfigValue>> default_values_;

    bool has_unsaved_changes_{false};
    std::string current_config_path_;

    std::function<void(const std::string&, const std::string&, const ConfigValue&)> change_callback_;

    auto initializeDefaultConfiguration() -> void;
    auto parseConfigFile(const std::string& content) -> bool;
    auto generateConfigFile() -> std::string;
    auto validateValue(const std::string& section, const std::string& key, const ConfigValue& value) -> bool;
    auto convertToString(const ConfigValue& value) -> std::string;
    auto parseFromString(const std::string& str, const std::string& type) -> std::optional<ConfigValue>;
};

} // namespace lithium::ascom::dome::components
