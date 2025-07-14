/*
 * telescope_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: Modular INDI Telescope Controller

This modular controller orchestrates the telescope components to provide
a clean, maintainable, and testable interface for INDI telescope control,
following the same architecture pattern as the ASI Camera system.

*************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include "components/hardware_interface.hpp"
#include "components/motion_controller.hpp"
#include "components/tracking_manager.hpp"
#include "components/parking_manager.hpp"
#include "components/coordinate_manager.hpp"
#include "components/guide_manager.hpp"

#include "device/template/telescope.hpp"

namespace lithium::device::indi::telescope {

// Forward declarations
namespace components {
class HardwareInterface;
class MotionController;
class TrackingManager;
class ParkingManager;
class CoordinateManager;
class GuideManager;
}

/**
 * @brief Modular INDI Telescope Controller
 *
 * This controller provides a clean interface to INDI telescope functionality by
 * orchestrating specialized components. Each component handles a specific
 * aspect of telescope operation, promoting separation of concerns and
 * testability.
 */
class INDITelescopeController : public AtomTelescope {
public:
    INDITelescopeController();
    explicit INDITelescopeController(const std::string& name);
    ~INDITelescopeController() override;

    // Non-copyable and non-movable
    INDITelescopeController(const INDITelescopeController&) = delete;
    INDITelescopeController& operator=(const INDITelescopeController&) = delete;
    INDITelescopeController(INDITelescopeController&&) = delete;
    INDITelescopeController& operator=(INDITelescopeController&&) = delete;

    // =========================================================================
    // Initialization and Device Management
    // =========================================================================

    /**
     * @brief Initialize the telescope controller
     * @return true if initialization successful, false otherwise
     */
    auto initialize() -> bool override;

    /**
     * @brief Shutdown and cleanup the controller
     * @return true if shutdown successful, false otherwise
     */
    auto destroy() -> bool override;

    /**
     * @brief Check if controller is initialized
     * @return true if initialized, false otherwise
     */
    [[nodiscard]] auto isInitialized() const -> bool;

    /**
     * @brief Connect to a specific telescope device
     * @param deviceName Device name to connect to
     * @param timeout Connection timeout in milliseconds
     * @param maxRetry Maximum retry attempts
     * @return true if connection successful, false otherwise
     */
    auto connect(const std::string& deviceName, int timeout, int maxRetry) -> bool override;

    /**
     * @brief Disconnect from current telescope
     * @return true if disconnection successful, false otherwise
     */
    auto disconnect() -> bool override;

    /**
     * @brief Reconnect to telescope with timeout and retry
     * @param timeout Connection timeout in milliseconds
     * @param maxRetry Maximum retry attempts
     * @return true if reconnection successful, false otherwise
     */
    auto reconnect(int timeout, int maxRetry) -> bool;

    /**
     * @brief Scan for available telescope devices
     * @return Vector of device names
     */
    auto scan() -> std::vector<std::string> override;

    /**
     * @brief Check if connected to a telescope
     * @return true if connected, false otherwise
     */
    [[nodiscard]] auto isConnected() const -> bool override;

    // =========================================================================
    // Telescope Information and Configuration
    // =========================================================================

    /**
     * @brief Get telescope information
     * @return Telescope parameters if available
     */
    auto getTelescopeInfo() -> std::optional<TelescopeParameters> override;

    /**
     * @brief Set telescope information
     * @param telescopeAperture Telescope aperture in mm
     * @param telescopeFocal Telescope focal length in mm
     * @param guiderAperture Guider aperture in mm
     * @param guiderFocal Guider focal length in mm
     * @return true if set successfully, false otherwise
     */
    auto setTelescopeInfo(double telescopeAperture, double telescopeFocal,
                         double guiderAperture, double guiderFocal) -> bool override;

    /**
     * @brief Get current telescope status
     * @return Status string if available
     */
    auto getStatus() -> std::optional<std::string> override;

    /**
     * @brief Get last error message
     * @return Error message
     */
    [[nodiscard]] auto getLastError() const -> std::string;

    // =========================================================================
    // Motion Control
    // =========================================================================

    /**
     * @brief Start slewing to RA/DEC coordinates
     * @param raHours Right ascension in hours
     * @param decDegrees Declination in degrees
     * @param enableTracking Enable tracking after slew
     * @return true if slew started successfully, false otherwise
     */
    auto slewToRADECJNow(double raHours, double decDegrees, bool enableTracking = true) -> bool override;

