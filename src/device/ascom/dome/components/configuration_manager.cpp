/*
 * configuration_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Configuration Management Component Implementation

*************************************************/

#include "configuration_manager.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace lithium::ascom::dome::components {

ConfigurationManager::ConfigurationManager() {
    spdlog::info("Initializing Configuration Manager");
    initializeDefaultConfiguration();
}

ConfigurationManager::~ConfigurationManager() {
    spdlog::info("Destroying Configuration Manager");
}

auto ConfigurationManager::loadConfiguration(const std::string& config_path) -> bool {
    spdlog::info("Loading configuration from: {}", config_path);

    std::ifstream file(config_path);
    if (!file.is_open()) {
        spdlog::error("Failed to open configuration file: {}", config_path);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    if (parseConfigFile(buffer.str())) {
        current_config_path_ = config_path;
        has_unsaved_changes_ = false;
        spdlog::info("Configuration loaded successfully");
        return true;
    }

    return false;
}

auto ConfigurationManager::saveConfiguration(const std::string& config_path) -> bool {
    spdlog::info("Saving configuration to: {}", config_path);

    std::string config_content = generateConfigFile();

    // Create directory if it doesn't exist
    std::filesystem::path path(config_path);
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(config_path);
    if (!file.is_open()) {
        spdlog::error("Failed to create configuration file: {}", config_path);
        return false;
    }

    file << config_content;
    file.close();

    current_config_path_ = config_path;
    has_unsaved_changes_ = false;
    spdlog::info("Configuration saved successfully");
    return true;
}

auto ConfigurationManager::getDefaultConfigPath() -> std::string {
    // Platform-specific default configuration path
#ifdef _WIN32
    return std::string(std::getenv("APPDATA")) + "\\Lithium\\ASCOMDome\\config.ini";
#else
    return std::string(std::getenv("HOME")) + "/.config/lithium/ascom_dome/config.ini";
#endif
}

auto ConfigurationManager::setValue(const std::string& section, const std::string& key, const ConfigValue& value) -> bool {
    if (!validateValue(section, key, value)) {
        spdlog::error("Invalid value for {}.{}", section, key);
        return false;
    }

    if (!hasSection(section)) {
        addSection(section);
    }

    config_sections_[section].values[key] = value;
    has_unsaved_changes_ = true;

    if (change_callback_) {
        change_callback_(section, key, value);
    }

    spdlog::debug("Set {}.{} = {}", section, key, convertToString(value));
    return true;
}

auto ConfigurationManager::getValue(const std::string& section, const std::string& key) -> std::optional<ConfigValue> {
    if (!hasSection(section)) {
        return std::nullopt;
    }

    auto& section_values = config_sections_[section].values;
    auto it = section_values.find(key);
    if (it != section_values.end()) {
        return it->second;
    }

    return std::nullopt;
}

auto ConfigurationManager::hasValue(const std::string& section, const std::string& key) -> bool {
    return getValue(section, key).has_value();
}

auto ConfigurationManager::removeValue(const std::string& section, const std::string& key) -> bool {
    if (!hasSection(section)) {
        return false;
    }

    auto& section_values = config_sections_[section].values;
    auto it = section_values.find(key);
    if (it != section_values.end()) {
        section_values.erase(it);
        has_unsaved_changes_ = true;
        spdlog::debug("Removed {}.{}", section, key);
        return true;
    }

    return false;
}

auto ConfigurationManager::getBool(const std::string& section, const std::string& key, bool default_value) -> bool {
    auto value = getValue(section, key);
    if (value && std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value);
    }
    return default_value;
}

auto ConfigurationManager::getInt(const std::string& section, const std::string& key, int default_value) -> int {
    auto value = getValue(section, key);
    if (value && std::holds_alternative<int>(*value)) {
        return std::get<int>(*value);
    }
    return default_value;
}

auto ConfigurationManager::getDouble(const std::string& section, const std::string& key, double default_value) -> double {
    auto value = getValue(section, key);
    if (value && std::holds_alternative<double>(*value)) {
        return std::get<double>(*value);
    }
    return default_value;
}

