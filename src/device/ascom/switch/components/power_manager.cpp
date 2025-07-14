/*
 * power_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Power Manager Component Implementation

This component manages power consumption monitoring, power limits,
and power-related safety features for ASCOM switch devices.

*************************************************/

#include "power_manager.hpp"
#include "switch_manager.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace lithium::device::ascom::sw::components {

PowerManager::PowerManager(std::shared_ptr<SwitchManager> switch_manager)
    : switch_manager_(std::move(switch_manager)),
      last_energy_update_(std::chrono::steady_clock::now()) {
    spdlog::debug("PowerManager component created");
}

auto PowerManager::initialize() -> bool {
    spdlog::info("Initializing Power Manager");

    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    // Initialize power data for all switches
    auto switchCount = switch_manager_->getSwitchCount();
    for (uint32_t i = 0; i < switchCount; ++i) {
        if (!ensurePowerDataExists(i)) {
            spdlog::warn("Failed to initialize power data for switch {}", i);
        }
    }

    // Reset energy tracking
    total_energy_consumed_ = 0.0;
    last_energy_update_ = std::chrono::steady_clock::now();

    return true;
}

auto PowerManager::destroy() -> bool {
    spdlog::info("Destroying Power Manager");

    std::lock_guard<std::mutex> data_lock(power_data_mutex_);
    std::lock_guard<std::mutex> history_lock(history_mutex_);
    std::lock_guard<std::mutex> essential_lock(essential_mutex_);

    power_data_.clear();
    power_history_.clear();
    essential_switches_.clear();

    return true;
}

auto PowerManager::reset() -> bool {
    if (!destroy()) {
        return false;
    }
    return initialize();
}

auto PowerManager::getTotalPowerConsumption() -> double {
    if (!monitoring_enabled_.load()) {
        return 0.0;
    }

    updateTotalPowerConsumption();
    return total_power_consumption_.load();
}

auto PowerManager::getSwitchPowerConsumption(uint32_t index) -> std::optional<double> {
    if (!monitoring_enabled_.load()) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(power_data_mutex_);
    auto it = power_data_.find(index);
    if (it == power_data_.end()) {
        return std::nullopt;
    }

    return calculateSwitchPower(index);
}

auto PowerManager::getSwitchPowerConsumption(const std::string& name) -> std::optional<double> {
    auto index = findPowerDataByName(name);
    if (!index) {
        return std::nullopt;
    }
    return getSwitchPowerConsumption(*index);
}

auto PowerManager::updatePowerConsumption() -> bool {
    if (!switch_manager_ || !monitoring_enabled_.load()) {
        return false;
    }

    updateTotalPowerConsumption();
    updateEnergyConsumption();

    double totalPower = total_power_consumption_.load();
    addPowerHistoryEntry(totalPower);
    checkPowerThresholds();

    return true;
}

auto PowerManager::enablePowerMonitoring(bool enable) -> bool {
    monitoring_enabled_ = enable;
    spdlog::debug("Power monitoring {}", enable ? "enabled" : "disabled");
    return true;
}

auto PowerManager::isPowerMonitoringEnabled() -> bool {
    return monitoring_enabled_.load();
}

auto PowerManager::setSwitchPowerData(uint32_t index, double nominalPower, double standbyPower) -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    if (index >= switch_manager_->getSwitchCount()) {
        setLastError("Invalid switch index: " + std::to_string(index));
        return false;
    }

    if (nominalPower < 0.0 || standbyPower < 0.0) {
        setLastError("Power values must be non-negative");
        return false;
    }

    std::lock_guard<std::mutex> lock(power_data_mutex_);

    PowerData& data = power_data_[index];
    data.switch_index = index;
    data.nominal_power = nominalPower;
    data.standby_power = standbyPower;
    data.last_update = std::chrono::steady_clock::now();
    data.monitoring_enabled = true;

    spdlog::debug("Set power data for switch {}: nominal={}W, standby={}W",
                  index, nominalPower, standbyPower);
    return true;
}

