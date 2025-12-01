/*
 * connector_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Manager Connector Interface - Abstract interface for server connectors

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_MANAGER_CONNECTOR_INTERFACE_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_CONNECTOR_INTERFACE_HPP

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lithium::client::indi_manager {

class DeviceContainer;

/**
 * @class ConnectorInterface
 * @brief Abstract interface for INDI server connectors.
 *
 * This interface defines the contract for managing INDI server connections,
 * driver lifecycle, and property access.
 */
class ConnectorInterface {
public:
    virtual ~ConnectorInterface() = default;

    // ==================== Server Lifecycle ====================

    /**
     * @brief Starts the INDI server.
     * @return True if the server was started successfully, false otherwise.
     */
    virtual auto startServer() -> bool = 0;

    /**
     * @brief Stops the INDI server.
     * @return True if the server was stopped successfully, false otherwise.
     */
    virtual auto stopServer() -> bool = 0;

    /**
     * @brief Checks if the INDI server is running.
     * @return True if the server is running, false otherwise.
     */
    virtual auto isRunning() -> bool = 0;

    // ==================== Driver Management ====================

    /**
     * @brief Starts an INDI driver.
     * @param driver A shared pointer to the DeviceContainer representing
     * the driver.
     * @return True if the driver was started successfully, false otherwise.
     */
    virtual auto startDriver(const std::shared_ptr<DeviceContainer>& driver)
        -> bool = 0;

    /**
     * @brief Stops an INDI driver.
     * @param driver A shared pointer to the DeviceContainer representing
     * the driver.
     * @return True if the driver was stopped successfully, false otherwise.
     */
    virtual auto stopDriver(const std::shared_ptr<DeviceContainer>& driver)
        -> bool = 0;

    // ==================== Property Access ====================

    /**
     * @brief Sets a property of an INDI device.
     * @param dev The name of the device.
     * @param prop The name of the property.
     * @param element The name of the element.
     * @param value The value to set.
     * @return True if the property was set successfully, false otherwise.
     */
    virtual auto setProp(const std::string& dev, const std::string& prop,
                         const std::string& element, const std::string& value)
        -> bool = 0;

    /**
     * @brief Gets a property of an INDI device.
     * @param dev The name of the device.
     * @param prop The name of the property.
     * @param element The name of the element.
     * @return The value of the property.
     */
    virtual auto getProp(const std::string& dev, const std::string& prop,
                         const std::string& element) -> std::string = 0;

    /**
     * @brief Gets the state of an INDI device property.
     * @param dev The name of the device.
     * @param prop The name of the property.
     * @return The state of the property.
     */
    virtual auto getState(const std::string& dev, const std::string& prop)
        -> std::string = 0;

    // ==================== Device/Driver Queries ====================

    /**
     * @brief Gets a list of running INDI drivers.
     * @return An unordered map where the key is the driver label and the value
     * is a shared pointer to the DeviceContainer.
     */
    virtual auto getRunningDrivers()
        -> std::unordered_map<std::string, std::shared_ptr<DeviceContainer>> = 0;

    /**
     * @brief Gets a list of INDI devices.
     * @return A vector of unordered maps, each representing a device with its
     * properties.
     */
    virtual auto getDevices()
        -> std::vector<std::unordered_map<std::string, std::string>> = 0;
};

// Backward compatibility alias
using Connector = ConnectorInterface;

}  // namespace lithium::client::indi_manager

// Global alias for backward compatibility
using Connector = lithium::client::indi_manager::ConnectorInterface;

#endif  // LITHIUM_CLIENT_INDI_MANAGER_CONNECTOR_INTERFACE_HPP
