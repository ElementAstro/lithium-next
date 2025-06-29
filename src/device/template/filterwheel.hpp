/*
 * filterwheel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced AtomFilterWheel following INDI architecture

*************************************************/

#pragma once

#include "device.hpp"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

enum class FilterWheelState {
    IDLE,
    MOVING,
    ERROR
};

// Filter information
struct FilterInfo {
    std::string name;
    std::string type;           // e.g., "L", "R", "G", "B", "Ha", "OIII", "SII"
    double wavelength{0.0};     // nm
    double bandwidth{0.0};      // nm
    std::string description;
} ATOM_ALIGNAS(64);

// Filter wheel capabilities
struct FilterWheelCapabilities {
    int maxFilters{8};
    bool canRename{true};
    bool hasNames{true};
    bool hasTemperature{false};
    bool canAbort{true};
} ATOM_ALIGNAS(8);

class AtomFilterWheel : public AtomDriver {
public:
    explicit AtomFilterWheel(std::string name) : AtomDriver(std::move(name)) {
        setType("FilterWheel");
        // Initialize with default filter names
        for (int i = 0; i < MAX_FILTERS; ++i) {
            filters_[i].name = "Filter " + std::to_string(i + 1);
            filters_[i].type = "Unknown";
        }
    }

    ~AtomFilterWheel() override = default;

    // Capabilities
    const FilterWheelCapabilities& getFilterWheelCapabilities() const { return filterwheel_capabilities_; }
    void setFilterWheelCapabilities(const FilterWheelCapabilities& caps) { filterwheel_capabilities_ = caps; }

    // State
    FilterWheelState getFilterWheelState() const { return filterwheel_state_; }
    virtual bool isMoving() const = 0;

    // Position control
    virtual auto getPosition() -> std::optional<int> = 0;
    virtual auto setPosition(int position) -> bool = 0;
    virtual auto getFilterCount() -> int = 0;
    virtual auto isValidPosition(int position) -> bool = 0;

    // Filter names and information
    virtual auto getSlotName(int slot) -> std::optional<std::string> = 0;
    virtual auto setSlotName(int slot, const std::string& name) -> bool = 0;
    virtual auto getAllSlotNames() -> std::vector<std::string> = 0;
    virtual auto getCurrentFilterName() -> std::string = 0;

    // Enhanced filter management
    virtual auto getFilterInfo(int slot) -> std::optional<FilterInfo> = 0;
    virtual auto setFilterInfo(int slot, const FilterInfo& info) -> bool = 0;
    virtual auto getAllFilterInfo() -> std::vector<FilterInfo> = 0;

    // Filter search and selection
    virtual auto findFilterByName(const std::string& name) -> std::optional<int> = 0;
    virtual auto findFilterByType(const std::string& type) -> std::vector<int> = 0;
    virtual auto selectFilterByName(const std::string& name) -> bool = 0;
    virtual auto selectFilterByType(const std::string& type) -> bool = 0;

    // Motion control
    virtual auto abortMotion() -> bool = 0;
    virtual auto homeFilterWheel() -> bool = 0;
    virtual auto calibrateFilterWheel() -> bool = 0;

    // Temperature (if supported)
    virtual auto getTemperature() -> std::optional<double> = 0;
    virtual auto hasTemperatureSensor() -> bool = 0;

    // Statistics
    virtual auto getTotalMoves() -> uint64_t = 0;
    virtual auto resetTotalMoves() -> bool = 0;
    virtual auto getLastMoveTime() -> int = 0;

    // Configuration presets
    virtual auto saveFilterConfiguration(const std::string& name) -> bool = 0;
    virtual auto loadFilterConfiguration(const std::string& name) -> bool = 0;
    virtual auto deleteFilterConfiguration(const std::string& name) -> bool = 0;
    virtual auto getAvailableConfigurations() -> std::vector<std::string> = 0;

    // Event callbacks
    using PositionCallback = std::function<void(int position, const std::string& filterName)>;
    using MoveCompleteCallback = std::function<void(bool success, const std::string& message)>;
    using TemperatureCallback = std::function<void(double temperature)>;

    virtual void setPositionCallback(PositionCallback callback) { position_callback_ = std::move(callback); }
    virtual void setMoveCompleteCallback(MoveCompleteCallback callback) { move_complete_callback_ = std::move(callback); }
    virtual void setTemperatureCallback(TemperatureCallback callback) { temperature_callback_ = std::move(callback); }

    // Utility methods
    virtual auto isValidSlot(int slot) -> bool {
        return slot >= 0 && slot < filterwheel_capabilities_.maxFilters;
    }
    virtual auto getMaxFilters() -> int { return filterwheel_capabilities_.maxFilters; }

protected:
    static constexpr int MAX_FILTERS = 20;

    FilterWheelState filterwheel_state_{FilterWheelState::IDLE};
    FilterWheelCapabilities filterwheel_capabilities_;

    // Filter storage
    std::array<FilterInfo, MAX_FILTERS> filters_;
    int current_position_{0};
    int target_position_{0};

    // Statistics
    uint64_t total_moves_{0};
    int last_move_time_{0};

    // Callbacks
    PositionCallback position_callback_;
    MoveCompleteCallback move_complete_callback_;
    TemperatureCallback temperature_callback_;

    // Utility methods
    virtual void updateFilterWheelState(FilterWheelState state) { filterwheel_state_ = state; }
    virtual void notifyPositionChange(int position, const std::string& filterName);
    virtual void notifyMoveComplete(bool success, const std::string& message = "");
    virtual void notifyTemperatureChange(double temperature);
};
