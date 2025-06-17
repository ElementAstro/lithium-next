#pragma once

#include <optional>
#include <atomic>
#include <spdlog/spdlog.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "device/template/telescope.hpp"

/**
 * @brief Parking and homing component for INDI telescopes
 * 
 * Handles telescope parking, homing, and safety operations
 */
class TelescopeParking {
public:
    explicit TelescopeParking(const std::string& name);
    ~TelescopeParking() = default;

    /**
     * @brief Initialize parking component
     */
    auto initialize(INDI::BaseDevice device) -> bool;

    /**
     * @brief Destroy parking component
     */
    auto destroy() -> bool;

    // Parking operations
    /**
     * @brief Check if telescope supports parking
     */
    auto canPark() -> bool;

    /**
     * @brief Check if telescope is currently parked
     */
    auto isParked() -> bool;

    /**
     * @brief Park the telescope
     */
    auto park() -> bool;

    /**
     * @brief Unpark the telescope
     */
    auto unpark() -> bool;

    /**
     * @brief Set parking option
     */
    auto setParkOption(ParkOptions option) -> bool;

    /**
     * @brief Get current park position
     */
    auto getParkPosition() -> std::optional<EquatorialCoordinates>;

    /**
     * @brief Set park position
     */
    auto setParkPosition(double parkRA, double parkDEC) -> bool;

    // Home operations
    /**
     * @brief Initialize home position
     */
    auto initializeHome(std::string_view command = "") -> bool;

    /**
     * @brief Find home position automatically
     */
    auto findHome() -> bool;

    /**
     * @brief Set current position as home
     */
    auto setHome() -> bool;

    /**
     * @brief Go to home position
     */
    auto gotoHome() -> bool;

    /**
     * @brief Check if telescope is at home position
     */
    auto isAtHome() -> bool;

    /**
     * @brief Check if home position is set
     */
    auto isHomeSet() -> bool;

private:
    std::string name_;
    INDI::BaseDevice device_;
    
    // Parking state
    std::atomic_bool isParkEnabled_{false};
    std::atomic_bool isParked_{false};
    ParkOptions parkOption_{ParkOptions::CURRENT};
    EquatorialCoordinates parkPosition_{};
    
    // Home state
    std::atomic_bool isHomed_{false};
    std::atomic_bool isHomeSet_{false};
    std::atomic_bool isHomeInitEnabled_{false};
    std::atomic_bool isHomeInitInProgress_{false};
    EquatorialCoordinates homePosition_{};
    
    // Helper methods
    auto watchParkingProperties() -> void;
    auto watchHomeProperties() -> void;
    auto updateParkingState() -> void;
    auto updateHomeState() -> void;
};
