/*
 * dome_parking.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Parking - Parking Control Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_DOME_PARKING_HPP
#define LITHIUM_DEVICE_INDI_DOME_PARKING_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <optional>

class INDIDomeClient;

/**
 * @brief Dome parking control component
 *
 * Handles dome parking operations and park position management for INDI domes.
 * Provides callback registration and device synchronization.
 */
class DomeParkingManager {
public:
    /**
     * @brief Construct a DomeParkingManager for a given INDI dome client.
     * @param client Pointer to the associated INDIDomeClient.
     */
    explicit DomeParkingManager(INDIDomeClient* client);
    ~DomeParkingManager() = default;

    /**
     * @brief Park the dome (move to park position and set park state).
     * @return True if the park command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto park() -> bool;

    /**
     * @brief Unpark the dome (clear park state).
     * @return True if the unpark command was issued successfully, false
     * otherwise.
     */
    [[nodiscard]] auto unpark() -> bool;

    /**
     * @brief Check if the dome is currently parked.
     * @return True if parked, false otherwise.
     */
    [[nodiscard]] auto isParked() -> bool;

    /**
     * @brief Check if the dome is currently parking (in progress).
     * @return True if parking, false otherwise.
     */
    [[nodiscard]] auto isParking() -> bool;

    /**
     * @brief Set the park position azimuth.
     * @param azimuth Park position in degrees (0-360).
     * @return True if set successfully, false otherwise.
     */
    [[nodiscard]] auto setParkPosition(double azimuth) -> bool;

    /**
     * @brief Get the current park position azimuth (if set).
     * @return Optional azimuth value.
     */
    [[nodiscard]] auto getParkPosition() -> std::optional<double>;

    /**
     * @brief Get the default park position azimuth.
     * @return Default azimuth value.
     */
    [[nodiscard]] auto getDefaultParkPosition() -> double;

    /**
     * @brief Handle an INDI property update related to parking.
     * @param property The INDI property to process.
     */
    void handleParkingProperty(const INDI::Property& property);

    /**
     * @brief Update parking state from an INDI property switch.
     * @param property The INDI property switch.
     */
    void updateParkingFromProperty(const INDI::PropertySwitch& property);

    /**
     * @brief Synchronize internal state with the device's current properties.
     */
    void synchronizeWithDevice();

    /**
     * @brief Register a callback for parking state changes.
     * @param callback Function to call on parking state changes.
     */
    using ParkingCallback = std::function<void(bool parked, bool parking)>;
    void setParkingCallback(ParkingCallback callback);

private:
    INDIDomeClient* client_;            ///< Associated INDI dome client
    mutable std::mutex parking_mutex_;  ///< Mutex for thread-safe state access

    std::atomic<bool> is_parked_{false};   ///< Dome parked state
    std::atomic<bool> is_parking_{false};  ///< Dome parking in progress state
    std::optional<double> park_position_;  ///< Park position azimuth
    double default_park_position_{0.0};    ///< Default park position

    ParkingCallback parking_callback_;  ///< Registered parking event callback

    /**
     * @brief Notify the registered callback of a parking state change.
     * @param parked True if dome is parked, false otherwise.
     * @param parking True if dome is parking, false otherwise.
     */
    void notifyParkingStateChange(bool parked, bool parking);

    /**
     * @brief Get the INDI property for dome parking (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    [[nodiscard]] auto getDomeParkProperty() -> INDI::PropertyViewSwitch*;

    /**
     * @brief Update parking state from an INDI property view switch.
     * @param property The INDI property view switch.
     */
    void updateParkingFromProperty(INDI::PropertyViewSwitch* property);
};

// Forward declarations
class INDIDomeClient;

#endif  // LITHIUM_DEVICE_INDI_DOME_PARKING_HPP
