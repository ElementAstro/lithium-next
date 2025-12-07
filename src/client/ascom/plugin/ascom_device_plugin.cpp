/*
 * ascom_device_plugin.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "ascom_device_plugin.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::ascom {

// ============================================================================
// Construction / Destruction
// ============================================================================

ASCOMDevicePlugin::ASCOMDevicePlugin() {
    LOG_DEBUG("ASCOMDevicePlugin created");
}

ASCOMDevicePlugin::~ASCOMDevicePlugin() {
    if (state_ != device::DevicePluginState::Unloaded) {
        shutdown();
    }
    LOG_DEBUG("ASCOMDevicePlugin destroyed");
}

// ============================================================================
// IPlugin Interface
// ============================================================================

auto ASCOMDevicePlugin::initialize() -> bool {
    std::lock_guard lock(mutex_);

    if (state_ != device::DevicePluginState::Unloaded) {
        lastError_ = "Plugin already initialized";
        return false;
    }

    LOG_INFO("Initializing ASCOM device plugin");

    state_ = device::DevicePluginState::Initializing;

    // Create Alpaca client
    alpacaClient_ = std::make_shared<AlpacaClient>(serverHost_, serverPort_);

    state_ = device::DevicePluginState::Ready;
    LOG_INFO("ASCOM device plugin initialized successfully");

    return true;
}

void ASCOMDevicePlugin::shutdown() {
    std::lock_guard lock(mutex_);

    LOG_INFO("Shutting down ASCOM device plugin");

    state_ = device::DevicePluginState::Unloading;

    // Disconnect all devices
    deviceManager_.disconnectAll();
    deviceManager_.clear();
    discoveredDevices_.clear();

    // Clear Alpaca client
    alpacaClient_.reset();

    backendRunning_ = false;
    state_ = device::DevicePluginState::Unloaded;

    LOG_INFO("ASCOM device plugin shutdown complete");
}

// ============================================================================
// IDevicePlugin Interface
// ============================================================================

auto ASCOMDevicePlugin::getDeviceMetadata() const
    -> device::DevicePluginMetadata {
    device::DevicePluginMetadata metadata;
    metadata.name = PLUGIN_NAME;
    metadata.version = PLUGIN_VERSION;
    metadata.description =
        "ASCOM/Alpaca device driver plugin for astronomical equipment";
    metadata.author = "Max Qian <lightapt.com>";
    metadata.license = "GPL-3.0";
    metadata.backendName = "ASCOM/Alpaca";
    metadata.backendVersion = "Alpaca 1.0+";
    metadata.supportsHotPlug = true;
    metadata.supportsAutoDiscovery = true;
    metadata.requiresServer = true;
    metadata.supportedDeviceCategories = {"Camera",      "Focuser",
                                          "FilterWheel", "Telescope",
                                          "Rotator",     "Dome",
                                          "ObservingConditions"};
    metadata.tags = {"ascom", "alpaca", "astronomy", "device-control"};
    metadata.capabilities["rest_api"] = true;
    metadata.capabilities["device_number"] = true;
    metadata.capabilities["client_id"] = true;

    return metadata;
}

auto ASCOMDevicePlugin::getDeviceTypes() const
    -> std::vector<device::DeviceTypeInfo> {
    std::vector<device::DeviceTypeInfo> types;

    types.push_back(buildTypeInfo(ASCOMDeviceType::Camera));
    types.push_back(buildTypeInfo(ASCOMDeviceType::Focuser));
    types.push_back(buildTypeInfo(ASCOMDeviceType::FilterWheel));
    types.push_back(buildTypeInfo(ASCOMDeviceType::Telescope));
    types.push_back(buildTypeInfo(ASCOMDeviceType::Rotator));
    types.push_back(buildTypeInfo(ASCOMDeviceType::Dome));
    types.push_back(buildTypeInfo(ASCOMDeviceType::ObservingConditions));

    return types;
}

auto ASCOMDevicePlugin::registerDeviceTypes(
    device::DeviceTypeRegistry& registry) -> device::DeviceResult<size_t> {
    size_t registered = 0;

    auto types = getDeviceTypes();
    for (const auto& type : types) {
        if (registry.registerType(type)) {
            ++registered;
            LOG_DEBUG("Registered ASCOM device type: {}", type.typeName);
        } else {
            LOG_WARNING("Failed to register ASCOM device type: {}",
                        type.typeName);
        }
    }

    LOG_INFO("Registered {} ASCOM device types", registered);
    return registered;
}

void ASCOMDevicePlugin::registerDeviceCreators(device::DeviceFactory& factory) {
    LOG_INFO("Registering ASCOM device creators");

    // Register creators for each device type
    factory.registerCreator("ASCOM:Camera",
                            createDeviceCreator(ASCOMDeviceType::Camera));
    factory.registerCreator("ASCOM:Focuser",
                            createDeviceCreator(ASCOMDeviceType::Focuser));
    factory.registerCreator("ASCOM:FilterWheel",
                            createDeviceCreator(ASCOMDeviceType::FilterWheel));
    factory.registerCreator("ASCOM:Telescope",
                            createDeviceCreator(ASCOMDeviceType::Telescope));
    factory.registerCreator("ASCOM:Rotator",
                            createDeviceCreator(ASCOMDeviceType::Rotator));
    factory.registerCreator("ASCOM:Dome",
                            createDeviceCreator(ASCOMDeviceType::Dome));
    factory.registerCreator(
        "ASCOM:ObservingConditions",
        createDeviceCreator(ASCOMDeviceType::ObservingConditions));

    LOG_INFO("ASCOM device creators registered");
}

void ASCOMDevicePlugin::unregisterDeviceCreators(
    device::DeviceFactory& factory) {
    LOG_INFO("Unregistering ASCOM device creators");

    factory.unregisterCreator("ASCOM:Camera");
    factory.unregisterCreator("ASCOM:Focuser");
    factory.unregisterCreator("ASCOM:FilterWheel");
    factory.unregisterCreator("ASCOM:Telescope");
    factory.unregisterCreator("ASCOM:Rotator");
    factory.unregisterCreator("ASCOM:Dome");
    factory.unregisterCreator("ASCOM:ObservingConditions");

    LOG_INFO("ASCOM device creators unregistered");
}

auto ASCOMDevicePlugin::createBackend() -> std::shared_ptr<DeviceBackend> {
    // For ASCOM, the "backend" is the Alpaca client
    LOG_DEBUG("ASCOM backend is managed via AlpacaClient");
    return nullptr;
}

auto ASCOMDevicePlugin::prepareHotPlug()
    -> device::DeviceResult<std::vector<device::DeviceMigrationContext>> {
    std::lock_guard lock(mutex_);

    LOG_INFO("Preparing ASCOM devices for hot-plug");

    std::vector<device::DeviceMigrationContext> contexts;

    auto devices = deviceManager_.getAllDevices();
    for (const auto& device : devices) {
        device::DeviceMigrationContext ctx;
        ctx.deviceName = device->getName();
        ctx.deviceType = ascomDeviceTypeToString(device->getDeviceType());
        ctx.wasConnected = device->isConnected();

        // Save device-specific state
        ctx.state["server_host"] = serverHost_;
        ctx.state["server_port"] = serverPort_;
        ctx.state["device_number"] = device->getDeviceNumber();

        contexts.push_back(std::move(ctx));

        LOG_DEBUG("Prepared migration context for ASCOM device: {}",
                  device->getName());
    }

    // Disconnect all devices
    deviceManager_.disconnectAll();

    LOG_INFO("ASCOM hot-plug preparation complete: {} devices prepared",
             contexts.size());

    return contexts;
}

auto ASCOMDevicePlugin::completeHotPlug(
    const std::vector<device::DeviceMigrationContext>& contexts)
    -> device::DeviceResult<bool> {
    std::lock_guard lock(mutex_);

    LOG_INFO("Completing ASCOM hot-plug for {} devices", contexts.size());

    for (const auto& ctx : contexts) {
        // Restore server connection settings
        if (ctx.state.contains("server_host")) {
            serverHost_ = ctx.state["server_host"].get<std::string>();
        }
        if (ctx.state.contains("server_port")) {
            serverPort_ = ctx.state["server_port"].get<int>();
        }

        // Update Alpaca client if settings changed
        if (alpacaClient_) {
            alpacaClient_ =
                std::make_shared<AlpacaClient>(serverHost_, serverPort_);
        }

        // Reconnect if was connected
        if (ctx.wasConnected) {
            auto device = deviceManager_.getDevice(ctx.deviceName);
            if (device && alpacaClient_) {
                LOG_DEBUG("Reconnecting ASCOM device: {}", ctx.deviceName);
                device->connect(alpacaClient_);
            }
        }
    }

    LOG_INFO("ASCOM hot-plug completion finished");
    return true;
}

auto ASCOMDevicePlugin::discoverDevices()
    -> device::DeviceResult<std::vector<device::DiscoveredDevice>> {
    std::lock_guard lock(mutex_);

    LOG_INFO("Discovering ASCOM devices from {}:{}", serverHost_, serverPort_);

    discoveredDevices_.clear();

    if (!alpacaClient_) {
        lastError_ = "Alpaca client not initialized";
        return device::DeviceError{device::DeviceErrorCode::NotConnected,
                                   lastError_};
    }

    // Query Alpaca management API for configured devices
    // Endpoint: GET /management/v1/configureddevices
    try {
        auto response = alpacaClient_->getConfiguredDevices();
        if (!response.has_value()) {
            lastError_ = "Failed to query configured devices: " +
                         response.error().message;
            LOG_WARNING("{}", lastError_);
            return device::DeviceError{device::DeviceErrorCode::CommunicationError,
                                       lastError_};
        }

        // Parse response and build discovered device list
        for (const auto& deviceInfo : response.value()) {
            device::DiscoveredDevice discovered;
            discovered.name = deviceInfo.deviceName;
            discovered.uniqueId = fmt::format("ASCOM:{}:{}",
                                              deviceInfo.deviceType,
                                              deviceInfo.deviceNumber);
            discovered.backend = "ASCOM";
            discovered.category = deviceInfo.deviceType;
            discovered.available = true;

            // Map ASCOM device type to our type
            discovered.type = "ASCOM:" + deviceInfo.deviceType;

            // Store additional info
            discovered.properties["device_number"] = deviceInfo.deviceNumber;
            discovered.properties["server_host"] = serverHost_;
            discovered.properties["server_port"] = serverPort_;

            discoveredDevices_.push_back(std::move(discovered));
        }

        LOG_INFO("ASCOM device discovery complete: {} devices found",
                 discoveredDevices_.size());

    } catch (const std::exception& e) {
        lastError_ = fmt::format("ASCOM discovery error: {}", e.what());
        LOG_ERROR("{}", lastError_);
        return device::DeviceError{device::DeviceErrorCode::CommunicationError,
                                   lastError_};
    }

    return discoveredDevices_;
}

auto ASCOMDevicePlugin::getDiscoveredDevices() const
    -> std::vector<device::DiscoveredDevice> {
    std::lock_guard lock(mutex_);
    return discoveredDevices_;
}

auto ASCOMDevicePlugin::getLastError() const -> std::string {
    std::lock_guard lock(mutex_);
    return lastError_;
}

auto ASCOMDevicePlugin::isBackendRunning() const -> bool {
    return backendRunning_.load();
}

auto ASCOMDevicePlugin::isHealthy() const -> bool {
    return state_ == device::DevicePluginState::Ready ||
           state_ == device::DevicePluginState::Active;
}

// ============================================================================
// ASCOM-Specific Methods
// ============================================================================

void ASCOMDevicePlugin::setServerConnection(const std::string& host, int port) {
    std::lock_guard lock(mutex_);
    serverHost_ = host;
    serverPort_ = port;

    // Recreate Alpaca client with new settings
    if (state_ != device::DevicePluginState::Unloaded) {
        alpacaClient_ = std::make_shared<AlpacaClient>(serverHost_, serverPort_);
    }

    LOG_DEBUG("ASCOM server connection set to {}:{}", host, port);
}

// ============================================================================
// Private Helper Methods
// ============================================================================

auto ASCOMDevicePlugin::buildTypeInfo(ASCOMDeviceType type) const
    -> device::DeviceTypeInfo {
    device::DeviceTypeInfo info;

    info.typeName = "ASCOM:" + ascomDeviceTypeToString(type);
    info.pluginName = PLUGIN_NAME;
    info.version = PLUGIN_VERSION;

    switch (type) {
        case ASCOMDeviceType::Camera:
            info.category = "Camera";
            info.displayName = "ASCOM Camera";
            info.description = "ASCOM/Alpaca-compatible CCD/CMOS camera";
            info.capabilities.canCapture = true;
            info.capabilities.canStream = true;
            info.capabilities.hasTemperatureControl = true;
            info.capabilities.supportsAsync = true;
            break;

        case ASCOMDeviceType::Focuser:
            info.category = "Focuser";
            info.displayName = "ASCOM Focuser";
            info.description = "ASCOM/Alpaca-compatible focuser";
            info.capabilities.supportsAsync = true;
            break;

        case ASCOMDeviceType::FilterWheel:
            info.category = "FilterWheel";
            info.displayName = "ASCOM Filter Wheel";
            info.description = "ASCOM/Alpaca-compatible filter wheel";
            info.capabilities.supportsAsync = true;
            break;

        case ASCOMDeviceType::Telescope:
            info.category = "Mount";
            info.displayName = "ASCOM Telescope/Mount";
            info.description = "ASCOM/Alpaca-compatible telescope mount";
            info.capabilities.canTrack = true;
            info.capabilities.canSlew = true;
            info.capabilities.canSync = true;
            info.capabilities.supportsAsync = true;
            break;

        case ASCOMDeviceType::Rotator:
            info.category = "Rotator";
            info.displayName = "ASCOM Rotator";
            info.description = "ASCOM/Alpaca-compatible rotator";
            info.capabilities.supportsAsync = true;
            break;

        case ASCOMDeviceType::Dome:
            info.category = "Dome";
            info.displayName = "ASCOM Dome";
            info.description = "ASCOM/Alpaca-compatible observatory dome";
            info.capabilities.canSlew = true;
            info.capabilities.supportsAsync = true;
            break;

        case ASCOMDeviceType::ObservingConditions:
            info.category = "Weather";
            info.displayName = "ASCOM Observing Conditions";
            info.description = "ASCOM/Alpaca-compatible weather/conditions sensor";
            info.capabilities.supportsAsync = true;
            break;

        default:
            info.category = "Unknown";
            info.displayName = "ASCOM Unknown Device";
            info.description = "Unknown ASCOM device type";
            break;
    }

    return info;
}

auto ASCOMDevicePlugin::createDeviceCreator(ASCOMDeviceType type)
    -> device::DeviceFactory::Creator {
    return [this, type](const std::string& name, const nlohmann::json& config)
               -> std::shared_ptr<AtomDriver> {
        int deviceNumber = config.value("device_number", 0);

        auto device = ASCOMDeviceFactory::getInstance().createDevice(
            type, name, deviceNumber);

        if (device) {
            // Connect to Alpaca server if client is available
            if (alpacaClient_) {
                device->connect(alpacaClient_);
            }

            // Add to our device manager
            deviceManager_.addDevice(device);
        }

        return device;
    };
}

auto ASCOMDevicePlugin::getRegistrationKey(ASCOMDeviceType type) -> std::string {
    return "ASCOM:" + ascomDeviceTypeToString(type);
}

// ============================================================================
// Plugin Entry Points
// ============================================================================

extern "C" {

auto createDevicePlugin() -> device::IDevicePlugin* {
    return new ASCOMDevicePlugin();
}

void destroyDevicePlugin(device::IDevicePlugin* plugin) {
    delete plugin;
}

auto getDevicePluginMetadata() -> device::DevicePluginMetadata {
    ASCOMDevicePlugin plugin;
    return plugin.getDeviceMetadata();
}

}

}  // namespace lithium::client::ascom
