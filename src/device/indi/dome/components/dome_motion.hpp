/*
 * dome_motion.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Motion - Dome Movement Control Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_DOME_MOTION_HPP
#define LITHIUM_DEVICE_INDI_DOME_MOTION_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <functional>
#include <mutex>

#include "device/template/dome.hpp"

// Forward declarations
class INDIDomeClient;

/**
 * @brief Dome motion control component
 *
 * Handles dome rotation, positioning, and movement operations for INDI domes.
 * Provides speed/limit/backlash control, callback registration, and device
 * synchronization.
 */
class DomeMotionManager {
public:
    /**
     * @brief Construct a DomeMotionManager for a given INDI dome client.
     * @param client Pointer to the associated INDIDomeClient.
     */
    explicit DomeMotionManager(INDIDomeClient* client);
    ~DomeMotionManager() = default;

    /**
     * @brief Move the dome to the specified azimuth.
     * @param azimuth Target azimuth in degrees.
     * @return True if the move command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto moveToAzimuth(double azimuth) -> bool;

    /**
     * @brief Rotate the dome by a relative number of degrees.
     * @param degrees Relative degrees to rotate (positive or negative).
     * @return True if the move command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto rotateRelative(double degrees) -> bool;

    /**
     * @brief Start continuous dome rotation in the specified direction.
     * @param direction DomeMotion::CLOCKWISE or DomeMotion::COUNTER_CLOCKWISE.
     * @return True if the rotation command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto startRotation(DomeMotion direction) -> bool;

    /**
     * @brief Stop dome rotation (soft stop).
     * @return True if the stop command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto stopRotation() -> bool;

    /**
     * @brief Abort all dome motion (emergency stop).
     * @return True if the abort command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto abortMotion() -> bool;

    /**
     * @brief Get the current dome azimuth.
     * @return Current azimuth in degrees.
     */
    [[nodiscard]] auto getCurrentAzimuth() -> double;

    /**
     * @brief Get the target dome azimuth (if moving).
     * @return Target azimuth in degrees.
     */
    [[nodiscard]] auto getTargetAzimuth() -> double;

    /**
     * @brief Check if the dome is currently moving.
     * @return True if moving, false otherwise.
     */
    [[nodiscard]] auto isMoving() -> bool;

    /**
     * @brief Set the dome rotation speed.
     * @param degreesPerSecond Speed in degrees per second.
     * @return True if the speed was set successfully, false otherwise.
     */
    [[nodiscard]] auto setRotationSpeed(double degreesPerSecond) -> bool;

    /**
     * @brief Get the current dome rotation speed.
     * @return Speed in degrees per second.
     */
    [[nodiscard]] auto getRotationSpeed() -> double;

    /**
     * @brief Get the maximum allowed dome rotation speed.
     * @return Maximum speed in degrees per second.
     */
    [[nodiscard]] auto getMaxSpeed() -> double;

    /**
     * @brief Get the minimum allowed dome rotation speed.
     * @return Minimum speed in degrees per second.
     */
    [[nodiscard]] auto getMinSpeed() -> double;

    /**
     * @brief Set azimuth limits for dome movement.
     * @param minAz Minimum allowed azimuth (degrees).
     * @param maxAz Maximum allowed azimuth (degrees).
     * @return True if limits were set successfully, false otherwise.
     */
    [[nodiscard]] auto setAzimuthLimits(double minAz, double maxAz) -> bool;

    /**
     * @brief Get the current azimuth limits.
     * @return Pair of (min, max) azimuth in degrees.
     */
    [[nodiscard]] auto getAzimuthLimits() -> std::pair<double, double>;

    /**
     * @brief Check if azimuth limits are enabled.
     * @return True if limits are enabled, false otherwise.
     */
    [[nodiscard]] auto hasAzimuthLimits() -> bool;

    /**
     * @brief Get the current backlash compensation value.
     * @return Backlash compensation in degrees.
     */
    [[nodiscard]] auto getBacklash() -> double;

    /**
     * @brief Set the backlash compensation value.
     * @param backlash Compensation in degrees.
     * @return True if set successfully, false otherwise.
     */
    [[nodiscard]] auto setBacklash(double backlash) -> bool;

