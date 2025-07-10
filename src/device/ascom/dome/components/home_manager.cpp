/*
 * home_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Home Manager Component Implementation

*************************************************/

#include "home_manager.hpp"
#include "hardware_interface.hpp"
#include "azimuth_manager.hpp"

#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

namespace lithium::ascom::dome::components {

HomeManager::HomeManager(std::shared_ptr<HardwareInterface> hardware,
                        std::shared_ptr<AzimuthManager> azimuth_manager)
    : hardware_interface_(std::move(hardware))
    , azimuth_manager_(std::move(azimuth_manager)) {
    spdlog::debug("HomeManager initialized");
    
    // Detect if home sensor is available
    has_home_sensor_ = detectHomeSensor();
    
    // Some domes don't require homing
    requires_homing_.store(has_home_sensor_.load());
}

HomeManager::~HomeManager() {
    if (homing_thread_ && homing_thread_->joinable()) {
        abort_homing_ = true;
        homing_thread_->join();
    }
}

auto HomeManager::findHome() -> bool {
    if (is_homing_) {
        spdlog::warn("Homing already in progress");
        return false;
    }
    
    if (!hardware_interface_) {
        spdlog::error("Hardware interface not available");
        return false;
    }
    
    spdlog::info("Starting dome homing sequence");
    
    // Start homing in separate thread
    abort_homing_ = false;
    is_homing_ = true;
    
    homing_thread_ = std::make_unique<std::thread>([this]() {
        performHomingSequence();
    });
    
    return true;
}

auto HomeManager::setHomePosition(double azimuth) -> bool {
    if (azimuth < 0.0 || azimuth >= 360.0) {
        spdlog::error("Invalid home position: {}", azimuth);
        return false;
    }
    
    home_position_ = azimuth;
    is_homed_ = true;
    last_home_time_ = std::chrono::steady_clock::now();
    
    spdlog::info("Home position set to {:.2f} degrees", azimuth);
    notifyHomeComplete(true, azimuth);
    
    return true;
}

auto HomeManager::getHomePosition() -> std::optional<double> {
    return home_position_;
}

auto HomeManager::isHomed() -> bool {
    return is_homed_;
}

auto HomeManager::isHoming() -> bool {
    return is_homing_;
}

auto HomeManager::abortHoming() -> bool {
    if (!is_homing_) {
        return true;
    }
    
    spdlog::info("Aborting homing sequence");
    abort_homing_ = true;
    
    // Wait for homing thread to finish
    if (homing_thread_ && homing_thread_->joinable()) {
        homing_thread_->join();
    }
    
    return true;
}

auto HomeManager::hasHomeSensor() -> bool {
    return has_home_sensor_;
}

auto HomeManager::isAtHome() -> bool {
    if (!has_home_sensor_) {
        return false;
    }
    
    // Check if dome is at home position
    if (auto current_az = azimuth_manager_->getCurrentAzimuth()) {
        if (home_position_) {
            double diff = std::abs(*current_az - *home_position_);
            return diff < 1.0; // Within 1 degree
        }
    }
    
    return false;
}

auto HomeManager::calibrateHome() -> bool {
    if (!has_home_sensor_) {
        spdlog::warn("No home sensor available for calibration");
        return false;
    }
    
    spdlog::info("Calibrating home position");
    
    // Find exact home sensor position
    auto home_pos = findHomeSensorPosition();
    if (home_pos) {
        return setHomePosition(*home_pos);
    }
    
    return false;
}

auto HomeManager::getHomingTimeout() -> int {
    return homing_timeout_ms_;
}

auto HomeManager::setHomingTimeout(int timeout_ms) {
    homing_timeout_ms_ = timeout_ms;
}

auto HomeManager::getHomingSpeed() -> double {
    return homing_speed_;
}

auto HomeManager::setHomingSpeed(double speed) {
    homing_speed_ = speed;
}

void HomeManager::setHomeCallback(HomeCallback callback) {
    home_callback_ = std::move(callback);
}

void HomeManager::setStatusCallback(StatusCallback callback) {
    status_callback_ = std::move(callback);
}

auto HomeManager::requiresHoming() -> bool {
    return requires_homing_;
}

auto HomeManager::getTimeSinceLastHome() -> std::chrono::milliseconds {
    if (!is_homed_) {
        return std::chrono::milliseconds::max();
    }
    
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_home_time_);
}

void HomeManager::performHomingSequence() {
    notifyStatus("Starting homing sequence");
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        if (has_home_sensor_) {
            // Use home sensor for homing
            auto home_pos = findHomeSensorPosition();
            if (home_pos && !abort_homing_) {
                home_position_ = *home_pos;
                is_homed_ = true;
                last_home_time_ = std::chrono::steady_clock::now();
                
                notifyStatus("Homing completed successfully");
                notifyHomeComplete(true, *home_pos);
            } else {
                notifyStatus("Failed to find home sensor");
                notifyHomeComplete(false, 0.0);
            }
        } else {
            // Manual homing - just set current position as home
            if (auto current_az = azimuth_manager_->getCurrentAzimuth()) {
                home_position_ = *current_az;
                is_homed_ = true;
                last_home_time_ = std::chrono::steady_clock::now();
                
                notifyStatus("Manual homing completed");
                notifyHomeComplete(true, *current_az);
            } else {
                notifyStatus("Failed to get current azimuth");
                notifyHomeComplete(false, 0.0);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Homing sequence failed: {}", e.what());
        notifyStatus("Homing failed: " + std::string(e.what()));
        notifyHomeComplete(false, 0.0);
    }
    
    is_homing_ = false;
}

void HomeManager::notifyHomeComplete(bool success, double azimuth) {
    if (home_callback_) {
        home_callback_(success, azimuth);
    }
}

void HomeManager::notifyStatus(const std::string& status) {
    spdlog::info("Home Manager: {}", status);
    if (status_callback_) {
        status_callback_(status);
    }
}

auto HomeManager::detectHomeSensor() -> bool {
    // This would typically involve checking hardware capabilities
    // For now, assume no home sensor unless explicitly configured
    return false;
}

auto HomeManager::findHomeSensorPosition() -> std::optional<double> {
    if (!has_home_sensor_) {
        return std::nullopt;
    }
    
    // Implementation would depend on specific hardware
    // This is a placeholder that would need to be implemented
    // based on the actual ASCOM dome's capabilities
    
    notifyStatus("Searching for home sensor");
    
    // Simulate home sensor search
    for (int i = 0; i < 10 && !abort_homing_; ++i) {
        std::this_thread::sleep_for(100ms);
        
        // Check if we found the home sensor
        // This is where actual hardware interaction would occur
        if (i == 5) { // Simulate finding home at iteration 5
            return 0.0; // Home at 0 degrees
        }
    }
    
    return std::nullopt;
}

} // namespace lithium::ascom::dome::components
