/*
 * ascom_backend.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM device backend implementation

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_ASCOM_BACKEND_HPP
#define LITHIUM_DEVICE_SERVICE_ASCOM_BACKEND_HPP

#include "device_backend.hpp"
#include "ascom_adapter.hpp"

#include <memory>
#include <mutex>

namespace lithium::device {

/**
 * @brief ASCOM device backend implementation
 *
 * Provides device discovery and management through ASCOM Alpaca protocol.
 * Wraps ASCOMAdapter to provide unified DeviceBackend interface.
 */
class ASCOMBackend : public DeviceBackend {
public:
    /**
     * @brief Construct with existing adapter
     * @param adapter ASCOM adapter instance
     */
    explicit ASCOMBackend(std::shared_ptr<ASCOMAdapter> adapter);

    /**
     * @brief Construct with new adapter
     */
    ASCOMBackend();

    ~ASCOMBackend() override;

    // ==================== Backend Identity ====================

    [[nodiscard]] auto getBackendName() const -> std::string override {
        return "ASCOM";
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

    // ==================== ASCOM-Specific ====================

    /**
     * @brief Get underlying ASCOM adapter
     * @return ASCOM adapter pointer
     */
    auto getAdapter() const -> std::shared_ptr<ASCOMAdapter> {
        return adapter_;
    }

    /**
     * @brief Execute device action
     * @param deviceId Device identifier
     * @param action Action name
     * @param parameters Action parameters
     * @return Action result
     */
    auto executeAction(const std::string& deviceId,
                       const std::string& action,
                       const std::string& parameters = "") -> std::string;

    /**
     * @brief Get supported actions for device
     * @param deviceId Device identifier
     * @return Vector of action names
     */
    auto getSupportedActions(const std::string& deviceId) -> std::vector<std::string>;

    /**
     * @brief Discover ASCOM Alpaca servers on network
     * @param timeout Discovery timeout in ms
     * @return Vector of server addresses
     */
    static auto discoverServers(int timeout = 5000) -> std::vector<std::string>;

private:
    /**
     * @brief Convert ASCOMDeviceInfo to DiscoveredDevice
     */
    auto convertToDiscoveredDevice(const ASCOMDeviceInfo& info) -> DiscoveredDevice;

    /**
     * @brief Convert ASCOMPropertyValue to JSON
     */
    auto convertPropertyToJson(const ASCOMPropertyValue& prop) -> json;

    /**
     * @brief Handle ASCOM events and forward to backend callback
     */
    void handleASCOMEvent(const ASCOMEvent& event);

    std::shared_ptr<ASCOMAdapter> adapter_;
    mutable std::mutex mutex_;
    std::vector<DiscoveredDevice> cachedDevices_;
};

/**
 * @brief ASCOM backend factory
 */
class ASCOMBackendFactory : public DeviceBackendFactory {
public:
    auto createBackend() -> std::shared_ptr<DeviceBackend> override {
        return std::make_shared<ASCOMBackend>();
    }

    [[nodiscard]] auto getBackendName() const -> std::string override {
        return "ASCOM";
    }

    /**
     * @brief Create backend with existing adapter
     */
    static auto createWithAdapter(std::shared_ptr<ASCOMAdapter> adapter)
        -> std::shared_ptr<ASCOMBackend> {
        return std::make_shared<ASCOMBackend>(std::move(adapter));
    }
};

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_ASCOM_BACKEND_HPP