auto ConfigurationManager::getString(const std::string& section, const std::string& key, const std::string& default_value) -> std::string {
    auto value = getValue(section, key);
    if (value && std::holds_alternative<std::string>(*value)) {
        return std::get<std::string>(*value);
    }
    return default_value;
}

auto ConfigurationManager::addSection(const std::string& section, const std::string& description) -> bool {
    config_sections_[section] = ConfigSection{{}, description};
    has_unsaved_changes_ = true;
    spdlog::debug("Added section: {}", section);
    return true;
}

auto ConfigurationManager::removeSection(const std::string& section) -> bool {
    auto it = config_sections_.find(section);
    if (it != config_sections_.end()) {
        config_sections_.erase(it);
        has_unsaved_changes_ = true;
        spdlog::debug("Removed section: {}", section);
        return true;
    }
    return false;
}

auto ConfigurationManager::hasSection(const std::string& section) -> bool {
    return config_sections_.find(section) != config_sections_.end();
}

auto ConfigurationManager::getSectionNames() -> std::vector<std::string> {
    std::vector<std::string> names;
    for (const auto& [name, _] : config_sections_) {
        names.push_back(name);
    }
    return names;
}

auto ConfigurationManager::getSection(const std::string& section) -> std::optional<ConfigSection> {
    auto it = config_sections_.find(section);
    if (it != config_sections_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ConfigurationManager::hasUnsavedChanges() -> bool {
    return has_unsaved_changes_;
}

auto ConfigurationManager::markAsSaved() -> void {
    has_unsaved_changes_ = false;
}

auto ConfigurationManager::setChangeCallback(std::function<void(const std::string&, const std::string&, const ConfigValue&)> callback) -> void {
    change_callback_ = callback;
}

auto ConfigurationManager::loadDefaultConfiguration() -> bool {
    initializeDefaultConfiguration();
    has_unsaved_changes_ = false;
    spdlog::info("Loaded default configuration");
    return true;
}

auto ConfigurationManager::resetToDefaults() -> bool {
    config_sections_.clear();
    return loadDefaultConfiguration();
}

auto ConfigurationManager::initializeDefaultConfiguration() -> void {
    // Connection settings
    addSection("connection", "ASCOM connection settings");
    setValue("connection", "default_connection_type", std::string("alpaca"));
    setValue("connection", "alpaca_host", std::string("localhost"));
    setValue("connection", "alpaca_port", 11111);
    setValue("connection", "alpaca_device_number", 0);
    setValue("connection", "connection_timeout", 30);
    setValue("connection", "max_retries", 3);

    // Dome settings
    addSection("dome", "Dome physical parameters");
    setValue("dome", "diameter", 3.0);
    setValue("dome", "height", 2.5);
    setValue("dome", "slit_width", 1.0);
    setValue("dome", "slit_height", 1.5);
    setValue("dome", "park_position", 0.0);
    setValue("dome", "home_position", 0.0);

    // Movement settings
    addSection("movement", "Dome movement parameters");
    setValue("movement", "default_speed", 5.0);
    setValue("movement", "max_speed", 10.0);
    setValue("movement", "min_speed", 1.0);
    setValue("movement", "position_tolerance", 0.5);
    setValue("movement", "movement_timeout", 300);
    setValue("movement", "backlash_compensation", 0.0);
    setValue("movement", "backlash_enabled", false);

    // Telescope coordination
    addSection("telescope", "Telescope coordination settings");
    setValue("telescope", "radius_from_center", 0.0);
    setValue("telescope", "height_offset", 0.0);
    setValue("telescope", "azimuth_offset", 0.0);
    setValue("telescope", "altitude_offset", 0.0);
    setValue("telescope", "following_tolerance", 1.0);
    setValue("telescope", "following_delay", 1000);
    setValue("telescope", "auto_following", false);

    // Weather safety
    addSection("weather", "Weather safety parameters");
    setValue("weather", "safety_enabled", true);
    setValue("weather", "max_wind_speed", 15.0);
    setValue("weather", "max_rain_rate", 0.1);
    setValue("weather", "min_temperature", -20.0);
    setValue("weather", "max_temperature", 50.0);
    setValue("weather", "max_humidity", 95.0);

    // Logging
    addSection("logging", "Logging configuration");
    setValue("logging", "log_level", std::string("info"));
    setValue("logging", "log_to_file", true);
    setValue("logging", "log_file_path", std::string("ascom_dome.log"));
    setValue("logging", "max_log_size", 10485760); // 10MB
}

auto ConfigurationManager::parseConfigFile(const std::string& content) -> bool {
    // Simple INI-style parser
    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        // Remove whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Section header
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            if (!hasSection(current_section)) {
                addSection(current_section);
            }
            continue;
        }

        // Key-value pair
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos && !current_section.empty()) {
            std::string key = line.substr(0, eq_pos);
            std::string value_str = line.substr(eq_pos + 1);

            // Remove whitespace
            key.erase(key.find_last_not_of(" \t") + 1);
            value_str.erase(0, value_str.find_first_not_of(" \t"));

            // Try to parse value
            auto value = parseFromString(value_str, "auto");
            if (value) {
                setValue(current_section, key, *value);
            }
        }
    }

    return true;
}

