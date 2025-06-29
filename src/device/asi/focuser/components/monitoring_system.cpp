/*
 * monitoring_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Monitoring System Implementation

*************************************************/

#include "monitoring_system.hpp"

#include <spdlog/spdlog.h>
#include "hardware_interface.hpp"
#include "position_manager.hpp"
#include "temperature_system.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace lithium::device::asi::focuser::components {

MonitoringSystem::MonitoringSystem(HardwareInterface* hardware,
                                   PositionManager* positionManager,
                                   TemperatureSystem* temperatureSystem)
    : hardware_(hardware),
      positionManager_(positionManager),
      temperatureSystem_(temperatureSystem) {
    spdlog::info("Created ASI Focuser Monitoring System");
}

MonitoringSystem::~MonitoringSystem() {
    stopMonitoring();
    spdlog::info("Destroyed ASI Focuser Monitoring System");
}

bool MonitoringSystem::startMonitoring() {
    std::lock_guard<std::mutex> lock(monitoringMutex_);

    if (monitoringActive_) {
        return true;
    }

    if (!hardware_ || !hardware_->isConnected()) {
        spdlog::error("Cannot start monitoring: hardware not connected");
        return false;
    }

    spdlog::info("Starting focuser monitoring (interval: {}ms)",
                 monitoringInterval_);

    monitoringActive_ = true;
    startTime_ = std::chrono::steady_clock::now();
    monitoringCycles_ = 0;
    errorCount_ = 0;

    // Initialize state
    if (positionManager_) {
        lastKnownPosition_ = positionManager_->getCurrentPosition();
    }
    if (temperatureSystem_) {
        auto temp = temperatureSystem_->getCurrentTemperature();
        if (temp.has_value()) {
            lastKnownTemperature_ = temp.value();
        }
    }

    // Start monitoring thread
    monitoringThread_ = std::thread(&MonitoringSystem::monitoringWorker, this);

    addOperationHistory("Monitoring started");
    spdlog::info("Focuser monitoring started successfully");
    return true;
}

bool MonitoringSystem::stopMonitoring() {
    std::lock_guard<std::mutex> lock(monitoringMutex_);

    if (!monitoringActive_) {
        return true;
    }

    spdlog::info("Stopping focuser monitoring");

    monitoringActive_ = false;

    if (monitoringThread_.joinable()) {
        monitoringThread_.join();
    }

    addOperationHistory("Monitoring stopped");
    spdlog::info("Focuser monitoring stopped");
    return true;
}

bool MonitoringSystem::setMonitoringInterval(int intervalMs) {
    if (intervalMs < 100 || intervalMs > 10000) {
        return false;
    }

    monitoringInterval_ = intervalMs;
    spdlog::info("Set monitoring interval to: {}ms", intervalMs);
    return true;
}

bool MonitoringSystem::waitForMovement(int timeoutMs) {
    if (!positionManager_) {
        return false;
    }

    auto start = std::chrono::steady_clock::now();

    while (positionManager_->isMoving()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - start)
                           .count();

        if (elapsed > timeoutMs) {
            spdlog::warn("Movement timeout after {}ms", timeoutMs);
            return false;
        }
    }

    if (movementCompleteCallback_) {
        movementCompleteCallback_(true);
    }

    return true;
}

bool MonitoringSystem::isMovementComplete() const {
    if (!positionManager_) {
        return true;
    }

    return !positionManager_->isMoving();
}

void MonitoringSystem::addOperationHistory(const std::string& operation) {
    std::lock_guard<std::mutex> lock(historyMutex_);

    std::string entry = formatTimestamp() + " - " + operation;
    operationHistory_.push_back(entry);

    // Keep only last MAX_HISTORY_ENTRIES
    if (operationHistory_.size() > MAX_HISTORY_ENTRIES) {
        operationHistory_.erase(operationHistory_.begin());
    }
}

