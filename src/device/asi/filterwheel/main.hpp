/*
 * asi_filterwheel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Electronic Filter Wheel (EFW) dedicated module

*************************************************/

#pragma once

#include "device/template/filterwheel.hpp"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declaration
namespace lithium::device::asi::filterwheel {
class ASIFilterwheelController;
}

#include "controller_stub.hpp"

namespace lithium::device::asi::filterwheel {

/**
 * @brief Dedicated ASI Electronic Filter Wheel (EFW) controller
 *
 * This class provides complete control over ASI EFW filter wheels,
 * including 5, 7, and 8-position models with advanced features like
 * unidirectional mode, custom filter naming, and sequence automation.
 */
class ASIFilterWheel : public AtomFilterWheel {
public:
    explicit ASIFilterWheel(const std::string& name = "ASI Filter Wheel");
    ~ASIFilterWheel() override;

    // Non-copyable and non-movable
    ASIFilterWheel(const ASIFilterWheel&) = delete;
    ASIFilterWheel& operator=(const ASIFilterWheel&) = delete;
    ASIFilterWheel(ASIFilterWheel&&) = delete;
    ASIFilterWheel& operator=(ASIFilterWheel&&) = delete;

    // Basic device interface (from AtomDriver)
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName = "", int timeout = 30000,
                 int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto isConnected() const -> bool override;
    auto scan() -> std::vector<std::string> override;

    // AtomFilterWheel interface implementation
    auto isMoving() const -> bool override;
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

    // ASI-specific extended functionality
    auto setFilterNames(const std::vector<std::string>& names) -> bool;
    auto getFilterNames() const -> std::vector<std::string>;
    auto getFilterName(int position) const -> std::string;
    auto setFilterName(int position, const std::string& name) -> bool;

    // Advanced ASI features
    auto enableUnidirectionalMode(bool enable) -> bool;
    auto isUnidirectionalMode() const -> bool;

    // Filter offset compensation (for focus)
    auto setFilterOffset(int position, double offset) -> bool;
    auto getFilterOffset(int position) const -> double;
    auto clearFilterOffsets() -> bool;

    // Sequence automation
    auto startFilterSequence(const std::vector<int>& positions,
                             double delayBetweenFilters = 0.0) -> bool;
    auto stopFilterSequence() -> bool;
    auto isSequenceRunning() const -> bool;
    auto getSequenceProgress() const -> std::pair<int, int>;  // current, total

    // Configuration management
    auto saveConfiguration(const std::string& filename) -> bool;
    auto loadConfiguration(const std::string& filename) -> bool;
    auto resetToDefaults() -> bool;

    // Callbacks and monitoring
    auto setMovementCallback(
        std::function<void(int position, bool moving)> callback) -> void;
    auto setSequenceCallback(
        std::function<void(int current, int total, bool completed)> callback)
        -> void;

    // Hardware information
    auto getFirmwareVersion() const -> std::string;
    auto getSerialNumber() const -> std::string;
    auto getModelName() const -> std::string;
    auto getWheelType() const
        -> std::string;  // "5-position", "7-position", "8-position"

    // Status and diagnostics
    auto getLastError() const -> std::string;
    auto getMovementCount() const -> uint32_t;
    auto getOperationHistory() const -> std::vector<std::string>;
    auto performSelfTest() -> bool;

    // Extended temperature monitoring (if available)
    auto hasTemperatureSensorExtended() const -> bool;
    auto getTemperatureExtended() const -> std::optional<double>;

private:
    std::unique_ptr<ASIFilterwheelController> controller_;

    // Constants
    static constexpr int MAX_FILTERS = 20;

    // Internal storage for filter information
    std::array<FilterInfo, MAX_FILTERS> filters_;
};

} // namespace lithium::device::asi::filterwheel
