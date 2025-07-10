/*
 * controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Filter Wheel Controller

This modular controller orchestrates the filterwheel components to provide
a clean, maintainable, and testable interface for ASCOM filterwheel control.

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "device/template/filterwheel.hpp"

// Forward declarations for components to avoid circular dependencies
namespace lithium::device::ascom::filterwheel::components {
class HardwareInterface;
class PositionManager;
class ConfigurationManager;
class MonitoringSystem;
class CalibrationSystem;
}  // namespace lithium::device::ascom::filterwheel::components

namespace lithium::device::ascom::filterwheel {

/**
 * @brief Modular ASCOM Filter Wheel Controller
 *
 * This controller provides a clean interface to ASCOM filterwheel functionality
 * by orchestrating specialized components. Each component handles a specific
 * aspect of filterwheel operation, promoting separation of concerns and
 * testability.
 */
class ASCOMFilterwheelController : public AtomFilterWheel {
public:
    explicit ASCOMFilterwheelController(std::string name);
    ~ASCOMFilterwheelController() override;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout, int maxRetry)
        -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Filter wheel state
    auto isMoving() const -> bool override;

    // Position control
    auto getPosition() -> std::optional<int> override;
    auto setPosition(int position) -> bool override;
    auto getFilterCount() -> int override;
    auto isValidPosition(int position) -> bool override;

    // Filter names and information
    auto getSlotName(int slot) -> std::optional<std::string> override;
    auto setSlotName(int slot, const std::string& name) -> bool override;
    auto getAllSlotNames() -> std::vector<std::string> override;
    auto getCurrentFilterName() -> std::string override;

    // Enhanced filter management
    auto getFilterInfo(int slot) -> std::optional<FilterInfo> override;
    auto setFilterInfo(int slot, const FilterInfo& info) -> bool override;
    auto getAllFilterInfo() -> std::vector<FilterInfo> override;

    // Filter search and selection
    auto findFilterByName(const std::string& name)
        -> std::optional<int> override;
    auto findFilterByType(const std::string& type) -> std::vector<int> override;
    auto selectFilterByName(const std::string& name) -> bool override;
    auto selectFilterByType(const std::string& type) -> bool override;

    // Motion control
    auto abortMotion() -> bool override;
    auto homeFilterWheel() -> bool override;
    auto calibrateFilterWheel() -> bool override;

    // Temperature (if supported)
    auto getTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Statistics
    auto getTotalMoves() -> uint64_t override;
    auto resetTotalMoves() -> bool override;
    auto getLastMoveTime() -> int override;

    // Configuration presets
    auto saveFilterConfiguration(const std::string& name) -> bool override;
    auto loadFilterConfiguration(const std::string& name) -> bool override;
    auto deleteFilterConfiguration(const std::string& name) -> bool override;
    auto getAvailableConfigurations() -> std::vector<std::string> override;

    // ASCOM-specific functionality
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string& clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

    // Connection type management
    auto connectToCOMDriver(const std::string& progId) -> bool;
    auto connectToAlpacaDevice(const std::string& host, int port,
                               int deviceNumber) -> bool;
    auto discoverAlpacaDevices() -> std::vector<std::string>;

    // Advanced features
    auto performSelfTest() -> bool;
    auto getConnectionType() -> std::string;
    auto getConnectionStatus() -> std::string;

    // Sequence control
    auto createSequence(const std::string& name,
                        const std::vector<int>& positions,
                        int dwell_time_ms = 1000) -> bool;
    auto startSequence(const std::string& name) -> bool;
    auto pauseSequence() -> bool;
    auto resumeSequence() -> bool;
    auto stopSequence() -> bool;
    auto isSequenceRunning() const -> bool;
    auto getSequenceProgress() const -> double;

    // Error handling
    auto getLastError() const -> std::string;
    auto clearError() -> void;

private:
    // Component management
    std::unique_ptr<components::HardwareInterface> hardware_interface_;
    std::unique_ptr<components::PositionManager> position_manager_;
    std::unique_ptr<components::ConfigurationManager> configuration_manager_;
    std::unique_ptr<components::MonitoringSystem> monitoring_system_;
    std::unique_ptr<components::CalibrationSystem> calibration_system_;

    // Internal state
    std::atomic<bool> is_initialized_{false};
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // Component initialization
    auto initializeComponents() -> bool;
    auto destroyComponents() -> void;
    auto checkComponentHealth() -> bool;

    // Error handling
    auto setError(const std::string& error) -> void;
};

}  // namespace lithium::device::ascom::filterwheel
