/*
 * position_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Filter Wheel Position Manager Component

This component manages filter positioning, validation, and movement tracking.

*************************************************/

#pragma once

#include "hardware_interface.hpp"
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>

namespace lithium::device::asi::filterwheel::components {

/**
 * @brief Position manager for filter wheel operations
 *
 * Handles filter positioning with validation, movement tracking,
 * and callback notifications.
 */
class PositionManager {
public:
    /**
     * @brief Position change callback signature
     * @param currentPosition Current filter position
     * @param isMoving Whether the wheel is currently moving
     */
    using PositionCallback = std::function<void(int currentPosition, bool isMoving)>;

    explicit PositionManager(std::shared_ptr<HardwareInterface> hwInterface);
    ~PositionManager();

    // Non-copyable and non-movable
    PositionManager(const PositionManager&) = delete;
    PositionManager& operator=(const PositionManager&) = delete;
    PositionManager(PositionManager&&) = delete;
    PositionManager& operator=(PositionManager&&) = delete;

    // Initialization
    bool initialize();
    bool destroy();

    // Position control
    bool setPosition(int position);
    int getCurrentPosition();
    bool isMoving() const;
    bool stopMovement();

    // Position validation
    bool isValidPosition(int position) const;
    int getFilterCount() const;

    // Movement tracking
    bool waitForMovement(int timeoutMs = 10000);
    void startMovementMonitoring();
    void stopMovementMonitoring();

    // Callbacks
    void setPositionCallback(PositionCallback callback);

    // Home position
    bool moveToHome();

    // Statistics
    uint32_t getMovementCount() const;
    void resetMovementCount();

    // State
    bool isInitialized() const;
    std::string getLastError() const;

private:
    std::shared_ptr<HardwareInterface> hwInterface_;
    mutable std::mutex posMutex_;

    bool initialized_;
    int currentPosition_;
    std::atomic<bool> isMoving_;
    uint32_t movementCount_;
    std::string lastError_;

    // Movement monitoring
    bool monitoringEnabled_;
    std::thread monitoringThread_;
    std::atomic<bool> shouldStopMonitoring_;

    // Callback
    PositionCallback positionCallback_;

    // Helper methods
    void setError(const std::string& error);
    void notifyPositionChange(int position, bool moving);
    void monitoringWorker();
    void updateCurrentPosition();
};

} // namespace lithium::device::asi::filterwheel::components
