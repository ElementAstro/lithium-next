/*
 * dome_shutter.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Shutter - Shutter Control Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_DOME_SHUTTER_HPP
#define LITHIUM_DEVICE_INDI_DOME_SHUTTER_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <functional>
#include <mutex>

#include "device/template/dome.hpp"

// Forward declarations
class INDIDomeClient;

/**
 * @brief Dome shutter control component
 *
 * Handles shutter opening, closing, aborting, and status monitoring for INDI
 * domes. Provides safety checks, operation statistics, callback registration,
 * and device synchronization.
 */
class DomeShutterManager {
public:
    /**
     * @brief Construct a DomeShutterManager for a given INDI dome client.
     * @param client Pointer to the associated INDIDomeClient.
     */
    explicit DomeShutterManager(INDIDomeClient* client);
    ~DomeShutterManager() = default;

    /**
     * @brief Open the dome shutter (if safe).
     * @return True if the open command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto openShutter() -> bool;

    /**
     * @brief Close the dome shutter.
     * @return True if the close command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto closeShutter() -> bool;

    /**
     * @brief Abort any ongoing shutter operation.
     * @return True if the abort command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto abortShutter() -> bool;

    /**
     * @brief Get the current shutter state.
     * @return Current state (OPEN, CLOSED, OPENING, CLOSING, UNKNOWN).
     */
    [[nodiscard]] auto getShutterState() -> ShutterState;

    /**
     * @brief Check if the shutter is currently moving (opening or closing).
     * @return True if moving, false otherwise.
     */
    [[nodiscard]] auto isShutterMoving() -> bool;

    /**
     * @brief Check if it is safe to open the shutter (weather, parking, etc).
     * @return True if safe, false otherwise.
     */
    [[nodiscard]] auto canOpenShutter() -> bool;

    /**
     * @brief Check if it is safe to operate the shutter (not parked, etc).
     * @return True if safe, false otherwise.
     */
    [[nodiscard]] auto isSafeToOperate() -> bool;

    /**
     * @brief Get the number of shutter open/close operations performed.
     * @return Operation count.
     */
    [[nodiscard]] auto getShutterOperations() -> uint64_t;

    /**
     * @brief Reset the shutter operation count to zero.
     * @return True if reset successfully.
     */
    [[nodiscard]] auto resetShutterOperations() -> bool;

    /**
     * @brief Handle an INDI property update related to the shutter.
     * @param property The INDI property to process.
     */
    void handleShutterProperty(const INDI::Property& property);

    /**
     * @brief Update shutter state from an INDI property switch.
     * @param property The INDI property switch.
     */
    void updateShutterFromProperty(const INDI::PropertySwitch& property);

    /**
     * @brief Synchronize internal state with the device's current properties.
     */
    void synchronizeWithDevice();

    /**
     * @brief Register a callback for shutter state changes.
     * @param callback Function to call on shutter state changes.
     */
    using ShutterCallback = std::function<void(ShutterState state)>;
    void setShutterCallback(ShutterCallback callback);

private:
    INDIDomeClient* client_;            ///< Associated INDI dome client
    mutable std::mutex shutter_mutex_;  ///< Mutex for thread-safe state access

    ShutterState current_state_{
        ShutterState::UNKNOWN};                    ///< Current shutter state
    std::atomic<uint64_t> shutter_operations_{0};  ///< Shutter operation count

    ShutterCallback shutter_callback_;  ///< Registered shutter event callback

    /**
     * @brief Notify the registered callback of a shutter state change.
     * @param state The new shutter state.
     */
    void notifyShutterStateChange(ShutterState state);

    /**
     * @brief Increment the shutter operation count.
     */
    void incrementOperationCount();

    /**
     * @brief Get the INDI property for dome shutter (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getDomeShutterProperty() -> INDI::PropertyViewSwitch*;

    /**
     * @brief Convert an INDI ISState to a ShutterState.
     * @param state The INDI ISState value.
     * @return Corresponding ShutterState.
     */
    [[nodiscard]] auto convertShutterState(ISState state) -> ShutterState;

    /**
     * @brief Convert a boolean value to an INDI ISState.
     * @param value Boolean value.
     * @return ISS_ON if true, ISS_OFF if false.
     */
    [[nodiscard]] auto convertToISState(bool value) -> ISState;

    /**
     * @brief Update shutter state from an INDI property view switch.
     * @param property The INDI property view switch.
     */
    void updateShutterFromProperty(INDI::PropertyViewSwitch* property);
};

#endif  // LITHIUM_DEVICE_INDI_DOME_SHUTTER_HPP
