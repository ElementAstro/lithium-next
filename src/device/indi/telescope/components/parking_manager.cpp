/*
 * parking_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Parking Manager Implementation

This component manages telescope parking operations including
park positions, parking sequences, and unparking procedures.

*************************************************/

#include "parking_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include "atom/utils/string.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace lithium::device::indi::telescope::components {

const std::string ParkingManager::PARK_POSITIONS_FILE = "park_positions.json";

ParkingManager::ParkingManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {
    if (!hardware_) {
        throw std::invalid_argument("Hardware interface cannot be null");
    }

    // Initialize default park position
    defaultParkPosition_.ra = 0.0;
    defaultParkPosition_.dec = 90.0;  // Point to NCP
    defaultParkPosition_.name = "Default";
    defaultParkPosition_.description = "Default park position at North Celestial Pole";
    defaultParkPosition_.isDefault = true;
    defaultParkPosition_.createdTime = std::chrono::system_clock::now();

    currentParkPosition_ = defaultParkPosition_;
}

ParkingManager::~ParkingManager() {
    shutdown();
}

bool ParkingManager::initialize() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (initialized_) {
        logWarning("Parking manager already initialized");
        return true;
    }

    if (!hardware_->isConnected()) {
        logError("Hardware interface not connected");
        return false;
    }

    try {
        // Load saved park positions
        loadSavedParkPositions();

        // Get current park state from hardware
        auto parkData = hardware_->getProperty("TELESCOPE_PARK");
        if (parkData && !parkData->empty()) {
            auto parkSwitch = parkData->find("PARK");
            auto unparkSwitch = parkData->find("UNPARK");

            if (parkSwitch != parkData->end() && parkSwitch->second.value == "On") {
                currentState_ = ParkState::PARKED;
            } else if (unparkSwitch != parkData->end() && unparkSwitch->second.value == "On") {
                currentState_ = ParkState::UNPARKED;
            } else {
                currentState_ = ParkState::UNKNOWN;
            }
        }

        // Get current park position if available
        auto parkPosData = hardware_->getProperty("TELESCOPE_PARK_POSITION");
        if (parkPosData && !parkPosData->empty()) {
            auto raElement = parkPosData->find("PARK_RA");
            auto decElement = parkPosData->find("PARK_DEC");

            if (raElement != parkPosData->end() && decElement != parkPosData->end()) {
                currentParkPosition_.ra = std::stod(raElement->second.value);
                currentParkPosition_.dec = std::stod(decElement->second.value);
            }
        }

        initialized_ = true;
        logInfo("Parking manager initialized successfully");
        return true;

    } catch (const std::exception& e) {
        logError("Failed to initialize parking manager: " + std::string(e.what()));
        return false;
    }
}

bool ParkingManager::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        return true;
    }

    try {
        // Save current park positions to file
        saveParkPositionsToFile();

        // If auto-park on disconnect is enabled and telescope is unparked, park it
        if (autoParkOnDisconnect_ && currentState_ == ParkState::UNPARKED) {
            logInfo("Auto-parking telescope on disconnect");
            park();
        }

        initialized_ = false;
        logInfo("Parking manager shut down successfully");
        return true;

    } catch (const std::exception& e) {
        logError("Error during parking manager shutdown: " + std::string(e.what()));
        return false;
    }
}

bool ParkingManager::park() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Parking manager not initialized");
        return false;
    }

    if (currentState_ == ParkState::PARKED) {
        logInfo("Telescope already parked");
        return true;
    }

    if (currentState_ == ParkState::PARKING || currentState_ == ParkState::UNPARKING) {
        logWarning("Parking operation already in progress");
        return false;
    }

    if (!isSafeToPark()) {
        logError("Safety checks failed - cannot park telescope");
        return false;
    }

    try {
        currentState_ = ParkState::PARKING;
        operationStartTime_ = std::chrono::steady_clock::now();
        parkingProgress_ = 0.0;

        // Execute parking sequence
        if (!executeParkingSequence()) {
            currentState_ = ParkState::PARK_ERROR;
            logError("Failed to execute parking sequence");
            return false;
        }

        logInfo("Park command sent successfully");
        return true;

    } catch (const std::exception& e) {
        currentState_ = ParkState::PARK_ERROR;
        logError("Error during park operation: " + std::string(e.what()));
        return false;
    }
}

