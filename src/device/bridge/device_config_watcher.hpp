/*
 * device_config_watcher.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Configuration file watcher for device hot-reload

**************************************************/

#ifndef LITHIUM_DEVICE_BRIDGE_DEVICE_CONFIG_WATCHER_HPP
#define LITHIUM_DEVICE_BRIDGE_DEVICE_CONFIG_WATCHER_HPP

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "atom/type/json.hpp"

namespace fs = std::filesystem;

namespace lithium::device {

// Forward declarations
class DeviceComponentBridge;

/**
 * @brief Configuration change event types
 */
enum class ConfigChangeType {
    Created,   ///< New config file created
    Modified,  ///< Config file modified
    Deleted    ///< Config file deleted
};

/**
 * @brief Configuration change event
 */
struct ConfigChangeEvent {
    ConfigChangeType type;
    fs::path filePath;
    std::string deviceName;  ///< Extracted device name from file
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json newConfig;  ///< New config content (for Created/Modified)

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Configuration change callback
 */
using ConfigChangeCallback = std::function<void(const ConfigChangeEvent&)>;

/**
 * @brief Device configuration watcher configuration
 */
struct ConfigWatcherConfig {
    fs::path configDirectory;           ///< Directory to watch
    std::vector<std::string> fileTypes{".json", ".json5"};  ///< File extensions
    bool recursive{false};              ///< Watch subdirectories
    bool autoReload{true};              ///< Auto-reload on change
    int debounceMs{500};                ///< Debounce time for rapid changes

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> ConfigWatcherConfig;
};

/**
 * @brief Device configuration file watcher
 *
 * Monitors a directory for configuration file changes and triggers
 * device configuration updates through the DeviceComponentBridge.
 *
 * Features:
 * - File system monitoring using FileTracker
 * - Automatic configuration reload
 * - Debouncing for rapid changes
 * - Support for JSON and JSON5 formats
 * - Event callbacks for custom handling
 *
 * Usage:
 * @code
 * auto watcher = std::make_shared<DeviceConfigWatcher>(bridge);
 * watcher->configure({
 *     .configDirectory = "/path/to/configs",
 *     .autoReload = true
 * });
 * watcher->startWatching();
 * @endcode
 */
class DeviceConfigWatcher {
public:
    /**
     * @brief Construct watcher with bridge reference
     * @param bridge Device component bridge for config updates
     */
    explicit DeviceConfigWatcher(
        std::weak_ptr<DeviceComponentBridge> bridge);

    ~DeviceConfigWatcher();

    // Disable copy
    DeviceConfigWatcher(const DeviceConfigWatcher&) = delete;
    DeviceConfigWatcher& operator=(const DeviceConfigWatcher&) = delete;

    // ==================== Configuration ====================

    /**
     * @brief Configure the watcher
     * @param config Watcher configuration
     * @return true if configuration is valid
     */
    auto configure(const ConfigWatcherConfig& config) -> bool;

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] auto getConfig() const -> ConfigWatcherConfig;

    /**
     * @brief Set the configuration directory
     * @param dir Directory path
     */
    void setConfigDirectory(const fs::path& dir);

    /**
     * @brief Get the configuration directory
     */
    [[nodiscard]] auto getConfigDirectory() const -> fs::path;

    // ==================== Watching Control ====================

    /**
     * @brief Start watching for configuration changes
     * @return true if watching started successfully
     */
    auto startWatching() -> bool;

    /**
     * @brief Stop watching for configuration changes
     */
    void stopWatching();

    /**
     * @brief Check if currently watching
     */
    [[nodiscard]] auto isWatching() const -> bool;

    /**
     * @brief Perform a manual scan for changes
     */
    void scanOnce();

    // ==================== Event Subscription ====================

    /**
     * @brief Subscribe to configuration change events
     * @param callback Event callback
     * @return Subscription ID
     */
    auto subscribe(ConfigChangeCallback callback) -> uint64_t;

    /**
     * @brief Unsubscribe from events
     * @param subscriptionId Subscription ID
     */
    void unsubscribe(uint64_t subscriptionId);

    // ==================== Manual Operations ====================

    /**
     * @brief Load a configuration file manually
     * @param filePath Path to configuration file
     * @return Parsed configuration or empty JSON on error
     */
    [[nodiscard]] auto loadConfigFile(const fs::path& filePath) const
        -> nlohmann::json;

    /**
     * @brief Extract device name from configuration file
     * @param filePath Path to configuration file
     * @return Device name or empty string
     */
    [[nodiscard]] auto extractDeviceName(const fs::path& filePath) const
        -> std::string;

    /**
     * @brief Reload configuration for a specific device
     * @param deviceName Device name
     * @return true if reload was successful
     */
    auto reloadDevice(const std::string& deviceName) -> bool;

    /**
     * @brief Reload all device configurations
     * @return Number of devices reloaded
     */
    auto reloadAll() -> size_t;

    // ==================== Statistics ====================

    /**
     * @brief Get watcher statistics
     */
    [[nodiscard]] auto getStatistics() const -> nlohmann::json;

private:
    /**
     * @brief Handle file change from FileTracker
     */
    void onFileChange(const fs::path& path, const std::string& changeType);

    /**
     * @brief Apply configuration change to device
     */
    void applyConfigChange(const ConfigChangeEvent& event);

    /**
     * @brief Emit event to subscribers
     */
    void emitEvent(const ConfigChangeEvent& event);

    /**
     * @brief Create a change event
     */
    [[nodiscard]] auto createEvent(ConfigChangeType type,
                                    const fs::path& path) const
        -> ConfigChangeEvent;

    // PIMPL for FileTracker integration
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Configuration
    ConfigWatcherConfig config_;
    bool watching_{false};

    // Bridge reference
    std::weak_ptr<DeviceComponentBridge> bridge_;

    // Event subscribers
    mutable std::shared_mutex eventMutex_;
    std::unordered_map<uint64_t, ConfigChangeCallback> eventSubscribers_;
    uint64_t nextSubscriberId_{1};

    // Statistics
    size_t changesDetected_{0};
    size_t reloadsSuccessful_{0};
    size_t reloadsFailed_{0};
};

/**
 * @brief Factory function to create config watcher
 * @param bridge Device component bridge
 * @param config Watcher configuration
 * @return Shared pointer to watcher
 */
[[nodiscard]] auto createDeviceConfigWatcher(
    std::weak_ptr<DeviceComponentBridge> bridge,
    const ConfigWatcherConfig& config = {})
    -> std::shared_ptr<DeviceConfigWatcher>;

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_BRIDGE_DEVICE_CONFIG_WATCHER_HPP
