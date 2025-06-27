/*
 * telescope_v2.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: Modular INDI Telescope V2 Implementation

This is a refactored version of INDITelescope that uses the modular
architecture pattern similar to ASICamera, providing better maintainability,
testability, and separation of concerns.

*************************************************/

#ifndef LITHIUM_CLIENT_INDI_TELESCOPE_V2_HPP
#define LITHIUM_CLIENT_INDI_TELESCOPE_V2_HPP

#include <memory>
#include <string>
#include <atomic>
#include <mutex>

#include "device/template/telescope.hpp"
#include "telescope/telescope_controller.hpp"
#include "telescope/controller_factory.hpp"

/**
 * @brief Modular INDI Telescope V2
 * 
 * This class provides a backward-compatible interface to the original INDITelescope
 * while using the new modular architecture internally. It delegates all operations
 * to the modular telescope controller.
 */
class INDITelescopeV2 : public AtomTelescope {
public:
    explicit INDITelescopeV2(const std::string& name);
    ~INDITelescopeV2() override = default;

    // Non-copyable, non-movable
    INDITelescopeV2(const INDITelescopeV2& other) = delete;
    INDITelescopeV2& operator=(const INDITelescopeV2& other) = delete;
    INDITelescopeV2(INDITelescopeV2&& other) = delete;
    INDITelescopeV2& operator=(INDITelescopeV2&& other) = delete;

    // =========================================================================
    // Base Device Interface
    // =========================================================================
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    [[nodiscard]] auto isConnected() const -> bool override;

    // Additional connection methods
    auto reconnect(int timeout, int maxRetry) -> bool;
    auto watchAdditionalProperty() -> bool;

    // =========================================================================
    // Telescope Information
    // =========================================================================
    auto getTelescopeInfo() -> std::optional<TelescopeParameters> override;
    auto setTelescopeInfo(double telescopeAperture, double telescopeFocal,
                          double guiderAperture, double guiderFocal) -> bool override;
    auto getPierSide() -> std::optional<PierSide> override;
    auto setPierSide(PierSide side) -> bool override;

    // =========================================================================
    // Tracking Control
    // =========================================================================
    auto getTrackRate() -> std::optional<TrackMode> override;
    auto setTrackRate(TrackMode rate) -> bool override;
    auto isTrackingEnabled() -> bool override;
    auto enableTracking(bool enable) -> bool override;
    auto getTrackRates() -> MotionRates override;
    auto setTrackRates(const MotionRates& rates) -> bool override;

    // =========================================================================
    // Motion Control
    // =========================================================================
    auto abortMotion() -> bool override;
    auto getStatus() -> std::optional<std::string> override;
    auto emergencyStop() -> bool override;
    auto isMoving() -> bool override;

    // =========================================================================
    // Parking Operations
    // =========================================================================
    auto setParkOption(ParkOptions option) -> bool override;
    auto getParkPosition() -> std::optional<EquatorialCoordinates> override;
    auto setParkPosition(double parkRA, double parkDEC) -> bool override;
    auto isParked() -> bool override;
    auto park() -> bool override;
    auto unpark() -> bool override;
    auto canPark() -> bool override;

    // =========================================================================
    // Home Position
    // =========================================================================
    auto initializeHome(std::string_view command = "") -> bool override;
    auto findHome() -> bool override;
    auto setHome() -> bool override;
    auto gotoHome() -> bool override;

    // =========================================================================
    // Slew Rates
    // =========================================================================
    auto getSlewRate() -> std::optional<double> override;
    auto setSlewRate(double speed) -> bool override;
    auto getSlewRates() -> std::vector<double> override;
    auto setSlewRateIndex(int index) -> bool override;

    // =========================================================================
    // Directional Movement
    // =========================================================================
    auto getMoveDirectionEW() -> std::optional<MotionEW> override;
    auto setMoveDirectionEW(MotionEW direction) -> bool override;
    auto getMoveDirectionNS() -> std::optional<MotionNS> override;
    auto setMoveDirectionNS(MotionNS direction) -> bool override;
    auto startMotion(MotionNS nsDirection, MotionEW ewDirection) -> bool override;
    auto stopMotion(MotionNS nsDirection, MotionEW ewDirection) -> bool override;

    // =========================================================================
    // Guiding
    // =========================================================================
    auto guideNS(int direction, int duration) -> bool override;
    auto guideEW(int direction, int duration) -> bool override;
    auto guidePulse(double ra_ms, double dec_ms) -> bool override;

    // =========================================================================
    // Coordinate Systems
    // =========================================================================
    auto getRADECJ2000() -> std::optional<EquatorialCoordinates> override;
    auto setRADECJ2000(double raHours, double decDegrees) -> bool override;