std::vector<std::string> MonitoringSystem::getOperationHistory() const {
    std::lock_guard<std::mutex> lock(historyMutex_);
    return operationHistory_;
}

bool MonitoringSystem::clearOperationHistory() {
    std::lock_guard<std::mutex> lock(historyMutex_);
    operationHistory_.clear();
    spdlog::info("Operation history cleared");
    return true;
}

bool MonitoringSystem::saveOperationHistory(const std::string& filename) {
    std::lock_guard<std::mutex> lock(historyMutex_);

    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Could not open file for writing: {}", filename);
            return false;
        }

        file << "# ASI Focuser Operation History\n";
        file << "# Generated on: " << formatTimestamp() << "\n\n";

        for (const auto& entry : operationHistory_) {
            file << entry << "\n";
        }

        spdlog::info("Operation history saved to: {}", filename);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save operation history: {}", e.what());
        return false;
    }
}

std::chrono::duration<double> MonitoringSystem::getUptime() const {
    if (!monitoringActive_) {
        return std::chrono::duration<double>::zero();
    }

    return std::chrono::steady_clock::now() - startTime_;
}

void MonitoringSystem::monitoringWorker() {
    spdlog::info("Focuser monitoring worker started");

    while (monitoringActive_) {
        try {
            checkPositionChanges();
            checkTemperatureChanges();
            checkMovementStatus();

            monitoringCycles_++;

        } catch (const std::exception& e) {
            handleMonitoringError("Monitoring error: " + std::string(e.what()));
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(monitoringInterval_));
    }

    spdlog::info("Focuser monitoring worker stopped");
}

void MonitoringSystem::checkPositionChanges() {
    if (!positionManager_) {
        return;
    }

    int currentPosition = positionManager_->getCurrentPosition();

    if (currentPosition != lastKnownPosition_ && currentPosition >= 0) {
        lastKnownPosition_ = currentPosition;

        if (positionUpdateCallback_) {
            positionUpdateCallback_(currentPosition);
        }

        spdlog::debug("Position changed to: {}", currentPosition);
    }
}

void MonitoringSystem::checkTemperatureChanges() {
    if (!temperatureSystem_ || !temperatureSystem_->hasTemperatureSensor()) {
        return;
    }

    auto temp = temperatureSystem_->getCurrentTemperature();
    if (temp.has_value()) {
        double currentTemp = temp.value();

        if (std::abs(currentTemp - lastKnownTemperature_) > 0.1) {
            lastKnownTemperature_ = currentTemp;

            if (temperatureUpdateCallback_) {
                temperatureUpdateCallback_(currentTemp);
            }

            spdlog::debug("Temperature changed to: {:.1f}°C", currentTemp);

            // Apply temperature compensation if enabled
            if (temperatureSystem_->isTemperatureCompensationEnabled()) {
                temperatureSystem_->applyTemperatureCompensation();
            }
        }
    }
}

void MonitoringSystem::checkMovementStatus() {
    if (!positionManager_) {
        return;
    }

    bool currentlyMoving = positionManager_->isMoving();

    if (lastMovingState_ && !currentlyMoving) {
        // Movement just completed
        if (movementCompleteCallback_) {
            movementCompleteCallback_(true);
        }

        addOperationHistory(
            "Movement completed at position " +
            std::to_string(positionManager_->getCurrentPosition()));
    }

    lastMovingState_ = currentlyMoving;
}

void MonitoringSystem::handleMonitoringError(const std::string& error) {
    errorCount_++;
    lastMonitoringError_ = error;
    spdlog::error("Monitoring error: {}", error);

    // Add to operation history
    addOperationHistory("ERROR: " + error);

    // If too many errors, consider stopping monitoring
    if (errorCount_ > 100) {
        spdlog::error("Too many monitoring errors, stopping monitoring");
        monitoringActive_ = false;
    }
}

std::string MonitoringSystem::formatTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

}  // namespace lithium::device::asi::focuser::components
