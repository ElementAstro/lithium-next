/*
 * device_component_bridge.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device component bridge implementation

**************************************************/

#include "device_component_bridge.hpp"

#include "components/manager/manager.hpp"
#include "device/plugin/device_plugin_loader.hpp"

#include <algorithm>
#include <loguru.hpp>

namespace lithium::device {

// ============================================================================
// BridgeEvent Implementation
// ============================================================================

auto BridgeEvent::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["type"] = static_cast<int>(type);
    j["deviceName"] = deviceName;
    j["componentName"] = componentName;
    j["deviceState"] = deviceComponentStateToString(deviceState);
    j["componentState"] = lithium::componentStateToString(componentState);
    j["message"] = message;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["data"] = data;
    return j;
}

// ============================================================================
// BridgeConfig Implementation
// ============================================================================

auto BridgeConfig::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["autoRegister"] = autoRegister;
    j["autoSync"] = autoSync;
    j["bidirectional"] = bidirectional;
    j["deviceGroup"] = deviceGroup;
    j["syncInterval"] = syncInterval;

    // Auto-grouping configuration
    j["autoGroupByType"] = autoGroupByType;
    j["autoGroupByState"] = autoGroupByState;
    j["cameraGroup"] = cameraGroup;
    j["mountGroup"] = mountGroup;
    j["focuserGroup"] = focuserGroup;
    j["filterWheelGroup"] = filterWheelGroup;
    j["domeGroup"] = domeGroup;
    j["guiderGroup"] = guiderGroup;
    j["rotatorGroup"] = rotatorGroup;
    j["connectedGroup"] = connectedGroup;
    j["errorGroup"] = errorGroup;
    j["idleGroup"] = idleGroup;

    return j;
}

auto BridgeConfig::fromJson(const nlohmann::json& j) -> BridgeConfig {
    BridgeConfig config;

    if (j.contains("autoRegister")) {
        config.autoRegister = j["autoRegister"].get<bool>();
    }
    if (j.contains("autoSync")) {
        config.autoSync = j["autoSync"].get<bool>();
    }
    if (j.contains("bidirectional")) {
        config.bidirectional = j["bidirectional"].get<bool>();
    }
    if (j.contains("deviceGroup")) {
        config.deviceGroup = j["deviceGroup"].get<std::string>();
    }
    if (j.contains("syncInterval")) {
        config.syncInterval = j["syncInterval"].get<int>();
    }

    // Auto-grouping configuration
    if (j.contains("autoGroupByType")) {
        config.autoGroupByType = j["autoGroupByType"].get<bool>();
    }
    if (j.contains("autoGroupByState")) {
        config.autoGroupByState = j["autoGroupByState"].get<bool>();
    }
    if (j.contains("cameraGroup")) {
        config.cameraGroup = j["cameraGroup"].get<std::string>();
    }
    if (j.contains("mountGroup")) {
        config.mountGroup = j["mountGroup"].get<std::string>();
    }
    if (j.contains("focuserGroup")) {
        config.focuserGroup = j["focuserGroup"].get<std::string>();
    }
    if (j.contains("filterWheelGroup")) {
        config.filterWheelGroup = j["filterWheelGroup"].get<std::string>();
    }
    if (j.contains("domeGroup")) {
        config.domeGroup = j["domeGroup"].get<std::string>();
    }
    if (j.contains("guiderGroup")) {
        config.guiderGroup = j["guiderGroup"].get<std::string>();
    }
    if (j.contains("rotatorGroup")) {
        config.rotatorGroup = j["rotatorGroup"].get<std::string>();
    }
    if (j.contains("connectedGroup")) {
        config.connectedGroup = j["connectedGroup"].get<std::string>();
    }
    if (j.contains("errorGroup")) {
        config.errorGroup = j["errorGroup"].get<std::string>();
    }
    if (j.contains("idleGroup")) {
        config.idleGroup = j["idleGroup"].get<std::string>();
    }

    return config;
}

// ============================================================================
// DeviceComponentBridge Implementation
// ============================================================================

