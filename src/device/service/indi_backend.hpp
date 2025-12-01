/*
 * indi_backend.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI device backend implementation

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_INDI_BACKEND_HPP
#define LITHIUM_DEVICE_SERVICE_INDI_BACKEND_HPP

#include "device_backend.hpp"
#include "indi_adapter.hpp"

#include <memory>
#include <mutex>

namespace lithium::device {

/**
 * @brief INDI device backend implementation
 *
 * Provides device discovery and management through INDI protocol.
 * Wraps INDIAdapter to provide unified DeviceBackend interface.
 */
class INDIBackend : public DeviceBackend {
public:
    /**
     * @brief Construct with existing adapter
     * @param adapter INDI adapter instance
     */
    explicit INDIBackend(std::shared_ptr<INDIAdapter> adapter);

    /**
     * @brief Construct with new adapter
     */
    INDIBackend();

    ~INDIBackend() override;

    // ==================== Backend Identity ====================

    [[nodiscard]] auto getBackendName() const -> std::string override {
        return "INDI";
    }

    [[nodiscard]] auto getBackendVersion() const -> std::string override {
        return "1.0.0";
    }

    // ==================== Server Connection ====================

    auto connectServer(const BackendConfig& config) -> bool override;
    auto disconnectServer() -> bool override;
    [[nodiscard]] auto isServerConnected() const -> bool override;
    [[nodiscard]] auto getServerStatus() const -> json override;

    // ==================== Device Discovery ====================

    auto discoverDevices(int timeout = 5000) -> std::vector<DiscoveredDevice> override;
    auto getDevices() -> std::vector<DiscoveredDevice> override;
    auto getDevice(const std::string& deviceId) -> std::optional<DiscoveredDevice> override;
    auto refreshDevices() -> int override;

    // ==================== Device Connection ====================

    auto connectDevice(const std::string& deviceId) -> bool override;
    auto disconnectDevice(const std::string& deviceId) -> bool override;
    [[nodiscard]] auto isDeviceConnected(const std::string& deviceId) const -> bool override;

    // ==================== Property Access ====================

    auto getProperty(const std::string& deviceId, const std::string& propertyName)
        -> std::optional<json> override;
    auto setProperty(const std::string& deviceId, const std::string& propertyName,
                     const json& value) -> bool override;
    auto getAllProperties(const std::string& deviceId)
        -> std::unordered_map<std::string, json> override;

    // ==================== Event System ====================

    void registerEventCallback(BackendEventCallback callback) override;
    void unregisterEventCallback() override;

    // ==================== INDI-Specific ====================

    /**
     * @brief Get underlying INDI adapter
     * @return INDI adapter pointer
     */
    auto getAdapter() const -> std::shared_ptr<INDIAdapter> {
        return adapter_;
    }

    /**
     * @brief Set number property
     */
    auto setNumberProperty(const std::string& deviceId,
                           const std::string& propertyName,
                           const std::string& elementName,
                           double value) -> bool;

    /**
     * @brief Set switch property
     */
    auto setSwitchProperty(const std::string& deviceId,
                           const std::string& propertyName,
                           const std::string& elementName,
                           bool value) -> bool;

    /**
     * @brief Set text property
     */
    auto setTextProperty(const std::string& deviceId,
                         const std::string& propertyName,
                         const std::string& elementName,
                         const std::string& value) -> bool;

    /**
     * @brief Wait for property state
     */
    auto waitForPropertyState(const std::string& deviceId,
                              const std::string& propertyName,
                              INDIPropertyState targetState,
                              std::chrono::milliseconds timeout = std::chrono::seconds(30))
        -> bool;

private:
    /**
     * @brief Convert INDIDeviceInfo to DiscoveredDevice
     */
    auto convertToDiscoveredDevice(const INDIDeviceInfo& info) -> DiscoveredDevice;

    /**
     * @brief Convert INDIPropertyValue to JSON
     */
    auto convertPropertyToJson(const INDIPropertyValue& prop) -> json;

    /**
     * @brief Handle INDI events and forward to backend callback
     */
    void handleINDIEvent(const INDIEvent& event);

    std::shared_ptr<INDIAdapter> adapter_;
    mutable std::mutex mutex_;
    std::vector<DiscoveredDevice> cachedDevices_;
};

/**
 * @brief INDI backend factory
 */
class INDIBackendFactory : public DeviceBackendFactory {
public:
    auto createBackend() -> std::shared_ptr<DeviceBackend> override {
        return std::make_shared<INDIBackend>();
    }

    [[nodiscard]] auto getBackendName() const -> std::string override {
        return "INDI";
    }

    /**
     * @brief Create backend with existing adapter
     */
    static auto createWithAdapter(std::shared_ptr<INDIAdapter> adapter)
        -> std::shared_ptr<INDIBackend> {
        return std::make_shared<INDIBackend>(std::move(adapter));
    }
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_INDI_BACKEND_HPP
