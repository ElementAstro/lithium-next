/*
 * asi_filterwheel_controller_v2.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASI Filter Wheel Controller V2

This modular controller orchestrates the filterwheel components to provide
a clean, maintainable, and testable interface for ASI EFW control.

*************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

// Forward declarations for components to avoid circular dependencies
namespace lithium::device::asi::filterwheel::components {
class HardwareInterface;
class PositionManager;
class ConfigurationManager;
class SequenceManager;
class MonitoringSystem;
class CalibrationSystem;
}

namespace lithium::device::asi::filterwheel {

// Forward declarations
namespace components {
class HardwareInterface;
class PositionManager;
}

/**
 * @brief Modular ASI Filter Wheel Controller V2
 *
 * This controller provides a clean interface to ASI EFW functionality by
 * orchestrating specialized components. Each component handles a specific
 * aspect of filterwheel operation, promoting separation of concerns and
 * testability.
 */
class ASIFilterwheelController {
public:
    ASIFilterwheelController();
    ~ASIFilterwheelController();

    // Initialization and cleanup
    bool initialize(const std::string& device_path = "");
    bool shutdown();
    bool isInitialized() const;

    // Basic position control
    bool moveToPosition(int position);
    int getCurrentPosition() const;
    bool isMoving() const;
    bool stopMovement();
    bool waitForMovement(int timeout_ms = 30000);
    int getSlotCount() const;

    // Filter management
    bool setFilterName(int slot, const std::string& name);
    std::string getFilterName(int slot) const;
    std::vector<std::string> getFilterNames() const;
    bool setFocusOffset(int slot, double offset);
    double getFocusOffset(int slot) const;

    // Profile management
    bool createProfile(const std::string& name,
                       const std::string& description = "");
    bool setCurrentProfile(const std::string& name);
    std::string getCurrentProfile() const;
    std::vector<std::string> getProfiles() const;
    bool deleteProfile(const std::string& name);

    // Sequence control
    bool createSequence(const std::string& name,
                        const std::vector<int>& positions,
                        int dwell_time_ms = 1000);
    bool startSequence(const std::string& name);
    bool pauseSequence();
    bool resumeSequence();
    bool stopSequence();
    bool isSequenceRunning() const;
    double getSequenceProgress() const;

    // Calibration and testing
    bool performCalibration();
    bool performSelfTest();
    bool testPosition(int position);
    std::string getCalibrationStatus() const;
    bool hasValidCalibration() const;

    // Monitoring and diagnostics
    double getSuccessRate() const;
    int getConsecutiveFailures() const;
    std::string getHealthStatus() const;
    bool isHealthy() const;
    void startHealthMonitoring(int interval_ms = 5000);
    void stopHealthMonitoring();

    // Configuration persistence
    bool saveConfiguration(const std::string& filepath = "");
    bool loadConfiguration(const std::string& filepath = "");

    // Event callbacks
    using PositionCallback =
        std::function<void(int old_position, int new_position)>;
    using SequenceCallback =
        std::function<void(const std::string& event, int step, int position)>;
    using HealthCallback =
        std::function<void(const std::string& status, bool is_healthy)>;

    void setPositionCallback(PositionCallback callback);
    void setSequenceCallback(SequenceCallback callback);
    void setHealthCallback(HealthCallback callback);
    void clearCallbacks();

    // Status and information
    std::string getDeviceInfo() const;
    std::string getVersion() const;
    std::string getLastError() const;

    // Component access (for advanced usage)
    std::shared_ptr<components::HardwareInterface> getHardwareInterface() const;
    std::shared_ptr<components::PositionManager> getPositionManager() const;
    std::shared_ptr<components::ConfigurationManager> getConfigurationManager() const;
    std::shared_ptr<components::SequenceManager> getSequenceManager() const;
    std::shared_ptr<components::MonitoringSystem> getMonitoringSystem() const;
    std::shared_ptr<components::CalibrationSystem> getCalibrationSystem() const;

private:
    // Component instances
    std::shared_ptr<components::HardwareInterface> hardware_interface_;
    std::shared_ptr<components::PositionManager> position_manager_;
    std::shared_ptr<components::ConfigurationManager> configuration_manager_;
    std::shared_ptr<components::SequenceManager> sequence_manager_;
    std::shared_ptr<components::MonitoringSystem> monitoring_system_;
    std::shared_ptr<components::CalibrationSystem> calibration_system_;

    // State
    bool initialized_;
    std::string last_error_;
    int last_position_;

    // Callbacks
    PositionCallback position_callback_;
    SequenceCallback sequence_callback_;
    HealthCallback health_callback_;

    // Internal methods
    bool initializeComponents();
    void setupCallbacks();
    void cleanupComponents();
    bool validateComponentsReady() const;
    void setLastError(const std::string& error);
    void notifyPositionChange(int new_position);
    void onSequenceEvent(const std::string& event, int step, int position);
    void onHealthUpdate(const std::string& status, bool is_healthy);

    // Component validation
    bool validateConfiguration() const;
    std::vector<std::string> getComponentErrors() const;
};

}  // namespace lithium::device::asi::filterwheel
