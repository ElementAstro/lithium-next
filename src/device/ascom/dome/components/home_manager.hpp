/*
 * home_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Home Manager Component

*************************************************/

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace lithium::ascom::dome::components {

class HardwareInterface;
class AzimuthManager;

/**
 * @brief Home Manager Component
 * 
 * Manages dome homing operations including finding home position,
 * setting home position, and managing home-related safety operations.
 */
class HomeManager {
public:
    using HomeCallback = std::function<void(bool success, double azimuth)>;
    using StatusCallback = std::function<void(const std::string& status)>;

    explicit HomeManager(std::shared_ptr<HardwareInterface> hardware,
                        std::shared_ptr<AzimuthManager> azimuth_manager);
    ~HomeManager();

    // === Home Operations ===
    auto findHome() -> bool;
    auto setHomePosition(double azimuth) -> bool;
    auto getHomePosition() -> std::optional<double>;
    auto isHomed() -> bool;
    auto isHoming() -> bool;
    auto abortHoming() -> bool;

    // === Home Detection ===
    auto hasHomeSensor() -> bool;
    auto isAtHome() -> bool;
    auto calibrateHome() -> bool;

    // === Status and Configuration ===
    auto getHomingTimeout() -> int;
    auto setHomingTimeout(int timeout_ms);
    auto getHomingSpeed() -> double;
    auto setHomingSpeed(double speed);

    // === Callbacks ===
    void setHomeCallback(HomeCallback callback);
    void setStatusCallback(StatusCallback callback);

    // === Safety ===
    auto requiresHoming() -> bool;
    auto getTimeSinceLastHome() -> std::chrono::milliseconds;

private:
    std::shared_ptr<HardwareInterface> hardware_interface_;
    std::shared_ptr<AzimuthManager> azimuth_manager_;
    
    std::atomic<bool> is_homed_{false};
    std::atomic<bool> is_homing_{false};
    std::atomic<bool> has_home_sensor_{false};
    std::atomic<bool> requires_homing_{true};
    
    std::optional<double> home_position_;
    std::chrono::steady_clock::time_point last_home_time_;
    
    int homing_timeout_ms_{30000};  // 30 seconds
    double homing_speed_{5.0};      // degrees per second
    
    HomeCallback home_callback_;
    StatusCallback status_callback_;
    
    std::unique_ptr<std::thread> homing_thread_;
    std::atomic<bool> abort_homing_{false};
    
    // === Internal Methods ===
    void performHomingSequence();
    void notifyHomeComplete(bool success, double azimuth);
    void notifyStatus(const std::string& status);
    auto detectHomeSensor() -> bool;
    auto findHomeSensorPosition() -> std::optional<double>;
};

} // namespace lithium::ascom::dome::components
