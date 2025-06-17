/*
 * dome_home.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Home - Home Position Management Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_DOME_HOME_HPP
#define LITHIUM_DEVICE_INDI_DOME_HOME_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <functional>
#include <mutex>
#include <optional>
#include <atomic>

// Forward declarations
class INDIDomeClient;

/**
 * @brief Dome home position management component
 *
 * Handles home position discovery, setting, and navigation for INDI domes.
 * Provides auto-home, callback registration, and device synchronization.
 */
class DomeHomeManager {
public:
    /**
     * @brief Construct a DomeHomeManager for a given INDI dome client.
     * @param client Pointer to the associated INDIDomeClient.
     */
    explicit DomeHomeManager(INDIDomeClient* client);
    ~DomeHomeManager() = default;

    /**
     * @brief Initiate home position discovery (automatic or manual fallback).
     * @return True if home finding started or completed successfully, false otherwise.
     */
    [[nodiscard]] auto findHome() -> bool;

    /**
     * @brief Set the current dome position as the home position.
     * @return True if home position was set successfully, false otherwise.
     */
    [[nodiscard]] auto setHome() -> bool;

    /**
     * @brief Move the dome to the stored home position.
     * @return True if the move command was issued successfully, false otherwise.
     */
    [[nodiscard]] auto gotoHome() -> bool;

    /**
     * @brief Get the current home position value (if set).
     * @return Optional azimuth value of the home position.
     */
    [[nodiscard]] auto getHomePosition() -> std::optional<double>;

    /**
     * @brief Check if the home position is set.
     * @return True if home position is set, false otherwise.
     */
    [[nodiscard]] auto isHomeSet() -> bool;
    
    /**
     * @brief Enable or disable auto-home functionality.
     * @param enable True to enable, false to disable.
     * @return True if the operation succeeded.
     */
    [[nodiscard]] auto enableAutoHome(bool enable) -> bool;

    /**
     * @brief Check if auto-home is enabled.
     * @return True if enabled, false otherwise.
     */
    [[nodiscard]] auto isAutoHomeEnabled() -> bool;

    /**
     * @brief Enable or disable auto-home on startup.
     * @param enable True to enable, false to disable.
     * @return True if the operation succeeded.
     */
    [[nodiscard]] auto setAutoHomeOnStartup(bool enable) -> bool;

    /**
     * @brief Check if auto-home on startup is enabled.
     * @return True if enabled, false otherwise.
     */
    [[nodiscard]] auto isAutoHomeOnStartupEnabled() -> bool;
    
    /**
     * @brief Handle an INDI property update related to home position.
     * @param property The INDI property to process.
     */
    void handleHomeProperty(const INDI::Property& property);

    /**
     * @brief Synchronize internal state with the device's current properties.
     */
    void synchronizeWithDevice();
    
    /**
     * @brief Register a callback for home position events.
     * @param callback Function to call on home found/set events.
     */
    using HomeCallback = std::function<void(bool homeFound, double homePosition)>;
    void setHomeCallback(HomeCallback callback);

private:
    INDIDomeClient* client_; ///< Associated INDI dome client
    mutable std::mutex home_mutex_; ///< Mutex for thread-safe state access
    
    std::optional<double> home_position_; ///< Current home position (azimuth)
    std::atomic<bool> auto_home_enabled_{false}; ///< Auto-home enabled flag
    std::atomic<bool> auto_home_on_startup_{false}; ///< Auto-home on startup flag
    std::atomic<bool> home_finding_in_progress_{false}; ///< Home finding in progress flag
    
    HomeCallback home_callback_; ///< Registered home event callback
    
    /**
     * @brief Notify the registered callback of a home event.
     * @param homeFound True if home was found/set, false otherwise.
     * @param homePosition The azimuth of the home position.
     */
    void notifyHomeEvent(bool homeFound, double homePosition);

    /**
     * @brief Perform manual home finding procedure (fallback).
     * @return True if home was found, false otherwise.
     */
    [[nodiscard]] auto performHomeFinding() -> bool;
    
    /**
     * @brief Get the INDI property for home position (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getHomeProperty() -> INDI::PropertyViewSwitch*;

    /**
     * @brief Get the INDI property for home discovery (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getHomeDiscoverProperty() -> INDI::PropertyViewSwitch*;

    /**
     * @brief Get the INDI property for setting home (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getHomeSetProperty() -> INDI::PropertyViewSwitch*;

    /**
     * @brief Get the INDI property for going to home (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getHomeGotoProperty() -> INDI::PropertyViewSwitch*;
};

#endif // LITHIUM_DEVICE_INDI_DOME_HOME_HPP
