/*
 * mock_filterwheel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "mock_filterwheel.hpp"

#include <algorithm>

MockFilterWheel::MockFilterWheel(const std::string& name)
    : AtomFilterWheel(name), gen_(rd_()), temp_dist_(15.0, 25.0) {
    // Set default capabilities
    FilterWheelCapabilities caps;
    caps.maxFilters = 8;
    caps.canRename = true;
    caps.hasNames = true;
    caps.hasTemperature = true;
    caps.canAbort = true;
    setFilterWheelCapabilities(caps);

    // Initialize default filters
    initializeDefaultFilters();

    // Initialize state
    current_position_ = 0;
    target_position_ = 0;
}

bool MockFilterWheel::initialize() {
    setState(DeviceState::IDLE);
    updateFilterWheelState(FilterWheelState::IDLE);
    return true;
}

bool MockFilterWheel::destroy() {
    if (is_moving_) {
        abortMotion();
    }
    setState(DeviceState::UNKNOWN);
    return true;
}

bool MockFilterWheel::connect(const std::string& port, int timeout, int maxRetry) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!isSimulated()) {
        return false;
    }

    connected_ = true;
    setState(DeviceState::IDLE);
    updateFilterWheelState(FilterWheelState::IDLE);
    return true;
}

bool MockFilterWheel::disconnect() {
    if (is_moving_) {
        abortMotion();
    }
    connected_ = false;
    setState(DeviceState::UNKNOWN);
    return true;
}

std::vector<std::string> MockFilterWheel::scan() {
    if (isSimulated()) {
        return {"MockFilterWheel_1", "MockFilterWheel_2"};
    }
    return {};
}

bool MockFilterWheel::isMoving() const {
    std::lock_guard<std::mutex> lock(move_mutex_);
    return is_moving_;
}

auto MockFilterWheel::getPosition() -> std::optional<int> {
    if (!isConnected()) return std::nullopt;
    return current_position_;
}

auto MockFilterWheel::setPosition(int position) -> bool {
    if (!isConnected()) return false;
    if (!isValidPosition(position)) return false;
    if (isMoving()) return false;

    target_position_ = position;
    updateFilterWheelState(FilterWheelState::MOVING);

    if (move_thread_.joinable()) {
        move_thread_.join();
    }

    move_thread_ = std::thread(&MockFilterWheel::simulateMove, this, position);
    return true;
}

auto MockFilterWheel::getFilterCount() -> int {
    return filter_count_;
}

auto MockFilterWheel::isValidPosition(int position) -> bool {
    return position >= 0 && position < filter_count_;
}

auto MockFilterWheel::getSlotName(int slot) -> std::optional<std::string> {
    if (!isValidSlot(slot)) return std::nullopt;
    return filters_[slot].name;
}

auto MockFilterWheel::setSlotName(int slot, const std::string& name) -> bool {
    if (!isValidSlot(slot)) return false;

    filters_[slot].name = name;
    return true;
}

auto MockFilterWheel::getAllSlotNames() -> std::vector<std::string> {
    std::vector<std::string> names;
    for (int i = 0; i < filter_count_; ++i) {
        names.push_back(filters_[i].name);
    }
    return names;
}

auto MockFilterWheel::getCurrentFilterName() -> std::string {
    if (isValidSlot(current_position_)) {
        return filters_[current_position_].name;
    }
    return "Unknown";
}

auto MockFilterWheel::getFilterInfo(int slot) -> std::optional<FilterInfo> {
    if (!isValidSlot(slot)) return std::nullopt;
    return filters_[slot];
}

auto MockFilterWheel::setFilterInfo(int slot, const FilterInfo& info) -> bool {
    if (!isValidSlot(slot)) return false;

    filters_[slot] = info;
    return true;
}

auto MockFilterWheel::getAllFilterInfo() -> std::vector<FilterInfo> {
    std::vector<FilterInfo> info;
    for (int i = 0; i < filter_count_; ++i) {
        info.push_back(filters_[i]);
    }
    return info;
}

auto MockFilterWheel::findFilterByName(const std::string& name) -> std::optional<int> {
    for (int i = 0; i < filter_count_; ++i) {
        if (filters_[i].name == name) {
            return i;
        }
    }
    return std::nullopt;
}

auto MockFilterWheel::findFilterByType(const std::string& type) -> std::vector<int> {
    std::vector<int> positions;
    for (int i = 0; i < filter_count_; ++i) {
        if (filters_[i].type == type) {
            positions.push_back(i);
        }
    }
    return positions;
}

auto MockFilterWheel::selectFilterByName(const std::string& name) -> bool {
    auto position = findFilterByName(name);
    if (position) {
        return setPosition(*position);
    }
    return false;
}

auto MockFilterWheel::selectFilterByType(const std::string& type) -> bool {
    auto positions = findFilterByType(type);
    if (!positions.empty()) {
        return setPosition(positions[0]); // Select first match
    }
    return false;
}

auto MockFilterWheel::abortMotion() -> bool {
    if (!isConnected()) return false;

    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_moving_ = false;
    }

    if (move_thread_.joinable()) {
        move_thread_.join();
    }

    updateFilterWheelState(FilterWheelState::IDLE);
    return true;
}

auto MockFilterWheel::homeFilterWheel() -> bool {
    if (!isConnected()) return false;

    // Simulate homing sequence
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return setPosition(0);
}

auto MockFilterWheel::calibrateFilterWheel() -> bool {
    if (!isConnected()) return false;

    // Simulate calibration sequence
    updateFilterWheelState(FilterWheelState::MOVING);

    // Test each filter position
    for (int i = 0; i < filter_count_; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        current_position_ = i;
    }

    current_position_ = 0;
    updateFilterWheelState(FilterWheelState::IDLE);
    return true;
}

auto MockFilterWheel::getTemperature() -> std::optional<double> {
    if (!isConnected()) return std::nullopt;
    if (!filterwheel_capabilities_.hasTemperature) return std::nullopt;

    return generateTemperature();
}

auto MockFilterWheel::hasTemperatureSensor() -> bool {
    return filterwheel_capabilities_.hasTemperature;
}

auto MockFilterWheel::getTotalMoves() -> uint64_t {
    return total_moves_;
}

auto MockFilterWheel::resetTotalMoves() -> bool {
    total_moves_ = 0;
    return true;
}

auto MockFilterWheel::getLastMoveTime() -> int {
    return last_move_time_;
}

auto MockFilterWheel::saveFilterConfiguration(const std::string& name) -> bool {
    if (!isConnected()) return false;

    std::vector<FilterInfo> config;
    for (int i = 0; i < filter_count_; ++i) {
        config.push_back(filters_[i]);
    }

    saved_configurations_[name] = config;
    return true;
}

auto MockFilterWheel::loadFilterConfiguration(const std::string& name) -> bool {
    if (!isConnected()) return false;

    auto it = saved_configurations_.find(name);
    if (it == saved_configurations_.end()) return false;

    const auto& config = it->second;
    for (size_t i = 0; i < config.size() && i < static_cast<size_t>(filter_count_); ++i) {
        filters_[i] = config[i];
    }

    return true;
}

auto MockFilterWheel::deleteFilterConfiguration(const std::string& name) -> bool {
    if (!isConnected()) return false;

    auto it = saved_configurations_.find(name);
    if (it == saved_configurations_.end()) return false;

    saved_configurations_.erase(it);
    return true;
}

auto MockFilterWheel::getAvailableConfigurations() -> std::vector<std::string> {
    std::vector<std::string> configs;
    for (const auto& [name, _] : saved_configurations_) {
        configs.push_back(name);
    }
    return configs;
}

void MockFilterWheel::simulateMove(int target_position) {
    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_moving_ = true;
    }

    auto start_time = std::chrono::steady_clock::now();
    int start_position = current_position_;

    // Calculate the shortest path around the wheel
    int forward_distance = (target_position - current_position_ + filter_count_) % filter_count_;
    int backward_distance = (current_position_ - target_position + filter_count_) % filter_count_;

    int distance = std::min(forward_distance, backward_distance);
    int direction = (forward_distance <= backward_distance) ? 1 : -1;

    // Simulate movement step by step
    for (int i = 0; i < distance; ++i) {
        {
            std::lock_guard<std::mutex> lock(move_mutex_);
            if (!is_moving_) break; // Check for abort
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(move_time_per_slot_ * 1000)));

        current_position_ = (current_position_ + direction + filter_count_) % filter_count_;

        // Notify position change
        if (position_callback_) {
            position_callback_(current_position_, getCurrentFilterName());
        }
    }

    // Ensure we're at the exact target
    current_position_ = target_position;

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Update statistics
    last_move_time_ = duration.count();
    total_moves_++;

    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_moving_ = false;
    }

    updateFilterWheelState(FilterWheelState::IDLE);

    // Notify move complete
    if (move_complete_callback_) {
        move_complete_callback_(true, "Filter change completed successfully");
    }
}

void MockFilterWheel::initializeDefaultFilters() {
    // Initialize with common astronomical filters
    const std::vector<std::tuple<std::string, std::string, double, double, std::string>> default_filters = {
        {"Luminance", "L", 550.0, 200.0, "Clear/Luminance filter"},
        {"Red", "R", 650.0, 100.0, "Red RGB filter"},
        {"Green", "G", 530.0, 100.0, "Green RGB filter"},
        {"Blue", "B", 460.0, 100.0, "Blue RGB filter"},
        {"Hydrogen Alpha", "Ha", 656.3, 7.0, "Hydrogen Alpha narrowband filter"},
        {"Oxygen III", "OIII", 500.7, 8.5, "Oxygen III narrowband filter"},
        {"Sulfur II", "SII", 672.4, 8.0, "Sulfur II narrowband filter"},
        {"Empty", "Empty", 0.0, 0.0, "Empty filter slot"}
    };

    for (size_t i = 0; i < default_filters.size() && i < MAX_FILTERS; ++i) {
        const auto& [name, type, wavelength, bandwidth, description] = default_filters[i];
        filters_[i].name = name;
        filters_[i].type = type;
        filters_[i].wavelength = wavelength;
        filters_[i].bandwidth = bandwidth;
        filters_[i].description = description;
    }

    // Fill remaining slots if any
    for (int i = default_filters.size(); i < filter_count_ && i < MAX_FILTERS; ++i) {
        filters_[i].name = "Filter " + std::to_string(i + 1);
        filters_[i].type = "Unknown";
        filters_[i].wavelength = 0.0;
        filters_[i].bandwidth = 0.0;
        filters_[i].description = "Undefined filter slot";
    }
}

double MockFilterWheel::generateTemperature() const {
    return temp_dist_(gen_);
}
