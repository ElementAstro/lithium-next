/*
 * device_config.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Device Configuration System

*************************************************/

#pragma once

#include "device_factory.hpp"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <optional>

// Device configuration structure
struct DeviceConfiguration {
    std::string name;
    DeviceType type;
    DeviceBackend backend;
    std::string driver;
    std::string port;
    int timeout{5000};
    int maxRetry{3};
    bool autoConnect{false};
    bool simulationMode{false};
    nlohmann::json parameters;

    // Serialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DeviceConfiguration,
        name, type, backend, driver, port, timeout, maxRetry,
        autoConnect, simulationMode, parameters)
};

// Device profile - collection of devices for a specific setup
struct DeviceProfile {
    std::string name;
    std::string description;
    std::vector<DeviceConfiguration> devices;
    nlohmann::json globalSettings;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(DeviceProfile,
        name, description, devices, globalSettings)
};

class DeviceConfigManager {
public:
    static DeviceConfigManager& getInstance() {
        static DeviceConfigManager instance;
        return instance;
    }

    // Configuration file management
    bool loadConfiguration(const std::string& filePath);
    bool saveConfiguration(const std::string& filePath) const;
    bool loadProfile(const std::string& profileName);
    bool saveProfile(const std::string& profileName) const;

    // Device configuration management
    bool addDeviceConfig(const DeviceConfiguration& config);
    bool removeDeviceConfig(const std::string& deviceName);
    std::optional<DeviceConfiguration> getDeviceConfig(const std::string& deviceName) const;
    std::vector<DeviceConfiguration> getAllDeviceConfigs() const;
    bool updateDeviceConfig(const std::string& deviceName, const DeviceConfiguration& config);

    // Profile management
    bool addProfile(const DeviceProfile& profile);
    bool removeProfile(const std::string& profileName);
    std::optional<DeviceProfile> getProfile(const std::string& profileName) const;
    std::vector<std::string> getAvailableProfiles() const;
    bool setActiveProfile(const std::string& profileName);
    std::string getActiveProfile() const;

    // Device creation from configuration
    std::unique_ptr<AtomDriver> createDeviceFromConfig(const std::string& deviceName);
    std::vector<std::unique_ptr<AtomDriver>> createAllDevicesFromActiveProfile();

    // Configuration validation
    bool validateConfiguration(const DeviceConfiguration& config) const;
    bool validateProfile(const DeviceProfile& profile) const;
    std::vector<std::string> getConfigurationErrors(const DeviceConfiguration& config) const;

    // Default configurations
    DeviceConfiguration createDefaultCameraConfig(const std::string& name = "Camera") const;
    DeviceConfiguration createDefaultTelescopeConfig(const std::string& name = "Telescope") const;
    DeviceConfiguration createDefaultFocuserConfig(const std::string& name = "Focuser") const;
    DeviceConfiguration createDefaultFilterWheelConfig(const std::string& name = "FilterWheel") const;
    DeviceConfiguration createDefaultRotatorConfig(const std::string& name = "Rotator") const;
    DeviceConfiguration createDefaultDomeConfig(const std::string& name = "Dome") const;

    // Configuration templates
    std::vector<DeviceConfiguration> getConfigTemplates(DeviceType type) const;
    DeviceProfile createMockProfile() const;
    DeviceProfile createINDIProfile() const;

    // Global settings
    void setGlobalSetting(const std::string& key, const nlohmann::json& value);
    nlohmann::json getGlobalSetting(const std::string& key) const;
    nlohmann::json getAllGlobalSettings() const;

private:
    DeviceConfigManager() = default;
    ~DeviceConfigManager() = default;

    // Disable copy and assignment
    DeviceConfigManager(const DeviceConfigManager&) = delete;
    DeviceConfigManager& operator=(const DeviceConfigManager&) = delete;

    // Internal data
    std::vector<DeviceConfiguration> device_configs_;
    std::vector<DeviceProfile> profiles_;
    std::string active_profile_;
    nlohmann::json global_settings_;

    // Helper methods
    std::vector<DeviceConfiguration>::iterator findDeviceConfig(const std::string& deviceName);
    std::vector<DeviceProfile>::iterator findProfile(const std::string& profileName);
    void applyConfigurationToDevice(AtomDriver* device, const DeviceConfiguration& config) const;
};

// JSON serialization for enums
NLOHMANN_JSON_SERIALIZE_ENUM(DeviceType, {
    {DeviceType::UNKNOWN, "unknown"},
    {DeviceType::CAMERA, "camera"},
    {DeviceType::TELESCOPE, "telescope"},
    {DeviceType::FOCUSER, "focuser"},
    {DeviceType::FILTERWHEEL, "filterwheel"},
    {DeviceType::ROTATOR, "rotator"},
    {DeviceType::DOME, "dome"},
    {DeviceType::GUIDER, "guider"},
    {DeviceType::WEATHER_STATION, "weather"},
    {DeviceType::SAFETY_MONITOR, "safety"},
    {DeviceType::ADAPTIVE_OPTICS, "ao"}
})

NLOHMANN_JSON_SERIALIZE_ENUM(DeviceBackend, {
    {DeviceBackend::MOCK, "mock"},
    {DeviceBackend::INDI, "indi"},
    {DeviceBackend::ASCOM, "ascom"},
    {DeviceBackend::NATIVE, "native"}
})