DeviceComponentBridge::DeviceComponentBridge(
    std::shared_ptr<lithium::ComponentManager> componentManager)
    : componentManager_(std::move(componentManager)) {}

DeviceComponentBridge::~DeviceComponentBridge() {
    if (initialized_) {
        shutdown();
    }
}

auto DeviceComponentBridge::initialize(const BridgeConfig& config)
    -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    if (initialized_) {
        return true;  // Already initialized
    }

    if (!componentManager_) {
        return std::unexpected(
            error::operationFailed("initialize", "Component manager is null"));
    }

    config_ = config;

    // Register event listener with component manager
    componentManager_->addEventListener(
        ComponentEvent::StateChanged,
        [this](const std::string& name, ComponentEvent event,
               const nlohmann::json& data) {
            onComponentStateChanged(name, event, data);
        });

    initialized_ = true;
    LOG_F(INFO, "DeviceComponentBridge initialized");

    return true;
}

void DeviceComponentBridge::shutdown() {
    std::unique_lock lock(mutex_);

    if (!initialized_) {
        return;
    }

    LOG_F(INFO, "Shutting down DeviceComponentBridge...");

    // Unregister all devices
    lock.unlock();
    unregisterAllDevices();
    lock.lock();

    // Remove event listener
    if (componentManager_) {
        componentManager_->removeEventListener(ComponentEvent::StateChanged);
    }

    // Clear subscribers
    {
        std::unique_lock eventLock(eventMutex_);
        eventSubscribers_.clear();
    }

    initialized_ = false;
    LOG_F(INFO, "DeviceComponentBridge shutdown complete");
}

auto DeviceComponentBridge::isInitialized() const -> bool {
    std::shared_lock lock(mutex_);
    return initialized_;
}

auto DeviceComponentBridge::registerDevice(
    std::shared_ptr<DeviceComponentAdapter> adapter) -> DeviceResult<bool> {
    if (!adapter) {
        return std::unexpected(
            error::invalidArgument("adapter", "Adapter is null"));
    }

    std::string name = adapter->getName();

    {
        std::unique_lock lock(mutex_);

        if (!initialized_) {
            return std::unexpected(
                error::operationFailed("registerDevice", "Bridge not initialized"));
        }

        if (deviceAdapters_.contains(name)) {
            return std::unexpected(
                error::operationFailed("registerDevice",
                                       "Device already registered: " + name));
        }
    }

    // Initialize the adapter if not already
    if (adapter->getDeviceState() == DeviceComponentState::Created) {
        if (!adapter->initialize()) {
            return std::unexpected(
                error::operationFailed("registerDevice",
                                       "Adapter initialization failed: " +
                                           adapter->getLastError()));
        }
    }

    // Register with component manager
    nlohmann::json params;
    params["name"] = name;
    params["config"] = adapter->getConfig().toJson();
    params["autoStart"] = false;  // Don't auto-start, let bridge control

    if (!componentManager_->loadComponent(params)) {
        return std::unexpected(
            error::operationFailed("registerDevice",
                                   "Component registration failed"));
    }

    // Add to device group
    componentManager_->addToGroup(name, config_.deviceGroup);

    // Auto-group by device type
    if (config_.autoGroupByType) {
        std::string deviceType = adapter->getDeviceType();
        addToTypeGroup(name, deviceType);
    }

    // Auto-group by initial state
    if (config_.autoGroupByState) {
        DeviceComponentState state = adapter->getDeviceState();
        std::string stateGroup = stateToGroup(state);
        if (!stateGroup.empty()) {
            componentManager_->addToGroup(name, stateGroup);
        }
    }

    // Store adapter
    {
        std::unique_lock lock(mutex_);
        deviceAdapters_[name] = adapter;
        registrationCount_++;
    }

    emitEvent(createEvent(BridgeEventType::DeviceRegistered, name,
                          "Device registered as component"));

    LOG_F(INFO, "Device '%s' registered as component", name.c_str());

    return true;
}

auto DeviceComponentBridge::registerDevice(std::shared_ptr<AtomDriver> device,
                                           const DeviceAdapterConfig& config,
                                           const std::string& name)
    -> DeviceResult<bool> {
    auto adapter = createDeviceAdapter(std::move(device), config, name);
    return registerDevice(adapter);
}

