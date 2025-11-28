#ifndef LITHIUM_INDISERVER_CONNECTOR_HPP
#define LITHIUM_INDISERVER_CONNECTOR_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "connector.hpp"
#include "server_manager.hpp"
#include "fifo_channel.hpp"

/**
 * @brief Callback for driver events
 */
using DriverEventCallback = std::function<void(const std::string& driver, bool started)>;

/**
 * @class INDIConnector
 * @brief A class to manage the connection and interaction with an INDI server.
 *
 * This class provides functionality to start and stop the INDI server, manage drivers,
 * and set or get properties of INDI devices.
 * 
 * Enhanced features:
 * - Configurable server startup via INDIServerConfig
 * - Reliable FIFO communication via FifoChannel
 * - Driver lifecycle management
 * - Event callbacks for state changes
 */
class INDIConnector : public Connector {
public:
    /**
     * @brief Constructs an INDIConnector with the given parameters.
     * @param hst The hostname of the INDI server (default is "localhost").
     * @param prt The port number of the INDI server (default is 7624).
     * @param cfg The path to the INDI configuration files (default is an empty string).
     * @param dta The path to the INDI data files (default is "/usr/share/indi").
     * @param fif The path to the INDI FIFO file (default is "/tmp/indi.fifo").
     */
    INDIConnector(const std::string& hst = "localhost", int prt = 7624, 
                  const std::string& cfg = "",
                  const std::string& dta = "/usr/share/indi", 
                  const std::string& fif = "/tmp/indi.fifo");

    /**
     * @brief Constructs an INDIConnector with server configuration.
     * @param config Server configuration
     */
    explicit INDIConnector(const lithium::client::indi::INDIServerConfig& config);

    /**
     * @brief Destructor for INDIConnector.
     */
    ~INDIConnector() override;

    // ==================== Server Lifecycle ====================

    /**
     * @brief Starts the INDI server.
     * @return True if the server was started successfully, false otherwise.
     */
    auto startServer() -> bool override;

    /**
     * @brief Stops the INDI server.
     * @return True if the server was stopped successfully, false otherwise.
     */
    auto stopServer() -> bool override;

    /**
     * @brief Restarts the INDI server.
     * @return True if restarted successfully.
     */
    auto restartServer() -> bool;

    /**
     * @brief Checks if the INDI server is running.
     * @return True if the server is running, false otherwise.
     */
    auto isRunning() -> bool override;

    /**
     * @brief Checks if the INDI server software is installed.
     * @return True if the software is installed, false otherwise.
     */
    auto isInstalled() -> bool;

    /**
     * @brief Get server state
     * @return Current server state
     */
    [[nodiscard]] auto getServerState() const -> lithium::client::indi::ServerState;

    /**
     * @brief Get server uptime
     * @return Uptime in seconds or nullopt if not running
     */
    [[nodiscard]] auto getServerUptime() const -> std::optional<std::chrono::seconds>;

    /**
     * @brief Get last server error
     * @return Error message
     */
    [[nodiscard]] auto getLastError() const -> std::string;

    // ==================== Configuration ====================

    /**
     * @brief Set server configuration (only when stopped)
     * @param config New configuration
     * @return true if set successfully
     */
    auto setServerConfig(const lithium::client::indi::INDIServerConfig& config) -> bool;

    /**
     * @brief Get current server configuration
     * @return Configuration reference
     */
    [[nodiscard]] auto getServerConfig() const -> const lithium::client::indi::INDIServerConfig&;

    /**
     * @brief Set FIFO channel configuration
     * @param config FIFO configuration
     */
    void setFifoConfig(const lithium::client::indi::FifoChannelConfig& config);

    // ==================== Driver Management ====================

    /**
     * @brief Starts an INDI driver.
     * @param driver A shared pointer to the INDIDeviceContainer representing the driver.
     * @return True if the driver was started successfully, false otherwise.
     */
    auto startDriver(const std::shared_ptr<class INDIDeviceContainer>& driver) -> bool override;

    /**
     * @brief Stops an INDI driver.
     * @param driver A shared pointer to the INDIDeviceContainer representing the driver.
     * @return True if the driver was stopped successfully, false otherwise.
     */
    auto stopDriver(const std::shared_ptr<class INDIDeviceContainer>& driver) -> bool override;

    /**
     * @brief Restart a driver
     * @param driver Driver to restart
     * @return true if restarted successfully
     */
    auto restartDriver(const std::shared_ptr<class INDIDeviceContainer>& driver) -> bool;