auto PowerManager::setSwitchPowerData(const std::string& name, double nominalPower, double standbyPower) -> bool {
    auto index = findPowerDataByName(name);
    if (!index) {
        setLastError("Switch not found: " + name);
        return false;
    }
    return setSwitchPowerData(*index, nominalPower, standbyPower);
}

auto PowerManager::getSwitchPowerData(uint32_t index) -> std::optional<PowerData> {
    std::lock_guard<std::mutex> lock(power_data_mutex_);
    auto it = power_data_.find(index);
    if (it != power_data_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto PowerManager::getSwitchPowerData(const std::string& name) -> std::optional<PowerData> {
    auto index = findPowerDataByName(name);
    if (!index) {
        return std::nullopt;
    }
    return getSwitchPowerData(*index);
}

auto PowerManager::getAllPowerData() -> std::vector<PowerData> {
    std::lock_guard<std::mutex> lock(power_data_mutex_);

    std::vector<PowerData> result;
    for (const auto& [index, data] : power_data_) {
        result.push_back(data);
    }

    return result;
}

auto PowerManager::setPowerLimit(double maxWatts) -> bool {
    if (!validatePowerLimit(maxWatts)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(power_limit_mutex_);
    power_limit_.max_total_power = maxWatts;

    spdlog::debug("Set power limit to {}W", maxWatts);
    return true;
}

auto PowerManager::getPowerLimit() -> double {
    std::lock_guard<std::mutex> lock(power_limit_mutex_);
    return power_limit_.max_total_power;
}

auto PowerManager::setPowerThresholds(double warning, double critical) -> bool {
    if (warning < 0.0 || warning > 1.0 || critical < 0.0 || critical > 1.0) {
        setLastError("Thresholds must be between 0.0 and 1.0");
        return false;
    }

    if (warning >= critical) {
        setLastError("Warning threshold must be less than critical threshold");
        return false;
    }

    std::lock_guard<std::mutex> lock(power_limit_mutex_);
    power_limit_.warning_threshold = warning;
    power_limit_.critical_threshold = critical;

    spdlog::debug("Set power thresholds: warning={}%, critical={}%",
                  warning * 100, critical * 100);
    return true;
}

auto PowerManager::getPowerThresholds() -> std::pair<double, double> {
    std::lock_guard<std::mutex> lock(power_limit_mutex_);
    return {power_limit_.warning_threshold, power_limit_.critical_threshold};
}

auto PowerManager::enablePowerLimits(bool enforce) -> bool {
    std::lock_guard<std::mutex> lock(power_limit_mutex_);
    power_limit_.enforce_limits = enforce;

    spdlog::debug("Power limits enforcement {}", enforce ? "enabled" : "disabled");
    return true;
}

auto PowerManager::arePowerLimitsEnabled() -> bool {
    std::lock_guard<std::mutex> lock(power_limit_mutex_);
    return power_limit_.enforce_limits;
}

auto PowerManager::enableAutoShutdown(bool enable) -> bool {
    std::lock_guard<std::mutex> lock(power_limit_mutex_);
    power_limit_.auto_shutdown = enable;

    spdlog::debug("Auto shutdown {}", enable ? "enabled" : "disabled");
    return true;
}

auto PowerManager::isAutoShutdownEnabled() -> bool {
    std::lock_guard<std::mutex> lock(power_limit_mutex_);
    return power_limit_.auto_shutdown;
}

auto PowerManager::checkPowerLimits() -> bool {
    if (!monitoring_enabled_.load()) {
        return true;
    }

    double totalPower = getTotalPowerConsumption();
    double powerLimit = getPowerLimit();

    return totalPower <= powerLimit;
}

auto PowerManager::isPowerLimitExceeded() -> bool {
    return !checkPowerLimits();
}

auto PowerManager::getPowerUtilization() -> double {
    if (!monitoring_enabled_.load()) {
        return 0.0;
    }

    double totalPower = getTotalPowerConsumption();
    double powerLimit = getPowerLimit();

    if (powerLimit <= 0.0) {
        return 0.0;
    }

    return (totalPower / powerLimit) * 100.0;
}

auto PowerManager::getAvailablePower() -> double {
    if (!monitoring_enabled_.load()) {
        return 0.0;
    }

    double totalPower = getTotalPowerConsumption();
    double powerLimit = getPowerLimit();

    return std::max(0.0, powerLimit - totalPower);
}

auto PowerManager::canSwitchBeActivated(uint32_t index) -> bool {
    if (!switch_manager_ || !monitoring_enabled_.load()) {
        return true; // Allow if monitoring is disabled
    }

    // Check if switch is already on
    auto state = switch_manager_->getSwitchState(index);
    if (state && *state == SwitchState::ON) {
        return true; // Already on
    }

    // Get switch power requirements
    auto powerData = getSwitchPowerData(index);
    if (!powerData) {
        return true; // No power data, allow by default
    }

    double requiredPower = powerData->nominal_power - powerData->standby_power;
    double availablePower = getAvailablePower();

    bool canActivate = requiredPower <= availablePower;

    if (!canActivate) {
        spdlog::debug("Cannot activate switch {}: requires {}W, available {}W",
                      index, requiredPower, availablePower);
    }

    return canActivate;
}

auto PowerManager::canSwitchBeActivated(const std::string& name) -> bool {
    auto index = findPowerDataByName(name);
    if (!index) {
        return true; // No power data, allow by default
    }
    return canSwitchBeActivated(*index);
}

auto PowerManager::getTotalEnergyConsumed() -> double {
    updateEnergyConsumption();
    return total_energy_consumed_.load();
}

auto PowerManager::getSwitchEnergyConsumed(uint32_t index) -> std::optional<double> {
    // For now, return proportional energy based on power consumption
    // In a real implementation, this would track per-switch energy consumption
    auto powerData = getSwitchPowerData(index);
    if (!powerData) {
        return std::nullopt;
    }

    double totalEnergy = getTotalEnergyConsumed();
    double totalPower = getTotalPowerConsumption();

    if (totalPower <= 0.0) {
        return 0.0;
    }

    double switchPower = calculateSwitchPower(index);
    return (switchPower / totalPower) * totalEnergy;
}

auto PowerManager::getSwitchEnergyConsumed(const std::string& name) -> std::optional<double> {
    auto index = findPowerDataByName(name);
    if (!index) {
        return std::nullopt;
    }
    return getSwitchEnergyConsumed(*index);
}

auto PowerManager::resetEnergyCounters() -> bool {
    total_energy_consumed_ = 0.0;
    last_energy_update_ = std::chrono::steady_clock::now();

    spdlog::debug("Energy counters reset");
    return true;
}

auto PowerManager::getPowerHistory(uint32_t samples) -> std::vector<std::pair<std::chrono::steady_clock::time_point, double>> {
    std::lock_guard<std::mutex> lock(history_mutex_);

    size_t count = std::min(static_cast<size_t>(samples), power_history_.size());
    std::vector<std::pair<std::chrono::steady_clock::time_point, double>> result;

    if (count > 0) {
        auto start_it = power_history_.end() - count;
        result.assign(start_it, power_history_.end());
    }

    return result;
}

auto PowerManager::emergencyPowerOff() -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    spdlog::warn("Emergency power off initiated");

    bool success = true;
    auto switchCount = switch_manager_->getSwitchCount();

    for (uint32_t i = 0; i < switchCount; ++i) {
        if (!isSwitchEssential(i)) {
            if (!switch_manager_->setSwitchState(i, SwitchState::OFF)) {
                spdlog::error("Failed to turn off switch {} during emergency power off", i);
                success = false;
            }
        }
    }

    executeEmergencyShutdown("Emergency power off executed");
    return success;
}

auto PowerManager::powerOffNonEssentialSwitches() -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    spdlog::info("Powering off non-essential switches");

    bool success = true;
    auto switchCount = switch_manager_->getSwitchCount();

    for (uint32_t i = 0; i < switchCount; ++i) {
        if (!isSwitchEssential(i)) {
            auto state = switch_manager_->getSwitchState(i);
            if (state && *state == SwitchState::ON) {
                if (!switch_manager_->setSwitchState(i, SwitchState::OFF)) {
                    spdlog::error("Failed to turn off non-essential switch {}", i);
                    success = false;
                }
            }
        }
    }

    return success;
}

auto PowerManager::markSwitchAsEssential(uint32_t index, bool essential) -> bool {
    if (!switch_manager_) {
        setLastError("Switch manager not available");
        return false;
    }

    if (index >= switch_manager_->getSwitchCount()) {
        setLastError("Invalid switch index: " + std::to_string(index));
        return false;
    }

    std::lock_guard<std::mutex> lock(essential_mutex_);
    essential_switches_[index] = essential;

    spdlog::debug("Switch {} marked as {}", index, essential ? "essential" : "non-essential");
    return true;
}

auto PowerManager::markSwitchAsEssential(const std::string& name, bool essential) -> bool {
    auto index = findPowerDataByName(name);
    if (!index) {
        setLastError("Switch not found: " + name);
        return false;
    }
    return markSwitchAsEssential(*index, essential);
}

auto PowerManager::isSwitchEssential(uint32_t index) -> bool {
    std::lock_guard<std::mutex> lock(essential_mutex_);
    auto it = essential_switches_.find(index);
    return (it != essential_switches_.end()) ? it->second : false;
}

auto PowerManager::isSwitchEssential(const std::string& name) -> bool {
    auto index = findPowerDataByName(name);
    if (!index) {
        return false;
    }
    return isSwitchEssential(*index);
}

void PowerManager::setPowerLimitCallback(PowerLimitCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    power_limit_callback_ = std::move(callback);
}

void PowerManager::setPowerWarningCallback(PowerWarningCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    power_warning_callback_ = std::move(callback);
}

void PowerManager::setEmergencyShutdownCallback(EmergencyShutdownCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    emergency_shutdown_callback_ = std::move(callback);
}

auto PowerManager::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto PowerManager::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// =========================================================================
// Internal Methods
// =========================================================================

auto PowerManager::calculateSwitchPower(uint32_t index) -> double {
    auto it = power_data_.find(index);
    if (it == power_data_.end()) {
        return 0.0;
    }

    const PowerData& data = it->second;
    if (!data.monitoring_enabled || !switch_manager_) {
        return data.standby_power;
    }

    auto state = switch_manager_->getSwitchState(index);
    if (!state) {
        return data.standby_power;
    }

    return (*state == SwitchState::ON) ? data.nominal_power : data.standby_power;
}

auto PowerManager::updateTotalPowerConsumption() -> void {
    std::lock_guard<std::mutex> lock(power_data_mutex_);

    double totalPower = 0.0;
    for (const auto& [index, data] : power_data_) {
        totalPower += calculateSwitchPower(index);
    }

    total_power_consumption_ = totalPower;
}

auto PowerManager::updateEnergyConsumption() -> void {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_energy_update_);

    if (elapsed.count() > 0) {
        double hours = elapsed.count() / (1000.0 * 3600.0); // Convert ms to hours
        double currentPower = total_power_consumption_.load();
        double energy = currentPower * hours / 1000.0; // Convert W·h to kWh

        total_energy_consumed_ += energy;
        last_energy_update_ = now;
    }
}