auto DeviceComponentBridge::unregisterDevice(const std::string& name)
    -> DeviceResult<bool> {
    std::shared_ptr<DeviceComponentAdapter> adapter;

    {
        std::unique_lock lock(mutex_);

        auto it = deviceAdapters_.find(name);
        if (it == deviceAdapters_.end()) {
            return std::unexpected(
                error::notFound(name, "Device not registered"));
        }

        adapter = it->second;
        deviceAdapters_.erase(it);
        unregistrationCount_++;
    }

    // Stop and destroy the adapter
    if (adapter->isConnected()) {
        adapter->disconnect();
    }
    adapter->destroy();

    // Unload from component manager
    nlohmann::json params;
    params["name"] = name;
    componentManager_->unloadComponent(params);

    emitEvent(createEvent(BridgeEventType::DeviceUnregistered, name,
                          "Device unregistered from components"));

    LOG_F(INFO, "Device '%s' unregistered", name.c_str());

    return true;
}

auto DeviceComponentBridge::unregisterAllDevices() -> size_t {
    std::vector<std::string> names;

    {
        std::shared_lock lock(mutex_);
        for (const auto& [name, adapter] : deviceAdapters_) {
            names.push_back(name);
        }
    }

    size_t count = 0;
    for (const auto& name : names) {
        if (unregisterDevice(name)) {
            count++;
        }
    }

    return count;
}

auto DeviceComponentBridge::isDeviceRegistered(const std::string& name) const
    -> bool {
    std::shared_lock lock(mutex_);
    return deviceAdapters_.contains(name);
}

auto DeviceComponentBridge::getDeviceAdapter(const std::string& name) const
    -> std::shared_ptr<DeviceComponentAdapter> {
    std::shared_lock lock(mutex_);
    auto it = deviceAdapters_.find(name);
    if (it != deviceAdapters_.end()) {
        return it->second;
    }
    return nullptr;
}

auto DeviceComponentBridge::getRegisteredDevices() const
    -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(deviceAdapters_.size());
    for (const auto& [name, adapter] : deviceAdapters_) {
        names.push_back(name);
    }
    return names;
}

auto DeviceComponentBridge::getDevicesByType(const std::string& type) const
    -> std::vector<std::shared_ptr<DeviceComponentAdapter>> {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<DeviceComponentAdapter>> result;

    for (const auto& [name, adapter] : deviceAdapters_) {
        if (adapter->getDeviceType() == type) {
            result.push_back(adapter);
        }
    }

    return result;
}

auto DeviceComponentBridge::getDevicesByState(DeviceComponentState state) const
    -> std::vector<std::shared_ptr<DeviceComponentAdapter>> {
    std::shared_lock lock(mutex_);
    std::vector<std::shared_ptr<DeviceComponentAdapter>> result;

    for (const auto& [name, adapter] : deviceAdapters_) {
        if (adapter->getDeviceState() == state) {
            result.push_back(adapter);
        }
    }

    return result;
}

void DeviceComponentBridge::syncDeviceToComponent(const std::string& name) {
    std::shared_ptr<DeviceComponentAdapter> adapter;

    {
        std::shared_lock lock(mutex_);
        auto it = deviceAdapters_.find(name);
        if (it == deviceAdapters_.end()) {
            return;
        }
        adapter = it->second;
    }

    // Get device state
    DeviceComponentState deviceState = adapter->getDeviceState();
    ComponentState componentState = toComponentState(deviceState);

    // Update component state through component manager
    // This would typically trigger state change callbacks
    switch (componentState) {
        case ComponentState::Running:
            componentManager_->startComponent(name);
            break;
        case ComponentState::Paused:
            componentManager_->pauseComponent(name);
            break;
        case ComponentState::Stopped:
            componentManager_->stopComponent(name);
            break;
        default:
            break;
    }

    syncCount_++;
}

