/*
 * device_plugin_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device plugin interface implementation

**************************************************/

#include "device_plugin_interface.hpp"

#include <chrono>

namespace lithium::device {

// ============================================================================
// DevicePluginMetadata Implementation
// ============================================================================

auto DevicePluginMetadata::toJson() const -> nlohmann::json {
    nlohmann::json j = PluginMetadata::toJson();
    j["backendName"] = backendName;
    j["backendVersion"] = backendVersion;
    j["supportsHotPlug"] = supportsHotPlug;
    j["supportsAutoDiscovery"] = supportsAutoDiscovery;
    j["requiresServer"] = requiresServer;
    j["supportedDeviceCategories"] = supportedDeviceCategories;
    return j;
}

auto DevicePluginMetadata::fromJson(const nlohmann::json& j)
    -> DevicePluginMetadata {
    DevicePluginMetadata metadata;

    // Parse base metadata
    if (j.contains("name")) {
        metadata.name = j["name"].get<std::string>();
    }
    if (j.contains("version")) {
        metadata.version = j["version"].get<std::string>();
    }
    if (j.contains("description")) {
        metadata.description = j["description"].get<std::string>();
    }
    if (j.contains("author")) {
        metadata.author = j["author"].get<std::string>();
    }
    if (j.contains("license")) {
        metadata.license = j["license"].get<std::string>();
    }
    if (j.contains("homepage")) {
        metadata.homepage = j["homepage"].get<std::string>();
    }
    if (j.contains("repository")) {
        metadata.repository = j["repository"].get<std::string>();
    }
    if (j.contains("priority")) {
        metadata.priority = j["priority"].get<int>();
    }
    if (j.contains("dependencies")) {
        metadata.dependencies =
            j["dependencies"].get<std::vector<std::string>>();
    }
    if (j.contains("optionalDeps")) {
        metadata.optionalDeps =
            j["optionalDeps"].get<std::vector<std::string>>();
    }
    if (j.contains("conflicts")) {
        metadata.conflicts = j["conflicts"].get<std::vector<std::string>>();
    }
    if (j.contains("tags")) {
        metadata.tags = j["tags"].get<std::vector<std::string>>();
    }
    if (j.contains("capabilities")) {
        metadata.capabilities =
            j["capabilities"].get<std::vector<std::string>>();
    }

    // Parse device-specific metadata
    if (j.contains("backendName")) {
        metadata.backendName = j["backendName"].get<std::string>();
    }
    if (j.contains("backendVersion")) {
        metadata.backendVersion = j["backendVersion"].get<std::string>();
    }
    if (j.contains("supportsHotPlug")) {
        metadata.supportsHotPlug = j["supportsHotPlug"].get<bool>();
    }
    if (j.contains("supportsAutoDiscovery")) {
        metadata.supportsAutoDiscovery = j["supportsAutoDiscovery"].get<bool>();
    }
    if (j.contains("requiresServer")) {
        metadata.requiresServer = j["requiresServer"].get<bool>();
    }
    if (j.contains("supportedDeviceCategories")) {
        metadata.supportedDeviceCategories =
            j["supportedDeviceCategories"].get<std::vector<std::string>>();
    }

    return metadata;
}

// ============================================================================
// DevicePluginState String Conversion
// ============================================================================

auto devicePluginStateToString(DevicePluginState state) -> std::string {
    switch (state) {
        case DevicePluginState::Unloaded:
            return "Unloaded";
        case DevicePluginState::Loading:
            return "Loading";
        case DevicePluginState::Loaded:
            return "Loaded";
        case DevicePluginState::Initializing:
            return "Initializing";
        case DevicePluginState::Ready:
            return "Ready";
        case DevicePluginState::Running:
            return "Running";
        case DevicePluginState::Paused:
            return "Paused";
        case DevicePluginState::Stopping:
            return "Stopping";
        case DevicePluginState::Error:
            return "Error";
        case DevicePluginState::Disabled:
            return "Disabled";
        default:
            return "Unknown";
    }
}

// ============================================================================
// DevicePluginEvent Implementation
// ============================================================================

auto DevicePluginEvent::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["type"] = static_cast<int>(type);
    j["pluginName"] = pluginName;
    j["typeName"] = typeName;
    j["deviceId"] = deviceId;
    j["message"] = message;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["data"] = data;
    return j;
}

// ============================================================================
// DevicePluginBase Implementation
// ============================================================================

DevicePluginBase::DevicePluginBase(DevicePluginMetadata metadata)
    : metadata_(std::move(metadata)) {
    statistics_.loadTime = std::chrono::system_clock::now();
}

auto DevicePluginBase::getMetadata() const
    -> const server::plugin::PluginMetadata& {
    return metadata_;
}

auto DevicePluginBase::initialize(const nlohmann::json& config) -> bool {
    setState(DevicePluginState::Initializing);
    config_ = config;

    try {
        // Validate configuration
        auto [valid, errorMsg] = validateConfig(config);
        if (!valid) {
            setLastError("Configuration validation failed: " + errorMsg);
            setState(DevicePluginState::Error);
            return false;
        }

        setState(DevicePluginState::Ready);
        pluginState_ = server::plugin::PluginState::Initialized;
        return true;
    } catch (const std::exception& e) {
        setLastError(std::string("Initialization failed: ") + e.what());
        setState(DevicePluginState::Error);
        return false;
    }
}

