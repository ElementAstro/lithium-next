/*
 * controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Focuser Controller

This modular controller orchestrates the focuser components to provide
a clean, maintainable, and testable interface for ASCOM focuser control.

*************************************************/

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <chrono>

#include "./components/hardware_interface.hpp"
#include "./components/movement_controller.hpp"
#include "./components/temperature_controller.hpp"
#include "./components/position_manager.hpp"
#include "./components/backlash_compensator.hpp"
#include "./components/property_manager.hpp"
#include "device/template/focuser.hpp"

namespace lithium::device::ascom::focuser {

// Forward declarations
namespace components {
class HardwareInterface;
class MovementController;
class TemperatureController;
class PositionManager;
class BacklashCompensator;
class PropertyManager;
}

/**
 * @brief Modular ASCOM Focuser Controller
 *
 * This controller provides a clean interface to ASCOM focuser functionality by
 * orchestrating specialized components. Each component handles a specific
 * aspect of focuser operation, promoting separation of concerns and
 * testability.
 */
class ASCOMFocuserController : public AtomFocuser {
public:
    explicit ASCOMFocuserController(const std::string& name);
    ~ASCOMFocuserController() override;

    // Non-copyable and non-movable
    ASCOMFocuserController(const ASCOMFocuserController&) = delete;
    ASCOMFocuserController& operator=(const ASCOMFocuserController&) = delete;
    ASCOMFocuserController(ASCOMFocuserController&&) = delete;
    ASCOMFocuserController& operator=(ASCOMFocuserController&&) = delete;

    // =========================================================================
    // AtomDriver Interface Implementation
    // =========================================================================
    
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout = 5000, int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Movement Control
    // =========================================================================
    
    auto isMoving() const -> bool override;
    auto moveSteps(int steps) -> bool override;
    auto moveToPosition(int position) -> bool override;
    auto getPosition() -> std::optional<int> override;
    auto moveForDuration(int durationMs) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(int position) -> bool override;
    auto moveInward(int steps) -> bool override;
    auto moveOutward(int steps) -> bool override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Speed Control
    // =========================================================================
    
    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> int override;
    auto getSpeedRange() -> std::pair<int, int> override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Direction Control
    // =========================================================================
    
    auto getDirection() -> std::optional<FocusDirection> override;
    auto setDirection(FocusDirection direction) -> bool override;
    auto isReversed() -> std::optional<bool> override;
    auto setReversed(bool reversed) -> bool override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Limits Control
    // =========================================================================
    
    auto getMaxLimit() -> std::optional<int> override;
    auto setMaxLimit(int maxLimit) -> bool override;
    auto getMinLimit() -> std::optional<int> override;
    auto setMinLimit(int minLimit) -> bool override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Temperature
    // =========================================================================
    
    auto getExternalTemperature() -> std::optional<double> override;
    auto getChipTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;
    auto getTemperatureCompensation() -> TemperatureCompensation override;
    auto setTemperatureCompensation(const TemperatureCompensation& comp) -> bool override;
    auto enableTemperatureCompensation(bool enable) -> bool override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Backlash Compensation
    // =========================================================================
    
