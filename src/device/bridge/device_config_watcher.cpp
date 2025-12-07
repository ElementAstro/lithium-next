/*
 * device_config_watcher.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Configuration file watcher implementation

**************************************************/

#include "device_config_watcher.hpp"

#include "components/core/tracker.hpp"
#include "device_component_bridge.hpp"

#include <fstream>
#include <loguru.hpp>

namespace lithium::device {

// ============================================================================
// ConfigChangeEvent Implementation
// ============================================================================

auto ConfigChangeEvent::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["type"] = static_cast<int>(type);
    j["filePath"] = filePath.string();
    j["deviceName"] = deviceName;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["newConfig"] = newConfig;
    return j;
}

// ============================================================================
// ConfigWatcherConfig Implementation
// ============================================================================

auto ConfigWatcherConfig::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["configDirectory"] = configDirectory.string();
    j["fileTypes"] = fileTypes;
    j["recursive"] = recursive;
    j["autoReload"] = autoReload;
    j["debounceMs"] = debounceMs;
    return j;
}

auto ConfigWatcherConfig::fromJson(const nlohmann::json& j)
    -> ConfigWatcherConfig {
    ConfigWatcherConfig config;

    if (j.contains("configDirectory")) {
        config.configDirectory = j["configDirectory"].get<std::string>();
    }
    if (j.contains("fileTypes")) {
        config.fileTypes = j["fileTypes"].get<std::vector<std::string>>();
    }
    if (j.contains("recursive")) {
        config.recursive = j["recursive"].get<bool>();
    }
    if (j.contains("autoReload")) {
        config.autoReload = j["autoReload"].get<bool>();
    }
    if (j.contains("debounceMs")) {
        config.debounceMs = j["debounceMs"].get<int>();
    }

    return config;
}

// ============================================================================
// DeviceConfigWatcher::Impl
// ============================================================================

struct DeviceConfigWatcher::Impl {
    std::unique_ptr<FileTracker> tracker;
    fs::path jsonCachePath;

    // Debounce state
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        lastChangeTime;
    std::mutex debounceMutex;
};

// ============================================================================
// DeviceConfigWatcher Implementation
// ============================================================================

DeviceConfigWatcher::DeviceConfigWatcher(
    std::weak_ptr<DeviceComponentBridge> bridge)
    : impl_(std::make_unique<Impl>()), bridge_(std::move(bridge)) {}

DeviceConfigWatcher::~DeviceConfigWatcher() {
    if (watching_) {
        stopWatching();
    }
}

auto DeviceConfigWatcher::configure(const ConfigWatcherConfig& config) -> bool {
    if (watching_) {
        LOG_F(WARNING, "Cannot configure while watching, stop first");
        return false;
    }

    if (!fs::exists(config.configDirectory)) {
        LOG_F(WARNING, "Config directory does not exist: %s",
              config.configDirectory.string().c_str());
        return false;
    }

    config_ = config;

    // Set up cache path for FileTracker
    impl_->jsonCachePath = config_.configDirectory / ".device_config_cache.json";

    LOG_F(INFO, "DeviceConfigWatcher configured for directory: %s",
          config_.configDirectory.string().c_str());

    return true;
}

auto DeviceConfigWatcher::getConfig() const -> ConfigWatcherConfig {
    return config_;
}

void DeviceConfigWatcher::setConfigDirectory(const fs::path& dir) {
    config_.configDirectory = dir;
}

auto DeviceConfigWatcher::getConfigDirectory() const -> fs::path {
    return config_.configDirectory;
}

auto DeviceConfigWatcher::startWatching() -> bool {
    if (watching_) {
        return true;  // Already watching
    }

    if (config_.configDirectory.empty() ||
        !fs::exists(config_.configDirectory)) {
        LOG_F(ERROR, "Invalid config directory");
        return false;
    }

    try {
        // Create FileTracker
        impl_->tracker = std::make_unique<FileTracker>(
            config_.configDirectory.string(),
            impl_->jsonCachePath.string(),
            std::span<const std::string>(config_.fileTypes),
            config_.recursive);

        // Set change callback
        impl_->tracker->setChangeCallback(
            [this](const fs::path& path, const std::string& changeType) {
                onFileChange(path, changeType);
            });

        // Initial scan
        impl_->tracker->scan();

        // Start watching
        impl_->tracker->startWatching();

        watching_ = true;
        LOG_F(INFO, "Started watching config directory: %s",
              config_.configDirectory.string().c_str());

        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to start watching: %s", e.what());
        return false;
    }
}