    /**
     * @brief Sync telescope to RA/DEC coordinates
     * @param raHours Right ascension in hours
     * @param decDegrees Declination in degrees
     * @return true if sync successful, false otherwise
     */
    auto syncToRADECJNow(double raHours, double decDegrees) -> bool override;

    /**
     * @brief Slew to Alt/Az coordinates
     * @param azDegrees Azimuth in degrees
     * @param altDegrees Altitude in degrees
     * @return true if slew started successfully, false otherwise
     */
    auto slewToAZALT(double azDegrees, double altDegrees) -> bool override;

    /**
     * @brief Abort current motion
     * @return true if abort successful, false otherwise
     */
    auto abortMotion() -> bool override;

    /**
     * @brief Emergency stop all motion
     * @return true if emergency stop successful, false otherwise
     */
    auto emergencyStop() -> bool override;

    /**
     * @brief Check if telescope is moving
     * @return true if moving, false otherwise
     */
    auto isMoving() -> bool override;

    // =========================================================================
    // Directional Movement
    // =========================================================================

    /**
     * @brief Start directional movement
     * @param nsDirection North/South direction
     * @param ewDirection East/West direction
     * @return true if movement started, false otherwise
     */
    auto startMotion(MotionNS nsDirection, MotionEW ewDirection) -> bool override;

    /**
     * @brief Stop directional movement
     * @param nsDirection North/South direction
     * @param ewDirection East/West direction
     * @return true if movement stopped, false otherwise
     */
    auto stopMotion(MotionNS nsDirection, MotionEW ewDirection) -> bool override;

    // =========================================================================
    // Tracking Control
    // =========================================================================

    /**
     * @brief Enable or disable tracking
     * @param enable True to enable tracking, false to disable
     * @return true if tracking state changed successfully, false otherwise
     */
    auto enableTracking(bool enable) -> bool override;

    /**
     * @brief Check if tracking is enabled
     * @return true if tracking enabled, false otherwise
     */
    auto isTrackingEnabled() -> bool override;

    /**
     * @brief Set tracking mode
     * @param rate Tracking mode to set
     * @return true if tracking mode set successfully, false otherwise
     */
    auto setTrackRate(TrackMode rate) -> bool override;

    /**
     * @brief Get current tracking mode
     * @return Current tracking mode if available
     */
    auto getTrackRate() -> std::optional<TrackMode> override;

    /**
     * @brief Set custom tracking rates
     * @param rates Motion rates for RA and DEC
     * @return true if rates set successfully, false otherwise
     */
    auto setTrackRates(const MotionRates& rates) -> bool override;

    /**
     * @brief Get current tracking rates
     * @return Current tracking rates
     */
    auto getTrackRates() -> MotionRates override;

    // =========================================================================
    // Parking Operations
    // =========================================================================

    /**
     * @brief Park the telescope
     * @return true if parking started successfully, false otherwise
     */
    auto park() -> bool override;

    /**
     * @brief Unpark the telescope
     * @return true if unparking started successfully, false otherwise
     */
    auto unpark() -> bool override;

    /**
     * @brief Check if telescope is parked
     * @return true if parked, false otherwise
     */
    auto isParked() -> bool override;

    /**
     * @brief Check if telescope can park
     * @return true if can park, false otherwise
     */
    auto canPark() -> bool override;

    /**
     * @brief Set park position
     * @param parkRA Park position RA in hours
     * @param parkDEC Park position DEC in degrees
     * @return true if park position set successfully, false otherwise
     */
    auto setParkPosition(double parkRA, double parkDEC) -> bool override;

    /**
     * @brief Get park position
     * @return Park position if available
     */
    auto getParkPosition() -> std::optional<EquatorialCoordinates> override;

    /**
     * @brief Set park option
     * @param option Park option to set
     * @return true if option set successfully, false otherwise
     */
    auto setParkOption(ParkOptions option) -> bool override;

    // =========================================================================
    // Coordinate Access
    // =========================================================================

    /**
     * @brief Get current RA/DEC J2000 coordinates
     * @return Current coordinates if available
     */
    auto getRADECJ2000() -> std::optional<EquatorialCoordinates> override;

    /**
     * @brief Set target RA/DEC J2000 coordinates
     * @param raHours Right ascension in hours
     * @param decDegrees Declination in degrees
     * @return true if coordinates set successfully, false otherwise
     */
    auto setRADECJ2000(double raHours, double decDegrees) -> bool override;

