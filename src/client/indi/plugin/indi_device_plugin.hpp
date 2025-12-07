/*
 * indi_device_plugin.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Device Plugin - Implements IDevicePlugin for INDI backend

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_DEVICE_PLUGIN_HPP
#define LITHIUM_CLIENT_INDI_DEVICE_PLUGIN_HPP

#include "device/plugin/device_plugin_interface.hpp"

#include "../indi_device_factory.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace lithium::client::indi {

/**
 * @brief INDI Device Plugin
 *
 * Implements the IDevicePlugin interface for INDI devices. This plugin:
 * - Registers all INDI device types (Camera, Focuser, FilterWheel, etc.)
 * - Creates INDI device instances through the factory
 * - Supports device discovery via INDI server connection
 * - Supports hot-plug operations
 */
class INDIDevicePlugin : public device::DevicePluginBase {
public:
    /**
     * @brief Default plugin name
     */
    static constexpr const char* PLUGIN_NAME = "INDI";

    /**
     * @brief Plugin version
     */
    static constexpr const char* PLUGIN_VERSION = "1.0.0";

    /**
     * @brief Construct INDI device plugin
     */
    INDIDevicePlugin();

    /**
     * @brief Destructor
     */
    ~INDIDevicePlugin() override;

    // ==================== IPlugin Interface ====================

    /**
     * @brief Get plugin name
     */
    [[nodiscard]] auto getName() const -> std::string override {
        return PLUGIN_NAME;
    }

    /**
     * @brief Get plugin version
     */
    [[nodiscard]] auto getVersion() const -> std::string override {
        return PLUGIN_VERSION;
    }

    /**
     * @brief Initialize the plugin
     */
    auto initialize() -> bool override;

    /**
     * @brief Shutdown the plugin
     */
    void shutdown() override;

    // ==================== IDevicePlugin Interface ====================

    /**
     * @brief Get device metadata
     */
    [[nodiscard]] auto getDeviceMetadata() const
        -> device::DevicePluginMetadata override;

    /**
     * @brief Get device types provided by this plugin
     */
    [[nodiscard]] auto getDeviceTypes() const
        -> std::vector<device::DeviceTypeInfo> override;

    /**
     * @brief Register device types with the registry
     */
    auto registerDeviceTypes(device::DeviceTypeRegistry& registry)
        -> device::DeviceResult<size_t> override;

    /**
     * @brief Register device creators with the factory
     */
    void registerDeviceCreators(device::DeviceFactory& factory) override;

    /**
     * @brief Unregister device creators from the factory
     */
    void unregisterDeviceCreators(device::DeviceFactory& factory) override;

    /**
     * @brief Check if this plugin has a backend
     */
    [[nodiscard]] auto hasBackend() const -> bool override { return true; }

    /**
     * @brief Create backend instance (INDI server connection)
     */
    auto createBackend() -> std::shared_ptr<DeviceBackend> override;

    /**
     * @brief Check if this plugin supports hot-plug
     */
    [[nodiscard]] auto supportsHotPlug() const -> bool override { return true; }

    /**
     * @brief Prepare for hot-plug (save device states)
     */
    auto prepareHotPlug()
        -> device::DeviceResult<std::vector<device::DeviceMigrationContext>>
            override;

    /**
     * @brief Complete hot-plug (restore device states)
     */
    auto completeHotPlug(
        const std::vector<device::DeviceMigrationContext>& contexts)
        -> device::DeviceResult<bool> override;

    /**
     * @brief Discover devices from INDI server
     */
    auto discoverDevices()
        -> device::DeviceResult<std::vector<device::DiscoveredDevice>> override;

    /**
     * @brief Get discovered devices
     */
    [[nodiscard]] auto getDiscoveredDevices() const
        -> std::vector<device::DiscoveredDevice> override;

    /**
     * @brief Get last error
     */
    [[nodiscard]] auto getLastError() const -> std::string override;

    /**
     * @brief Check if backend is running
     */
    [[nodiscard]] auto isBackendRunning() const -> bool override;

    /**
     * @brief Check health status
     */
    [[nodiscard]] auto isHealthy() const -> bool override;

    // ==================== INDI-Specific Methods ====================

    /**
     * @brief Set INDI server connection parameters
     * @param host Server hostname
     * @param port Server port
     */
    void setServerConnection(const std::string& host, int port);

    /**
     * @brief Get the INDI device factory
     */
    auto getDeviceFactory() -> INDIDeviceFactory& {
        return INDIDeviceFactory::getInstance();
    }

    /**
     * @brief Get the INDI device manager
     */
    auto getDeviceManager() -> INDIDeviceManager& { return deviceManager_; }

private:
    /**
     * @brief Build device type info for a given INDI device type
     */
    auto buildTypeInfo(DeviceType type) const -> device::DeviceTypeInfo;

    /**
     * @brief Create device creator function for the factory
     */
    auto createDeviceCreator(DeviceType type) -> device::DeviceFactory::Creator;

    /**
     * @brief Convert INDI device type to string for registration
     */
    static auto getRegistrationKey(DeviceType type) -> std::string;

    // Server connection
    std::string serverHost_{"localhost"};
    int serverPort_{7624};

    // Device management
    INDIDeviceManager deviceManager_;
    std::vector<device::DiscoveredDevice> discoveredDevices_;

    // State
    std::atomic<bool> backendRunning_{false};
    std::string lastError_;
    mutable std::mutex mutex_;
};

/**
 * @brief Plugin entry point for dynamic loading
 */
extern "C" {

/**
 * @brief Create plugin instance
 */
LITHIUM_EXPORT auto createDevicePlugin() -> device::IDevicePlugin*;

/**
 * @brief Destroy plugin instance
 */
LITHIUM_EXPORT void destroyDevicePlugin(device::IDevicePlugin* plugin);

/**
 * @brief Get plugin metadata
 */
LITHIUM_EXPORT auto getDevicePluginMetadata() -> device::DevicePluginMetadata;
}

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_DEVICE_PLUGIN_HPP