void DeviceConfigWatcher::stopWatching() {
    if (!watching_) {
        return;
    }

    if (impl_->tracker) {
        impl_->tracker->stopWatching();
        impl_->tracker.reset();
    }

    watching_ = false;
    LOG_F(INFO, "Stopped watching config directory");
}

auto DeviceConfigWatcher::isWatching() const -> bool {
    return watching_;
}

void DeviceConfigWatcher::scanOnce() {
    if (impl_->tracker) {
        impl_->tracker->scan();
        impl_->tracker->compare();

        // Process any differences
        auto differences = impl_->tracker->getDifferences();
        for (const auto& [key, value] : differences.items()) {
            if (key == "new_files") {
                for (const auto& file : value) {
                    auto event = createEvent(ConfigChangeType::Created,
                                             file.get<std::string>());
                    emitEvent(event);
                    if (config_.autoReload) {
                        applyConfigChange(event);
                    }
                }
            } else if (key == "modified_files") {
                for (const auto& file : value) {
                    auto event = createEvent(ConfigChangeType::Modified,
                                             file.get<std::string>());
                    emitEvent(event);
                    if (config_.autoReload) {
                        applyConfigChange(event);
                    }
                }
            } else if (key == "deleted_files") {
                for (const auto& file : value) {
                    auto event = createEvent(ConfigChangeType::Deleted,
                                             file.get<std::string>());
                    emitEvent(event);
                }
            }
        }
    }
}

auto DeviceConfigWatcher::subscribe(ConfigChangeCallback callback) -> uint64_t {
    std::unique_lock lock(eventMutex_);
    uint64_t id = nextSubscriberId_++;
    eventSubscribers_[id] = std::move(callback);
    return id;
}

void DeviceConfigWatcher::unsubscribe(uint64_t subscriptionId) {
    std::unique_lock lock(eventMutex_);
    eventSubscribers_.erase(subscriptionId);
}

auto DeviceConfigWatcher::loadConfigFile(const fs::path& filePath) const
    -> nlohmann::json {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            LOG_F(WARNING, "Cannot open config file: %s",
                  filePath.string().c_str());
            return nlohmann::json{};
        }

        nlohmann::json config;
        file >> config;
        return config;
    } catch (const nlohmann::json::parse_error& e) {
        LOG_F(ERROR, "Failed to parse config file %s: %s",
              filePath.string().c_str(), e.what());
        return nlohmann::json{};
    }
}

auto DeviceConfigWatcher::extractDeviceName(const fs::path& filePath) const
    -> std::string {
    // Try to extract device name from:
    // 1. File name (e.g., "camera_zwo.json" -> "camera_zwo")
    // 2. Content field "name" or "deviceName"

    // First, try file name
    std::string name = filePath.stem().string();

    // Try to load and get name from content
    auto config = loadConfigFile(filePath);
    if (!config.empty()) {
        if (config.contains("name")) {
            return config["name"].get<std::string>();
        }
        if (config.contains("deviceName")) {
            return config["deviceName"].get<std::string>();
        }
    }

    return name;
}

auto DeviceConfigWatcher::reloadDevice(const std::string& deviceName) -> bool {
    auto bridge = bridge_.lock();
    if (!bridge) {
        LOG_F(ERROR, "Bridge is no longer available");
        return false;
    }

    // Find config file for this device
    fs::path configFile;
    for (const auto& entry : fs::directory_iterator(config_.configDirectory)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (std::find(config_.fileTypes.begin(), config_.fileTypes.end(),
                          ext) != config_.fileTypes.end()) {
                if (extractDeviceName(entry.path()) == deviceName) {
                    configFile = entry.path();
                    break;
                }
            }
        }
    }

    if (configFile.empty()) {
        LOG_F(WARNING, "No config file found for device: %s", deviceName.c_str());
        return false;
    }

    auto config = loadConfigFile(configFile);
    if (config.empty()) {
        return false;
    }

    auto result = bridge->updateDeviceConfig(deviceName, config);
    if (result) {
        reloadsSuccessful_++;
        LOG_F(INFO, "Reloaded config for device: %s", deviceName.c_str());
        return true;
    } else {
        reloadsFailed_++;
        LOG_F(ERROR, "Failed to reload config for device: %s", deviceName.c_str());
        return false;
    }
}

auto DeviceConfigWatcher::reloadAll() -> size_t {
    size_t count = 0;

    for (const auto& entry : fs::directory_iterator(config_.configDirectory)) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (std::find(config_.fileTypes.begin(), config_.fileTypes.end(),
                          ext) != config_.fileTypes.end()) {
                std::string deviceName = extractDeviceName(entry.path());
                if (reloadDevice(deviceName)) {
                    count++;
                }
            }
        }
    }

    return count;
}

