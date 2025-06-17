/*
 * dome_telescope.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Telescope - Telescope Coordination Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_DOME_TELESCOPE_HPP
#define LITHIUM_DEVICE_INDI_DOME_TELESCOPE_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <functional>
#include <mutex>

// Forward declarations
class INDIDomeClient;

/**
 * @brief Dome telescope coordination component
 *
 * Handles telescope following and dome-telescope synchronization for INDI
 * domes. Provides offset/radius configuration, callback registration, and
 * device synchronization.
 */
class DomeTelescopeManager {
public:
    /**
     * @brief Construct a DomeTelescopeManager for a given INDI dome client.
     * @param client Pointer to the associated INDIDomeClient.
     */
    explicit DomeTelescopeManager(INDIDomeClient* client);
    ~DomeTelescopeManager() = default;

    /**
     * @brief Enable or disable dome following the telescope.
     * @param enable True to enable, false to disable.
     * @return True if the operation succeeded.
     */
    auto followTelescope(bool enable) -> bool;

    /**
     * @brief Check if dome is currently following the telescope.
     * @return True if following, false otherwise.
     */
    auto isFollowingTelescope() -> bool;

    /**
     * @brief Set the current telescope position (azimuth, altitude).
     * @param az Telescope azimuth in degrees.
     * @param alt Telescope altitude in degrees.
     * @return True if set successfully.
     */
    auto setTelescopePosition(double az, double alt) -> bool;

    /**
     * @brief Calculate the dome azimuth required to follow the telescope.
     * @param telescopeAz Telescope azimuth in degrees.
     * @param telescopeAlt Telescope altitude in degrees.
     * @return Dome azimuth in degrees.
     */
    auto calculateDomeAzimuth(double telescopeAz, double telescopeAlt)
        -> double;

    /**
     * @brief Set the telescope offset from dome center (north/east).
     * @param northOffset North offset in meters.
     * @param eastOffset East offset in meters.
     * @return True if set successfully.
     */
    auto setTelescopeOffset(double northOffset, double eastOffset) -> bool;

    /**
     * @brief Get the current telescope offset (north, east).
     * @return Pair of (north, east) offset in meters.
     */
    auto getTelescopeOffset() -> std::pair<double, double>;

    /**
     * @brief Set the telescope radius (distance from dome center).
     * @param radius Radius in meters.
     * @return True if set successfully.
     */
    auto setTelescopeRadius(double radius) -> bool;

    /**
     * @brief Get the current telescope radius.
     * @return Radius in meters.
     */
    auto getTelescopeRadius() -> double;

    /**
     * @brief Set the minimum angular threshold for dome movement.
     * @param threshold Threshold in degrees (0-180).
     * @return True if set successfully.
     */
    auto setFollowingThreshold(double threshold) -> bool;

    /**
     * @brief Get the current following threshold.
     * @return Threshold in degrees.
     */
    auto getFollowingThreshold() -> double;

    /**
     * @brief Set the delay between following updates.
     * @param delayMs Delay in milliseconds.
     * @return True if set successfully.
     */
    auto setFollowingDelay(uint32_t delayMs) -> bool;

    /**
     * @brief Get the current following delay.
     * @return Delay in milliseconds.
     */
    auto getFollowingDelay() -> uint32_t;

    /**
     * @brief Handle an INDI property update related to telescope/dome sync.
     * @param property The INDI property to process.
     */
    void handleTelescopeProperty(const INDI::Property& property);

    /**
     * @brief Synchronize internal state with the device's current properties.
     */
    void synchronizeWithDevice();

    /**
     * @brief Register a callback for telescope/dome sync events.
     * @param callback Function to call on sync events.
     */
    using TelescopeCallback = std::function<void(
        double telescopeAz, double telescopeAlt, double domeAz)>;
    void setTelescopeCallback(TelescopeCallback callback);

private:
    INDIDomeClient* client_;  ///< Associated INDI dome client
    mutable std::mutex
        telescope_mutex_;  ///< Mutex for thread-safe state access

    bool following_enabled_{false};      ///< Following enabled flag
    double current_telescope_az_{0.0};   ///< Current telescope azimuth
    double current_telescope_alt_{0.0};  ///< Current telescope altitude

    double telescope_north_offset_{0.0};  ///< Telescope north offset (meters)
    double telescope_east_offset_{0.0};   ///< Telescope east offset (meters)
    double telescope_radius_{0.0};        ///< Telescope radius (meters)
    double following_threshold_{1.0};  ///< Dome following threshold (degrees)
    uint32_t following_delay_{1000};   ///< Dome following delay (milliseconds)

    TelescopeCallback
        telescope_callback_;  ///< Registered telescope event callback

    /**
     * @brief Notify the registered callback of a telescope/dome sync event.
     * @param telescopeAz Telescope azimuth.
     * @param telescopeAlt Telescope altitude.
     * @param domeAz Dome azimuth.
     */
    void notifyTelescopeEvent(double telescopeAz, double telescopeAlt,
                              double domeAz);

    /**
     * @brief Check if the dome should move to follow the telescope.
     * @param newDomeAz Target dome azimuth.
     * @param currentDomeAz Current dome azimuth.
     * @return True if dome should move, false otherwise.
     */
    auto shouldMoveDome(double newDomeAz, double currentDomeAz) -> bool;

    /**
     * @brief Normalize an azimuth value to [0, 360) degrees.
     * @param azimuth Input azimuth.
     * @return Normalized azimuth.
     */
    auto normalizeAzimuth(double azimuth) -> double;

    /**
     * @brief Calculate the offset correction for dome azimuth.
     * @param az Telescope azimuth.
     * @param alt Telescope altitude.
     * @return Offset correction in degrees.
     */
    auto calculateOffsetCorrection(double az, double alt) -> double;
};

#endif  // LITHIUM_DEVICE_INDI_DOME_TELESCOPE_HPP