    /**
     * @brief Get current RA/DEC JNow coordinates
     * @return Current coordinates if available
     */
    auto getRADECJNow() -> std::optional<EquatorialCoordinates> override;

    /**
     * @brief Set target RA/DEC JNow coordinates
     * @param raHours Right ascension in hours
     * @param decDegrees Declination in degrees
     * @return true if coordinates set successfully, false otherwise
     */
    auto setRADECJNow(double raHours, double decDegrees) -> bool override;

    /**
     * @brief Get target RA/DEC JNow coordinates
     * @return Target coordinates if available
     */
    auto getTargetRADECJNow() -> std::optional<EquatorialCoordinates> override;

    /**
     * @brief Set target RA/DEC JNow coordinates
     * @param raHours Right ascension in hours
     * @param decDegrees Declination in degrees
     * @return true if coordinates set successfully, false otherwise
     */
    auto setTargetRADECJNow(double raHours, double decDegrees) -> bool override;

    /**
     * @brief Get current Alt/Az coordinates
     * @return Current coordinates if available
     */
    auto getAZALT() -> std::optional<HorizontalCoordinates> override;

    /**
     * @brief Set Alt/Az coordinates
     * @param azDegrees Azimuth in degrees
     * @param altDegrees Altitude in degrees
     * @return true if coordinates set successfully, false otherwise
     */
    auto setAZALT(double azDegrees, double altDegrees) -> bool override;

    // =========================================================================
    // Location and Time
    // =========================================================================

    /**
     * @brief Get observer location
     * @return Geographic location if available
     */
    auto getLocation() -> std::optional<GeographicLocation> override;

    /**
     * @brief Set observer location
     * @param location Geographic location to set
     * @return true if location set successfully, false otherwise
     */
    auto setLocation(const GeographicLocation& location) -> bool override;

    /**
     * @brief Get UTC time
     * @return UTC time if available
     */
    auto getUTCTime() -> std::optional<std::chrono::system_clock::time_point> override;

    /**
     * @brief Set UTC time
     * @param time UTC time to set
     * @return true if time set successfully, false otherwise
     */
    auto setUTCTime(const std::chrono::system_clock::time_point& time) -> bool override;

    /**
     * @brief Get local time
     * @return Local time if available
     */
    auto getLocalTime() -> std::optional<std::chrono::system_clock::time_point> override;

    // =========================================================================
    // Guiding Operations
    // =========================================================================

    /**
     * @brief Send guide pulse in North/South direction
     * @param direction Direction (1 = North, -1 = South)
     * @param duration Duration in milliseconds
     * @return true if pulse sent successfully, false otherwise
     */
    auto guideNS(int direction, int duration) -> bool override;

    /**
     * @brief Send guide pulse in East/West direction
     * @param direction Direction (1 = East, -1 = West)
     * @param duration Duration in milliseconds
     * @return true if pulse sent successfully, false otherwise
     */
    auto guideEW(int direction, int duration) -> bool override;

    /**
     * @brief Send guide pulse with RA/DEC corrections
     * @param ra_ms RA correction in milliseconds
     * @param dec_ms DEC correction in milliseconds
     * @return true if pulse sent successfully, false otherwise
     */
    auto guidePulse(double ra_ms, double dec_ms) -> bool override;

    // =========================================================================
    // Slew Rate Control
    // =========================================================================

    /**
     * @brief Set slew rate
     * @param speed Slew rate (0-3: Guide, Centering, Find, Max)
     * @return true if rate set successfully, false otherwise
     */
    auto setSlewRate(double speed) -> bool override;

    /**
     * @brief Get current slew rate
     * @return Current slew rate if available
     */
    auto getSlewRate() -> std::optional<double> override;

    /**
     * @brief Get available slew rates
     * @return Vector of available slew rates
     */
    auto getSlewRates() -> std::vector<double> override;

    /**
     * @brief Set slew rate by index
     * @param index Slew rate index
     * @return true if rate set successfully, false otherwise
     */
    auto setSlewRateIndex(int index) -> bool override;

    // =========================================================================
    // Pier Side
    // =========================================================================

    /**
     * @brief Get pier side
     * @return Current pier side if available
     */
    auto getPierSide() -> std::optional<PierSide> override;

