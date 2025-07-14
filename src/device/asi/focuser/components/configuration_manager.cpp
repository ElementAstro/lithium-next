/*
 * configuration_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Configuration Manager Implementation

*************************************************/

#include "configuration_manager.hpp"

#include "hardware_interface.hpp"
#include "position_manager.hpp"
#include "temperature_system.hpp"

#include <filesystem>
#include <fstream>

#include <spdlog/spdlog.h>

namespace lithium::device::asi::focuser::components {

ConfigurationManager::ConfigurationManager(HardwareInterface* hardware,
                                           PositionManager* positionManager,
                                           TemperatureSystem* temperatureSystem)
    : hardware_(hardware),
      positionManager_(positionManager),
      temperatureSystem_(temperatureSystem) {
    spdlog::info("Created ASI Focuser Configuration Manager");
}

ConfigurationManager::~ConfigurationManager() {
    spdlog::info("Destroyed ASI Focuser Configuration Manager");
}

bool ConfigurationManager::saveConfiguration(const std::string& filename) {
    std::lock_guard<std::mutex> lock(configMutex_);

    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            lastError_ = "Could not open file for writing: " + filename;
            return false;
        }

        // Save current settings to config values
        saveCurrentSettings();

        file << "# ASI Focuser Configuration\n";
        file << "# Generated automatically - do not edit manually\n\n";

        // Save all configuration values
        for (const auto& [key, value] : configValues_) {
            file << key << "=" << value << "\n";
        }

        spdlog::info("Configuration saved to: {}", filename);
        return true;

    } catch (const std::exception& e) {
        lastError_ = "Failed to save configuration: " + std::string(e.what());
        spdlog::error("Failed to save configuration: {}", e.what());
        return false;
    }
}

bool ConfigurationManager::loadConfiguration(const std::string& filename) {
    std::lock_guard<std::mutex> lock(configMutex_);

    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            lastError_ = "Could not open file for reading: " + filename;
            return false;
        }

        configValues_.clear();

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::string key, value;
            if (parseConfigLine(line, key, value)) {
                configValues_[key] = value;
            }
        }

        // Apply loaded configuration
        if (!applyConfiguration()) {
            return false;
        }

        spdlog::info("Configuration loaded from: {}", filename);
        return true;

    } catch (const std::exception& e) {
        lastError_ = "Failed to load configuration: " + std::string(e.what());
        spdlog::error("Failed to load configuration: {}", e.what());
        return false;
    }
}

bool ConfigurationManager::saveDeviceProfile(const std::string& deviceName) {
    std::string profilePath = getProfilePath(deviceName);
    return saveConfiguration(profilePath);
}

bool ConfigurationManager::loadDeviceProfile(const std::string& deviceName) {
    std::string profilePath = getProfilePath(deviceName);
    return loadConfiguration(profilePath);
}

bool ConfigurationManager::enableBeep(bool enable) {
    beepEnabled_ = enable;
    spdlog::info("Beep {}", enable ? "enabled" : "disabled");
    return true;
}

bool ConfigurationManager::enableHighResolutionMode(bool enable) {
    highResolutionMode_ = enable;
    if (enable) {
        stepResolution_ = 0.1;  // Higher resolution
    } else {
        stepResolution_ = 0.5;  // Standard resolution
    }
    spdlog::info("High resolution mode {}, step resolution: {:.1f} µm",
                 enable ? "enabled" : "disabled", stepResolution_);
    return true;
}

bool ConfigurationManager::enableBacklashCompensation(bool enable) {
    backlashEnabled_ = enable;

    // Apply to hardware if connected
    if (hardware_ && hardware_->isConnected()) {
        if (enable && backlashSteps_ > 0) {
            hardware_->setBacklash(backlashSteps_);
        }
    }

    spdlog::info("Backlash compensation {}", enable ? "enabled" : "disabled");
    return true;
}

bool ConfigurationManager::setBacklashSteps(int steps) {
    if (steps < 0 || steps > 999) {
        return false;
    }

    backlashSteps_ = steps;

    // Apply to hardware if connected and enabled
    if (hardware_ && hardware_->isConnected() && backlashEnabled_) {
        hardware_->setBacklash(steps);
    }

    spdlog::info("Set backlash steps to: {}", steps);
    return true;
}

bool ConfigurationManager::validateConfiguration() const {
    // Validate position limits
    if (positionManager_) {
        if (positionManager_->getMinLimit() >=
            positionManager_->getMaxLimit()) {
            return false;
        }
    }

    // Validate temperature coefficient
    if (temperatureSystem_) {
        double coeff = temperatureSystem_->getTemperatureCoefficient();
        if (std::abs(coeff) > 1000.0) {  // Reasonable limit
            return false;
        }
    }

    // Validate backlash settings
    if (backlashSteps_ < 0 || backlashSteps_ > 999) {
        return false;
    }

    return true;
}

bool ConfigurationManager::resetToDefaults() {
    std::lock_guard<std::mutex> lock(configMutex_);

    spdlog::info("Resetting to default configuration");

    loadDefaultSettings();

    if (!applyConfiguration()) {
        spdlog::error("Failed to apply default configuration");
        return false;
    }

    spdlog::info("Reset to defaults completed");
    return true;
}

bool ConfigurationManager::createDefaultProfile(const std::string& deviceName) {
    resetToDefaults();
    return saveDeviceProfile(deviceName);
}

std::vector<std::string> ConfigurationManager::getAvailableProfiles() const {
    std::vector<std::string> profiles;

    try {
        std::string configDir = getConfigDirectory();
        if (!std::filesystem::exists(configDir)) {
            return profiles;
        }

        for (const auto& entry :
             std::filesystem::directory_iterator(configDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (filename.ends_with(".cfg")) {
                    profiles.push_back(
                        filename.substr(0, filename.length() - 4));
                }
            }
        }

    } catch (const std::exception& e) {
        spdlog::error("Failed to get available profiles: {}", e.what());
    }

    return profiles;
}

