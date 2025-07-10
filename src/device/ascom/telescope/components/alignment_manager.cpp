/*
 * alignment_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Alignment Manager Implementation

This component manages telescope alignment functionality including
alignment modes, alignment points, and coordinate transformations
for accurate pointing and tracking.

*************************************************/

#include "alignment_manager.hpp"
#include "hardware_interface.hpp"
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace lithium::device::ascom::telescope::components {

AlignmentManager::AlignmentManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    if (!hardware_) {
        throw std::invalid_argument("Hardware interface cannot be null");
    }
    clearError();
}

AlignmentManager::~AlignmentManager() = default;

// Helper function to convert between alignment mode types
lithium::device::ascom::telescope::components::AlignmentMode convertTemplateToASCOMAlignmentMode(::AlignmentMode mode) {
    // Since the template AlignmentMode doesn't directly map to ASCOM alignment modes,
    // we'll use a sensible mapping
    switch (mode) {
        case ::AlignmentMode::EQ_NORTH_POLE:
        case ::AlignmentMode::EQ_SOUTH_POLE:
        case ::AlignmentMode::GERMAN_POLAR:
            return lithium::device::ascom::telescope::components::AlignmentMode::THREE_STAR;
        case ::AlignmentMode::ALTAZ:
            return lithium::device::ascom::telescope::components::AlignmentMode::TWO_STAR;
        case ::AlignmentMode::FORK:
            return lithium::device::ascom::telescope::components::AlignmentMode::ONE_STAR;
        default:
            return lithium::device::ascom::telescope::components::AlignmentMode::UNKNOWN;
    }
}

::AlignmentMode convertASCOMToTemplateAlignmentMode(lithium::device::ascom::telescope::components::AlignmentMode mode) {
    switch (mode) {
        case lithium::device::ascom::telescope::components::AlignmentMode::ONE_STAR:
            return ::AlignmentMode::FORK;
        case lithium::device::ascom::telescope::components::AlignmentMode::TWO_STAR:
            return ::AlignmentMode::ALTAZ;
        case lithium::device::ascom::telescope::components::AlignmentMode::THREE_STAR:
            return ::AlignmentMode::GERMAN_POLAR;
        case lithium::device::ascom::telescope::components::AlignmentMode::AUTO:
            return ::AlignmentMode::EQ_NORTH_POLE;
        default:
            return ::AlignmentMode::ALTAZ;
    }
}

// Helper function to convert coordinates
lithium::device::ascom::telescope::components::EquatorialCoordinates convertTemplateToASCOMCoordinates(const ::EquatorialCoordinates& coords) {
    lithium::device::ascom::telescope::components::EquatorialCoordinates result;
    result.ra = coords.ra;
    result.dec = coords.dec;
    return result;
}

::AlignmentMode AlignmentManager::getAlignmentMode() const {
    try {
        if (!hardware_->isConnected()) {
            setLastError("Telescope not connected");
            return ::AlignmentMode::ALTAZ; // Default fallback
        }

        // Get current alignment mode from hardware
        auto result = hardware_->getAlignmentMode();
        if (!result) {
            setLastError("Failed to retrieve alignment mode from hardware");
            return ::AlignmentMode::ALTAZ;
        }

        // Convert ASCOM alignment mode to template alignment mode
        return convertASCOMToTemplateAlignmentMode(*result);
    } catch (const std::exception& e) {
        setLastError("Exception in getAlignmentMode: " + std::string(e.what()));
        return ::AlignmentMode::ALTAZ;
    }
}