    /**
     * @brief Set pier side
     * @param side Pier side to set
     * @return true if pier side set successfully, false otherwise
     */
    auto setPierSide(PierSide side) -> bool override;

    // =========================================================================
    // Home Position
    // =========================================================================

    /**
     * @brief Initialize home position
     * @param command Initialization command
     * @return true if initialization started successfully, false otherwise
     */
    auto initializeHome(std::string_view command = "") -> bool override;

    /**
     * @brief Find home position
     * @return true if home search started successfully, false otherwise
     */
    auto findHome() -> bool override;

    /**
     * @brief Set current position as home
     * @return true if home position set successfully, false otherwise
     */
    auto setHome() -> bool override;

    /**
     * @brief Go to home position
     * @return true if slew to home started successfully, false otherwise
     */
    auto gotoHome() -> bool override;

    // =========================================================================
    // Alignment
    // =========================================================================

    /**
     * @brief Get alignment mode
     * @return Current alignment mode
     */
    auto getAlignmentMode() -> AlignmentMode override;

    /**
     * @brief Set alignment mode
     * @param mode Alignment mode to set
     * @return true if mode set successfully, false otherwise
     */
    auto setAlignmentMode(AlignmentMode mode) -> bool override;

    /**
     * @brief Add alignment point
     * @param measured Measured coordinates
     * @param target Target coordinates
     * @return true if point added successfully, false otherwise
     */
    auto addAlignmentPoint(const EquatorialCoordinates& measured,
                          const EquatorialCoordinates& target) -> bool override;

    /**
     * @brief Clear alignment
     * @return true if alignment cleared successfully, false otherwise
     */
    auto clearAlignment() -> bool override;

    // =========================================================================
    // Utility Methods
    // =========================================================================

    /**
     * @brief Convert degrees to DMS format
     * @param degrees Angle in degrees
     * @return Tuple of degrees, minutes, seconds
     */
    auto degreesToDMS(double degrees) -> std::tuple<int, int, double> override;

    /**
     * @brief Convert degrees to HMS format
     * @param degrees Angle in degrees
     * @return Tuple of hours, minutes, seconds
     */
    auto degreesToHMS(double degrees) -> std::tuple<int, int, double> override;

    // =========================================================================
    // Component Access (for advanced users)
    // =========================================================================

    /**
     * @brief Get hardware interface component
     * @return Shared pointer to hardware interface
     */
    std::shared_ptr<components::HardwareInterface> getHardwareInterface() const { return hardware_; }

    /**
     * @brief Get motion controller component
     * @return Shared pointer to motion controller
     */
    std::shared_ptr<components::MotionController> getMotionController() const { return motionController_; }

    /**
     * @brief Get tracking manager component
     * @return Shared pointer to tracking manager
     */
    std::shared_ptr<components::TrackingManager> getTrackingManager() const { return trackingManager_; }

    /**
     * @brief Get parking manager component
     * @return Shared pointer to parking manager
     */
    std::shared_ptr<components::ParkingManager> getParkingManager() const { return parkingManager_; }

    /**
     * @brief Get coordinate manager component
     * @return Shared pointer to coordinate manager
     */
    std::shared_ptr<components::CoordinateManager> getCoordinateManager() const { return coordinateManager_; }

    /**
     * @brief Get guide manager component
     * @return Shared pointer to guide manager
     */
    std::shared_ptr<components::GuideManager> getGuideManager() const { return guideManager_; }

private:
    // Telescope name
    std::string telescopeName_;

    // Component instances
    std::shared_ptr<components::HardwareInterface> hardware_;
    std::shared_ptr<components::MotionController> motionController_;
    std::shared_ptr<components::TrackingManager> trackingManager_;
    std::shared_ptr<components::ParkingManager> parkingManager_;
    std::shared_ptr<components::CoordinateManager> coordinateManager_;
    std::shared_ptr<components::GuideManager> guideManager_;

    // Controller state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};

    // Error handling
    mutable std::string lastError_;
    mutable std::mutex errorMutex_;

    // Internal methods
    bool initializeComponents();
    bool shutdownComponents();
    void setupComponentCallbacks();
    void handleComponentError(const std::string& component, const std::string& error);

    // Component coordination
    void coordinateComponentStates();
    void validateComponentDependencies();

    // Error management
    void setLastError(const std::string& error);
    void clearLastError();

    // Utility methods
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);
};

} // namespace lithium::device::indi::telescope