auto PowerManager::addPowerHistoryEntry(double power) -> void {
    std::lock_guard<std::mutex> lock(history_mutex_);

    power_history_.emplace_back(std::chrono::steady_clock::now(), power);

    // Keep history size manageable
    if (power_history_.size() > MAX_HISTORY_SIZE) {
        power_history_.erase(power_history_.begin(),
                           power_history_.begin() + (power_history_.size() - MAX_HISTORY_SIZE));
    }
}

auto PowerManager::validatePowerData(const PowerData& data) -> bool {
    if (data.nominal_power < 0.0 || data.standby_power < 0.0) {
        setLastError("Power values must be non-negative");
        return false;
    }

    if (data.standby_power > data.nominal_power) {
        setLastError("Standby power cannot exceed nominal power");
        return false;
    }

    return true;
}

auto PowerManager::validatePowerLimit(double limit) -> bool {
    if (limit <= 0.0) {
        setLastError("Power limit must be positive");
        return false;
    }

    return true;
}

auto PowerManager::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("PowerManager Error: {}", error);
}

auto PowerManager::checkPowerThresholds() -> void {
    if (!monitoring_enabled_.load()) {
        return;
    }

    double totalPower = total_power_consumption_.load();
    double powerLimit = 0.0;
    double warningThreshold = 0.0;
    double criticalThreshold = 0.0;

    {
        std::lock_guard<std::mutex> lock(power_limit_mutex_);
        if (!power_limit_.enforce_limits) {
            return;
        }

        powerLimit = power_limit_.max_total_power;
        warningThreshold = powerLimit * power_limit_.warning_threshold;
        criticalThreshold = powerLimit * power_limit_.critical_threshold;
    }

    if (totalPower >= criticalThreshold) {
        executePowerLimitActions();
    } else if (totalPower >= warningThreshold) {
        notifyPowerWarning(totalPower, warningThreshold);
    }
}

