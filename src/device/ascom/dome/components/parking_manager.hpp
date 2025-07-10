/*
 * parking_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Parking Management Component

*************************************************/

#pragma once

#include <memory>
#include <optional>
#include <atomic>
#include <functional>

namespace lithium::ascom::dome::components {

class HardwareInterface;
class AzimuthManager;

/**
 * @brief Parking Management Component for ASCOM Dome
 */
class ParkingManager {
public:
    explicit ParkingManager(std::shared_ptr<HardwareInterface> hardware,
                           std::shared_ptr<AzimuthManager> azimuth_manager);
    virtual ~ParkingManager();

    // === Parking Operations ===
    auto park() -> bool;
    auto unpark() -> bool;
    auto isParked() -> bool;
    auto canPark() -> bool;

    // === Park Position Management ===
    auto getParkPosition() -> std::optional<double>;
    auto setParkPosition(double azimuth) -> bool;
    auto canSetParkPosition() -> bool;

    // === Home Position Management ===
    auto findHome() -> bool;
    auto setHome() -> bool;
    auto gotoHome() -> bool;
    auto getHomePosition() -> std::optional<double>;
    auto canFindHome() -> bool;

    // === Status and Monitoring ===
    auto isParkingInProgress() -> bool;
    auto isHomingInProgress() -> bool;
    auto getParkingProgress() -> double;

    // === Configuration ===
    auto setParkingTimeout(int timeout) -> bool;
    auto getParkingTimeout() -> int;
    auto setAutoParking(bool enable) -> bool;
    auto isAutoParking() -> bool;

    // === Callbacks ===
    auto setParkingCallback(std::function<void(bool, const std::string&)> callback) -> void;
    auto setHomingCallback(std::function<void(bool, const std::string&)> callback) -> void;

private:
    std::shared_ptr<HardwareInterface> hardware_;
    std::shared_ptr<AzimuthManager> azimuth_manager_;

    std::atomic<bool> is_parked_{false};
    std::atomic<bool> is_parking_{false};
    std::atomic<bool> is_homing_{false};
    std::atomic<bool> auto_parking_{false};
    std::atomic<double> park_position_{0.0};
    std::atomic<double> home_position_{0.0};

    int parking_timeout_{300}; // seconds

    std::function<void(bool, const std::string&)> parking_callback_;
    std::function<void(bool, const std::string&)> homing_callback_;

    auto updateParkStatus() -> void;
    auto executeParkingSequence() -> bool;
    auto executeHomingSequence() -> bool;
};

} // namespace lithium::ascom::dome::components
