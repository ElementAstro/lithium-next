#pragma once

#include <optional>
#include <atomic>
#include <spdlog/spdlog.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "device/template/telescope.hpp"

/**
 * @brief Motion control component for INDI telescopes
 * 
 * Handles telescope movement, slewing, tracking, and guiding
 */
class TelescopeMotion {
public:
    explicit TelescopeMotion(const std::string& name);
    ~TelescopeMotion() = default;

    /**
     * @brief Initialize motion control component
     */
    auto initialize(INDI::BaseDevice device) -> bool;

    /**
     * @brief Destroy motion control component
     */
    auto destroy() -> bool;

    // Motion control
    /**
     * @brief Abort all telescope motion immediately
     */
    auto abortMotion() -> bool;

    /**
     * @brief Emergency stop - immediate halt of all operations
     */
    auto emergencyStop() -> bool;

    /**
     * @brief Check if telescope is currently moving
     */
    auto isMoving() -> bool;

    /**
     * @brief Get telescope status
     */
    auto getStatus() -> std::optional<std::string>;

    // Directional movement
    /**
     * @brief Get current East-West motion direction
     */
    auto getMoveDirectionEW() -> std::optional<MotionEW>;

    /**
     * @brief Set East-West motion direction
     */
    auto setMoveDirectionEW(MotionEW direction) -> bool;

    /**
     * @brief Get current North-South motion direction
     */
    auto getMoveDirectionNS() -> std::optional<MotionNS>;

    /**
     * @brief Set North-South motion direction
     */
    auto setMoveDirectionNS(MotionNS direction) -> bool;

    /**
     * @brief Start motion in specified directions
     */
    auto startMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool;

    /**
     * @brief Stop motion in specified directions
     */
    auto stopMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool;

    // Slew rates
    /**
     * @brief Get current slew rate
     */
    auto getSlewRate() -> std::optional<double>;

    /**
     * @brief Set slew rate by speed value
     */
    auto setSlewRate(double speed) -> bool;

    /**
     * @brief Get available slew rates
     */
    auto getSlewRates() -> std::vector<double>;

    /**
     * @brief Set slew rate by index
     */
    auto setSlewRateIndex(int index) -> bool;

    // Guiding
    /**
     * @brief Guide telescope in North-South direction
     * @param direction 1 for North, -1 for South
     * @param duration Guide duration in milliseconds
     */
    auto guideNS(int direction, int duration) -> bool;

    /**
     * @brief Guide telescope in East-West direction
     * @param direction 1 for East, -1 for West
     * @param duration Guide duration in milliseconds
     */
    auto guideEW(int direction, int duration) -> bool;

    /**
     * @brief Send guide pulse in both RA and DEC
     * @param ra_ms RA guide duration in milliseconds
     * @param dec_ms DEC guide duration in milliseconds
     */
    auto guidePulse(double ra_ms, double dec_ms) -> bool;

    // Coordinate slewing
    /**
     * @brief Slew telescope to RA/DEC J2000 coordinates
     */
    auto slewToRADECJ2000(double raHours, double decDegrees, bool enableTracking = true) -> bool;

    /**
     * @brief Slew telescope to RA/DEC JNow coordinates
     */
    auto slewToRADECJNow(double raHours, double decDegrees, bool enableTracking = true) -> bool;

    /**
     * @brief Slew telescope to AZ/ALT coordinates
     */
    auto slewToAZALT(double azDegrees, double altDegrees) -> bool;

    /**
     * @brief Sync telescope to RA/DEC JNow coordinates
     */
    auto syncToRADECJNow(double raHours, double decDegrees) -> bool;

    /**
     * @brief Set action to perform after coordinate set
     */
    auto setActionAfterPositionSet(std::string_view action) -> bool;

private:
    std::string name_;
    INDI::BaseDevice device_;
    
    // Motion state
    std::atomic_bool isMoving_{false};
    MotionEW motionEW_{MotionEW::NONE};
    MotionNS motionNS_{MotionNS::NONE};
    
    // Slew rates
    std::vector<double> slewRates_;
    int currentSlewRateIndex_{0};
    
    // Helper methods
    auto watchMotionProperties() -> void;
    auto watchSlewRateProperties() -> void;
    auto watchGuideProperties() -> void;
};
