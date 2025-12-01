#ifndef LITHIUM_SERVER_COMMAND_DEVICE_HPP
#define LITHIUM_SERVER_COMMAND_DEVICE_HPP

#include <memory>

// Aggregator header for per-device command registration helpers.
// Each device has its own header/implementation pair.

#include "camera.hpp"
#include "device_manager.hpp"
#include "dome.hpp"
#include "filterwheel.hpp"
#include "focuser.hpp"
#include "mount.hpp"

namespace lithium::app {

class CommandDispatcher;

/**
 * @brief Register all device-related commands
 *
 * This includes:
 * - Device manager commands (device.list, device.connect, etc.)
 * - Per-device type commands (camera.*, mount.*, etc.)
 */
void registerDeviceCommands(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_DEVICE_HPP
