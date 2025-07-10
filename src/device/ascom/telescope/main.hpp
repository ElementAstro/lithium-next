/*
 * main.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Modular Integration Interface

This file provides the main integration interface for the modular ASCOM telescope
system, providing simplified access to telescope functionality.

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <mutex>

#include "device/template/telescope.hpp"

namespace lithium::device::ascom::telescope {

// Forward declarations
namespace components {
    class HardwareInterface;
    class MotionController;
    class CoordinateManager;
    class GuideManager;
    class TrackingManager;
    class ParkingManager;
    class AlignmentManager;
}

/**
 * @brief Telescope states for state machine management
 */
enum class TelescopeState {
    DISCONNECTED,
    CONNECTED,
    IDLE,
    SLEWING,
    TRACKING,
    PARKED,
    HOMING,
    GUIDING,
    ERROR
};

/**
 * @brief Main ASCOM Telescope integration class
 * 
 * This class provides a simplified interface to the modular telescope components,
 * managing their lifecycle and coordinating their interactions.
 */
class ASCOMTelescopeMain {
public:
    ASCOMTelescopeMain();
    ~ASCOMTelescopeMain();

    // Non-copyable and non-movable
    ASCOMTelescopeMain(const ASCOMTelescopeMain&) = delete;
    ASCOMTelescopeMain& operator=(const ASCOMTelescopeMain&) = delete;
    ASCOMTelescopeMain(ASCOMTelescopeMain&&) = delete;
    ASCOMTelescopeMain& operator=(ASCOMTelescopeMain&&) = delete;

    // =========================================================================
    // Basic Device Operations
    // =========================================================================

    /**
     * @brief Initialize the telescope system
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the telescope system
     * @return true if shutdown successful
     */
    bool shutdown();

    /**
     * @brief Connect to a telescope device
     * @param deviceName Device name or identifier
     * @param timeout Connection timeout in seconds
     * @param maxRetry Maximum number of connection attempts
     * @return true if connection successful
     */
    bool connect(const std::string& deviceName, int timeout = 30, int maxRetry = 3);

    /**
     * @brief Disconnect from current telescope
     * @return true if disconnection successful
     */
    bool disconnect();

    /**
     * @brief Scan for available telescope devices
     * @return Vector of device names/identifiers
     */
    std::vector<std::string> scanDevices();

    /**
     * @brief Check if telescope is connected
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Get current telescope state
     * @return Current state
     */
    TelescopeState getState() const;

    // =========================================================================
    // Coordinate and Position Management
    // =========================================================================

    /**
     * @brief Get current Right Ascension and Declination
     * @return Optional coordinate pair
     */
    std::optional<EquatorialCoordinates> getCurrentRADEC();

    /**
     * @brief Get current Azimuth and Altitude
     * @return Optional coordinate pair
     */
    std::optional<HorizontalCoordinates> getCurrentAZALT();

    /**
     * @brief Slew to specified RA/DEC coordinates
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @param enableTracking Enable tracking after slew
     * @return true if slew started successfully
     */
    bool slewToRADEC(double ra, double dec, bool enableTracking = true);

    /**
     * @brief Slew to specified AZ/ALT coordinates
     * @param az Azimuth in degrees
     * @param alt Altitude in degrees
     * @return true if slew started successfully
     */
    bool slewToAZALT(double az, double alt);

    /**
     * @brief Sync telescope to specified coordinates
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if sync successful
     */
    bool syncToRADEC(double ra, double dec);

    // =========================================================================
    // Motion Control
    // =========================================================================

    /**
     * @brief Check if telescope is currently moving
     * @return true if moving
     */
    bool isSlewing();

    /**
     * @brief Abort current motion
     * @return true if abort successful
     */
    bool abortSlew();

    /**
     * @brief Emergency stop all motion
     * @return true if stop successful
     */
    bool emergencyStop();

    /**
     * @brief Start directional movement
     * @param direction Movement direction
     * @param rate Movement rate
     * @return true if movement started
     */
    bool startDirectionalMove(const std::string& direction, double rate);

    /**
     * @brief Stop directional movement
     * @param direction Movement direction
     * @return true if movement stopped
     */
    bool stopDirectionalMove(const std::string& direction);

    // =========================================================================
    // Tracking Control
    // =========================================================================

    /**
     * @brief Check if tracking is enabled
     * @return true if tracking
     */
    bool isTracking();

    /**
     * @brief Enable or disable tracking
     * @param enable true to enable tracking
     * @return true if operation successful
     */
    bool setTracking(bool enable);

    /**
     * @brief Get current tracking rate
     * @return Optional tracking rate
     */
    std::optional<TrackMode> getTrackingRate();

    /**
     * @brief Set tracking rate
     * @param rate Tracking rate mode
     * @return true if operation successful
     */
    bool setTrackingRate(TrackMode rate);

    // =========================================================================
    // Parking Operations
    // =========================================================================

    /**
     * @brief Check if telescope is parked
     * @return true if parked
     */
    bool isParked();

    /**
     * @brief Park the telescope
     * @return true if park operation successful
     */
    bool park();

    /**
     * @brief Unpark the telescope
     * @return true if unpark operation successful
     */
    bool unpark();

    /**
     * @brief Set park position
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if operation successful
     */
    bool setParkPosition(double ra, double dec);

    // =========================================================================
    // Guiding Operations
    // =========================================================================

    /**
     * @brief Send guide pulse
     * @param direction Guide direction
     * @param duration Duration in milliseconds
     * @return true if guide pulse sent
     */
    bool guidePulse(const std::string& direction, int duration);

    /**
     * @brief Send RA/DEC guide pulse
     * @param ra_ms RA correction in milliseconds
     * @param dec_ms DEC correction in milliseconds
     * @return true if guide pulse sent
     */
    bool guideRADEC(double ra_ms, double dec_ms);

    // =========================================================================
    // Status and Information
    // =========================================================================

    /**
     * @brief Get telescope information
     * @return Optional telescope parameters
     */
    std::optional<TelescopeParameters> getTelescopeInfo();

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
    // Component instances
    std::shared_ptr<components::HardwareInterface> hardware_;
    std::shared_ptr<components::MotionController> motion_;
    std::shared_ptr<components::CoordinateManager> coordinates_;
    std::shared_ptr<components::GuideManager> guide_;
    std::shared_ptr<components::TrackingManager> tracking_;
    std::shared_ptr<components::ParkingManager> parking_;
    std::shared_ptr<components::AlignmentManager> alignment_;

    // State management
    std::atomic<TelescopeState> state_;
    mutable std::mutex stateMutex_;

    // Error handling
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;

    // Helper methods
    void setState(TelescopeState newState);
    void setLastError(const std::string& error) const;
    bool validateConnection() const;
    bool initializeComponents();
    void shutdownComponents();
};

} // namespace lithium::device::ascom::telescope