    /**
     * @brief Enable or disable backlash compensation.
     * @param enable True to enable, false to disable.
     * @return True if the operation succeeded.
     */
    [[nodiscard]] auto enableBacklashCompensation(bool enable) -> bool;

    /**
     * @brief Check if backlash compensation is enabled.
     * @return True if enabled, false otherwise.
     */
    [[nodiscard]] auto isBacklashCompensationEnabled() -> bool;

    /**
     * @brief Handle an INDI property update related to dome motion.
     * @param property The INDI property to process.
     */
    void handleMotionProperty(const INDI::Property& property);

    /**
     * @brief Update azimuth from an INDI number property.
     * @param property The INDI number property.
     */
    void updateAzimuthFromProperty(INDI::PropertyViewNumber* property);

    /**
     * @brief Update speed from an INDI number property.
     * @param property The INDI number property.
     */
    void updateSpeedFromProperty(INDI::PropertyViewNumber* property);

    /**
     * @brief Synchronize internal state with the device's current properties.
     */
    void synchronizeWithDevice();

    /**
     * @brief Normalize an azimuth value to [0, 360) degrees.
     * @param azimuth Input azimuth.
     * @return Normalized azimuth.
     */
    [[nodiscard]] double normalizeAzimuth(double azimuth);

    /**
     * @brief Register a callback for dome motion events.
     * @param callback Function to call on motion events.
     */
    using MotionCallback =
        std::function<void(double currentAz, double targetAz, bool moving)>;
    void setMotionCallback(MotionCallback callback);

private:
    INDIDomeClient* client_;           ///< Associated INDI dome client
    mutable std::mutex motion_mutex_;  ///< Mutex for thread-safe state access

    std::atomic<double> current_azimuth_{0.0};  ///< Current dome azimuth
    std::atomic<double> target_azimuth_{0.0};   ///< Target dome azimuth
    std::atomic<double> rotation_speed_{1.0};   ///< Dome rotation speed
    std::atomic<bool> is_moving_{false};        ///< Dome moving state

    std::atomic<bool> has_azimuth_limits_{
        false};                  ///< Azimuth limits enabled flag
    double min_azimuth_{0.0};    ///< Minimum azimuth
    double max_azimuth_{360.0};  ///< Maximum azimuth
    double max_speed_{10.0};     ///< Maximum speed
    double min_speed_{0.1};      ///< Minimum speed

    double backlash_compensation_{0.0};  ///< Backlash compensation value
    std::atomic<bool> backlash_enabled_{false};  ///< Backlash enabled flag

    MotionCallback motion_callback_;  ///< Registered motion event callback

    /**
     * @brief Notify the registered callback of a motion event.
     * @param currentAz Current azimuth.
     * @param targetAz Target azimuth.
     * @param moving True if dome is moving, false otherwise.
     */
    void notifyMotionEvent(double currentAz, double targetAz, bool moving);

    /**
     * @brief Check if an azimuth value is valid (within limits if enabled).
     * @param azimuth Azimuth to check.
     * @return True if valid, false otherwise.
     */
    [[nodiscard]] auto isValidAzimuth(double azimuth) -> bool;

    /**
     * @brief Calculate the shortest path between two azimuths.
     * @param from Start azimuth.
     * @param to End azimuth.
     * @return Shortest path in degrees.
     */
    [[nodiscard]] auto calculateShortestPath(double from, double to) -> double;

    /**
     * @brief Get the INDI property for dome azimuth (number type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getDomeAzimuthProperty() -> INDI::PropertyViewNumber*;

    /**
     * @brief Get the INDI property for dome speed (number type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getDomeSpeedProperty() -> INDI::PropertyViewNumber*;

    /**
     * @brief Get the INDI property for dome motion (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getDomeMotionProperty() -> INDI::PropertyViewSwitch*;

    /**
     * @brief Get the INDI property for dome abort (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getDomeAbortProperty() -> INDI::PropertyViewSwitch*;
};

#endif  // LITHIUM_DEVICE_INDI_DOME_MOTION_HPP
