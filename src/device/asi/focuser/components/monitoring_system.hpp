/*
 * monitoring_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Focuser Monitoring System Component
Handles background monitoring and status tracking

*************************************************/

#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lithium::device::asi::focuser::components {

// Forward declarations
class HardwareInterface;
class PositionManager;
class TemperatureSystem;

/**
 * @brief Background monitoring system for ASI Focuser
 *
 * This component handles background monitoring of position and temperature,
 * operation history tracking, and status reporting.
 */
class MonitoringSystem {
public:
    MonitoringSystem(HardwareInterface* hardware,
                     PositionManager* positionManager,
                     TemperatureSystem* temperatureSystem);
    ~MonitoringSystem();

    // Non-copyable and non-movable
    MonitoringSystem(const MonitoringSystem&) = delete;
    MonitoringSystem& operator=(const MonitoringSystem&) = delete;
    MonitoringSystem(MonitoringSystem&&) = delete;
    MonitoringSystem& operator=(MonitoringSystem&&) = delete;

    // Monitoring control
    bool startMonitoring();
    bool stopMonitoring();
    bool isMonitoring() const { return monitoringActive_; }

    // Monitoring intervals
    bool setMonitoringInterval(int intervalMs);
    int getMonitoringInterval() const { return monitoringInterval_; }

    // Movement monitoring
    bool waitForMovement(int timeoutMs = 30000);
    bool isMovementComplete() const;

    // Operation history
    void addOperationHistory(const std::string& operation);
    std::vector<std::string> getOperationHistory() const;
    bool clearOperationHistory();
    bool saveOperationHistory(const std::string& filename);

    // Status callbacks
    void setPositionUpdateCallback(std::function<void(int)> callback) {
        positionUpdateCallback_ = callback;
    }
    void setTemperatureUpdateCallback(std::function<void(double)> callback) {
        temperatureUpdateCallback_ = callback;
    }
    void setMovementCompleteCallback(std::function<void(bool)> callback) {
        movementCompleteCallback_ = callback;
    }

    // Statistics
    std::chrono::steady_clock::time_point getStartTime() const {
        return startTime_;
    }
    std::chrono::duration<double> getUptime() const;
    uint32_t getMonitoringCycles() const { return monitoringCycles_; }

    // Error tracking
    uint32_t getErrorCount() const { return errorCount_; }
    std::string getLastMonitoringError() const { return lastMonitoringError_; }

private:
    // Dependencies
    HardwareInterface* hardware_;
    PositionManager* positionManager_;
    TemperatureSystem* temperatureSystem_;

    // Monitoring state
    bool monitoringActive_ = false;
    std::thread monitoringThread_;
    int monitoringInterval_ = 1000;  // milliseconds

    // Monitoring statistics
    std::chrono::steady_clock::time_point startTime_;
    uint32_t monitoringCycles_ = 0;
    uint32_t errorCount_ = 0;
    std::string lastMonitoringError_;

    // State tracking
    int lastKnownPosition_ = -1;
    double lastKnownTemperature_ = -999.0;
    bool lastMovingState_ = false;

    // Operation history
    std::vector<std::string> operationHistory_;
    static constexpr size_t MAX_HISTORY_ENTRIES = 100;

    // Callbacks
    std::function<void(int)> positionUpdateCallback_;
    std::function<void(double)> temperatureUpdateCallback_;
    std::function<void(bool)> movementCompleteCallback_;

    // Thread safety
    mutable std::mutex monitoringMutex_;
    mutable std::mutex historyMutex_;

    // Worker methods
    void monitoringWorker();
    void checkPositionChanges();
    void checkTemperatureChanges();
    void checkMovementStatus();
    void handleMonitoringError(const std::string& error);
    std::string formatTimestamp() const;
};

}  // namespace lithium::device::asi::focuser::components
