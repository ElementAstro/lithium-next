/*
 * device_component_adapter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Adapter to wrap device instances as components

**************************************************/

#ifndef LITHIUM_DEVICE_BRIDGE_DEVICE_COMPONENT_ADAPTER_HPP
#define LITHIUM_DEVICE_BRIDGE_DEVICE_COMPONENT_ADAPTER_HPP

#include <memory>
#include <shared_mutex>
#include <string>

#include "atom/components/component.hpp"
#include "atom/type/json.hpp"
#include "device/common/device_result.hpp"
#include "device/template/device.hpp"

namespace lithium::device {

/**
 * @brief Device component state (mirrors ComponentState with device semantics)
 */
enum class DeviceComponentState {
    Created,      ///< Device wrapper created but not initialized
    Initialized,  ///< Device initialized but not connected
    Connecting,   ///< Device is connecting
    Connected,    ///< Device connected and ready (Running)
    Paused,       ///< Device paused
    Disconnecting,///< Device is disconnecting
    Disconnected, ///< Device disconnected (Stopped)
    Error,        ///< Device in error state
    Disabled      ///< Device disabled
};

/**
 * @brief Convert device component state to string
 */
[[nodiscard]] auto deviceComponentStateToString(DeviceComponentState state)
    -> std::string;

/**
 * @brief Map device component state to component state
 */
[[nodiscard]] auto toComponentState(DeviceComponentState state)
    -> ComponentState;

/**
 * @brief Map component state to device component state
 */
[[nodiscard]] auto fromComponentState(ComponentState state)
    -> DeviceComponentState;

/**
 * @brief Device component adapter configuration
 */
struct DeviceAdapterConfig {
    std::string connectionPort;          ///< Connection port/address
    int connectionTimeout{5000};         ///< Connection timeout in ms
    int maxRetries{3};                   ///< Max connection retries
    bool autoConnect{false};             ///< Auto-connect on start
    bool autoReconnect{true};            ///< Auto-reconnect on disconnect
    int reconnectDelay{1000};            ///< Reconnect delay in ms
    nlohmann::json deviceConfig;         ///< Device-specific configuration

    // Lazy loading configuration
    bool lazyConnect{false};             ///< Connect on first use instead of start
    bool lazyInitialize{false};          ///< Initialize on first use

    // Metrics configuration
    bool collectMetrics{true};           ///< Collect performance metrics
    bool trackOperations{true};          ///< Track individual operations

    // Dependencies
    std::vector<std::string> dependencies; ///< Devices this adapter depends on

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> DeviceAdapterConfig;
};

/**
 * @brief Adapter class to wrap AtomDriver devices as Component instances
 *
 * This adapter allows devices to be managed through the ComponentManager,
 * providing a unified lifecycle management interface.
 *
 * Lifecycle mapping:
 * - Component::initialize() -> Device::initialize()
 * - Component::start() -> Device::connect()
 * - Component::stop() -> Device::disconnect()
 * - Component::destroy() -> Device::destroy()
 */
class DeviceComponentAdapter : public Component {
public:
    /**
     * @brief Construct adapter with device instance
     * @param device Device to wrap
     * @param name Component name (defaults to device name)
     */
    explicit DeviceComponentAdapter(std::shared_ptr<AtomDriver> device,
                                    const std::string& name = {});

    /**
     * @brief Construct adapter with device and configuration
     * @param device Device to wrap
     * @param config Adapter configuration
     * @param name Component name
     */
    DeviceComponentAdapter(std::shared_ptr<AtomDriver> device,
                           const DeviceAdapterConfig& config,
                           const std::string& name = {});

    ~DeviceComponentAdapter() override;

    // ==================== Component Interface ====================

    /**
     * @brief Initialize the device
     * @return true if initialization successful
     */
    auto initialize() -> bool override;

    /**
     * @brief Destroy the device
     * @return true if destruction successful
     */
    auto destroy() -> bool override;

    /**
     * @brief Get component name
     */
    [[nodiscard]] auto getName() const -> std::string override;

    // ==================== Extended Component Operations ====================

    /**
     * @brief Start the device (connect)
     * @return true if connected successfully
     */
    auto start() -> bool;

    /**
     * @brief Stop the device (disconnect)
     * @return true if disconnected successfully
     */
    auto stop() -> bool;

    /**
     * @brief Pause the device
     * @return true if paused successfully
     */
    auto pause() -> bool;

    /**
     * @brief Resume the device
     * @return true if resumed successfully
     */
    auto resume() -> bool;

    /**
     * @brief Check if device is healthy
     * @return true if device is operational
     */
    [[nodiscard]] auto isHealthy() const -> bool;

