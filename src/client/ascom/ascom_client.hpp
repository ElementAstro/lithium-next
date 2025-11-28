/*
 * ascom_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: ASCOM server client implementation

*************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_CLIENT_HPP
#define LITHIUM_CLIENT_ASCOM_CLIENT_HPP

#include "../common/server_client.hpp"
#include "alpaca_client.hpp"
#include "ascom_types.hpp"

#include <memory>

namespace lithium::client {

/**
 * @brief ASCOM-specific driver information (extends base DriverInfo)
 */
struct ASCOMDriverInfo : public DriverInfo {
    ascom::ASCOMDeviceType deviceType{ascom::ASCOMDeviceType::Unknown};
    int deviceNumber{0};
    std::string uniqueId;
    std::string progId;  // COM ProgID for native ASCOM
    
    ASCOMDriverInfo() {
        backend = "ASCOM";
    }
    
    // Convert from ASCOMDeviceDescription
    static ASCOMDriverInfo fromDescription(const ascom::ASCOMDeviceDescription& desc) {
        ASCOMDriverInfo info;
        info.name = desc.deviceName;
        info.label = desc.deviceName;
        info.deviceType = desc.deviceType;
        info.deviceNumber = desc.deviceNumber;
        info.uniqueId = desc.uniqueId;
        info.backend = "ASCOM";
        return info;
    }
};

/**
 * @brief ASCOM server client
 *
 * Manages ASCOM Alpaca server and device lifecycle.
 * Implements the ServerClient interface for unified device management.
 */
class ASCOMClient : public ServerClient {
public:
    /**
     * @brief Construct a new ASCOMClient
     * @param name Instance name
     */
    explicit ASCOMClient(std::string name = "ascom");

    /**
     * @brief Destructor
     */
    ~ASCOMClient() override;

    // ==================== Lifecycle ====================

    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& target, int timeout = 5000,
                 int maxRetry = 3) override;
    bool disconnect() override;
    bool isConnected() const override;
    std::vector<std::string> scan() override;

    // ==================== Server Control ====================

    bool startServer() override;
    bool stopServer() override;
    bool isServerRunning() const override;
    bool isInstalled() const override;

    // ==================== Driver Management ====================

    bool startDriver(const DriverInfo& driver) override;
    bool stopDriver(const std::string& driverName) override;
    std::unordered_map<std::string, DriverInfo> getRunningDrivers() const override;
    std::vector<DriverInfo> getAvailableDrivers() const override;

    // ==================== Device Management ====================

    std::vector<DeviceInfo> getDevices() const override;
    std::optional<DeviceInfo> getDevice(const std::string& name) const override;
    bool connectDevice(const std::string& deviceName) override;
    bool disconnectDevice(const std::string& deviceName) override;

    // ==================== Property Access ====================

    bool setProperty(const std::string& device,
                     const std::string& property,
                     const std::string& element,
                     const std::string& value) override;

    std::string getProperty(const std::string& device,
                            const std::string& property,
                            const std::string& element) const override;

    std::string getPropertyState(const std::string& device,
                                 const std::string& property) const override;

    // ==================== ASCOM-Specific ====================

    /**
     * @brief Configure ASCOM server
     * @param host Host address
     * @param port Port number (default 11111 for Alpaca)
     */
    void configureASCOM(const std::string& host = "localhost",
                        int port = 11111);

    /**
     * @brief Get Alpaca client
     * @return Alpaca client pointer
     */
    ascom::AlpacaClient* getAlpacaClient() const { return alpacaClient_.get(); }

    /**
     * @brief Get server info
     * @return Server info or nullopt
     */
    std::optional<ascom::AlpacaServerInfo> getAlpacaServerInfo();

    /**
     * @brief Discover ASCOM Alpaca servers on network
     * @param timeout Discovery timeout in ms
     * @return Vector of server addresses
     */
    static std::vector<std::string> discoverServers(int timeout = 5000);

    /**
     * @brief Get backend name
     * @return "ASCOM"
     */
    [[nodiscard]] std::string getBackendName() const override { return "ASCOM"; }

    /**
     * @brief Execute device action
     * @param deviceName Device name
     * @param action Action name
     * @param parameters Action parameters (JSON)
     * @return Action result
     */
    std::string executeAction(const std::string& deviceName,
                              const std::string& action,
                              const std::string& parameters = "");

    /**
     * @brief Get supported actions for device
     * @param deviceName Device name
     * @return Vector of action names
     */
    std::vector<std::string> getSupportedActions(const std::string& deviceName) const;

private:
    // Helper to convert device description to DeviceInfo
    DeviceInfo convertToDeviceInfo(const ascom::ASCOMDeviceDescription& desc) const;
    
    // Helper to find device by name
    std::optional<std::pair<ascom::ASCOMDeviceType, int>> findDevice(
        const std::string& deviceName) const;
    
    // Convert ASCOM device type to DeviceInterface flags
    static DeviceInterface deviceTypeToInterface(ascom::ASCOMDeviceType type);

    std::unique_ptr<ascom::AlpacaClient> alpacaClient_;
    std::vector<ASCOMDriverInfo> availableDrivers_;
    
    // Cache of discovered devices
    mutable std::vector<ascom::ASCOMDeviceDescription> deviceCache_;
    mutable std::mutex cacheMutex_;

    std::string ascomHost_{"localhost"};
    int ascomPort_{11111};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_ASCOM_CLIENT_HPP