auto ConfigurationManager::generateConfigFile() -> std::string {
    std::stringstream ss;
    ss << "# ASCOM Dome Configuration File\n";
    ss << "# Generated by Lithium-Next\n\n";

    for (const auto& [section_name, section] : config_sections_) {
        ss << "[" << section_name << "]\n";
        if (!section.description.empty()) {
            ss << "# " << section.description << "\n";
        }

        for (const auto& [key, value] : section.values) {
            ss << key << " = " << convertToString(value) << "\n";
        }
        ss << "\n";
    }

    return ss.str();
}

auto ConfigurationManager::validateValue(const std::string& section, const std::string& key, const ConfigValue& value) -> bool {
    auto section_validators = validators_.find(section);
    if (section_validators != validators_.end()) {
        auto validator = section_validators->second.find(key);
        if (validator != section_validators->second.end()) {
            return validator->second(value);
        }
    }
    return true; // No validator means any value is valid
}

auto ConfigurationManager::convertToString(const ConfigValue& value) -> std::string {
    return std::visit([](const auto& v) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<decltype(v)>, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_same_v<std::decay_t<decltype(v)>, std::string>) {
            return v;
        } else {
            return std::to_string(v);
        }
    }, value);
}

auto ConfigurationManager::parseFromString(const std::string& str, const std::string& type) -> std::optional<ConfigValue> {
    // Try to auto-detect type
    if (str == "true" || str == "false") {
        return str == "true";
    }

    // Try integer
    try {
        size_t pos;
        int int_val = std::stoi(str, &pos);
        if (pos == str.length()) {
            return int_val;
        }
    } catch (...) {}

    // Try double
    try {
        size_t pos;
        double double_val = std::stod(str, &pos);
        if (pos == str.length()) {
            return double_val;
        }
    } catch (...) {}

    // Default to string
    return str;
}

// Placeholder implementations for preset and validation methods
auto ConfigurationManager::savePreset(const std::string& name, const std::string& description) -> bool {
    // TODO: Implement preset saving
    return false;
}

auto ConfigurationManager::loadPreset(const std::string& name) -> bool {
    // TODO: Implement preset loading
    return false;
}

auto ConfigurationManager::deletePreset(const std::string& name) -> bool {
    // TODO: Implement preset deletion
    return false;
}

auto ConfigurationManager::getPresetNames() -> std::vector<std::string> {
    // TODO: Implement preset enumeration
    return {};
}

auto ConfigurationManager::validateConfiguration() -> std::vector<std::string> {
    // TODO: Implement configuration validation
    return {};
}

auto ConfigurationManager::setValidator(const std::string& section, const std::string& key,
                                       std::function<bool(const ConfigValue&)> validator) -> bool {
    validators_[section][key] = validator;
    return true;
}

auto ConfigurationManager::isDefaultValue(const std::string& section, const std::string& key) -> bool {
    // TODO: Implement default value checking
    return false;
}

} // namespace lithium::ascom::dome::components
