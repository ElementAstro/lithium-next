/*
 * device_container.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Manager Device Container - Device information container

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_MANAGER_DEVICE_CONTAINER_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_DEVICE_CONTAINER_HPP

#include <string>
#include <utility>

namespace lithium::client::indi_manager {

/**
 * @class DeviceContainer
 * @brief A container class for INDI device information.
 *
 * This class holds information about an INDI device, including its name, label,
 * version, binary path, family, skeleton path, and whether it is a custom
 * device.
 */
class DeviceContainer {
public:
    std::string name;      ///< The name of the device.
    std::string label;     ///< The label of the device.
    std::string version;   ///< The version of the device.
    std::string binary;    ///< The binary path of the device.
    std::string family;    ///< The family to which the device belongs.
    std::string skeleton;  ///< The skeleton path of the device (optional).
    bool custom;           ///< Indicates whether the device is custom.

    /**
     * @brief Default constructor
     */
    DeviceContainer() : custom(false) {}

    /**
     * @brief Constructs a DeviceContainer with the given parameters.
     * @param name The name of the device.
     * @param label The label of the device.
     * @param version The version of the device.
     * @param binary The binary path of the device.
     * @param family The family to which the device belongs.
     * @param skeleton The skeleton path of the device (optional).
     * @param custom Indicates whether the device is custom (default is false).
     */
    DeviceContainer(std::string name, std::string label, std::string version,
                    std::string binary, std::string family,
                    std::string skeleton = "", bool custom = false)
        : name(std::move(name)),
          label(std::move(label)),
          version(std::move(version)),
          binary(std::move(binary)),
          family(std::move(family)),
          skeleton(std::move(skeleton)),
          custom(custom) {}
};

// Backward compatibility alias
using INDIDeviceContainer = DeviceContainer;

}  // namespace lithium::client::indi_manager

// Global alias for backward compatibility
using INDIDeviceContainer = lithium::client::indi_manager::DeviceContainer;

#endif  // LITHIUM_CLIENT_INDI_MANAGER_DEVICE_CONTAINER_HPP