void DeviceComponentBridge::syncComponentToDevice(const std::string& name) {
    if (!config_.bidirectional) {
        return;
    }

    std::shared_ptr<DeviceComponentAdapter> adapter;

    {
        std::shared_lock lock(mutex_);
        auto it = deviceAdapters_.find(name);
        if (it == deviceAdapters_.end()) {
            return;
        }
        adapter = it->second;
    }

    // Get component state
    ComponentState componentState = componentManager_->getComponentState(name);
    DeviceComponentState deviceState = fromComponentState(componentState);
    DeviceComponentState currentDeviceState = adapter->getDeviceState();

    // Sync state if different
    if (currentDeviceState != deviceState) {
        switch (deviceState) {
            case DeviceComponentState::Connected:
                adapter->start();
                break;
            case DeviceComponentState::Paused:
                adapter->pause();
                break;
            case DeviceComponentState::Disconnected:
                adapter->stop();
                break;
            default:
                break;
        }
    }

    syncCount_++;
}

void DeviceComponentBridge::syncAll() {
    std::vector<std::string> names;

    {
        std::shared_lock lock(mutex_);
        for (const auto& [name, adapter] : deviceAdapters_) {
            names.push_back(name);
        }
    }

    emitEvent(createEvent(BridgeEventType::SyncStarted, "",
                          "Synchronizing all devices"));

    for (const auto& name : names) {
        syncDeviceToComponent(name);
    }

    emitEvent(createEvent(BridgeEventType::SyncCompleted, "",
                          "Synchronization completed"));
}

auto DeviceComponentBridge::startAllDevices() -> size_t {
    auto names = getRegisteredDevices();
    size_t count = 0;

    for (const auto& name : names) {
        auto adapter = getDeviceAdapter(name);
        if (adapter && adapter->start()) {
            count++;
        }
    }

    return count;
}

auto DeviceComponentBridge::stopAllDevices() -> size_t {
    auto names = getRegisteredDevices();
    size_t count = 0;

    for (const auto& name : names) {
        auto adapter = getDeviceAdapter(name);
        if (adapter && adapter->stop()) {
            count++;
        }
    }

    return count;
}

auto DeviceComponentBridge::connectAllDevices() -> size_t {
    auto names = getRegisteredDevices();
    size_t count = 0;

    for (const auto& name : names) {
        auto adapter = getDeviceAdapter(name);
        if (adapter) {
            auto result = adapter->connect();
            if (result && *result) {
                count++;
            }
        }
    }

    return count;
}

auto DeviceComponentBridge::disconnectAllDevices() -> size_t {
    auto names = getRegisteredDevices();
    size_t count = 0;

    for (const auto& name : names) {
        auto adapter = getDeviceAdapter(name);
        if (adapter) {
            auto result = adapter->disconnect();
            if (result && *result) {
                count++;
            }
        }
    }

    return count;
}

auto DeviceComponentBridge::subscribe(BridgeEventCallback callback)
    -> uint64_t {
    std::unique_lock lock(eventMutex_);
    uint64_t id = nextSubscriberId_++;
    eventSubscribers_[id] = std::move(callback);
    return id;
}

void DeviceComponentBridge::unsubscribe(uint64_t subscriptionId) {
    std::unique_lock lock(eventMutex_);
    eventSubscribers_.erase(subscriptionId);
}

void DeviceComponentBridge::setPluginLoader(
    std::shared_ptr<DevicePluginLoader> loader) {
    std::unique_lock lock(mutex_);
    pluginLoader_ = std::move(loader);
}

auto DeviceComponentBridge::getPluginLoader() const
    -> std::shared_ptr<DevicePluginLoader> {
    std::shared_lock lock(mutex_);
    return pluginLoader_;
}

auto DeviceComponentBridge::getStatistics() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json stats;
    stats["registeredDevices"] = deviceAdapters_.size();
    stats["registrationCount"] = registrationCount_;
    stats["unregistrationCount"] = unregistrationCount_;
    stats["syncCount"] = syncCount_;
    stats["errorCount"] = errorCount_;
    stats["initialized"] = initialized_;
    stats["config"] = config_.toJson();

    // Device states summary
    nlohmann::json statesSummary;
    for (const auto& [name, adapter] : deviceAdapters_) {
        std::string state = deviceComponentStateToString(adapter->getDeviceState());
        if (!statesSummary.contains(state)) {
            statesSummary[state] = 0;
        }
        statesSummary[state] = statesSummary[state].get<int>() + 1;
    }
    stats["deviceStatesSummary"] = statesSummary;

    return stats;
}