auto DeviceConfigWatcher::getStatistics() const -> nlohmann::json {
    nlohmann::json stats;
    stats["watching"] = watching_;
    stats["configDirectory"] = config_.configDirectory.string();
    stats["changesDetected"] = changesDetected_;
    stats["reloadsSuccessful"] = reloadsSuccessful_;
    stats["reloadsFailed"] = reloadsFailed_;

    if (impl_->tracker) {
        auto trackerStats = impl_->tracker->getStatistics();
        stats["trackerStats"] = trackerStats;
    }

    return stats;
}

void DeviceConfigWatcher::onFileChange(const fs::path& path,
                                        const std::string& changeType) {
    // Check if this is a tracked file type
    auto ext = path.extension().string();
    if (std::find(config_.fileTypes.begin(), config_.fileTypes.end(), ext) ==
        config_.fileTypes.end()) {
        return;
    }

    // Debounce rapid changes
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(impl_->debounceMutex);
        auto pathStr = path.string();
        auto it = impl_->lastChangeTime.find(pathStr);
        if (it != impl_->lastChangeTime.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - it->second)
                               .count();
            if (elapsed < config_.debounceMs) {
                return;  // Ignore rapid change
            }
        }
        impl_->lastChangeTime[pathStr] = now;
    }

    changesDetected_++;

    // Determine change type
    ConfigChangeType type;
    if (changeType == "created" || changeType == "new") {
        type = ConfigChangeType::Created;
    } else if (changeType == "modified" || changeType == "changed") {
        type = ConfigChangeType::Modified;
    } else if (changeType == "deleted" || changeType == "removed") {
        type = ConfigChangeType::Deleted;
    } else {
        type = ConfigChangeType::Modified;  // Default to modified
    }

    auto event = createEvent(type, path);
    emitEvent(event);

    if (config_.autoReload && type != ConfigChangeType::Deleted) {
        applyConfigChange(event);
    }

    LOG_F(INFO, "Config file %s: %s", changeType.c_str(), path.string().c_str());
}

void DeviceConfigWatcher::applyConfigChange(const ConfigChangeEvent& event) {
    auto bridge = bridge_.lock();
    if (!bridge) {
        LOG_F(ERROR, "Bridge is no longer available");
        reloadsFailed_++;
        return;
    }

    if (event.deviceName.empty()) {
        LOG_F(WARNING, "Cannot apply config: no device name");
        reloadsFailed_++;
        return;
    }

    // Check if device is registered
    if (!bridge->isDeviceRegistered(event.deviceName)) {
        LOG_F(DEBUG, "Device not registered, skipping config update: %s",
              event.deviceName.c_str());
        return;
    }

    auto result = bridge->updateDeviceConfig(event.deviceName, event.newConfig);
    if (result) {
        reloadsSuccessful_++;
        LOG_F(INFO, "Applied config change for device: %s",
              event.deviceName.c_str());
    } else {
        reloadsFailed_++;
        LOG_F(ERROR, "Failed to apply config change for device: %s",
              event.deviceName.c_str());
    }
}

void DeviceConfigWatcher::emitEvent(const ConfigChangeEvent& event) {
    std::shared_lock lock(eventMutex_);
    for (const auto& [id, callback] : eventSubscribers_) {
        try {
            callback(event);
        } catch (...) {
            // Ignore callback exceptions
        }
    }
}

auto DeviceConfigWatcher::createEvent(ConfigChangeType type,
                                       const fs::path& path) const
    -> ConfigChangeEvent {
    ConfigChangeEvent event;
    event.type = type;
    event.filePath = path;
    event.deviceName = extractDeviceName(path);
    event.timestamp = std::chrono::system_clock::now();

    if (type != ConfigChangeType::Deleted && fs::exists(path)) {
        event.newConfig = loadConfigFile(path);
    }

    return event;
}

// ============================================================================
// Factory Function
// ============================================================================

auto createDeviceConfigWatcher(std::weak_ptr<DeviceComponentBridge> bridge,
                                const ConfigWatcherConfig& config)
    -> std::shared_ptr<DeviceConfigWatcher> {
    auto watcher = std::make_shared<DeviceConfigWatcher>(std::move(bridge));
    if (!config.configDirectory.empty()) {
        watcher->configure(config);
    }
    return watcher;
}

}  // namespace lithium::device
