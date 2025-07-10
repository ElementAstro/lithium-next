/*
 * shutter_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Shutter Management Component Implementation

*************************************************/

#include "shutter_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>

namespace lithium::ascom::dome::components {

ShutterManager::ShutterManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    spdlog::info("Initializing Shutter Manager");
}

ShutterManager::~ShutterManager() {
    spdlog::info("Destroying Shutter Manager");
}

auto ShutterManager::openShutter() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    spdlog::info("Opening dome shutter");

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("PUT", "openshutter");
        if (response) {
            operations_count_.fetch_add(1);
            return true;
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->invokeCOMMethod("OpenShutter");
        if (result) {
            operations_count_.fetch_add(1);
            return true;
        }
    }
#endif

    return false;
}

auto ShutterManager::closeShutter() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    spdlog::info("Closing dome shutter");

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("PUT", "closeshutter");
        if (response) {
            operations_count_.fetch_add(1);
            return true;
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->invokeCOMMethod("CloseShutter");
        if (result) {
            operations_count_.fetch_add(1);
            return true;
        }
    }
#endif

    return false;
}

auto ShutterManager::abortShutter() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    spdlog::info("Aborting shutter motion");

    // Most ASCOM domes don't support abort shutter
    // This is a placeholder implementation
    return false;
}

auto ShutterManager::getShutterState() -> ShutterState {
    if (!hardware_ || !hardware_->isConnected()) {
        return ShutterState::UNKNOWN;
    }

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("GET", "shutterstatus");
        if (response) {
            int status = std::stoi(*response);
            switch (status) {
                case 0: return ShutterState::OPEN;
                case 1: return ShutterState::CLOSED;
                case 2: return ShutterState::OPENING;
                case 3: return ShutterState::CLOSING;
                default: return ShutterState::ERROR;
            }
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->getCOMProperty("ShutterStatus");
        if (result) {
            int status = result->intVal;
            switch (status) {
                case 0: return ShutterState::OPEN;
                case 1: return ShutterState::CLOSED;
                case 2: return ShutterState::OPENING;
                case 3: return ShutterState::CLOSING;
                default: return ShutterState::ERROR;
            }
        }
    }
#endif

    return ShutterState::UNKNOWN;
}

auto ShutterManager::hasShutter() -> bool {
    if (!hardware_) {
        return false;
    }

    auto capabilities = hardware_->getDomeCapabilities();
    return capabilities.can_set_shutter;
}

auto ShutterManager::isShutterMoving() -> bool {
    auto state = getShutterState();
    return state == ShutterState::OPENING || state == ShutterState::CLOSING;
}

auto ShutterManager::canOpenShutter() -> bool {
    // Check weather conditions and safety
    return isSafeToOperate();
}

auto ShutterManager::isSafeToOperate() -> bool {
    // TODO: Implement weather monitoring integration
    return true;
}

auto ShutterManager::getWeatherStatus() -> std::string {
    // TODO: Implement weather monitoring integration
    return "Unknown";
}

auto ShutterManager::getOperationsCount() -> uint64_t {
    return operations_count_.load();
}

auto ShutterManager::resetOperationsCount() -> bool {
    operations_count_.store(0);
    spdlog::info("Reset shutter operations count");
    return true;
}

auto ShutterManager::getShutterTimeout() -> int {
    return shutter_timeout_;
}

auto ShutterManager::setShutterTimeout(int timeout) -> bool {
    shutter_timeout_ = timeout;
    spdlog::info("Set shutter timeout to: {} seconds", timeout);
    return true;
}

auto ShutterManager::setShutterCallback(std::function<void(ShutterState, const std::string&)> callback) -> void {
    shutter_callback_ = callback;
}

auto ShutterManager::getShutterStateString(ShutterState state) -> std::string {
    switch (state) {
        case ShutterState::OPEN: return "Open";
        case ShutterState::CLOSED: return "Closed";
        case ShutterState::OPENING: return "Opening";
        case ShutterState::CLOSING: return "Closing";
        case ShutterState::ERROR: return "Error";
        case ShutterState::UNKNOWN: return "Unknown";
        default: return "Invalid";
    }
}

} // namespace lithium::ascom::dome::components
