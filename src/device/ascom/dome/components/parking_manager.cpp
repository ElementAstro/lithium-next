/*
 * parking_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Parking Management Component Implementation

*************************************************/

#include "parking_manager.hpp"
#include "hardware_interface.hpp"
#include "azimuth_manager.hpp"

#include <spdlog/spdlog.h>

namespace lithium::ascom::dome::components {

ParkingManager::ParkingManager(std::shared_ptr<HardwareInterface> hardware,
                               std::shared_ptr<AzimuthManager> azimuth_manager)
    : hardware_(hardware), azimuth_manager_(azimuth_manager) {
    spdlog::info("Initializing Parking Manager");
}

ParkingManager::~ParkingManager() {
    spdlog::info("Destroying Parking Manager");
}

auto ParkingManager::park() -> bool {
    if (!hardware_ || !hardware_->isConnected() || is_parking_.load()) {
        return false;
    }

    spdlog::info("Parking dome");

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("PUT", "park");
        if (response) {
            is_parking_.store(true);
            return executeParkingSequence();
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->invokeCOMMethod("Park");
        if (result) {
            is_parking_.store(true);
            return executeParkingSequence();
        }
    }
#endif

    return false;
}

auto ParkingManager::unpark() -> bool {
    if (!hardware_ || !hardware_->isConnected() || !is_parked_.load()) {
        return false;
    }

    spdlog::info("Unparking dome");

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("PUT", "unpark");
        if (response) {
            is_parked_.store(false);
            return true;
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->invokeCOMMethod("Unpark");
        if (result) {
            is_parked_.store(false);
            return true;
        }
    }
#endif

    return false;
}

auto ParkingManager::isParked() -> bool {
    updateParkStatus();
    return is_parked_.load();
}

auto ParkingManager::canPark() -> bool {
    if (!hardware_) {
        return false;
    }

    auto capabilities = hardware_->getDomeCapabilities();
    return capabilities.can_park;
}

auto ParkingManager::getParkPosition() -> std::optional<double> {
    return park_position_.load();
}

auto ParkingManager::setParkPosition(double azimuth) -> bool {
    if (!canSetParkPosition()) {
        return false;
    }

    // Normalize azimuth
    while (azimuth < 0.0) azimuth += 360.0;
    while (azimuth >= 360.0) azimuth -= 360.0;

    park_position_.store(azimuth);
    spdlog::info("Set park position to: {:.2f}°", azimuth);
    return true;
}

auto ParkingManager::canSetParkPosition() -> bool {
    if (!hardware_) {
        return false;
    }

    auto capabilities = hardware_->getDomeCapabilities();
    return capabilities.can_set_park;
}

auto ParkingManager::findHome() -> bool {
    if (!hardware_ || !hardware_->isConnected() || is_homing_.load()) {
        return false;
    }

    spdlog::info("Finding dome home position");

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("PUT", "findhome");
        if (response) {
            is_homing_.store(true);
            return executeHomingSequence();
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->invokeCOMMethod("FindHome");
        if (result) {
            is_homing_.store(true);
            return executeHomingSequence();
        }
    }
#endif

    return false;
}

auto ParkingManager::setHome() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    auto current_azimuth = azimuth_manager_->getCurrentAzimuth();
    if (!current_azimuth) {
        return false;
    }

    home_position_.store(*current_azimuth);
    spdlog::info("Set home position to current azimuth: {:.2f}°", *current_azimuth);
    return true;
}

auto ParkingManager::gotoHome() -> bool {
    if (!azimuth_manager_) {
        return false;
    }

    double home_pos = home_position_.load();
    return azimuth_manager_->moveToAzimuth(home_pos);
}

auto ParkingManager::getHomePosition() -> std::optional<double> {
    return home_position_.load();
}

auto ParkingManager::canFindHome() -> bool {
    if (!hardware_) {
        return false;
    }

    auto capabilities = hardware_->getDomeCapabilities();
    return capabilities.can_find_home;
}

auto ParkingManager::isParkingInProgress() -> bool {
    return is_parking_.load();
}

auto ParkingManager::isHomingInProgress() -> bool {
    return is_homing_.load();
}

auto ParkingManager::getParkingProgress() -> double {
    if (!is_parking_.load()) {
        return 1.0;
    }

    if (azimuth_manager_) {
        return azimuth_manager_->getMovementProgress();
    }

    return 0.0;
}

auto ParkingManager::setParkingTimeout(int timeout) -> bool {
    parking_timeout_ = timeout;
    spdlog::info("Set parking timeout to: {} seconds", timeout);
    return true;
}

auto ParkingManager::getParkingTimeout() -> int {
    return parking_timeout_;
}

auto ParkingManager::setAutoParking(bool enable) -> bool {
    auto_parking_.store(enable);
    spdlog::info("{} auto parking", enable ? "Enabled" : "Disabled");
    return true;
}

auto ParkingManager::isAutoParking() -> bool {
    return auto_parking_.load();
}

auto ParkingManager::setParkingCallback(std::function<void(bool, const std::string&)> callback) -> void {
    parking_callback_ = callback;
}

auto ParkingManager::setHomingCallback(std::function<void(bool, const std::string&)> callback) -> void {
    homing_callback_ = callback;
}

auto ParkingManager::updateParkStatus() -> void {
    if (!hardware_ || !hardware_->isConnected()) {
        return;
    }

    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::ALPACA_REST) {
        auto response = hardware_->sendAlpacaRequest("GET", "athome");
        if (response) {
            bool atHome = (*response == "true");
            is_parked_.store(atHome);
        }
    }

#ifdef _WIN32
    if (hardware_->getConnectionType() == HardwareInterface::ConnectionType::COM_DRIVER) {
        auto result = hardware_->getCOMProperty("AtHome");
        if (result) {
            bool atHome = (result->boolVal == VARIANT_TRUE);
            is_parked_.store(atHome);
        }
    }
#endif
}

auto ParkingManager::executeParkingSequence() -> bool {
    if (!azimuth_manager_) {
        is_parking_.store(false);
        return false;
    }

    // Move to park position
    double park_pos = park_position_.load();
    if (azimuth_manager_->moveToAzimuth(park_pos)) {
        // Set callback to monitor parking completion
        azimuth_manager_->setMovementCallback([this](bool success, const std::string& message) {
            is_parking_.store(false);
            if (success) {
                is_parked_.store(true);
                spdlog::info("Dome parking completed");
            } else {
                spdlog::error("Dome parking failed: {}", message);
            }

            if (parking_callback_) {
                parking_callback_(success, message);
            }
        });
        return true;
    }

    is_parking_.store(false);
    return false;
}

auto ParkingManager::executeHomingSequence() -> bool {
    // For most ASCOM domes, homing is handled by the driver
    // We just need to monitor completion
    std::thread([this]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check if homing is complete
        updateParkStatus();

        is_homing_.store(false);
        if (homing_callback_) {
            homing_callback_(true, "Homing completed");
        }

        spdlog::info("Dome homing completed");
    }).detach();

    return true;
}

} // namespace lithium::ascom::dome::components