    auto getBacklash() -> int override;
    auto setBacklash(int backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Auto Focus
    // =========================================================================
    
    auto startAutoFocus() -> bool override;
    auto stopAutoFocus() -> bool override;
    auto isAutoFocusing() -> bool override;
    auto getAutoFocusProgress() -> double override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Presets
    // =========================================================================
    
    auto savePreset(int slot, int position) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<int> override;
    auto deletePreset(int slot) -> bool override;

    // =========================================================================
    // AtomFocuser Interface Implementation - Statistics
    // =========================================================================
    
    auto getTotalSteps() -> uint64_t override;
    auto resetTotalSteps() -> bool override;
    auto getLastMoveSteps() -> int override;
    auto getLastMoveDuration() -> int override;

    // =========================================================================
    // ASCOM-Specific Methods
    // =========================================================================

    /**
     * @brief Get ASCOM driver information
     */
    auto getASCOMDriverInfo() -> std::optional<std::string>;

    /**
     * @brief Get ASCOM driver version
     */
    auto getASCOMVersion() -> std::optional<std::string>;

    /**
     * @brief Get ASCOM interface version
     */
    auto getASCOMInterfaceVersion() -> std::optional<int>;

    /**
     * @brief Set ASCOM client ID
     */
    auto setASCOMClientID(const std::string &clientId) -> bool;

    /**
     * @brief Get ASCOM client ID
     */
    auto getASCOMClientID() -> std::optional<std::string>;

    /**
     * @brief Check if focuser is absolute
     */
    auto isAbsolute() -> bool;

    /**
     * @brief Get maximum increment
     */
    auto getMaxIncrement() -> int;

    /**
     * @brief Get maximum step
     */
    auto getMaxStep() -> int;

    /**
     * @brief Get step count
     */
    auto getStepCount() -> int;

    /**
     * @brief Get step size
     */
    auto getStepSize() -> double;

    /**
     * @brief Check if temperature compensation is available
     */
    auto getTempCompAvailable() -> bool;

    /**
     * @brief Get temperature compensation state
     */
    auto getTempComp() -> bool;

    /**
     * @brief Set temperature compensation state
     */
    auto setTempComp(bool enable) -> bool;

    // =========================================================================
    // Alpaca Discovery and Connection
    // =========================================================================

    /**
     * @brief Discover Alpaca devices
     */
    auto discoverAlpacaDevices() -> std::vector<std::string>;

    /**
     * @brief Connect to Alpaca device
     */
    auto connectToAlpacaDevice(const std::string &host, int port, int deviceNumber) -> bool;

    /**
     * @brief Disconnect from Alpaca device
     */
    auto disconnectFromAlpacaDevice() -> bool;

    // =========================================================================
    // COM Driver Connection (Windows only)
    // =========================================================================

#ifdef _WIN32
    /**
     * @brief Connect to COM driver
     */
    auto connectToCOMDriver(const std::string &progId) -> bool;

    /**
     * @brief Disconnect from COM driver
     */
    auto disconnectFromCOMDriver() -> bool;

    /**
     * @brief Show ASCOM chooser dialog
     */
    auto showASCOMChooser() -> std::optional<std::string>;
#endif

    // =========================================================================
    // Component Access (for testing and advanced usage)
    // =========================================================================

    /**
     * @brief Get hardware interface component
     */
    auto getHardwareInterface() -> std::shared_ptr<components::HardwareInterface>;

    /**
     * @brief Get movement controller component
     */
    auto getMovementController() -> std::shared_ptr<components::MovementController>;

    /**
     * @brief Get temperature controller component
     */
    auto getTemperatureController() -> std::shared_ptr<components::TemperatureController>;

    /**
     * @brief Get position manager component
     */
    auto getPositionManager() -> std::shared_ptr<components::PositionManager>;

    /**
     * @brief Get backlash compensator component
     */
    auto getBacklashCompensator() -> std::shared_ptr<components::BacklashCompensator>;

    /**
     * @brief Get property manager component
     */
    auto getPropertyManager() -> std::shared_ptr<components::PropertyManager>;

    // =========================================================================
    // Enhanced Features
    // =========================================================================

    /**
     * @brief Get movement progress (0.0 to 1.0)
     */
    auto getMovementProgress() -> double;

    /**
     * @brief Get estimated time remaining for current move
     */
    auto getEstimatedTimeRemaining() -> std::chrono::milliseconds;

    /**
     * @brief Get focuser capabilities
     */
    auto getFocuserCapabilities() -> FocuserCapabilities;

    /**
     * @brief Get comprehensive focuser status
     */
    auto getFocuserStatus() -> std::string;

    /**
     * @brief Enable/disable debug mode
     */
    auto setDebugMode(bool enabled) -> void;

    /**
     * @brief Check if debug mode is enabled
     */
    auto isDebugModeEnabled() -> bool;

    // =========================================================================
    // Calibration and Maintenance
    // =========================================================================

    /**
     * @brief Calibrate focuser
     */
    auto calibrateFocuser() -> bool;

    /**
     * @brief Test focuser functionality
     */
    auto testFocuser() -> bool;

    /**
     * @brief Get focuser health status
     */
    auto getFocuserHealth() -> std::string;

    /**
     * @brief Reset focuser to default settings
     */
    auto resetToDefaults() -> bool;

    /**
     * @brief Save focuser configuration
     */
    auto saveConfiguration(const std::string& filename) -> bool;

    /**
     * @brief Load focuser configuration
     */
    auto loadConfiguration(const std::string& filename) -> bool;

private:
    // Component instances
    std::shared_ptr<components::HardwareInterface> hardware_interface_;
    std::shared_ptr<components::MovementController> movement_controller_;
    std::shared_ptr<components::TemperatureController> temperature_controller_;
    std::shared_ptr<components::PositionManager> position_manager_;
    std::shared_ptr<components::BacklashCompensator> backlash_compensator_;
    std::shared_ptr<components::PropertyManager> property_manager_;

    // Controller state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    std::atomic<bool> debug_mode_{false};
    std::atomic<bool> auto_focus_active_{false};
    
    // Configuration
    std::string device_name_;
    std::string client_id_{"Lithium-Next"};
    
    // Synchronization
    mutable std::mutex controller_mutex_;
    std::condition_variable state_change_cv_;
    
    // Private methods
    auto initializeComponents() -> bool;
    auto destroyComponents() -> bool;
    auto setupComponentCallbacks() -> void;
    auto validateComponentStates() -> bool;
    
    // Component interaction helpers
    auto coordinateMovement(int targetPosition) -> bool;
    auto handleTemperatureCompensation() -> void;
    auto handleBacklashCompensation(int startPosition, int targetPosition) -> bool;
    auto updateFocuserCapabilities() -> void;
    
    // Event handling
    auto onPositionChanged(int position) -> void;
    auto onTemperatureChanged(double temperature) -> void;
    auto onMovementComplete(bool success, int finalPosition, const std::string& message) -> void;
    auto onPropertyChanged(const std::string& name, const std::string& value) -> void;
    
    // Utility methods
    auto parseDeviceString(const std::string& deviceName) -> std::tuple<std::string, int, int>;
    auto buildStatusString() -> std::string;
    auto validateConfiguration() -> bool;
    auto logComponentStatus() -> void;
    
    // Auto-focus implementation
    auto performAutoFocus() -> bool;
    auto findOptimalFocusPosition() -> std::optional<int>;
    auto measureFocusQuality(int position) -> double;
    
    // Calibration helpers
    auto calibrateBacklash() -> bool;
    auto calibrateTemperatureCompensation() -> bool;
    auto calibrateMovementLimits() -> bool;
    
    // Error handling
    auto handleComponentError(const std::string& component, const std::string& error) -> void;
    auto recoverFromError() -> bool;
    
    // Performance monitoring
    struct PerformanceMetrics {
        std::chrono::steady_clock::time_point last_move_time;
        std::chrono::milliseconds average_move_time{0};
        int total_moves{0};
        int successful_moves{0};
        int failed_moves{0};
        double success_rate{0.0};
    } performance_metrics_;
    
    auto updatePerformanceMetrics(bool success, std::chrono::milliseconds duration) -> void;
    auto getPerformanceReport() -> std::string;
};

} // namespace lithium::device::ascom::focuser