    /**
     * @brief Start driver by binary name
     * @param binary Driver binary name
     * @param skeleton Optional skeleton file
     * @return true if started successfully
     */
    auto startDriverByName(const std::string& binary, 
                          const std::string& skeleton = "") -> bool;

    /**
     * @brief Stop driver by binary name
     * @param binary Driver binary name
     * @return true if stopped successfully
     */
    auto stopDriverByName(const std::string& binary) -> bool;

    /**
     * @brief Check if a driver is running
     * @param driverLabel Driver label
     * @return true if running
     */
    [[nodiscard]] auto isDriverRunning(const std::string& driverLabel) const -> bool;

    // ==================== Property Access ====================

    /**
     * @brief Sets a property of an INDI device.
     * @param dev The name of the device.
     * @param prop The name of the property.
     * @param element The name of the element.
     * @param value The value to set.
     * @return True if the property was set successfully, false otherwise.
     */
    auto setProp(const std::string& dev, const std::string& prop, 
                 const std::string& element, const std::string& value) -> bool override;

    /**
     * @brief Gets a property of an INDI device.
     * @param dev The name of the device.
     * @param prop The name of the property.
     * @param element The name of the element.
     * @return The value of the property.
     */
    auto getProp(const std::string& dev, const std::string& prop, 
                 const std::string& element) -> std::string override;

    /**
     * @brief Gets the state of an INDI device property.
     * @param dev The name of the device.
     * @param prop The name of the property.
     * @return The state of the property.
     */
    auto getState(const std::string& dev, const std::string& prop) -> std::string override;

    // ==================== Device/Driver Queries ====================

    /**
     * @brief Gets a list of running INDI drivers.
     * @return An unordered map where the key is the driver label and the value is a shared pointer to the INDIDeviceContainer.
     */
    auto getRunningDrivers() -> std::unordered_map<std::string, std::shared_ptr<class INDIDeviceContainer>> override;

    /**
     * @brief Gets a list of INDI devices.
     * @return A vector of unordered maps, each representing a device with its properties.
     */
    auto getDevices() -> std::vector<std::unordered_map<std::string, std::string>> override;

    /**
     * @brief Get count of running drivers
     * @return Number of running drivers
     */
    [[nodiscard]] auto getRunningDriverCount() const -> size_t;

    // ==================== Events ====================

    /**
     * @brief Set callback for server state changes
     * @param callback Callback function
     */
    void setServerEventCallback(lithium::client::indi::ServerEventCallback callback);

    /**
     * @brief Set callback for driver events
     * @param callback Callback function
     */
    void setDriverEventCallback(DriverEventCallback callback);

    // ==================== FIFO Channel Access ====================

    /**
     * @brief Get FIFO channel for direct access
     * @return FIFO channel pointer
     */
    [[nodiscard]] auto getFifoChannel() -> lithium::client::indi::FifoChannel*;

    /**
     * @brief Send raw command to FIFO
     * @param command Command string
     * @return true if sent successfully
     */
    auto sendFifoCommand(const std::string& command) -> bool;

    // ==================== Statistics ====================

    /**
     * @brief Get total FIFO commands sent
     * @return Command count
     */
    [[nodiscard]] auto getTotalFifoCommands() const -> uint64_t;

    /**
     * @brief Get total FIFO errors
     * @return Error count
     */
    [[nodiscard]] auto getTotalFifoErrors() const -> uint64_t;

private:
    /**
     * @brief Validates the paths for configuration and data files.
     */
    void validatePaths();

    /**
     * @brief Initialize server manager and FIFO channel
     */
    void initializeComponents();

    /**
     * @brief Notify driver event
     */
    void notifyDriverEvent(const std::string& driver, bool started);

    std::string host_;         ///< The hostname of the INDI server.
    int port_;                 ///< The port number of the INDI server.
    std::string config_path_;  ///< The path to the INDI configuration files.
    std::string data_path_;    ///< The path to the INDI data files.
    std::string fifo_path_;    ///< The path to the INDI FIFO file.

    std::unique_ptr<lithium::client::indi::INDIServerManager> serverManager_;
    std::unique_ptr<lithium::client::indi::FifoChannel> fifoChannel_;

    DriverEventCallback driverEventCallback_;
    mutable std::mutex driverMutex_;

#if ENABLE_FASTHASH
    emhash8::HashMap<std::string, std::shared_ptr<INDIDeviceContainer>> running_drivers_;
#else
    std::unordered_map<std::string, std::shared_ptr<INDIDeviceContainer>> running_drivers_; ///< A list of running drivers.
#endif
};

#endif  // LITHIUM_INDISERVER_CONNECTOR_HPP