auto PowerManager::executePowerLimitActions() -> void {
    double totalPower = total_power_consumption_.load();
    double powerLimit = getPowerLimit();

    notifyPowerLimitExceeded(totalPower, powerLimit);

    if (isAutoShutdownEnabled()) {
        spdlog::warn("Power limit exceeded ({}W > {}W), executing auto shutdown",
                     totalPower, powerLimit);
        powerOffNonEssentialSwitches();
        executeEmergencyShutdown("Auto shutdown due to power limit exceeded");
    } else {
        spdlog::warn("Power limit exceeded ({}W > {}W), but auto shutdown is disabled",
                     totalPower, powerLimit);
    }
}

auto PowerManager::executeEmergencyShutdown(const std::string& reason) -> void {
    spdlog::critical("Emergency shutdown: {}", reason);
    notifyEmergencyShutdown(reason);
}

auto PowerManager::notifyPowerLimitExceeded(double currentPower, double limit) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (power_limit_callback_) {
        power_limit_callback_(currentPower, limit, true);
    }
}

auto PowerManager::notifyPowerWarning(double currentPower, double threshold) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (power_warning_callback_) {
        power_warning_callback_(currentPower, threshold);
    }
}

auto PowerManager::notifyEmergencyShutdown(const std::string& reason) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (emergency_shutdown_callback_) {
        emergency_shutdown_callback_(reason);
    }
}

auto PowerManager::findPowerDataByName(const std::string& name) -> std::optional<uint32_t> {
    if (!switch_manager_) {
        return std::nullopt;
    }

    return switch_manager_->getSwitchIndex(name);
}

auto PowerManager::ensurePowerDataExists(uint32_t index) -> bool {
    std::lock_guard<std::mutex> lock(power_data_mutex_);

    if (power_data_.find(index) == power_data_.end()) {
        PowerData data;
        data.switch_index = index;
        data.nominal_power = 0.0;
        data.standby_power = 0.0;
        data.current_power = 0.0;
        data.last_update = std::chrono::steady_clock::now();
        data.monitoring_enabled = true;

        power_data_[index] = data;
    }

    return true;
}

} // namespace lithium::device::ascom::sw::components