auto DeviceComponentBridge::getConfig() const -> BridgeConfig {
    std::shared_lock lock(mutex_);
    return config_;
}

void DeviceComponentBridge::emitEvent(const BridgeEvent& event) {
    std::shared_lock lock(eventMutex_);
    for (const auto& [id, callback] : eventSubscribers_) {
        try {
            callback(event);
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

auto DeviceComponentBridge::createEvent(BridgeEventType type,
                                         const std::string& deviceName,
                                         const std::string& message) const
    -> BridgeEvent {
    BridgeEvent event;
    event.type = type;
    event.deviceName = deviceName;
    event.componentName = deviceName;
    event.message = message;
    event.timestamp = std::chrono::system_clock::now();

    // Get current states if device exists
    auto adapter = getDeviceAdapter(deviceName);
    if (adapter) {
        event.deviceState = adapter->getDeviceState();
        event.componentState = toComponentState(event.deviceState);
    }

    return event;
}

void DeviceComponentBridge::onComponentStateChanged(
    const std::string& name, ComponentEvent event,
    const nlohmann::json& data) {
    // Check if this is a registered device
    if (!isDeviceRegistered(name)) {
        return;
    }

    // Sync component state to device if configured
    if (config_.autoSync && config_.bidirectional) {
        syncComponentToDevice(name);
    }

    // Emit bridge event
    auto adapter = getDeviceAdapter(name);
    if (adapter) {
        BridgeEvent bridgeEvent;
        bridgeEvent.type = BridgeEventType::ComponentStateChanged;
        bridgeEvent.deviceName = name;
        bridgeEvent.componentName = name;
        bridgeEvent.deviceState = adapter->getDeviceState();
        bridgeEvent.componentState = componentManager_->getComponentState(name);
        bridgeEvent.message = "Component state changed";
        bridgeEvent.timestamp = std::chrono::system_clock::now();
        bridgeEvent.data = data;

        emitEvent(bridgeEvent);
    }
}

void DeviceComponentBridge::onDeviceStateChanged(
    const std::string& name, DeviceComponentState oldState,
    DeviceComponentState newState) {
    // Update state groups if configured
    if (config_.autoGroupByState) {
        updateStateGroups(name, oldState, newState);
    }

    // Sync device state to component if configured
    if (config_.autoSync) {
        syncDeviceToComponent(name);
    }

    // Emit bridge event
    BridgeEvent event;
    event.type = BridgeEventType::DeviceStateChanged;
    event.deviceName = name;
    event.componentName = name;
    event.deviceState = newState;
    event.componentState = toComponentState(newState);
    event.message = "Device state changed from " +
                    deviceComponentStateToString(oldState) + " to " +
                    deviceComponentStateToString(newState);
    event.timestamp = std::chrono::system_clock::now();

    emitEvent(event);
}

// ============================================================================
// Group Operations
// ============================================================================

auto DeviceComponentBridge::startGroup(const std::string& group) -> size_t {
    if (!componentManager_) {
        return 0;
    }

    auto deviceNames = getDevicesInGroup(group);
    size_t count = 0;

    for (const auto& name : deviceNames) {
        auto adapter = getDeviceAdapter(name);
        if (adapter && adapter->start()) {
            count++;
        }
    }

    LOG_F(INFO, "Started %zu devices in group '%s'", count, group.c_str());
    return count;
}

auto DeviceComponentBridge::stopGroup(const std::string& group) -> size_t {
    if (!componentManager_) {
        return 0;
    }

    auto deviceNames = getDevicesInGroup(group);
    size_t count = 0;

    for (const auto& name : deviceNames) {
        auto adapter = getDeviceAdapter(name);
        if (adapter && adapter->stop()) {
            count++;
        }
    }

    LOG_F(INFO, "Stopped %zu devices in group '%s'", count, group.c_str());
    return count;
}

auto DeviceComponentBridge::connectGroup(const std::string& group) -> size_t {
    if (!componentManager_) {
        return 0;
    }

    auto deviceNames = getDevicesInGroup(group);
    size_t count = 0;

    for (const auto& name : deviceNames) {
        auto adapter = getDeviceAdapter(name);
        if (adapter) {
            auto result = adapter->connect();
            if (result && *result) {
                count++;
            }
        }
    }

    LOG_F(INFO, "Connected %zu devices in group '%s'", count, group.c_str());
    return count;
}

auto DeviceComponentBridge::disconnectGroup(const std::string& group) -> size_t {
    if (!componentManager_) {
        return 0;
    }

    auto deviceNames = getDevicesInGroup(group);
    size_t count = 0;

    for (const auto& name : deviceNames) {
        auto adapter = getDeviceAdapter(name);
        if (adapter) {
            auto result = adapter->disconnect();
            if (result && *result) {
                count++;
            }
        }
    }

    LOG_F(INFO, "Disconnected %zu devices in group '%s'", count, group.c_str());
    return count;
}

auto DeviceComponentBridge::getDeviceGroups() const -> std::vector<std::string> {
    if (!componentManager_) {
        return {};
    }

    // Get all groups and filter to device-related ones
    auto allGroups = componentManager_->getGroups();
    std::vector<std::string> deviceGroups;

    // Include the main device group
    deviceGroups.push_back(config_.deviceGroup);

    // Include type groups
    std::vector<std::string> typeGroups = {
        config_.cameraGroup, config_.mountGroup, config_.focuserGroup,
        config_.filterWheelGroup, config_.domeGroup, config_.guiderGroup,
        config_.rotatorGroup
    };

    // Include state groups
    std::vector<std::string> stateGroups = {
        config_.connectedGroup, config_.errorGroup, config_.idleGroup
    };

    // Add groups that exist in component manager
    for (const auto& group : allGroups) {
        if (std::find(typeGroups.begin(), typeGroups.end(), group) != typeGroups.end() ||
            std::find(stateGroups.begin(), stateGroups.end(), group) != stateGroups.end()) {
            if (std::find(deviceGroups.begin(), deviceGroups.end(), group) == deviceGroups.end()) {
                deviceGroups.push_back(group);
            }
        }
    }

    return deviceGroups;
}

auto DeviceComponentBridge::getDevicesInGroup(const std::string& group) const
    -> std::vector<std::string> {
    if (!componentManager_) {
        return {};
    }

    auto allComponents = componentManager_->getGroupComponents(group);
    std::vector<std::string> devices;

    // Filter to only registered devices
    std::shared_lock lock(mutex_);
    for (const auto& name : allComponents) {
        if (deviceAdapters_.contains(name)) {
            devices.push_back(name);
        }
    }

    return devices;
}

// ============================================================================
// Configuration Update
// ============================================================================

auto DeviceComponentBridge::updateDeviceConfig(const std::string& name,
                                                const nlohmann::json& config)
    -> DeviceResult<bool> {
    auto adapter = getDeviceAdapter(name);
    if (!adapter) {
        return std::unexpected(error::notFound(name, "Device not registered"));
    }

    // Update adapter config
    DeviceAdapterConfig adapterConfig = adapter->getConfig();

    // Merge new config values
    if (config.contains("connectionPort")) {
        adapterConfig.connectionPort = config["connectionPort"].get<std::string>();
    }
    if (config.contains("connectionTimeout")) {
        adapterConfig.connectionTimeout = config["connectionTimeout"].get<int>();
    }
    if (config.contains("maxRetries")) {
        adapterConfig.maxRetries = config["maxRetries"].get<int>();
    }
    if (config.contains("autoConnect")) {
        adapterConfig.autoConnect = config["autoConnect"].get<bool>();
    }
    if (config.contains("autoReconnect")) {
        adapterConfig.autoReconnect = config["autoReconnect"].get<bool>();
    }
    if (config.contains("reconnectDelay")) {
        adapterConfig.reconnectDelay = config["reconnectDelay"].get<int>();
    }
    if (config.contains("deviceConfig")) {
        adapterConfig.deviceConfig = config["deviceConfig"];
    }

    adapter->updateConfig(adapterConfig);

    // Also update component manager config
    componentManager_->updateConfig(name, config);

    LOG_F(INFO, "Updated config for device '%s'", name.c_str());
    return true;
}

auto DeviceComponentBridge::getDeviceConfig(const std::string& name) const
    -> nlohmann::json {
    auto adapter = getDeviceAdapter(name);
    if (!adapter) {
        return nlohmann::json{};
    }

    return adapter->getConfig().toJson();
}

// ============================================================================
// Group Helper Methods
// ============================================================================

auto DeviceComponentBridge::categoryToGroup(const std::string& category) const
    -> std::string {
    // Normalize category to lowercase for comparison
    std::string lowerCategory = category;
    std::transform(lowerCategory.begin(), lowerCategory.end(),
                   lowerCategory.begin(), ::tolower);

    // Map category to configured group name
    if (lowerCategory == "camera" || lowerCategory == "ccd" ||
        lowerCategory == "cmos" || lowerCategory.find("camera") != std::string::npos) {
        return config_.cameraGroup;
    }
    if (lowerCategory == "mount" || lowerCategory == "telescope" ||
        lowerCategory.find("mount") != std::string::npos) {
        return config_.mountGroup;
    }
    if (lowerCategory == "focuser" || lowerCategory.find("focus") != std::string::npos) {
        return config_.focuserGroup;
    }
    if (lowerCategory == "filterwheel" || lowerCategory == "filter_wheel" ||
        lowerCategory == "filter" || lowerCategory.find("filter") != std::string::npos) {
        return config_.filterWheelGroup;
    }
    if (lowerCategory == "dome" || lowerCategory.find("dome") != std::string::npos) {
        return config_.domeGroup;
    }
    if (lowerCategory == "guider" || lowerCategory.find("guid") != std::string::npos) {
        return config_.guiderGroup;
    }
    if (lowerCategory == "rotator" || lowerCategory.find("rotat") != std::string::npos) {
        return config_.rotatorGroup;
    }

    // Return empty for unknown categories
    return "";
}

auto DeviceComponentBridge::stateToGroup(DeviceComponentState state) const
    -> std::string {
    switch (state) {
        case DeviceComponentState::Connected:
            return config_.connectedGroup;
        case DeviceComponentState::Error:
            return config_.errorGroup;
        case DeviceComponentState::Initialized:
        case DeviceComponentState::Disconnected:
            return config_.idleGroup;
        default:
            return "";
    }
}

void DeviceComponentBridge::addToTypeGroup(const std::string& name,
                                            const std::string& deviceType) {
    std::string group = categoryToGroup(deviceType);
    if (!group.empty() && componentManager_) {
        componentManager_->addToGroup(name, group);
        LOG_F(DEBUG, "Added device '%s' to type group '%s'",
              name.c_str(), group.c_str());
    }
}

void DeviceComponentBridge::updateStateGroups(const std::string& name,
                                               DeviceComponentState oldState,
                                               DeviceComponentState newState) {
    if (!componentManager_) {
        return;
    }

    // Note: ComponentManager doesn't have removeFromGroup, so we work around
    // by just adding to the new group. The old group membership remains but
    // becomes stale. For accurate state groups, a query filters by actual state.

    std::string newGroup = stateToGroup(newState);
    if (!newGroup.empty()) {
        componentManager_->addToGroup(name, newGroup);
        LOG_F(DEBUG, "Added device '%s' to state group '%s'",
              name.c_str(), newGroup.c_str());
    }
}

// ============================================================================
// Factory Function
// ============================================================================

auto createDeviceComponentBridge(
    std::shared_ptr<lithium::ComponentManager> componentManager,
    const BridgeConfig& config) -> std::shared_ptr<DeviceComponentBridge> {
    auto bridge =
        std::make_shared<DeviceComponentBridge>(std::move(componentManager));
    bridge->initialize(config);
    return bridge;
}

}  // namespace lithium::device
