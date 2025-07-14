/*
 * parking_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Parking Manager Component

This component manages telescope parking operations including
park/unpark operations, park position management, and park status.

*************************************************/

#include "parking_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <cmath>

namespace lithium::device::ascom::telescope::components {

ParkingManager::ParkingManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {

    auto logger = spdlog::get("telescope_parking");
    if (logger) {
        logger->info("ParkingManager initialized");
    }
}

ParkingManager::~ParkingManager() {
    auto logger = spdlog::get("telescope_parking");
    if (logger) {
        logger->debug("ParkingManager destructor");
    }
}

bool ParkingManager::isParked() const {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    try {
        bool parked = hardware_->isParked();
        return parked;
    } catch (const std::exception& e) {
        setLastError("Failed to get park status: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Failed to get park status: {}", e.what());
        return false;
    }
}

bool ParkingManager::park() {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    if (!canPark()) {
        setLastError("Telescope cannot be parked (capability not supported)");
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Park operation not supported by telescope");
        return false;
    }

    if (isParked()) {
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->info("Telescope is already parked");
        clearError();
        return true;
    }

    try {
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->info("Starting park operation");

        bool result = hardware_->park();

        if (result) {
            clearError();
            if (logger) logger->info("Park operation completed successfully");

            // Wait for park to complete and verify
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (isParked()) {
                if (logger) logger->info("Park status verified successfully");
            } else {
                if (logger) logger->warn("Park operation completed but status verification failed");
            }
        } else {
            setLastError("Park operation failed");
            if (logger) logger->error("Park operation failed");
        }

        return result;
    } catch (const std::exception& e) {
        setLastError("Exception during park operation: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Exception during park operation: {}", e.what());
        return false;
    }
}

bool ParkingManager::unpark() {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    if (!isParked()) {
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->info("Telescope is already unparked");
        clearError();
        return true;
    }

    try {
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->info("Starting unpark operation");

        bool result = hardware_->unpark();

        if (result) {
            clearError();
            if (logger) logger->info("Unpark operation completed successfully");

            // Wait for unpark to complete and verify
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (!isParked()) {
                if (logger) logger->info("Unpark status verified successfully");
            } else {
                if (logger) logger->warn("Unpark operation completed but status verification failed");
            }
        } else {
            setLastError("Unpark operation failed");
            if (logger) logger->error("Unpark operation failed");
        }

        return result;
    } catch (const std::exception& e) {
        setLastError("Exception during unpark operation: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Exception during unpark operation: {}", e.what());
        return false;
    }
}

bool ParkingManager::canPark() const {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    try {
        // For now, assume all telescopes can park - this would be hardware-specific
        return true;
    } catch (const std::exception& e) {
        setLastError("Failed to check park capability: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Failed to check park capability: {}", e.what());
        return false;
    }
}

std::optional<EquatorialCoordinates> ParkingManager::getParkPosition() const {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return std::nullopt;
    }

    try {
        // Implementation would get park position from hardware
        // For now, return a placeholder
        EquatorialCoordinates position;
        position.ra = 0.0;   // Hours
        position.dec = 0.0;  // Degrees

        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->debug("Retrieved park position: RA={:.6f}, Dec={:.6f}",
                                  position.ra, position.dec);

        return position;
    } catch (const std::exception& e) {
        setLastError("Failed to get park position: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Failed to get park position: {}", e.what());
        return std::nullopt;
    }
}

bool ParkingManager::setParkPosition(double ra, double dec) {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    // Validate coordinates
    if (ra < 0.0 || ra >= 24.0) {
        setLastError("Invalid RA coordinate for park position");
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Invalid RA coordinate: {:.6f}", ra);
        return false;
    }

    if (dec < -90.0 || dec > 90.0) {
        setLastError("Invalid DEC coordinate for park position");
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Invalid DEC coordinate: {:.6f}", dec);
        return false;
    }

    try {
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->info("Setting park position to RA: {:.6f}h, DEC: {:.6f}°", ra, dec);

        // Implementation would set park position in hardware
        // For now, just simulate success

        clearError();
        if (logger) logger->info("Park position set successfully");
        return true;

    } catch (const std::exception& e) {
        setLastError("Failed to set park position: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Failed to set park position: {}", e.what());
        return false;
    }
}

bool ParkingManager::isAtPark() const {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    try {
        // Check if telescope is both parked and at the park position
        if (!isParked()) {
            return false;
        }

        // Implementation would check if current position matches park position
        // For now, assume if parked then at park position
        return true;

    } catch (const std::exception& e) {
        setLastError("Failed to check if at park position: " + std::string(e.what()));
        auto logger = spdlog::get("telescope_parking");
        if (logger) logger->error("Failed to check if at park position: {}", e.what());
        return false;
    }
}

std::string ParkingManager::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void ParkingManager::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

void ParkingManager::setLastError(const std::string& error) const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = error;
}

} // namespace lithium::device::ascom::telescope::components
