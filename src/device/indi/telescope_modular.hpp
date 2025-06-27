/*
 * telescope_modular.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: Modern Modular INDI Telescope Implementation

This class provides a backward-compatible interface to the original
INDITelescope while using the new modular architecture internally.

*************************************************/

#pragma once

#include "device/template/telescope.hpp"
#include "telescope/telescope_controller.hpp"
#include <memory>
#include <string>

namespace lithium::device::indi {

/**
 * @brief Modern modular INDI telescope implementation
 * 
 * This class wraps the new modular telescope controller while maintaining
 * compatibility with the existing AtomTelescope interface. It serves as
 * a drop-in replacement for the original INDITelescope class.
 */
class INDITelescopeModular : public AtomTelescope {
public:
    explicit INDITelescopeModular(const std::string& name);
    ~INDITelescopeModular() override = default;

    // Non-copyable, non-movable
    INDITelescopeModular(const INDITelescopeModular&) = delete;
    INDITelescopeModular& operator=(const INDITelescopeModular&) = delete;
    INDITelescopeModular(INDITelescopeModular&&) = delete;
    INDITelescopeModular& operator=(INDITelescopeModular&&) = delete;

    // =========================================================================
    // AtomTelescope Interface Implementation
    // =========================================================================

    // Device management
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Telescope information
    auto getTelescopeInfo() -> std::optional<TelescopeParameters> override;
    auto setTelescopeInfo(double telescopeAperture, double telescopeFocal,
                         double guiderAperture, double guiderFocal) -> bool override;
    auto getStatus() -> std::optional<std::string> override;

    // Motion control
    auto slewToRADECJNow(double raHours, double decDegrees, bool enableTracking = true) -> bool override;
    auto syncToRADECJNow(double raHours, double decDegrees) -> bool override;
    auto slewToAZALT(double azDegrees, double altDegrees) -> bool override;
    auto abortMotion() -> bool override;
    auto emergencyStop() -> bool override;
    auto isMoving() -> bool override;

    // Directional movement
    auto startMotion(MotionNS nsDirection, MotionEW ewDirection) -> bool override;
    auto stopMotion(MotionNS nsDirection, MotionEW ewDirection) -> bool override;

    // Tracking
    auto enableTracking(bool enable) -> bool override;
    auto isTrackingEnabled() -> bool override;
    auto setTrackRate(TrackMode rate) -> bool override;
    auto getTrackRate() -> std::optional<TrackMode> override;
    auto setTrackRates(const MotionRates& rates) -> bool override;
    auto getTrackRates() -> MotionRates override;

    // Parking
    auto park() -> bool override;
    auto unpark() -> bool override;
    auto isParked() -> bool override;
    auto canPark() -> bool override;
    auto setParkPosition(double parkRA, double parkDEC) -> bool override;
    auto getParkPosition() -> std::optional<EquatorialCoordinates> override;
    auto setParkOption(ParkOptions option) -> bool override;

    // Coordinates
    auto getRADECJ2000() -> std::optional<EquatorialCoordinates> override;
    auto setRADECJ2000(double raHours, double decDegrees) -> bool override;
    auto getRADECJNow() -> std::optional<EquatorialCoordinates> override;
    auto setRADECJNow(double raHours, double decDegrees) -> bool override;
    auto getTargetRADECJNow() -> std::optional<EquatorialCoordinates> override;
    auto setTargetRADECJNow(double raHours, double decDegrees) -> bool override;
    auto getAZALT() -> std::optional<HorizontalCoordinates> override;
    auto setAZALT(double azDegrees, double altDegrees) -> bool override;

    // Location and time
    auto getLocation() -> std::optional<GeographicLocation> override;
    auto setLocation(const GeographicLocation& location) -> bool override;
    auto getUTCTime() -> std::optional<std::chrono::system_clock::time_point> override;
    auto setUTCTime(const std::chrono::system_clock::time_point& time) -> bool override;
    auto getLocalTime() -> std::optional<std::chrono::system_clock::time_point> override;

    // Guiding
    auto guideNS(int direction, int duration) -> bool override;
    auto guideEW(int direction, int duration) -> bool override;
    auto guidePulse(double ra_ms, double dec_ms) -> bool override;

    // Slew rates
    auto setSlewRate(double speed) -> bool override;
    auto getSlewRate() -> std::optional<double> override;
    auto getSlewRates() -> std::vector<double> override;
    auto setSlewRateIndex(int index) -> bool override;

    // Pier side
    auto getPierSide() -> std::optional<PierSide> override;
    auto setPierSide(PierSide side) -> bool override;

    // Home position
    auto initializeHome(std::string_view command = "") -> bool override;
    auto findHome() -> bool override;
    auto setHome() -> bool override;
    auto gotoHome() -> bool override;

    // Alignment
    auto getAlignmentMode() -> AlignmentMode override;
    auto setAlignmentMode(AlignmentMode mode) -> bool override;
    auto addAlignmentPoint(const EquatorialCoordinates& measured,
                          const EquatorialCoordinates& target) -> bool override;
    auto clearAlignment() -> bool override;

    // Utility
    auto degreesToDMS(double degrees) -> std::tuple<int, int, double> override;
    auto degreesToHMS(double degrees) -> std::tuple<int, int, double> override;

    // =========================================================================
    // Additional Modular Features
    // =========================================================================

    /**
     * @brief Get the underlying modular controller
     * @return Shared pointer to the telescope controller
     */
    std::shared_ptr<telescope::INDITelescopeController> getController() const {
        return controller_;
    }

    /**
     * @brief Get hardware interface component
     * @return Shared pointer to hardware interface
     */
    std::shared_ptr<telescope::components::HardwareInterface> getHardwareInterface() const {
        return controller_ ? controller_->getHardwareInterface() : nullptr;
    }

    /**
     * @brief Get motion controller component
     * @return Shared pointer to motion controller
     */
    std::shared_ptr<telescope::components::MotionController> getMotionController() const {
        return controller_ ? controller_->getMotionController() : nullptr;
    }

    /**
     * @brief Get tracking manager component
     * @return Shared pointer to tracking manager
     */
    std::shared_ptr<telescope::components::TrackingManager> getTrackingManager() const {
        return controller_ ? controller_->getTrackingManager() : nullptr;
    }

    /**
     * @brief Get parking manager component
     * @return Shared pointer to parking manager
     */
    std::shared_ptr<telescope::components::ParkingManager> getParkingManager() const {
        return controller_ ? controller_->getParkingManager() : nullptr;
    }

    /**
     * @brief Get coordinate manager component
     * @return Shared pointer to coordinate manager
     */
    std::shared_ptr<telescope::components::CoordinateManager> getCoordinateManager() const {
        return controller_ ? controller_->getCoordinateManager() : nullptr;
    }

    /**
     * @brief Get guide manager component
     * @return Shared pointer to guide manager
     */
    std::shared_ptr<telescope::components::GuideManager> getGuideManager() const {
        return controller_ ? controller_->getGuideManager() : nullptr;
    }

    /**
     * @brief Configure controller with custom settings
     * @param config Controller configuration
     * @return true if configuration applied successfully
     */
    bool configureController(const telescope::TelescopeControllerConfig& config);

    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const;

    /**
     * @brief Reset to factory defaults
     * @return true if reset successful
     */
    bool resetToDefaults();

    /**
     * @brief Enable debug mode
     * @param enable True to enable debug logging
     */
    void setDebugMode(bool enable);

private:
    std::string telescopeName_;
    std::shared_ptr<telescope::INDITelescopeController> controller_;
    bool debugMode_{false};

    // Internal helper methods
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
};

} // namespace lithium::device::indi