bool ParkingManager::unpark() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!initialized_) {
        logError("Parking manager not initialized");
        return false;
    }

    if (currentState_ == ParkState::UNPARKED) {
        logInfo("Telescope already unparked");
        return true;
    }

    if (currentState_ == ParkState::PARKING || currentState_ == ParkState::UNPARKING) {
        logWarning("Parking operation already in progress");
        return false;
    }

    if (!isSafeToUnpark()) {
        logError("Safety checks failed - cannot unpark telescope");
        return false;
    }

    try {
        currentState_ = ParkState::UNPARKING;
        operationStartTime_ = std::chrono::steady_clock::now();
        parkingProgress_ = 0.0;

        // Execute unparking sequence
        if (!executeUnparkingSequence()) {
            currentState_ = ParkState::PARK_ERROR;
            logError("Failed to execute unparking sequence");
            return false;
        }

        logInfo("Unpark command sent successfully");
        return true;

    } catch (const std::exception& e) {
        currentState_ = ParkState::PARK_ERROR;
        logError("Error during unpark operation: " + std::string(e.what()));
        return false;
    }
}

bool ParkingManager::abortParkingOperation() {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (currentState_ != ParkState::PARKING && currentState_ != ParkState::UNPARKING) {
        logWarning("No parking operation in progress to abort");
        return false;
    }

    try {
        // Send abort command to hardware
        hardware_->sendCommand("TELESCOPE_ABORT_MOTION", {{"ABORT", "On"}});

        // Reset state
        if (currentState_ == ParkState::PARKING) {
            currentState_ = ParkState::UNPARKED;
        } else {
            currentState_ = ParkState::PARKED;
        }

        parkingProgress_ = 0.0;
        logInfo("Parking operation aborted");
        return true;

    } catch (const std::exception& e) {
        logError("Error aborting parking operation: " + std::string(e.what()));
        return false;
    }
}

bool ParkingManager::isParked() const {
    return currentState_ == ParkState::PARKED;
}

bool ParkingManager::isParking() const {
    return currentState_ == ParkState::PARKING;
}

bool ParkingManager::isUnparking() const {
    return currentState_ == ParkState::UNPARKING;
}

bool ParkingManager::canPark() const {
    return initialized_ &&
           currentState_ != ParkState::PARKING &&
           currentState_ != ParkState::UNPARKING &&
           isSafeToPark();
}

bool ParkingManager::canUnpark() const {
    return initialized_ &&
           currentState_ == ParkState::PARKED &&
           isSafeToUnpark();
}

bool ParkingManager::setParkPosition(double ra, double dec) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    if (!isValidParkCoordinates(ra, dec)) {
        logError("Invalid park coordinates: RA=" + std::to_string(ra) + ", DEC=" + std::to_string(dec));
        return false;
    }

    try {
        currentParkPosition_.ra = ra;
        currentParkPosition_.dec = dec;
        currentParkPosition_.name = "Custom";
        currentParkPosition_.description = "Custom park position";
        currentParkPosition_.createdTime = std::chrono::system_clock::now();

        // Sync to hardware
        syncParkPositionToHardware();

        logInfo("Park position set to RA=" + std::to_string(ra) + ", DEC=" + std::to_string(dec));
        return true;

    } catch (const std::exception& e) {
        logError("Error setting park position: " + std::string(e.what()));
        return false;
    }
}

bool ParkingManager::setParkPosition(const ParkPosition& position) {
    if (!validateParkPosition(position)) {
        logError("Invalid park position provided");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    currentParkPosition_ = position;
    syncParkPositionToHardware();

    logInfo("Park position set to: " + position.name);
    return true;
}

std::optional<ParkingManager::ParkPosition> ParkingManager::getCurrentParkPosition() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return currentParkPosition_;
}

std::optional<ParkingManager::ParkPosition> ParkingManager::getDefaultParkPosition() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return defaultParkPosition_;
}

