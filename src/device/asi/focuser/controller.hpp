/*
 * asi_focuser_controller_v2.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASI Focuser Controller
Uses component-based architecture for better maintainability

*************************************************/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declarations for components
namespace lithium::device::asi::focuser::components {
class HardwareInterface;
class PositionManager;
class TemperatureSystem;
class ConfigurationManager;
class MonitoringSystem;
class CalibrationSystem;
}  // namespace lithium::device::asi::focuser::components

// Forward declarations
namespace lithium::device::asi::focuser {
class ASIFocuser;
}

namespace lithium::device::asi::focuser::controller {

/**
 * @brief Modular ASI Focuser Controller
 *
 * This class orchestrates multiple focused components to provide
 * a complete focuser control system. Each component handles a specific
 * aspect of focuser functionality.
 */
class ASIFocuserControllerV2 {
public:
    explicit ASIFocuserControllerV2(ASIFocuser* parent);
    ~ASIFocuserControllerV2();

    // Non-copyable and non-movable
    ASIFocuserControllerV2(const ASIFocuserControllerV2&) = delete;
    ASIFocuserControllerV2& operator=(const ASIFocuserControllerV2&) = delete;
    ASIFocuserControllerV2(ASIFocuserControllerV2&&) = delete;
    ASIFocuserControllerV2& operator=(ASIFocuserControllerV2&&) = delete;

    // Lifecycle management
    bool initialize();
    bool destroy();
    bool connect(const std::string& deviceName, int timeout = 5000,
                 int maxRetry = 3);
    bool disconnect();
    bool scan(std::vector<std::string>& devices);

    // Position control (delegated to PositionManager)
    bool moveToPosition(int position);
    bool moveSteps(int steps);
    int getPosition();
    bool syncPosition(int position);
    bool isMoving() const;
    bool abortMove();

    // Position limits
    int getMaxPosition() const;
    int getMinPosition() const;
    bool setMaxLimit(int limit);
    bool setMinLimit(int limit);

    // Speed control
    bool setSpeed(double speed);
    double getSpeed() const;
    int getMaxSpeed() const;
    std::pair<int, int> getSpeedRange() const;

    // Direction control
    bool setDirection(bool inward);
    bool isDirectionReversed() const;

    // Home operations
    bool homeToZero();
    bool setHomePosition();
    bool goToHome();

    // Temperature operations (delegated to TemperatureSystem)
    std::optional<double> getTemperature() const;
    bool hasTemperatureSensor() const;
    bool setTemperatureCoefficient(double coefficient);
    double getTemperatureCoefficient() const;
    bool enableTemperatureCompensation(bool enable);
    bool isTemperatureCompensationEnabled() const;

    // Configuration operations (delegated to ConfigurationManager)
    bool saveConfiguration(const std::string& filename);
    bool loadConfiguration(const std::string& filename);
    bool enableBeep(bool enable);
    bool isBeepEnabled() const;
    bool enableHighResolutionMode(bool enable);
    bool isHighResolutionMode() const;
    double getResolution() const;
    bool setBacklash(int backlash);
    int getBacklash() const;
    bool enableBacklashCompensation(bool enable);
    bool isBacklashCompensationEnabled() const;

    // Monitoring operations (delegated to MonitoringSystem)
    bool startMonitoring();
    bool stopMonitoring();
    bool isMonitoring() const;
    std::vector<std::string> getOperationHistory() const;
    bool waitForMovement(int timeoutMs = 30000);

    // Calibration operations (delegated to CalibrationSystem)
    bool performSelfTest();
    bool calibrateFocuser();
    bool performFullCalibration();
    std::vector<std::string> getDiagnosticResults() const;

    // Hardware information
    std::string getFirmwareVersion() const;
    std::string getModelName() const;
    std::string getSerialNumber() const;
    bool setDeviceAlias(const std::string& alias);
    static std::string getSDKVersion();

    // Enhanced hardware control
    bool resetPosition(int position = 0);
    bool setBeep(bool enable);
    bool getBeep() const;
    bool setMaxStep(int maxStep);
    int getMaxStep() const;
    int getStepRange() const;

    // Statistics
    uint32_t getMovementCount() const;
    uint64_t getTotalSteps() const;
    int getLastMoveSteps() const;
    int getLastMoveDuration() const;

    // Connection state
    bool isInitialized() const;
    bool isConnected() const;
    std::string getLastError() const;

    // Callbacks
    void setPositionCallback(std::function<void(int)> callback);
    void setTemperatureCallback(std::function<void(double)> callback);
    void setMoveCompleteCallback(std::function<void(bool)> callback);

private:
    // Parent reference
    ASIFocuser* parent_;

    // Component instances
    std::unique_ptr<components::HardwareInterface> hardware_;
    std::unique_ptr<components::PositionManager> positionManager_;
    std::unique_ptr<components::TemperatureSystem> temperatureSystem_;
    std::unique_ptr<components::ConfigurationManager> configManager_;
    std::unique_ptr<components::MonitoringSystem> monitoringSystem_;
    std::unique_ptr<components::CalibrationSystem> calibrationSystem_;

    // Initialization state
    bool initialized_ = false;

    // Error tracking
    std::string lastError_;

    // Helper methods
    void setupComponentCallbacks();
    void updateLastError();
};

// Type alias for backward compatibility
using ASIFocuserController = ASIFocuserControllerV2;

}  // namespace lithium::device::asi::focuser::controller
