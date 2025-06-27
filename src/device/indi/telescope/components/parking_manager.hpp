/*
 * parking_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Parking Manager Component

This component manages telescope parking operations including
park positions, parking sequences, and unparking procedures.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "device/template/telescope.hpp"

namespace lithium::device::indi::telescope::components {

class HardwareInterface;

/**
 * @brief Parking Manager for INDI Telescope
 * 
 * Manages all telescope parking operations including custom park positions,
 * parking sequences, safety checks, and unparking procedures.
 */
class ParkingManager {
public:
    enum class ParkState {
        UNPARKED,
        PARKING,
        PARKED,
        UNPARKING,
        PARK_ERROR,
        UNKNOWN
    };

    struct ParkPosition {
        double ra = 0.0;        // hours
        double dec = 0.0;       // degrees
        double azimuth = 0.0;   // degrees (if alt-az mount)
        double altitude = 0.0;  // degrees (if alt-az mount)
        std::string name;
        std::string description;
        bool isDefault = false;
        std::chrono::system_clock::time_point createdTime;
    };

    struct ParkingStatus {
        ParkState state = ParkState::UNKNOWN;
        ParkPosition currentParkPosition;
        double parkProgress = 0.0;      // 0.0 to 1.0
        std::chrono::steady_clock::time_point operationStartTime;
        std::string statusMessage;
        bool canPark = false;
        bool canUnpark = false;
    };

    using ParkCompleteCallback = std::function<void(bool success, const std::string& message)>;
    using ParkProgressCallback = std::function<void(double progress, const std::string& status)>;

public:
    explicit ParkingManager(std::shared_ptr<HardwareInterface> hardware);
    ~ParkingManager();

    // Non-copyable and non-movable
    ParkingManager(const ParkingManager&) = delete;
    ParkingManager& operator=(const ParkingManager&) = delete;
    ParkingManager(ParkingManager&&) = delete;
    ParkingManager& operator=(ParkingManager&&) = delete;

    // Initialization
    bool initialize();
    bool shutdown();
    bool isInitialized() const { return initialized_; }

    // Basic Parking Operations
    bool park();
    bool unpark();
    bool abortParkingOperation();
    bool isParked() const;
    bool isParking() const;
    bool isUnparking() const;
    bool canPark() const;
    bool canUnpark() const;

    // Park Position Management
    bool setParkPosition(double ra, double dec);
    bool setParkPosition(const ParkPosition& position);
    std::optional<ParkPosition> getCurrentParkPosition() const;
    std::optional<ParkPosition> getDefaultParkPosition() const;
    bool setDefaultParkPosition(const ParkPosition& position);

    // Custom Park Positions
    bool saveParkPosition(const std::string& name, const std::string& description = "");
    bool loadParkPosition(const std::string& name);
    bool deleteParkPosition(const std::string& name);
    std::vector<ParkPosition> getAllParkPositions() const;
    bool setParkPositionFromCurrent(const std::string& name);

    // Park Options and Behavior
    bool setParkOption(ParkOptions option);
    ParkOptions getCurrentParkOption() const;
    bool setAutoParkOnDisconnect(bool enable);
    bool isAutoParkOnDisconnectEnabled() const;

    // Parking Status and Progress
    ParkingStatus getParkingStatus() const;
    ParkState getParkState() const { return currentState_; }
    std::string getParkStateString() const;
    double getParkingProgress() const;

    // Safety and Validation
    bool isSafeToPark() const;
    bool isSafeToUnpark() const;
    std::vector<std::string> getParkingSafetyChecks() const;
    bool validateParkPosition(const ParkPosition& position) const;

    // Advanced Features
    bool parkToPosition(const ParkPosition& position);
    bool parkToAltAz(double azimuth, double altitude);
    bool setCustomParkingSequence(const std::vector<std::string>& sequence);
    bool enableParkingConfirmation(bool enable);

    // Callback Registration
    void setParkCompleteCallback(ParkCompleteCallback callback) { parkCompleteCallback_ = std::move(callback); }
    void setParkProgressCallback(ParkProgressCallback callback) { parkProgressCallback_ = std::move(callback); }

    // Emergency Operations
    bool emergencyPark();
    bool forceParkState(ParkState state); // Use with caution
    bool recoverFromParkError();

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<ParkState> currentState_{ParkState::UNKNOWN};
    mutable std::recursive_mutex stateMutex_;

    // Park positions
    ParkPosition currentParkPosition_;
    ParkPosition defaultParkPosition_;
    std::vector<ParkPosition> savedParkPositions_;

    // Parking configuration
    ParkOptions currentParkOption_{ParkOptions::CURRENT};
    std::atomic<bool> autoParkOnDisconnect_{false};
    std::atomic<bool> parkingConfirmationEnabled_{true};

    // Operation tracking
    std::chrono::steady_clock::time_point operationStartTime_;
    std::atomic<double> parkingProgress_{0.0};
    std::string lastStatusMessage_;

    // Callbacks
    ParkCompleteCallback parkCompleteCallback_;
    ParkProgressCallback parkProgressCallback_;

    // Internal methods
    void updateParkingStatus();
    void updateParkingProgress();
    void handlePropertyUpdate(const std::string& propertyName);
    
    // Parking sequence management
    bool executeParkingSequence();
    bool executeUnparkingSequence();
    bool performSafetyChecks() const;
    
    // Position management
    void loadSavedParkPositions();
    void saveParkPositionsToFile();
    ParkPosition createParkPositionFromCurrent() const;
    
    // State conversion
    std::string stateToString(ParkState state) const;
    ParkState stringToState(const std::string& stateStr) const;
    
    // Validation helpers
    bool isValidParkCoordinates(double ra, double dec) const;
    bool isValidAltAzCoordinates(double azimuth, double altitude) const;
    
    // Hardware interaction
    void syncParkStateToHardware();
    void syncParkPositionToHardware();
    
    // Utility methods
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);
    
    // Configuration constants
    static constexpr double MAX_PARK_TIME_SECONDS = 300.0;  // 5 minutes max park time
    static constexpr double PARK_POSITION_TOLERANCE = 0.1;  // degrees
    static const std::string PARK_POSITIONS_FILE;
};

} // namespace lithium::device::indi::telescope::components