bool ParkingManager::setDefaultParkPosition(const ParkPosition& position) {
    if (!validateParkPosition(position)) {
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    defaultParkPosition_ = position;
    defaultParkPosition_.isDefault = true;

    logInfo("Default park position updated");
    return true;
}

bool ParkingManager::saveParkPosition(const std::string& name, const std::string& description) {
    if (name.empty()) {
        logError("Park position name cannot be empty");
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    // Get current telescope position
    auto coords = hardware_->getCurrentCoordinates();
    if (!coords) {
        logError("Could not get current telescope coordinates");
        return false;
    }

    ParkPosition newPosition;
    newPosition.ra = coords->ra;
    newPosition.dec = coords->dec;
    newPosition.name = name;
    newPosition.description = description.empty() ? "Saved park position" : description;
    newPosition.isDefault = false;
    newPosition.createdTime = std::chrono::system_clock::now();

    // Remove existing position with same name
    auto it = std::find_if(savedParkPositions_.begin(), savedParkPositions_.end(),
        [&name](const ParkPosition& pos) { return pos.name == name; });
    if (it != savedParkPositions_.end()) {
        savedParkPositions_.erase(it);
    }

    savedParkPositions_.push_back(newPosition);
    saveParkPositionsToFile();

    logInfo("Park position '" + name + "' saved");
    return true;
}

bool ParkingManager::loadParkPosition(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    auto it = std::find_if(savedParkPositions_.begin(), savedParkPositions_.end(),
        [&name](const ParkPosition& pos) { return pos.name == name; });

    if (it == savedParkPositions_.end()) {
        logError("Park position '" + name + "' not found");
        return false;
    }

    currentParkPosition_ = *it;
    syncParkPositionToHardware();

    logInfo("Park position '" + name + "' loaded");
    return true;
}

bool ParkingManager::deleteParkPosition(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    auto it = std::find_if(savedParkPositions_.begin(), savedParkPositions_.end(),
        [&name](const ParkPosition& pos) { return pos.name == name; });

    if (it == savedParkPositions_.end()) {
        logError("Park position '" + name + "' not found");
        return false;
    }

    savedParkPositions_.erase(it);
    saveParkPositionsToFile();

    logInfo("Park position '" + name + "' deleted");
    return true;
}

std::vector<ParkingManager::ParkPosition> ParkingManager::getAllParkPositions() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    return savedParkPositions_;
}

bool ParkingManager::setParkPositionFromCurrent(const std::string& name) {
    auto coords = hardware_->getCurrentCoordinates();
    if (!coords) {
        logError("Could not get current telescope coordinates");
        return false;
    }

    ParkPosition position;
    position.ra = coords->ra;
    position.dec = coords->dec;
    position.name = name;
    position.description = "Set from current position";
    position.createdTime = std::chrono::system_clock::now();

    return setParkPosition(position);
}

ParkingManager::ParkingStatus ParkingManager::getParkingStatus() const {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);

    ParkingStatus status;
    status.state = currentState_;
    status.currentParkPosition = currentParkPosition_;
    status.parkProgress = parkingProgress_;
    status.operationStartTime = operationStartTime_;
    status.statusMessage = lastStatusMessage_;
    status.canPark = canPark();
    status.canUnpark = canUnpark();

    return status;
}

std::string ParkingManager::getParkStateString() const {
    return stateToString(currentState_);
}

double ParkingManager::getParkingProgress() const {
    return parkingProgress_;
}

bool ParkingManager::isSafeToPark() const {
    if (!initialized_ || !hardware_->isConnected()) {
        return false;
    }

    // Check if telescope is tracking - should stop tracking before parking
    auto trackData = hardware_->getProperty("TELESCOPE_TRACK_STATE");
    if (trackData && !trackData->empty()) {
        auto trackSwitch = trackData->find("TRACK_ON");
        if (trackSwitch != trackData->end() && trackSwitch->second.value == "On") {
            logWarning("Telescope is still tracking - should stop tracking before parking");
            // This is just a warning, not a blocking condition
        }
    }

    // Add more safety checks as needed
    return true;
}

bool ParkingManager::isSafeToUnpark() const {
    return initialized_ && hardware_->isConnected();
}

std::vector<std::string> ParkingManager::getParkingSafetyChecks() const {
    std::vector<std::string> checks;

    if (!initialized_) {
        checks.push_back("Parking manager not initialized");
    }

    if (!hardware_->isConnected()) {
        checks.push_back("Hardware not connected");
    }

    // Add more safety checks
    auto trackData = hardware_->getProperty("TELESCOPE_TRACK_STATE");
    if (trackData && !trackData->empty()) {
        auto trackSwitch = trackData->find("TRACK_ON");
        if (trackSwitch != trackData->end() && trackSwitch->second.value == "On") {
            checks.push_back("Telescope is tracking - recommend stopping tracking first");
        }
    }

    return checks;
}

bool ParkingManager::validateParkPosition(const ParkPosition& position) const {
    return isValidParkCoordinates(position.ra, position.dec);
}

bool ParkingManager::executeParkingSequence() {
    try {
        // Set park position if needed
        syncParkPositionToHardware();

        // Send park command
        hardware_->sendCommand("TELESCOPE_PARK", {{"PARK", "On"}});

        parkingProgress_ = 0.5;  // Command sent

        if (parkProgressCallback_) {
            parkProgressCallback_(parkingProgress_, "Park command sent");
        }

        return true;

    } catch (const std::exception& e) {
        logError("Error in parking sequence: " + std::string(e.what()));
        return false;
    }
}