    // ==================== Device-Specific Operations ====================

    /**
     * @brief Get the wrapped device
     */
    [[nodiscard]] auto getDevice() const -> std::shared_ptr<AtomDriver>;

    /**
     * @brief Get the device UUID
     */
    [[nodiscard]] auto getDeviceUUID() const -> std::string;

    /**
     * @brief Get the device type
     */
    [[nodiscard]] auto getDeviceType() const -> std::string;

    /**
     * @brief Check if device is connected
     */
    [[nodiscard]] auto isConnected() const -> bool;

    /**
     * @brief Get current device component state
     */
    [[nodiscard]] auto getDeviceState() const -> DeviceComponentState;

    /**
     * @brief Connect to device
     * @param port Connection port/address
     * @param timeout Connection timeout in ms
     * @return Success or error
     */
    auto connect(const std::string& port = {}, int timeout = 0)
        -> DeviceResult<bool>;

    /**
     * @brief Disconnect from device
     * @return Success or error
     */
    auto disconnect() -> DeviceResult<bool>;

    /**
     * @brief Scan for available devices
     * @return List of available device ports/addresses
     */
    [[nodiscard]] auto scan() -> std::vector<std::string>;

    // ==================== Configuration ====================

    /**
     * @brief Update adapter configuration
     * @param config New configuration
     */
    void updateConfig(const DeviceAdapterConfig& config);

    /**
     * @brief Get current configuration
     */
    [[nodiscard]] auto getConfig() const -> DeviceAdapterConfig;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] auto getLastError() const -> std::string;

    // ==================== State Serialization ====================

    /**
     * @brief Save device state for migration
     * @return JSON containing device state
     */
    [[nodiscard]] auto saveState() const -> nlohmann::json;

    /**
     * @brief Restore device state from migration
     * @param state Previously saved state
     * @return Success or error
     */
    auto restoreState(const nlohmann::json& state) -> DeviceResult<bool>;

    // ==================== Component Info ====================

    /**
     * @brief Get component information as JSON
     */
    [[nodiscard]] auto getInfo() const -> nlohmann::json;

    /**
     * @brief Get component statistics
     */
    [[nodiscard]] auto getStatistics() const -> nlohmann::json;

    // ==================== Lazy Loading ====================

    /**
     * @brief Check if lazy loading is enabled
     */
    [[nodiscard]] auto isLazyConnect() const -> bool;

    /**
     * @brief Ensure device is connected (lazy connect if needed)
     * @return true if connected (or successfully connected)
     */
    auto ensureConnected() -> bool;

    /**
     * @brief Check if device needs lazy initialization
     */
    [[nodiscard]] auto needsLazyInit() const -> bool;

    // ==================== Dependencies ====================

    /**
     * @brief Get device dependencies
     */
    [[nodiscard]] auto getDependencies() const -> std::vector<std::string>;

    /**
     * @brief Check if all dependencies are satisfied
     * @param checkFunc Function to check if a dependency is ready
     */
    [[nodiscard]] auto areDependenciesSatisfied(
        std::function<bool(const std::string&)> checkFunc) const -> bool;

    /**
     * @brief Get device category (camera, mount, focuser, etc.)
     */
    [[nodiscard]] auto getDeviceCategory() const -> std::string;

private:
    /**
     * @brief Set device state
     */
    void setState(DeviceComponentState state);

    /**
     * @brief Set last error message
     */
    void setLastError(const std::string& error);

    /**
     * @brief Handle connection errors
     */
    void handleConnectionError(const std::string& error);

    /**
     * @brief Try to reconnect
     */
    void tryReconnect();

    // Members
    mutable std::shared_mutex mutex_;
    std::shared_ptr<AtomDriver> device_;
    std::string name_;
    DeviceAdapterConfig config_;
    DeviceComponentState state_{DeviceComponentState::Created};
    std::string lastError_;

    // Statistics
    size_t connectCount_{0};
    size_t disconnectCount_{0};
    size_t errorCount_{0};
    std::chrono::system_clock::time_point createdAt_;
    std::chrono::system_clock::time_point lastConnectedAt_;
    std::chrono::system_clock::time_point lastErrorAt_;
};

/**
 * @brief Factory function to create device component adapter
 * @param device Device to wrap
 * @param config Adapter configuration
 * @param name Component name
 * @return Shared pointer to adapter
 */
[[nodiscard]] auto createDeviceAdapter(
    std::shared_ptr<AtomDriver> device,
    const DeviceAdapterConfig& config = {},
    const std::string& name = {}) -> std::shared_ptr<DeviceComponentAdapter>;

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_BRIDGE_DEVICE_COMPONENT_ADAPTER_HPP
