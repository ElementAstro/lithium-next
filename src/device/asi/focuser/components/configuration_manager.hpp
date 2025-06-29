/*
 * configuration_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Configuration Manager Component
Handles settings storage, loading, and management

*************************************************/

#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace lithium::device::asi::focuser::components {

// Forward declarations
class HardwareInterface;
class PositionManager;
class TemperatureSystem;

/**
 * @brief Configuration management for ASI Focuser
 *
 * This component handles saving and loading focuser settings,
 * managing device profiles, and configuration validation.
 */
class ConfigurationManager {
public:
    ConfigurationManager(HardwareInterface* hardware,
                         PositionManager* positionManager,
                         TemperatureSystem* temperatureSystem);
    ~ConfigurationManager();

    // Non-copyable and non-movable
    ConfigurationManager(const ConfigurationManager&) = delete;
    ConfigurationManager& operator=(const ConfigurationManager&) = delete;
    ConfigurationManager(ConfigurationManager&&) = delete;
    ConfigurationManager& operator=(ConfigurationManager&&) = delete;

    // Configuration management
    bool saveConfiguration(const std::string& filename);
    bool loadConfiguration(const std::string& filename);
    bool saveDeviceProfile(const std::string& deviceName);
    bool loadDeviceProfile(const std::string& deviceName);

    // Hardware settings
    bool enableBeep(bool enable);
    bool isBeepEnabled() const { return beepEnabled_; }
    bool enableHighResolutionMode(bool enable);
    bool isHighResolutionMode() const { return highResolutionMode_; }
    double getResolution() const { return stepResolution_; }

    // Backlash settings
    bool enableBacklashCompensation(bool enable);
    bool isBacklashCompensationEnabled() const { return backlashEnabled_; }
    bool setBacklashSteps(int steps);
    int getBacklashSteps() const { return backlashSteps_; }

    // Configuration validation
    bool validateConfiguration() const;
    std::string getLastError() const { return lastError_; }

    // Default configurations
    bool resetToDefaults();
    bool createDefaultProfile(const std::string& deviceName);

    // Profile management
    std::vector<std::string> getAvailableProfiles() const;
    bool deleteProfile(const std::string& profileName);

    // Settings access
    void setConfigValue(const std::string& key, const std::string& value);
    std::string getConfigValue(const std::string& key,
                               const std::string& defaultValue = "") const;

private:
    // Dependencies
    HardwareInterface* hardware_;
    PositionManager* positionManager_;
    TemperatureSystem* temperatureSystem_;

    // Hardware settings
    bool beepEnabled_ = false;
    bool highResolutionMode_ = false;
    double stepResolution_ = 0.5;  // microns per step

    // Backlash settings
    bool backlashEnabled_ = false;
    int backlashSteps_ = 0;

    // Configuration storage
    std::map<std::string, std::string> configValues_;

    // Error tracking
    std::string lastError_;

    // Thread safety
    mutable std::mutex configMutex_;

    // Helper methods
    std::string getConfigDirectory() const;
    std::string getProfilePath(const std::string& profileName) const;
    bool parseConfigLine(const std::string& line, std::string& key,
                         std::string& value) const;
    bool applyConfiguration();
    void saveCurrentSettings();
    void loadDefaultSettings();
};

}  // namespace lithium::device::asi::focuser::components
