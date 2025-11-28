/*
 * server_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Base class for device server clients (INDI, etc.)

*************************************************/

#ifndef LITHIUM_CLIENT_SERVER_CLIENT_HPP
#define LITHIUM_CLIENT_SERVER_CLIENT_HPP

#include "client_base.hpp"

#include <unordered_map>

namespace lithium::client {

/**
 * @brief Device information structure
 */
struct DeviceInfo {
    std::string name;
    std::string driver;
    std::string version;
    std::string interface;
    bool connected{false};
    std::unordered_map<std::string, std::string> properties;
};

/**
 * @brief Driver information structure
 */
struct DriverInfo {
    std::string name;
    std::string label;
    std::string version;
    std::string binary;
    std::string skeleton;
    bool running{false};
};

/**
 * @brief Server configuration
 */
struct ServerConfig {
    std::string host{"localhost"};
    int port{7624};
    std::string configPath;
    std::string dataPath;
    std::string fifoPath;
    int maxClients{100};
    bool verbose{false};
};

/**
 * @brief Base class for device server clients
 *
 * Provides common interface for INDI server and similar device servers
 */
class ServerClient : public ClientBase {
public:
    /**
     * @brief Construct a new ServerClient
     * @param name Server name
     */
    explicit ServerClient(std::string name);

    /**
     * @brief Virtual destructor
     */
    ~ServerClient() override;

    // ==================== Server Control ====================

    /**
     * @brief Start the server
     * @return true if server started successfully
     */
    virtual bool startServer() = 0;

    /**
     * @brief Stop the server
     * @return true if server stopped successfully
     */
    virtual bool stopServer() = 0;

    /**
     * @brief Check if server is running
     * @return true if server is running
     */
    virtual bool isServerRunning() const = 0;

    /**
     * @brief Check if server software is installed
     * @return true if installed
     */
    virtual bool isInstalled() const = 0;

    // ==================== Driver Management ====================

    /**
     * @brief Start a driver
     * @param driver Driver information
     * @return true if driver started successfully
     */
    virtual bool startDriver(const DriverInfo& driver) = 0;

    /**
     * @brief Stop a driver
     * @param driverName Driver name
     * @return true if driver stopped successfully
     */
    virtual bool stopDriver(const std::string& driverName) = 0;

    /**
     * @brief Get running drivers
     * @return Map of driver name to driver info
     */
    virtual std::unordered_map<std::string, DriverInfo> getRunningDrivers() const = 0;

    /**
     * @brief Get available drivers
     * @return Vector of available driver info
     */
    virtual std::vector<DriverInfo> getAvailableDrivers() const = 0;

    // ==================== Device Management ====================

    /**
     * @brief Get connected devices
     * @return Vector of device info
     */
    virtual std::vector<DeviceInfo> getDevices() const = 0;

    /**
     * @brief Get device by name
     * @param name Device name
     * @return Device info or nullopt if not found
     */
    virtual std::optional<DeviceInfo> getDevice(const std::string& name) const = 0;

    // ==================== Property Access ====================

    /**
     * @brief Set device property
     * @param device Device name
     * @param property Property name
     * @param element Element name
     * @param value Value to set
     * @return true if property set successfully
     */
    virtual bool setProperty(const std::string& device,
                             const std::string& property,
                             const std::string& element,
                             const std::string& value) = 0;

    /**
     * @brief Get device property
     * @param device Device name
     * @param property Property name
     * @param element Element name
     * @return Property value or empty string if not found
     */
    virtual std::string getProperty(const std::string& device,
                                    const std::string& property,
                                    const std::string& element) const = 0;

    /**
     * @brief Get property state
     * @param device Device name
     * @param property Property name
     * @return Property state string
     */
    virtual std::string getPropertyState(const std::string& device,
                                         const std::string& property) const = 0;

    // ==================== Configuration ====================

    /**
     * @brief Configure server
     * @param config Server configuration
     * @return true if configuration successful
     */
    virtual bool configureServer(const ServerConfig& config);

    /**
     * @brief Get server configuration
     * @return Current server configuration
     */
    const ServerConfig& getServerConfig() const { return serverConfig_; }

protected:
    ServerConfig serverConfig_;
    std::atomic<bool> serverRunning_{false};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_SERVER_CLIENT_HPP