void DevicePluginBase::shutdown() {
    setState(DevicePluginState::Stopping);

    // Stop backend if running
    if (isBackendRunning()) {
        stopBackend();
    }

    // Clear subscribers
    {
        std::unique_lock lock(eventMutex_);
        eventSubscribers_.clear();
    }

    setState(DevicePluginState::Unloaded);
    pluginState_ = server::plugin::PluginState::Unloaded;
}

auto DevicePluginBase::getState() const -> server::plugin::PluginState {
    return pluginState_;
}

auto DevicePluginBase::getLastError() const -> std::string {
    return lastError_;
}

auto DevicePluginBase::isHealthy() const -> bool {
    return state_ == DevicePluginState::Ready ||
           state_ == DevicePluginState::Running;
}

auto DevicePluginBase::pause() -> bool {
    if (state_ != DevicePluginState::Running) {
        return false;
    }

    setState(DevicePluginState::Paused);
    pluginState_ = server::plugin::PluginState::Paused;
    return true;
}

auto DevicePluginBase::resume() -> bool {
    if (state_ != DevicePluginState::Paused) {
        return false;
    }

    setState(DevicePluginState::Running);
    pluginState_ = server::plugin::PluginState::Running;
    return true;
}

auto DevicePluginBase::getStatistics() const
    -> server::plugin::PluginStatistics {
    auto stats = statistics_;
    stats.lastAccessTime = std::chrono::system_clock::now();
    return stats;
}

auto DevicePluginBase::updateConfig(const nlohmann::json& config) -> bool {
    auto [valid, errorMsg] = validateConfig(config);
    if (!valid) {
        setLastError("Configuration validation failed: " + errorMsg);
        return false;
    }

    config_ = config;
    return true;
}

auto DevicePluginBase::getConfig() const -> nlohmann::json {
    return config_;
}

auto DevicePluginBase::getDeviceMetadata() const -> DevicePluginMetadata {
    return metadata_;
}

auto DevicePluginBase::getDevicePluginState() const -> DevicePluginState {
    return state_;
}

auto DevicePluginBase::subscribeEvents(DevicePluginEventCallback callback)
    -> uint64_t {
    std::unique_lock lock(eventMutex_);
    uint64_t id = nextSubscriberId_++;
    eventSubscribers_[id] = std::move(callback);
    return id;
}

void DevicePluginBase::unsubscribeEvents(uint64_t subscriptionId) {
    std::unique_lock lock(eventMutex_);
    eventSubscribers_.erase(subscriptionId);
}

auto DevicePluginBase::supportsHotPlug() const -> bool {
    return metadata_.supportsHotPlug;
}

auto DevicePluginBase::prepareHotPlug()
    -> DeviceResult<std::vector<DeviceMigrationContext>> {
    if (!supportsHotPlug()) {
        return std::unexpected(
            error::pluginError(metadata_.name, "Hot-plug not supported"));
    }

    emitEvent(createEvent(DevicePluginEventType::HotPlugStarted,
                          "Preparing for hot-plug"));

    // Default implementation returns empty list
    // Derived classes should override to save device states
    return std::vector<DeviceMigrationContext>{};
}

auto DevicePluginBase::completeHotPlug(
    const std::vector<DeviceMigrationContext>& contexts) -> DeviceResult<bool> {
    if (!supportsHotPlug()) {
        return std::unexpected(
            error::pluginError(metadata_.name, "Hot-plug not supported"));
    }

    // Default implementation does nothing
    // Derived classes should override to restore device states

    emitEvent(createEvent(DevicePluginEventType::HotPlugCompleted,
                          "Hot-plug completed"));

    return true;
}

void DevicePluginBase::abortHotPlug(
    const std::vector<DeviceMigrationContext>& contexts) {
    // Default implementation does nothing
    // Derived classes should override to handle abort
    emitEvent(createEvent(DevicePluginEventType::Error, "Hot-plug aborted"));
}

void DevicePluginBase::setState(DevicePluginState state) {
    state_ = state;

    // Map device plugin state to server plugin state
    switch (state) {
        case DevicePluginState::Unloaded:
            pluginState_ = server::plugin::PluginState::Unloaded;
            break;
        case DevicePluginState::Loading:
            pluginState_ = server::plugin::PluginState::Loading;
            break;
        case DevicePluginState::Loaded:
            pluginState_ = server::plugin::PluginState::Loaded;
            break;
        case DevicePluginState::Initializing:
        case DevicePluginState::Ready:
            pluginState_ = server::plugin::PluginState::Initialized;
            break;
        case DevicePluginState::Running:
            pluginState_ = server::plugin::PluginState::Running;
            break;
        case DevicePluginState::Paused:
            pluginState_ = server::plugin::PluginState::Paused;
            break;
        case DevicePluginState::Stopping:
            pluginState_ = server::plugin::PluginState::Stopping;
            break;
        case DevicePluginState::Error:
            pluginState_ = server::plugin::PluginState::Error;
            break;
        case DevicePluginState::Disabled:
            pluginState_ = server::plugin::PluginState::Disabled;
            break;
    }
}

void DevicePluginBase::setLastError(const std::string& error) {
    lastError_ = error;
    statistics_.errorCount++;
}

void DevicePluginBase::emitEvent(const DevicePluginEvent& event) {
    std::shared_lock lock(eventMutex_);
    for (const auto& [id, callback] : eventSubscribers_) {
        try {
            callback(event);
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

auto DevicePluginBase::createEvent(DevicePluginEventType type,
                                    const std::string& message) const
    -> DevicePluginEvent {
    DevicePluginEvent event;
    event.type = type;
    event.pluginName = metadata_.name;
    event.message = message;
    event.timestamp = std::chrono::system_clock::now();
    return event;
}

}  // namespace lithium::device
