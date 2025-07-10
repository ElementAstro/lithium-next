/*
 * motion_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Motion Controller Component

This component manages all motion-related functionality including
directional movement, slew operations, motion rates, and motion monitoring.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace lithium::device::ascom::telescope::components {

// Forward declaration
class HardwareInterface;

/**
 * @brief Motion states for movement tracking
 */
enum class MotionState {
    IDLE,
    SLEWING,
    TRACKING,
    MOVING_NORTH,
    MOVING_SOUTH,
    MOVING_EAST,
    MOVING_WEST,
    ABORTING,
    ERROR
};

/**
 * @brief Slew rates enumeration
 */
enum class SlewRate {
    GUIDE = 0,
    CENTERING = 1,
    FIND = 2,
    MAX = 3
};

/**
 * @brief Motion Controller for ASCOM Telescope
 *
 * This component handles all telescope motion operations including
 * slewing, directional movement, rate control, and motion monitoring.
 */
class MotionController {
public:
    explicit MotionController(std::shared_ptr<HardwareInterface> hardware);
    ~MotionController();

    // Non-copyable and non-movable
    MotionController(const MotionController&) = delete;
    MotionController& operator=(const MotionController&) = delete;
    MotionController(MotionController&&) = delete;
    MotionController& operator=(MotionController&&) = delete;

    // =========================================================================
    // Initialization and State Management
    // =========================================================================

    /**
     * @brief Initialize the motion controller
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the motion controller
     * @return true if shutdown successful
     */
    bool shutdown();

    /**
     * @brief Get current motion state
     * @return Current motion state
     */
    MotionState getState() const;

    /**
     * @brief Check if telescope is moving
     * @return true if any motion is active
     */
    bool isMoving() const;

    // =========================================================================
    // Slew Operations
    // =========================================================================

    /**
     * @brief Start slewing to RA/DEC coordinates
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @param async true for async slew
     * @return true if slew started successfully
     */
    bool slewToRADEC(double ra, double dec, bool async = false);

    /**
     * @brief Start slewing to AZ/ALT coordinates
     * @param az Azimuth in degrees
     * @param alt Altitude in degrees
     * @param async true for async slew
     * @return true if slew started successfully
     */
    bool slewToAZALT(double az, double alt, bool async = false);

    /**
     * @brief Check if telescope is slewing
     * @return true if slewing
     */
    bool isSlewing() const;

    /**
     * @brief Get slew progress (0.0 to 1.0)
     * @return Progress value or nullopt if not available
     */
    std::optional<double> getSlewProgress() const;

    /**
     * @brief Get estimated time remaining for slew
     * @return Remaining time in seconds
     */
    std::optional<double> getSlewTimeRemaining() const;

    /**
     * @brief Abort current slew operation
     * @return true if abort successful
     */
    bool abortSlew();

    // =========================================================================
    // Directional Movement
    // =========================================================================

    /**
     * @brief Start moving in specified direction
     * @param direction Movement direction (N, S, E, W)
     * @param rate Movement rate
     * @return true if movement started
     */
    bool startDirectionalMove(const std::string& direction, double rate);

    /**
     * @brief Stop movement in specified direction
     * @param direction Movement direction (N, S, E, W)
     * @return true if movement stopped
     */
    bool stopDirectionalMove(const std::string& direction);

    /**
     * @brief Stop all movement
     * @return true if all movement stopped
     */
    bool stopAllMovement();

    /**
     * @brief Emergency stop all motion
     * @return true if emergency stop successful
     */
    bool emergencyStop();

    // =========================================================================
    // Slew Rate Management
    // =========================================================================

    /**
     * @brief Get current slew rate
     * @return Current slew rate
     */
    std::optional<double> getCurrentSlewRate() const;

    /**
     * @brief Set slew rate
     * @param rate Slew rate value
     * @return true if operation successful
     */
    bool setSlewRate(double rate);

    /**
     * @brief Get available slew rates
     * @return Vector of available slew rates
     */
    std::vector<double> getAvailableSlewRates() const;

    /**
     * @brief Set slew rate by index
     * @param index Rate index
     * @return true if operation successful
     */
    bool setSlewRateIndex(int index);

    /**
     * @brief Get current slew rate index
     * @return Current rate index
     */
    std::optional<int> getCurrentSlewRateIndex() const;

    // =========================================================================
    // Motion Monitoring
    // =========================================================================

    /**
     * @brief Start motion monitoring
     * @return true if monitoring started
     */
    bool startMonitoring();

    /**
     * @brief Stop motion monitoring
     * @return true if monitoring stopped
     */
    bool stopMonitoring();

    /**
     * @brief Check if monitoring is active
     * @return true if monitoring
     */
    bool isMonitoring() const;

    /**
     * @brief Set motion update callback
     * @param callback Function to call on motion updates
     */
    void setMotionUpdateCallback(std::function<void(MotionState)> callback);

    // =========================================================================
    // Status and Information
    // =========================================================================

    /**
     * @brief Get motion status description
     * @return Status string
     */
    std::string getMotionStatus() const;

    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;

    /**
     * @brief Clear last error
     */
    void clearError();

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<MotionState> state_;
    mutable std::mutex stateMutex_;

    // Motion monitoring
    std::atomic<bool> monitorRunning_;
    std::unique_ptr<std::thread> monitorThread_;
    std::function<void(MotionState)> motionUpdateCallback_;

    // Slew rate management
    std::vector<double> availableSlewRates_;
    std::atomic<int> currentSlewRateIndex_;
    mutable std::mutex slewRateMutex_;

    // Motion tracking
    std::chrono::steady_clock::time_point slewStartTime_;
    std::atomic<bool> northMoving_;
    std::atomic<bool> southMoving_;
    std::atomic<bool> eastMoving_;
    std::atomic<bool> westMoving_;

    // Error handling
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;

    // Private methods
    void setState(MotionState newState);
    void setLastError(const std::string& error) const;
    void monitoringLoop();
    void updateMotionState();
    bool validateDirection(const std::string& direction) const;
    bool initializeSlewRates();
    MotionState determineCurrentState() const;
};

} // namespace lithium::device::ascom::telescope::components