bool ConfigurationManager::deleteProfile(const std::string& profileName) {
    try {
        std::string profilePath = getProfilePath(profileName);
        if (std::filesystem::exists(profilePath)) {
            std::filesystem::remove(profilePath);
            spdlog::info("Deleted profile: {}", profileName);
            return true;
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to delete profile {}: {}", profileName, e.what());
    }

    return false;
}

void ConfigurationManager::setConfigValue(const std::string& key,
                                          const std::string& value) {
    std::lock_guard<std::mutex> lock(configMutex_);
    configValues_[key] = value;
}

std::string ConfigurationManager::getConfigValue(
    const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(configMutex_);
    auto it = configValues_.find(key);
    return (it != configValues_.end()) ? it->second : defaultValue;
}

std::string ConfigurationManager::getConfigDirectory() const {
    // Create config directory in user's home
    std::string homeDir = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
    std::string configDir = homeDir + "/.lithium/focuser/asi";

    try {
        std::filesystem::create_directories(configDir);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create config directory: {}", e.what());
    }

    return configDir;
}

std::string ConfigurationManager::getProfilePath(
    const std::string& profileName) const {
    return getConfigDirectory() + "/" + profileName + ".cfg";
}

bool ConfigurationManager::parseConfigLine(const std::string& line,
                                           std::string& key,
                                           std::string& value) const {
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
        return false;
    }

    key = line.substr(0, pos);
    value = line.substr(pos + 1);

    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);

    return !key.empty();
}

bool ConfigurationManager::applyConfiguration() {
    try {
        // Apply position manager settings
        if (positionManager_) {
            if (auto value = getConfigValue("maxPosition"); !value.empty()) {
                positionManager_->setMaxLimit(std::stoi(value));
            }
            if (auto value = getConfigValue("minPosition"); !value.empty()) {
                positionManager_->setMinLimit(std::stoi(value));
            }
            if (auto value = getConfigValue("currentSpeed"); !value.empty()) {
                positionManager_->setSpeed(std::stod(value));
            }
            if (auto value = getConfigValue("directionReversed");
                !value.empty()) {
                positionManager_->setDirection(value == "true");
            }
        }

        // Apply temperature system settings
        if (temperatureSystem_) {
            if (auto value = getConfigValue("temperatureCoefficient");
                !value.empty()) {
                temperatureSystem_->setTemperatureCoefficient(std::stod(value));
            }
            if (auto value = getConfigValue("temperatureCompensationEnabled");
                !value.empty()) {
                temperatureSystem_->enableTemperatureCompensation(value ==
                                                                  "true");
            }
        }

        // Apply configuration manager settings
        if (auto value = getConfigValue("backlashSteps"); !value.empty()) {
            setBacklashSteps(std::stoi(value));
        }
        if (auto value = getConfigValue("backlashEnabled"); !value.empty()) {
            enableBacklashCompensation(value == "true");
        }
        if (auto value = getConfigValue("beepEnabled"); !value.empty()) {
            enableBeep(value == "true");
        }
        if (auto value = getConfigValue("highResolutionMode"); !value.empty()) {
            enableHighResolutionMode(value == "true");
        }

        return true;

    } catch (const std::exception& e) {
        lastError_ = "Failed to apply configuration: " + std::string(e.what());
        spdlog::error("Failed to apply configuration: {}", e.what());
        return false;
    }
}

void ConfigurationManager::saveCurrentSettings() {
    // Save position manager settings
    if (positionManager_) {
        configValues_["maxPosition"] =
            std::to_string(positionManager_->getMaxLimit());
        configValues_["minPosition"] =
            std::to_string(positionManager_->getMinLimit());
        configValues_["currentSpeed"] =
            std::to_string(positionManager_->getSpeed());
        configValues_["directionReversed"] =
            positionManager_->isDirectionReversed() ? "true" : "false";
    }

    // Save temperature system settings
    if (temperatureSystem_) {
        configValues_["temperatureCoefficient"] =
            std::to_string(temperatureSystem_->getTemperatureCoefficient());
        configValues_["temperatureCompensationEnabled"] =
            temperatureSystem_->isTemperatureCompensationEnabled() ? "true"
                                                                   : "false";
    }

    // Save configuration manager settings
    configValues_["backlashSteps"] = std::to_string(backlashSteps_);
    configValues_["backlashEnabled"] = backlashEnabled_ ? "true" : "false";
    configValues_["beepEnabled"] = beepEnabled_ ? "true" : "false";
    configValues_["highResolutionMode"] =
        highResolutionMode_ ? "true" : "false";
    configValues_["stepResolution"] = std::to_string(stepResolution_);
}

void ConfigurationManager::loadDefaultSettings() {
    configValues_.clear();

    // Default position settings
    configValues_["maxPosition"] = "30000";
    configValues_["minPosition"] = "0";
    configValues_["currentSpeed"] = "300.0";
    configValues_["directionReversed"] = "false";

    // Default temperature settings
    configValues_["temperatureCoefficient"] = "0.0";
    configValues_["temperatureCompensationEnabled"] = "false";

    // Default configuration settings
    configValues_["backlashSteps"] = "0";
    configValues_["backlashEnabled"] = "false";
    configValues_["beepEnabled"] = "false";
    configValues_["highResolutionMode"] = "false";
    configValues_["stepResolution"] = "0.5";
}

}  // namespace lithium::device::asi::focuser::components
