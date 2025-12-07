/*
 * indi_device_plugin.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "indi_device_plugin.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ============================================================================
// Construction / Destruction
// ============================================================================

INDIDevicePlugin::INDIDevicePlugin() {
    LOG_DEBUG("INDIDevicePlugin created");
}

INDIDevicePlugin::~INDIDevicePlugin() {
    if (state_ != device::DevicePluginState::Unloaded) {
        shutdown();
    }
    LOG_DEBUG("INDIDevicePlugin destroyed");
}

// ============================================================================
// IPlugin Interface
// ============================================================================

auto INDIDevicePlugin::initialize() -> bool {
    std::lock_guard lock(mutex_);

    if (state_ != device::DevicePluginState::Unloaded) {
        lastError_ = "Plugin already initialized";
        return false;
    }

    LOG_INFO("Initializing INDI device plugin");

    state_ = device::DevicePluginState::Initializing;

    // Register default INDI device creators
    // The factory is a singleton and maintains its own state

    state_ = device::DevicePluginState::Ready;
    LOG_INFO("INDI device plugin initialized successfully");

    return true;
}

void INDIDevicePlugin::shutdown() {
    std::lock_guard lock(mutex_);

    LOG_INFO("Shutting down INDI device plugin");

    state_ = device::DevicePluginState::Unloading;

    // Disconnect and clear all devices
    deviceManager_.disconnectAll();
    deviceManager_.clear();
    discoveredDevices_.clear();

    backendRunning_ = false;
    state_ = device::DevicePluginState::Unloaded;

    LOG_INFO("INDI device plugin shutdown complete");
}

// ============================================================================
// IDevicePlugin Interface
// ============================================================================

auto INDIDevicePlugin::getDeviceMetadata() const
    -> device::DevicePluginMetadata {
    device::DevicePluginMetadata metadata;
    metadata.name = PLUGIN_NAME;
    metadata.version = PLUGIN_VERSION;
    metadata.description =
        "INDI (Instrument Neutral Distributed Interface) device driver plugin";
    metadata.author = "Max Qian <lightapt.com>";
    metadata.license = "GPL-3.0";
    metadata.backendName = "INDI";
    metadata.backendVersion = "1.9+";
    metadata.supportsHotPlug = true;
    metadata.supportsAutoDiscovery = true;
    metadata.requiresServer = true;
    metadata.supportedDeviceCategories = {"Camera",      "Focuser",
                                          "FilterWheel", "Telescope",
                                          "Rotator",     "Dome",
                                          "Weather",     "GPS"};
    metadata.tags = {"indi", "astronomy", "device-control"};
    metadata.capabilities["server_discovery"] = true;
    metadata.capabilities["device_profiles"] = true;
    metadata.capabilities["property_vectors"] = true;

    return metadata;
}

auto INDIDevicePlugin::getDeviceTypes() const
    -> std::vector<device::DeviceTypeInfo> {
    std::vector<device::DeviceTypeInfo> types;

    types.push_back(buildTypeInfo(DeviceType::Camera));
    types.push_back(buildTypeInfo(DeviceType::Focuser));
    types.push_back(buildTypeInfo(DeviceType::FilterWheel));
    types.push_back(buildTypeInfo(DeviceType::Telescope));
    types.push_back(buildTypeInfo(DeviceType::Rotator));
    types.push_back(buildTypeInfo(DeviceType::Dome));
    types.push_back(buildTypeInfo(DeviceType::Weather));
    types.push_back(buildTypeInfo(DeviceType::GPS));

    return types;
}

auto INDIDevicePlugin::registerDeviceTypes(device::DeviceTypeRegistry& registry)
    -> device::DeviceResult<size_t> {
    size_t registered = 0;

    auto types = getDeviceTypes();
    for (const auto& type : types) {
        if (registry.registerType(type)) {
            ++registered;
            LOG_DEBUG("Registered INDI device type: {}", type.typeName);
        } else {
            LOG_WARNING("Failed to register INDI device type: {}",
                        type.typeName);
        }
    }

    LOG_INFO("Registered {} INDI device types", registered);
    return registered;
}

void INDIDevicePlugin::registerDeviceCreators(device::DeviceFactory& factory) {
    LOG_INFO("Registering INDI device creators");

    // Register creators for each device type
    factory.registerCreator("INDI:Camera", createDeviceCreator(DeviceType::Camera));
    factory.registerCreator("INDI:Focuser",
                            createDeviceCreator(DeviceType::Focuser));
    factory.registerCreator("INDI:FilterWheel",
                            createDeviceCreator(DeviceType::FilterWheel));
    factory.registerCreator("INDI:Telescope",
                            createDeviceCreator(DeviceType::Telescope));
    factory.registerCreator("INDI:Rotator",
                            createDeviceCreator(DeviceType::Rotator));
    factory.registerCreator("INDI:Dome", createDeviceCreator(DeviceType::Dome));
    factory.registerCreator("INDI:Weather",
                            createDeviceCreator(DeviceType::Weather));
    factory.registerCreator("INDI:GPS", createDeviceCreator(DeviceType::GPS));

    LOG_INFO("INDI device creators registered");
}

void INDIDevicePlugin::unregisterDeviceCreators(device::DeviceFactory& factory) {
    LOG_INFO("Unregistering INDI device creators");

    factory.unregisterCreator("INDI:Camera");
    factory.unregisterCreator("INDI:Focuser");
    factory.unregisterCreator("INDI:FilterWheel");
    factory.unregisterCreator("INDI:Telescope");
    factory.unregisterCreator("INDI:Rotator");
    factory.unregisterCreator("INDI:Dome");
    factory.unregisterCreator("INDI:Weather");
    factory.unregisterCreator("INDI:GPS");

    LOG_INFO("INDI device creators unregistered");
}

auto INDIDevicePlugin::createBackend() -> std::shared_ptr<DeviceBackend> {
    // For INDI, the "backend" is the INDI server connection
    // This would typically be managed by the INDIDeviceBase instances
    // For now, we return nullptr as INDI devices manage their own connection
    LOG_DEBUG("INDI backend creation - devices manage their own connections");
    return nullptr;
}

auto INDIDevicePlugin::prepareHotPlug()
    -> device::DeviceResult<std::vector<device::DeviceMigrationContext>> {
    std::lock_guard lock(mutex_);

    LOG_INFO("Preparing INDI devices for hot-plug");

    std::vector<device::DeviceMigrationContext> contexts;

    auto devices = deviceManager_.getDevices();
    for (const auto& device : devices) {
        device::DeviceMigrationContext ctx;
        ctx.deviceName = device->getName();
        ctx.deviceType = deviceTypeToString(device->getDeviceType());
        ctx.wasConnected = device->isConnected();

        // Save device-specific state
        ctx.state["server_host"] = serverHost_;
        ctx.state["server_port"] = serverPort_;

        // Additional device properties could be saved here

        contexts.push_back(std::move(ctx));

        LOG_DEBUG("Prepared migration context for INDI device: {}",
                  device->getName());
    }

    // Disconnect all devices
    deviceManager_.disconnectAll();

    LOG_INFO("INDI hot-plug preparation complete: {} devices prepared",
             contexts.size());

    return contexts;
}

auto INDIDevicePlugin::completeHotPlug(
    const std::vector<device::DeviceMigrationContext>& contexts)
    -> device::DeviceResult<bool> {
    std::lock_guard lock(mutex_);

    LOG_INFO("Completing INDI hot-plug for {} devices", contexts.size());

    size_t reconnected = 0;
    size_t failed = 0;

    for (const auto& ctx : contexts) {
        // Restore server connection settings
        if (ctx.state.contains("server_host")) {
            serverHost_ = ctx.state["server_host"].get<std::string>();
        }
        if (ctx.state.contains("server_port")) {
            serverPort_ = ctx.state["server_port"].get<int>();
        }

        // Reconnect if was connected
        if (ctx.wasConnected) {
            auto device = deviceManager_.getDevice(ctx.deviceName);
            if (device) {
                LOG_DEBUG("Reconnecting INDI device: {}", ctx.deviceName);

                try {
                    // Attempt to reconnect the device to INDI server
                    auto result = device->connect(serverHost_, serverPort_);
                    if (result.has_value() && result.value()) {
                        LOG_INFO("Successfully reconnected INDI device: {}",
                                 ctx.deviceName);
                        reconnected++;

                        // Restore additional device state if available
                        if (ctx.state.contains("device_properties")) {
                            auto properties = ctx.state["device_properties"];
                            // Apply saved properties to the device
                            for (const auto& [key, value] : properties.items()) {
                                device->setProperty(key, value);
                            }
                        }
                    } else {
                        LOG_WARNING("Failed to reconnect INDI device: {} - {}",
                                    ctx.deviceName,
                                    result.has_value() ? "connection refused"
                                                       : result.error().message);
                        failed++;
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception reconnecting INDI device {}: {}",
                              ctx.deviceName, e.what());
                    failed++;
                }
            } else {
                LOG_WARNING("Device {} not found in manager during hot-plug",
                            ctx.deviceName);
                failed++;
            }
        }
    }

    LOG_INFO("INDI hot-plug completion finished: {} reconnected, {} failed",
             reconnected, failed);

    if (failed > 0 && reconnected == 0) {
        return device::DeviceError{
            device::DeviceErrorCode::OperationFailed,
            fmt::format("All {} device reconnections failed", failed)};
    }

    return true;
}

auto INDIDevicePlugin::discoverDevices()
    -> device::DeviceResult<std::vector<device::DiscoveredDevice>> {
    std::lock_guard lock(mutex_);

    LOG_INFO("Discovering INDI devices from {}:{}", serverHost_, serverPort_);

    discoveredDevices_.clear();

    // In a real implementation, this would:
    // 1. Connect to the INDI server
    // 2. Request device list
    // 3. Parse and return discovered devices

    // For now, we return an empty list - actual implementation would
    // use libindi to query the server

    LOG_INFO("INDI device discovery complete: {} devices found",
             discoveredDevices_.size());

    return discoveredDevices_;
}

auto INDIDevicePlugin::getDiscoveredDevices() const
    -> std::vector<device::DiscoveredDevice> {
    std::lock_guard lock(mutex_);
    return discoveredDevices_;
}

auto INDIDevicePlugin::getLastError() const -> std::string {
    std::lock_guard lock(mutex_);
    return lastError_;
}

auto INDIDevicePlugin::isBackendRunning() const -> bool {
    return backendRunning_.load();
}

auto INDIDevicePlugin::isHealthy() const -> bool {
    return state_ == device::DevicePluginState::Ready ||
           state_ == device::DevicePluginState::Active;
}

// ============================================================================
// INDI-Specific Methods
// ============================================================================

void INDIDevicePlugin::setServerConnection(const std::string& host, int port) {
    std::lock_guard lock(mutex_);
    serverHost_ = host;
    serverPort_ = port;
    LOG_DEBUG("INDI server connection set to {}:{}", host, port);
}

// ============================================================================
// Private Helper Methods
// ============================================================================

auto INDIDevicePlugin::buildTypeInfo(DeviceType type) const
    -> device::DeviceTypeInfo {
    device::DeviceTypeInfo info;

    info.typeName = "INDI:" + deviceTypeToString(type);
    info.pluginName = PLUGIN_NAME;
    info.version = PLUGIN_VERSION;

    switch (type) {
        case DeviceType::Camera:
            info.category = "Camera";
            info.displayName = "INDI Camera";
            info.description = "INDI-compatible CCD/CMOS camera";
            info.capabilities.canCapture = true;
            info.capabilities.canStream = true;
            info.capabilities.hasTemperatureControl = true;
            info.capabilities.supportsAsync = true;
            break;

        case DeviceType::Focuser:
            info.category = "Focuser";
            info.displayName = "INDI Focuser";
            info.description = "INDI-compatible focuser";
            info.capabilities.supportsAsync = true;
            break;

        case DeviceType::FilterWheel:
            info.category = "FilterWheel";
            info.displayName = "INDI Filter Wheel";
            info.description = "INDI-compatible filter wheel";
            info.capabilities.supportsAsync = true;
            break;

        case DeviceType::Telescope:
            info.category = "Mount";
            info.displayName = "INDI Telescope/Mount";
            info.description = "INDI-compatible telescope mount";
            info.capabilities.canTrack = true;
            info.capabilities.canSlew = true;
            info.capabilities.canSync = true;
            info.capabilities.supportsAsync = true;
            break;

        case DeviceType::Rotator:
            info.category = "Rotator";
            info.displayName = "INDI Rotator";
            info.description = "INDI-compatible rotator";
            info.capabilities.supportsAsync = true;
            break;

        case DeviceType::Dome:
            info.category = "Dome";
            info.displayName = "INDI Dome";
            info.description = "INDI-compatible observatory dome";
            info.capabilities.canSlew = true;
            info.capabilities.supportsAsync = true;
            break;

        case DeviceType::Weather:
            info.category = "Weather";
            info.displayName = "INDI Weather Station";
            info.description = "INDI-compatible weather station";
            info.capabilities.supportsAsync = true;
            break;

        case DeviceType::GPS:
            info.category = "GPS";
            info.displayName = "INDI GPS";
            info.description = "INDI-compatible GPS receiver";
            info.capabilities.supportsAsync = true;
            break;

        default:
            info.category = "Unknown";
            info.displayName = "INDI Unknown Device";
            info.description = "Unknown INDI device type";
            break;
    }

    return info;
}

auto INDIDevicePlugin::createDeviceCreator(DeviceType type)
    -> device::DeviceFactory::Creator {
    return [this, type](const std::string& name,
                        const nlohmann::json& config) -> std::shared_ptr<AtomDriver> {
        auto device = INDIDeviceFactory::getInstance().createDevice(type, name);
        if (device) {
            // Apply configuration
            if (config.contains("server_host")) {
                // Set server connection on the device
            }

            // Add to our device manager
            deviceManager_.addDevice(device);
        }
        return device;
    };
}

auto INDIDevicePlugin::getRegistrationKey(DeviceType type) -> std::string {
    return "INDI:" + deviceTypeToString(type);
}

// ============================================================================
// Plugin Entry Points
// ============================================================================

extern "C" {

auto createDevicePlugin() -> device::IDevicePlugin* {
    return new INDIDevicePlugin();
}

void destroyDevicePlugin(device::IDevicePlugin* plugin) {
    delete plugin;
}

auto getDevicePluginMetadata() -> device::DevicePluginMetadata {
    INDIDevicePlugin plugin;
    return plugin.getDeviceMetadata();
}

}

}  // namespace lithium::client::indi
