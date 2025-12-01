/*
 * device_manager.hpp - Device Manager Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_COMMAND_DEVICE_MANAGER_HPP
#define LITHIUM_SERVER_COMMAND_DEVICE_MANAGER_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

/**
 * @brief Register device manager command handlers
 *
 * Registers the following commands:
 * - device.list: List all devices
 * - device.status: Get device manager status
 * - device.connect: Connect a device by name
 * - device.disconnect: Disconnect a device by name
 * - device.connect_batch: Connect multiple devices
 * - device.disconnect_batch: Disconnect multiple devices
 * - device.health: Get device health information
 * - device.statistics: Get operation statistics
 * - device.set_retry_config: Configure retry strategy for a device
 * - device.reset: Reset a device
 * - device.subscribe_events: Subscribe to device events
 */
void registerDeviceManager(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_DEVICE_MANAGER_HPP
