/*
 * manager_connector.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Manager Connector - Enhanced implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_MANAGER_CONNECTOR_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_CONNECTOR_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "connector_interface.hpp"
#include "device_container.hpp"
#include "fifo_channel.hpp"
#include "server_manager.hpp"

namespace lithium::client::indi_manager {

/**
 * @brief Callback for driver events
 */
using DriverEventCallback =
    std::function<void(const std::string& driver, bool started)>;

/**
 * @class ManagerConnector
 * @brief A class to manage the connection and interaction with an INDI server.
 *
 * This class provides functionality to start and stop the INDI server, manage
 * drivers, and set or get properties of INDI devices.
 *
 * Enhanced features:
 * - Configurable server startup via ServerConfig
 * - Reliable FIFO communication via FifoChannel
 * - Driver lifecycle management
 * - Event callbacks for state changes
 */
class ManagerConnector : public ConnectorInterface {
public:
    /**
     * @brief Constructs a ManagerConnector with the given parameters.
     * @param host The hostname of the INDI server (default is "localhost").
     * @param port The port number of the INDI server (default is 7624).
     * @param configPath The path to the INDI configuration files.
     * @param dataPath The path to the INDI data files.
     * @param fifoPath The path to the INDI FIFO file.
     */
    ManagerConnector(const std::string& host = "localhost", int port = 7624,
                     const std::string& configPath = "",
                     const std::string& dataPath = "/usr/share/indi",
                     const std::string& fifoPath = "/tmp/indi.fifo");

    /**
     * @brief Constructs a ManagerConnector with server configuration.
     * @param config Server configuration
     */
    explicit ManagerConnector(const ServerConfig& config);

    /**
     * @brief Destructor for ManagerConnector.
     */
    ~ManagerConnector() override;

    // ==================== Server Lifecycle ====================

    auto startServer() -> bool override;
    auto stopServer() -> bool override;
    auto restartServer() -> bool;
    auto isRunning() -> bool override;
    auto isInstalled() -> bool;

    [[nodiscard]] auto getServerState() const -> ServerState;
    [[nodiscard]] auto getServerUptime() const
        -> std::optional<std::chrono::seconds>;
    [[nodiscard]] auto getLastError() const -> std::string;

    // ==================== Configuration ====================

    auto setServerConfig(const ServerConfig& config) -> bool;
    [[nodiscard]] auto getServerConfig() const -> const ServerConfig&;
    void setFifoConfig(const FifoChannelConfig& config);

    // ==================== Driver Management ====================

    auto startDriver(const std::shared_ptr<DeviceContainer>& driver)
        -> bool override;
    auto stopDriver(const std::shared_ptr<DeviceContainer>& driver)
        -> bool override;
    auto restartDriver(const std::shared_ptr<DeviceContainer>& driver) -> bool;
    auto startDriverByName(const std::string& binary,
                           const std::string& skeleton = "") -> bool;
    auto stopDriverByName(const std::string& binary) -> bool;
    [[nodiscard]] auto isDriverRunning(const std::string& driverLabel) const
        -> bool;

    // ==================== Property Access ====================

    auto setProp(const std::string& dev, const std::string& prop,
                 const std::string& element, const std::string& value)
        -> bool override;
    auto getProp(const std::string& dev, const std::string& prop,
                 const std::string& element) -> std::string override;
    auto getState(const std::string& dev, const std::string& prop)
        -> std::string override;

    // ==================== Device/Driver Queries ====================

    auto getRunningDrivers()
        -> std::unordered_map<std::string,
                              std::shared_ptr<DeviceContainer>> override;
    auto getDevices()
        -> std::vector<std::unordered_map<std::string, std::string>> override;
    [[nodiscard]] auto getRunningDriverCount() const -> size_t;

    // ==================== Events ====================

    void setServerEventCallback(ServerEventCallback callback);
    void setDriverEventCallback(DriverEventCallback callback);

    // ==================== FIFO Channel Access ====================

    [[nodiscard]] auto getFifoChannel() -> FifoChannel*;
    auto sendFifoCommand(const std::string& command) -> bool;

    // ==================== Statistics ====================

    [[nodiscard]] auto getTotalFifoCommands() const -> uint64_t;
    [[nodiscard]] auto getTotalFifoErrors() const -> uint64_t;

private:
    void validatePaths();
    void initializeComponents();
    void notifyDriverEvent(const std::string& driver, bool started);

    std::string host_;
    int port_;
    std::string configPath_;
    std::string dataPath_;
    std::string fifoPath_;

    std::unique_ptr<ServerManager> serverManager_;
    std::unique_ptr<FifoChannel> fifoChannel_;

    DriverEventCallback driverEventCallback_;
    mutable std::mutex driverMutex_;

    std::unordered_map<std::string, std::shared_ptr<DeviceContainer>>
        runningDrivers_;
};

// Backward compatibility alias
using INDIConnector = ManagerConnector;

}  // namespace lithium::client::indi_manager

#endif  // LITHIUM_CLIENT_INDI_MANAGER_CONNECTOR_HPP
