/*
 * mock_filterwheel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Mock Filter Wheel Implementation for testing

*************************************************/

#pragma once

#include "../filterwheel.hpp"

#include <map>
#include <random>
#include <thread>

class MockFilterWheel : public AtomFilterWheel {
public:
    explicit MockFilterWheel(const std::string& name = "MockFilterWheel");
    ~MockFilterWheel() override = default;

    // AtomDriver interface
    bool initialize() override;
    bool destroy() override;
    bool connect(const std::string& port = "", int timeout = 5000, int maxRetry = 3) override;
    bool disconnect() override;
    std::vector<std::string> scan() override;

    // State
    bool isMoving() const override;

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
    auto findFilterByName(const std::string& name) -> std::optional<int> override;
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

private:
    // Simulation parameters
    bool is_moving_{false};
    int filter_count_{8}; // Default 8-slot filter wheel
    double move_time_per_slot_{0.5}; // seconds per slot
    
    std::thread move_thread_;
    mutable std::mutex move_mutex_;
    
    // Configuration storage
    std::map<std::string, std::vector<FilterInfo>> saved_configurations_;
    
    // Random number generation
    mutable std::random_device rd_;
    mutable std::mt19937 gen_;
    mutable std::uniform_real_distribution<> temp_dist_;
    
    // Simulation methods
    void simulateMove(int target_position);
    void initializeDefaultFilters();
    double generateTemperature() const;
};
