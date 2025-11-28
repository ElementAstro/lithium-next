/*
 * indi_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: INDI server client implementation

*************************************************/

#ifndef LITHIUM_CLIENT_INDI_CLIENT_HPP
#define LITHIUM_CLIENT_INDI_CLIENT_HPP

#include "../common/server_client.hpp"
#include "connector.hpp"
#include "container.hpp"
#include "indihub_agent.hpp"

#include <memory>

namespace lithium::client {

/**
 * @brief INDI-specific driver information
 */
struct INDIDriverInfo : public DriverInfo {
    std::string family;
    std::string manufacturer;
    std::string exec;
};

/**
 * @brief INDI server client
 *
 * Manages INDI server and driver lifecycle
 */
class INDIClient : public ServerClient {
public:
    /**
     * @brief Construct a new INDIClient
     * @param name Instance name
     */
    explicit INDIClient(std::string name = "indi");

    /**
     * @brief Destructor
     */
    ~INDIClient() override;

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

    /**
     * @brief Start driver by INDIDeviceContainer
     * @param container Device container
     * @return true if successful
     */
    bool startDriver(const std::shared_ptr<INDIDeviceContainer>& container);

    /**
     * @brief Stop driver by INDIDeviceContainer
     * @param container Device container
     * @return true if successful
     */
    bool stopDriver(const std::shared_ptr<INDIDeviceContainer>& container);

    // ==================== Device Management ====================

    std::vector<DeviceInfo> getDevices() const override;
    std::optional<DeviceInfo> getDevice(const std::string& name) const override;

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

    // ==================== INDI-Specific ====================

    /**
     * @brief Configure INDI server
     * @param host Host address
     * @param port Port number
     * @param configPath Configuration path
     * @param dataPath Data path
     * @param fifoPath FIFO path
     */
    void configureINDI(const std::string& host = "localhost",
                       int port = 7624,
                       const std::string& configPath = "",
                       const std::string& dataPath = "/usr/share/indi",
                       const std::string& fifoPath = "/tmp/indi.fifo");

    /**
     * @brief Get INDI connector
     * @return Connector pointer
     */
    Connector* getConnector() const { return connector_.get(); }

    /**
     * @brief Start IndiHub agent
     * @param profile Profile name
     * @param mode Running mode
     * @return true if successful
     */
    bool startIndiHub(const std::string& profile,
                      const std::string& mode = INDIHUB_AGENT_DEFAULT_MODE);

    /**
     * @brief Stop IndiHub agent
     */
    void stopIndiHub();

    /**
     * @brief Check if IndiHub is running
     * @return true if running
     */
    bool isIndiHubRunning() const;

    /**
     * @brief Get IndiHub mode
     * @return Current mode
     */
    std::string getIndiHubMode() const;

    /**
     * @brief Load drivers from XML files
     * @param path Path to driver XML files
     * @return Number of drivers loaded
     */
    int loadDriversFromXML(const std::string& path = "/usr/share/indi");

private:
    std::unique_ptr<Connector> connector_;
    std::unique_ptr<IndiHubAgent> indihubAgent_;
    std::vector<INDIDriverInfo> availableDrivers_;

    std::string indiHost_{"localhost"};
    int indiPort_{7624};
    std::string configPath_;
    std::string dataPath_{"/usr/share/indi"};
    std::string fifoPath_{"/tmp/indi.fifo"};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_INDI_CLIENT_HPP
