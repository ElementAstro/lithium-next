/*
 * device.cpp - Device Command Registration Entry Point
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "device.hpp"
#include "device_manager.hpp"

namespace lithium::app {

void registerDeviceCommands(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Register device manager commands
    registerDeviceManager(dispatcher);

    // Per-device command registrations are implemented in:
    // - camera.cpp
    // - mount.cpp
    // - focuser.cpp
    // - filterwheel.cpp
    // - dome.cpp
    // - guider.cpp
}

}  // namespace lithium::app