    auto getRADECJNow() -> std::optional<EquatorialCoordinates> override;
    auto setRADECJNow(double raHours, double decDegrees) -> bool override;

    auto getTargetRADECJNow() -> std::optional<EquatorialCoordinates> override;
    auto setTargetRADECJNow(double raHours, double decDegrees) -> bool override;
    auto slewToRADECJNow(double raHours, double decDegrees, bool enableTracking = true) -> bool override;
    auto syncToRADECJNow(double raHours, double decDegrees) -> bool override;
    
    auto getAZALT() -> std::optional<HorizontalCoordinates> override;
    auto setAZALT(double azDegrees, double altDegrees) -> bool override;
    auto slewToAZALT(double azDegrees, double altDegrees) -> bool override;

    // =========================================================================
    // Location and Time
    // =========================================================================
    auto getLocation() -> std::optional<GeographicLocation> override;
    auto setLocation(const GeographicLocation& location) -> bool override;
    auto getUTCTime() -> std::optional<std::chrono::system_clock::time_point> override;
    auto setUTCTime(const std::chrono::system_clock::time_point& time) -> bool override;
    auto getLocalTime() -> std::optional<std::chrono::system_clock::time_point> override;

    // =========================================================================
    // Alignment
    // =========================================================================
    auto getAlignmentMode() -> AlignmentMode override;
    auto setAlignmentMode(AlignmentMode mode) -> bool override;
    auto addAlignmentPoint(const EquatorialCoordinates& measured, 
                           const EquatorialCoordinates& target) -> bool override;
    auto clearAlignment() -> bool override;

    // =========================================================================
    // Utility Methods
    // =========================================================================
    auto degreesToDMS(double degrees) -> std::tuple<int, int, double> override;
    auto degreesToHMS(double degrees) -> std::tuple<int, int, double> override;

    // =========================================================================
    // Legacy Compatibility Methods
    // =========================================================================
    
    // Property setting methods for backward compatibility
    void setPropertyNumber(std::string_view propertyName, double value);
    auto setActionAfterPositionSet(std::string_view action) -> bool;

    // =========================================================================
    // Advanced Component Access
    // =========================================================================
    
    /**
     * @brief Get the underlying modular controller
     * @return Shared pointer to the telescope controller
     */
    std::shared_ptr<lithium::device::indi::telescope::INDITelescopeController> getController() const {
        return controller_;
    }

    /**
     * @brief Get specific component from the controller
     * @tparam T Component type
     * @return Shared pointer to the component
     */
    template<typename T>
    std::shared_ptr<T> getComponent() const {
        if (!controller_) return nullptr;
        
        // Map component types to controller methods
        if constexpr (std::is_same_v<T, lithium::device::indi::telescope::components::HardwareInterface>) {
            return controller_->getHardwareInterface();
        } else if constexpr (std::is_same_v<T, lithium::device::indi::telescope::components::MotionController>) {
            return controller_->getMotionController();
        } else if constexpr (std::is_same_v<T, lithium::device::indi::telescope::components::TrackingManager>) {
            return controller_->getTrackingManager();
        } else if constexpr (std::is_same_v<T, lithium::device::indi::telescope::components::ParkingManager>) {
            return controller_->getParkingManager();
        } else if constexpr (std::is_same_v<T, lithium::device::indi::telescope::components::CoordinateManager>) {
            return controller_->getCoordinateManager();
        } else if constexpr (std::is_same_v<T, lithium::device::indi::telescope::components::GuideManager>) {
            return controller_->getGuideManager();
        } else {
            return nullptr;
        }
    }

    /**
     * @brief Configure the telescope controller with custom settings
     * @param config Configuration to apply
     * @return true if configuration successful, false otherwise
     */
    bool configure(const lithium::device::indi::telescope::TelescopeControllerConfig& config);

    /**
     * @brief Create with custom controller configuration
     * @param name Telescope name
     * @param config Controller configuration
     * @return Unique pointer to INDITelescopeV2 instance
     */
    static std::unique_ptr<INDITelescopeV2> createWithConfig(
        const std::string& name,
        const lithium::device::indi::telescope::TelescopeControllerConfig& config = {});

private:
    // The modular telescope controller that does all the work
    std::shared_ptr<lithium::device::indi::telescope::INDITelescopeController> controller_;
    
    // Thread safety for controller access
    mutable std::mutex controllerMutex_;
    
    // Initialization state
    std::atomic<bool> initialized_{false};
    
    // Error handling
    mutable std::string lastError_;
    
    // Internal methods
    void ensureController();
    bool initializeController();
    void setLastError(const std::string& error);
    std::string getLastError() const;
    
    // Helper methods for validation
    bool validateController() const;
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
};

#endif // LITHIUM_CLIENT_INDI_TELESCOPE_V2_HPP