bool AlignmentManager::setAlignmentMode(::AlignmentMode mode) {
    try {
        if (!hardware_->isConnected()) {
            setLastError("Telescope not connected");
            return false;
        }

        // Validate alignment mode
        switch (mode) {
            case ::AlignmentMode::EQ_NORTH_POLE:
            case ::AlignmentMode::EQ_SOUTH_POLE:
            case ::AlignmentMode::ALTAZ:
            case ::AlignmentMode::GERMAN_POLAR:
            case ::AlignmentMode::FORK:
                break;
            default:
                setLastError("Invalid alignment mode");
                return false;
        }

        // Convert template alignment mode to ASCOM alignment mode
        auto ascomMode = convertTemplateToASCOMAlignmentMode(mode);
        
        // Set alignment mode through hardware interface
        bool success = hardware_->setAlignmentMode(ascomMode);
        if (!success) {
            setLastError("Failed to set alignment mode in hardware");
            return false;
        }

        clearError();
        return true;
    } catch (const std::exception& e) {
        setLastError("Exception in setAlignmentMode: " + std::string(e.what()));
        return false;
    }
}

bool AlignmentManager::addAlignmentPoint(const ::EquatorialCoordinates& measured,
                                       const ::EquatorialCoordinates& target) {
    try {
        if (!hardware_->isConnected()) {
            setLastError("Telescope not connected");
            return false;
        }

        // Validate coordinates
        if (measured.ra < 0.0 || measured.ra >= 24.0) {
            setLastError("Invalid measured RA coordinate (must be 0-24 hours)");
            return false;
        }
        if (measured.dec < -90.0 || measured.dec > 90.0) {
            setLastError("Invalid measured DEC coordinate (must be -90 to +90 degrees)");
            return false;
        }
        if (target.ra < 0.0 || target.ra >= 24.0) {
            setLastError("Invalid target RA coordinate (must be 0-24 hours)");
            return false;
        }
        if (target.dec < -90.0 || target.dec > 90.0) {
            setLastError("Invalid target DEC coordinate (must be -90 to +90 degrees)");
            return false;
        }

        // Check if we can add more alignment points
        int currentCount = getAlignmentPointCount();
        if (currentCount < 0) {
            setLastError("Failed to get current alignment point count");
            return false;
        }

        // Most telescopes support a maximum number of alignment points
        constexpr int MAX_ALIGNMENT_POINTS = 100;
        if (currentCount >= MAX_ALIGNMENT_POINTS) {
            setLastError("Maximum number of alignment points reached");
            return false;
        }

        // Convert coordinates
        auto ascomMeasured = convertTemplateToASCOMCoordinates(measured);
        auto ascomTarget = convertTemplateToASCOMCoordinates(target);

        // Add alignment point through hardware interface
        bool success = hardware_->addAlignmentPoint(ascomMeasured, ascomTarget);
        if (!success) {
            setLastError("Failed to add alignment point to hardware");
            return false;
        }

        clearError();
        return true;
    } catch (const std::exception& e) {
        setLastError("Exception in addAlignmentPoint: " + std::string(e.what()));
        return false;
    }
}

bool AlignmentManager::clearAlignment() {
    try {
        if (!hardware_->isConnected()) {
            setLastError("Telescope not connected");
            return false;
        }

        // Clear all alignment points through hardware interface
        bool success = hardware_->clearAlignment();
        if (!success) {
            setLastError("Failed to clear alignment in hardware");
            return false;
        }

        clearError();
        return true;
    } catch (const std::exception& e) {
        setLastError("Exception in clearAlignment: " + std::string(e.what()));
        return false;
    }
}

int AlignmentManager::getAlignmentPointCount() const {
    try {
        if (!hardware_->isConnected()) {
            setLastError("Telescope not connected");
            return -1;
        }

        // Get alignment point count from hardware
        auto result = hardware_->getAlignmentPointCount();
        if (!result) {
            setLastError("Failed to retrieve alignment point count from hardware");
            return -1;
        }

        return *result;
    } catch (const std::exception& e) {
        setLastError("Exception in getAlignmentPointCount: " + std::string(e.what()));
        return -1;
    }
}

std::string AlignmentManager::getLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

void AlignmentManager::clearError() {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_.clear();
}

void AlignmentManager::setLastError(const std::string& error) const {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = error;
}

} // namespace lithium::device::ascom::telescope::components
