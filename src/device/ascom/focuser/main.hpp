/*
 * main.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Module Entry Point

This file provides the main entry point and factory functions
for the modular ASCOM focuser implementation.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>

#include "controller.hpp"
#include "device/template/focuser.hpp"

namespace lithium::device::ascom::focuser {

/**
 * @brief Module information structure
 */
struct ModuleInfo {
    std::string name = "ASCOM Focuser";
    std::string version = "1.0.0";
    std::string description = "Lithium ASCOM Focuser Driver";
    std::string author = "Max Qian";
    std::string contact = "lightapt.com";
    std::string license = "MIT";
    std::vector<std::string> supportedDevices;
    std::map<std::string, std::string> capabilities;
};

/**
 * @brief Device discovery result
 */
struct DeviceInfo {
    std::string name;
    std::string identifier;
    std::string description;
    std::string manufacturer;
    std::string model;
    std::string serialNumber;
    std::string firmwareVersion;
    std::map<std::string, std::string> properties;
    bool isConnected = false;
    bool isAvailable = true;
};

/**
 * @brief Module factory class
 */
class ModuleFactory {
public:
    /**
     * @brief Get module information
     */
    static auto getModuleInfo() -> ModuleInfo;
    
    /**
     * @brief Create a new focuser controller instance
     */
    static auto createController(const std::string& name = "ASCOM Focuser") -> std::shared_ptr<Controller>;
    
    /**
     * @brief Create a focuser instance with configuration
     */
    static auto createController(const std::string& name, const ControllerConfig& config) -> std::shared_ptr<Controller>;
    
    /**
     * @brief Discover available ASCOM focuser devices
     */
    static auto discoverDevices() -> std::vector<DeviceInfo>;
    
    /**
     * @brief Check if a device is supported
     */
    static auto isDeviceSupported(const std::string& deviceName) -> bool;
    
    /**
     * @brief Get supported device list
     */
    static auto getSupportedDevices() -> std::vector<std::string>;
    
    /**
     * @brief Get device capabilities
     */
    static auto getDeviceCapabilities(const std::string& deviceName) -> std::map<std::string, std::string>;
    
    /**
     * @brief Validate device configuration
     */
    static auto validateConfiguration(const ControllerConfig& config) -> bool;
    
    /**
     * @brief Get default configuration
     */
    static auto getDefaultConfiguration() -> ControllerConfig;
};

/**
 * @brief Module initialization and cleanup
 */
class ModuleManager {
public:
    /**
     * @brief Initialize the module
     */
    static auto initialize() -> bool;
    
    /**
     * @brief Cleanup the module
     */
    static auto cleanup() -> void;
    
    /**
     * @brief Check if module is initialized
     */
    static auto isInitialized() -> bool;
    
    /**
     * @brief Get module version
     */
    static auto getVersion() -> std::string;
    
    /**
     * @brief Get module build info
     */
    static auto getBuildInfo() -> std::map<std::string, std::string>;
    
    /**
     * @brief Register module with the system
     */
    static auto registerModule() -> bool;
    
    /**
     * @brief Unregister module from the system
     */
    static auto unregisterModule() -> void;
    
    /**
     * @brief Get active controller instances
     */
    static auto getActiveControllers() -> std::vector<std::shared_ptr<Controller>>;
    
    /**
     * @brief Get controller by name
     */
    static auto getController(const std::string& name) -> std::shared_ptr<Controller>;
    
    /**
     * @brief Register controller instance
     */
    static auto registerController(std::shared_ptr<Controller> controller) -> bool;
    
    /**
     * @brief Unregister controller instance
     */
    static auto unregisterController(const std::string& name) -> bool;
    
    /**
     * @brief Get module statistics
     */
    static auto getModuleStatistics() -> std::map<std::string, std::string>;
    
    /**
     * @brief Enable/disable module logging
     */
    static auto enableLogging(bool enable) -> void;
    
    /**
     * @brief Check if logging is enabled
     */
    static auto isLoggingEnabled() -> bool;
    
    /**
     * @brief Set log level
     */
    static auto setLogLevel(int level) -> void;
    
    /**
     * @brief Get log level
     */
    static auto getLogLevel() -> int;

private:
    static bool initialized_;
    static std::vector<std::shared_ptr<Controller>> controllers_;
    static std::map<std::string, std::shared_ptr<Controller>> controller_map_;
    static bool logging_enabled_;
    static int log_level_;
    static std::mutex controllers_mutex_;
};

/**
 * @brief Legacy compatibility wrapper
 */
class LegacyWrapper {
public:
    /**
     * @brief Create legacy ASCOM focuser instance
     */
    static auto createLegacyFocuser(const std::string& name) -> std::shared_ptr<AtomFocuser>;
    
    /**
     * @brief Convert controller to legacy interface
     */
    static auto wrapController(std::shared_ptr<Controller> controller) -> std::shared_ptr<AtomFocuser>;
    
    /**
     * @brief Check if legacy mode is enabled
     */
    static auto isLegacyModeEnabled() -> bool;
    
    /**
     * @brief Enable/disable legacy mode
     */
    static auto enableLegacyMode(bool enable) -> void;
    
    /**
     * @brief Get legacy interface version
     */
    static auto getLegacyVersion() -> std::string;
    
    /**
     * @brief Get legacy compatibility information
     */
    static auto getLegacyCompatibility() -> std::map<std::string, std::string>;
};

/**
 * @brief Module configuration management
 */
class ConfigManager {
public:
    /**
     * @brief Load configuration from file
     */
    static auto loadConfiguration(const std::string& filename) -> bool;
    
    /**
     * @brief Save configuration to file
     */
    static auto saveConfiguration(const std::string& filename) -> bool;
    
    /**
     * @brief Get configuration value
     */
    static auto getConfigValue(const std::string& key) -> std::string;
    
    /**
     * @brief Set configuration value
     */
    static auto setConfigValue(const std::string& key, const std::string& value) -> bool;
    
    /**
     * @brief Get all configuration values
     */
    static auto getAllConfigValues() -> std::map<std::string, std::string>;
    
    /**
     * @brief Reset configuration to defaults
     */
    static auto resetToDefaults() -> bool;
    
    /**
     * @brief Validate configuration
     */
    static auto validateConfiguration() -> bool;
    
    /**
     * @brief Get configuration schema
     */
    static auto getConfigurationSchema() -> std::map<std::string, std::string>;

private:
    static std::map<std::string, std::string> config_values_;
    static std::mutex config_mutex_;
};

// Module export functions for C compatibility
extern "C" {
    /**
     * @brief Get module information (C interface)
     */
    const char* lithium_ascom_focuser_get_module_info();
    
    /**
     * @brief Create focuser instance (C interface)
     */
    void* lithium_ascom_focuser_create(const char* name);
    
    /**
     * @brief Destroy focuser instance (C interface)
     */
    void lithium_ascom_focuser_destroy(void* instance);
    
    /**
     * @brief Initialize module (C interface)
     */
    int lithium_ascom_focuser_initialize();
    
    /**
     * @brief Cleanup module (C interface)
     */
    void lithium_ascom_focuser_cleanup();
    
    /**
     * @brief Get version (C interface)
     */
    const char* lithium_ascom_focuser_get_version();
    
    /**
     * @brief Discover devices (C interface)
     */
    int lithium_ascom_focuser_discover_devices(char** devices, int max_devices);
    
    /**
     * @brief Check device support (C interface)
     */
    int lithium_ascom_focuser_is_device_supported(const char* device_name);
}

} // namespace lithium::device::ascom::focuser