bool ParkingManager::executeUnparkingSequence() {
    try {
        // Send unpark command
        hardware_->sendCommand("TELESCOPE_PARK", {{"UNPARK", "On"}});

        parkingProgress_ = 0.5;  // Command sent

        if (parkProgressCallback_) {
            parkProgressCallback_(parkingProgress_, "Unpark command sent");
        }

        return true;

    } catch (const std::exception& e) {
        logError("Error in unparking sequence: " + std::string(e.what()));
        return false;
    }
}

bool ParkingManager::performSafetyChecks() const {
    auto checks = getParkingSafetyChecks();
    return std::none_of(checks.begin(), checks.end(),
        [](const std::string& check) {
            return check.find("not") != std::string::npos; // Filter critical checks
        });
}

void ParkingManager::loadSavedParkPositions() {
    try {
        std::ifstream file(PARK_POSITIONS_FILE);
        if (!file.is_open()) {
            logInfo("No saved park positions file found");
            return;
        }

        nlohmann::json j;
        file >> j;

        savedParkPositions_.clear();

        for (const auto& item : j["positions"]) {
            ParkPosition position;
            position.ra = item["ra"];
            position.dec = item["dec"];
            position.name = item["name"];
            position.description = item["description"];
            position.isDefault = item.value("isDefault", false);

            savedParkPositions_.push_back(position);
        }

        logInfo("Loaded " + std::to_string(savedParkPositions_.size()) + " saved park positions");

    } catch (const std::exception& e) {
        logError("Error loading park positions: " + std::string(e.what()));
    }
}

void ParkingManager::saveParkPositionsToFile() {
    try {
        nlohmann::json j;
        j["positions"] = nlohmann::json::array();

        for (const auto& position : savedParkPositions_) {
            nlohmann::json pos;
            pos["ra"] = position.ra;
            pos["dec"] = position.dec;
            pos["name"] = position.name;
            pos["description"] = position.description;
            pos["isDefault"] = position.isDefault;

            j["positions"].push_back(pos);
        }

        std::ofstream file(PARK_POSITIONS_FILE);
        file << j.dump(4);

        logInfo("Saved park positions to file");

    } catch (const std::exception& e) {
        logError("Error saving park positions: " + std::string(e.what()));
    }
}

std::string ParkingManager::stateToString(ParkState state) const {
    switch (state) {
        case ParkState::UNPARKED: return "Unparked";
        case ParkState::PARKING: return "Parking";
        case ParkState::PARKED: return "Parked";
        case ParkState::UNPARKING: return "Unparking";
        case ParkState::PARK_ERROR: return "Park Error";
        case ParkState::UNKNOWN: return "Unknown";
        default: return "Invalid";
    }
}

ParkingManager::ParkState ParkingManager::stringToState(const std::string& stateStr) const {
    if (stateStr == "Unparked") return ParkState::UNPARKED;
    if (stateStr == "Parking") return ParkState::PARKING;
    if (stateStr == "Parked") return ParkState::PARKED;
    if (stateStr == "Unparking") return ParkState::UNPARKING;
    if (stateStr == "Park Error") return ParkState::PARK_ERROR;
    return ParkState::UNKNOWN;
}

bool ParkingManager::isValidParkCoordinates(double ra, double dec) const {
    return ra >= 0.0 && ra < 24.0 && dec >= -90.0 && dec <= 90.0;
}

bool ParkingManager::isValidAltAzCoordinates(double azimuth, double altitude) const {
    return azimuth >= 0.0 && azimuth < 360.0 && altitude >= 0.0 && altitude <= 90.0;
}

void ParkingManager::syncParkStateToHardware() {
    // This would sync the park state to the hardware device
    // Implementation depends on the specific INDI driver
}

void ParkingManager::syncParkPositionToHardware() {
    try {
        std::map<std::string, PropertyElement> elements;
        elements["PARK_RA"] = {std::to_string(currentParkPosition_.ra), ""};
        elements["PARK_DEC"] = {std::to_string(currentParkPosition_.dec), ""};

        hardware_->sendCommand("TELESCOPE_PARK_POSITION", elements);

    } catch (const std::exception& e) {
        logError("Error syncing park position to hardware: " + std::string(e.what()));
    }
}

void ParkingManager::logInfo(const std::string& message) {
    LOG_F(INFO, "[ParkingManager] %s", message.c_str());
}

void ParkingManager::logWarning(const std::string& message) {
    LOG_F(WARNING, "[ParkingManager] %s", message.c_str());
}

void ParkingManager::logError(const std::string& message) {
    LOG_F(ERROR, "[ParkingManager] %s", message.c_str());
}

} // namespace lithium::device::indi::telescope::components
