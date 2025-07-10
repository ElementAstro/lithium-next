/*
 * controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Modular ASCOM Switch Controller Implementation

*************************************************/

#include "controller.hpp"

#include <spdlog/spdlog.h>

#include "components/hardware_interface.hpp"
#include "components/switch_manager.hpp"
#include "components/group_manager.hpp"
#include "components/timer_manager.hpp"
#include "components/power_manager.hpp"
#include "components/state_manager.hpp"

namespace lithium::device::ascom::sw {

ASCOMSwitchController::ASCOMSwitchController(const std::string& name)
    : AtomSwitch(name) {
    spdlog::info("ASCOMSwitchController constructor called with name: {}", name);
}

ASCOMSwitchController::~ASCOMSwitchController() {
    spdlog::info("ASCOMSwitchController destructor called");
    destroy();
}

// =========================================================================
// AtomDriver Interface Implementation
// =========================================================================

auto ASCOMSwitchController::initialize() -> bool {
    std::lock_guard<std::mutex> lock(controller_mutex_);
    
    if (initialized_.load()) {
        spdlog::warn("Switch controller already initialized");
        return true;
    }

    spdlog::info("Initializing ASCOM Switch Controller");

    try {
        if (!initializeComponents()) {
            setLastError("Failed to initialize components");
            return false;
        }

        if (!validateConfiguration()) {
            setLastError("Configuration validation failed");
            return false;
        }

        initialized_.store(true);
        spdlog::info("ASCOM Switch Controller initialized successfully");
        return true;

    } catch (const std::exception& e) {
        setLastError("Initialization exception: " + std::string(e.what()));
        spdlog::error("Initialization failed: {}", e.what());
        return false;
    }
}

auto ASCOMSwitchController::destroy() -> bool {
    std::lock_guard<std::mutex> lock(controller_mutex_);
    
    if (!initialized_.load()) {
        return true;
    }

    spdlog::info("Destroying ASCOM Switch Controller");

    try {
        disconnect();
        cleanupComponents();
        initialized_.store(false);
        
        spdlog::info("ASCOM Switch Controller destroyed successfully");
        return true;

    } catch (const std::exception& e) {
        setLastError("Destruction exception: " + std::string(e.what()));
        spdlog::error("Destruction failed: {}", e.what());
        return false;
    }
}

auto ASCOMSwitchController::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    std::lock_guard<std::mutex> lock(controller_mutex_);
    
    if (!initialized_.load()) {
        setLastError("Controller not initialized");
        return false;
    }

    if (connected_.load()) {
        spdlog::warn("Already connected, disconnecting first");
        disconnect();
    }

    spdlog::info("Connecting to ASCOM switch device: {}", deviceName);

    try {
        if (!hardware_interface_->connect(deviceName, timeout, maxRetry)) {
            setLastError("Hardware interface connection failed");
            return false;
        }

        // Notify all components of successful connection
        notifyComponentsOfConnection(true);

        // Synchronize component states
        if (!synchronizeComponentStates()) {
            setLastError("Failed to synchronize component states");
            hardware_interface_->disconnect();
            return false;
        }

        connected_.store(true);
        logOperation("connect", true);
        spdlog::info("Successfully connected to device: {}", deviceName);
        return true;

    } catch (const std::exception& e) {
        setLastError("Connection exception: " + std::string(e.what()));
        spdlog::error("Connection failed: {}", e.what());
        logOperation("connect", false);
        return false;
    }
}

auto ASCOMSwitchController::disconnect() -> bool {
    std::lock_guard<std::mutex> lock(controller_mutex_);
    
    if (!connected_.load()) {
        return true;
    }

    spdlog::info("Disconnecting ASCOM Switch");

    try {
        // Save current state before disconnecting
        if (state_manager_) {
            state_manager_->saveState();
        }

        // Notify components of disconnection
        notifyComponentsOfConnection(false);

        // Disconnect hardware
        if (hardware_interface_) {
            hardware_interface_->disconnect();
        }

        connected_.store(false);
        logOperation("disconnect", true);
        spdlog::info("Successfully disconnected");
        return true;

    } catch (const std::exception& e) {
        setLastError("Disconnection exception: " + std::string(e.what()));
        spdlog::error("Disconnection failed: {}", e.what());
        logOperation("disconnect", false);
        return false;
    }
}

auto ASCOMSwitchController::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM switch devices");

    try {
        if (hardware_interface_) {
            auto devices = hardware_interface_->scan();
            spdlog::info("Found {} ASCOM switch devices", devices.size());
            return devices;
        }
        
        setLastError("Hardware interface not available");
        return {};

    } catch (const std::exception& e) {
        setLastError("Scan exception: " + std::string(e.what()));
        spdlog::error("Scan failed: {}", e.what());
        return {};
    }
}

auto ASCOMSwitchController::isConnected() const -> bool {
    return connected_.load();
}

// =========================================================================
// AtomSwitch Interface Implementation - Switch Management
// =========================================================================

auto ASCOMSwitchController::addSwitch(const SwitchInfo& switchInfo) -> bool {
    if (!isConnected()) {
        setLastError("Not connected to device");
        return false;
    }

    try {
        if (switch_manager_) {
            bool result = switch_manager_->addSwitch(switchInfo);
            logOperation("addSwitch", result);
            return result;
        }
        
        setLastError("Switch manager not available");
        return false;

    } catch (const std::exception& e) {
        setLastError("Add switch exception: " + std::string(e.what()));
        logOperation("addSwitch", false);
        return false;
    }
}

auto ASCOMSwitchController::removeSwitch(uint32_t index) -> bool {
    if (!isConnected()) {
        setLastError("Not connected to device");
        return false;
    }

    try {
        if (switch_manager_) {
            bool result = switch_manager_->removeSwitch(index);
            logOperation("removeSwitch", result);
            return result;
        }
        
        setLastError("Switch manager not available");
        return false;

    } catch (const std::exception& e) {
        setLastError("Remove switch exception: " + std::string(e.what()));
        logOperation("removeSwitch", false);
        return false;
    }
}

auto ASCOMSwitchController::removeSwitch(const std::string& name) -> bool {
    if (!isConnected()) {
        setLastError("Not connected to device");
        return false;
    }

    try {
        if (switch_manager_) {
            bool result = switch_manager_->removeSwitch(name);
            logOperation("removeSwitch", result);
            return result;
        }
        
        setLastError("Switch manager not available");
        return false;

    } catch (const std::exception& e) {
        setLastError("Remove switch exception: " + std::string(e.what()));
        logOperation("removeSwitch", false);
        return false;
    }
}

auto ASCOMSwitchController::getSwitchCount() -> uint32_t {
    try {
        if (switch_manager_) {
            return switch_manager_->getSwitchCount();
        }
        
        setLastError("Switch manager not available");
        return 0;

    } catch (const std::exception& e) {
        setLastError("Get switch count exception: " + std::string(e.what()));
        return 0;
    }
}

auto ASCOMSwitchController::getSwitchInfo(uint32_t index) -> std::optional<SwitchInfo> {
    try {
        if (switch_manager_) {
            return switch_manager_->getSwitchInfo(index);
        }
        
        setLastError("Switch manager not available");
        return std::nullopt;

    } catch (const std::exception& e) {
        setLastError("Get switch info exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::getSwitchInfo(const std::string& name) -> std::optional<SwitchInfo> {
    try {
        if (switch_manager_) {
            return switch_manager_->getSwitchInfo(name);
        }
        
        setLastError("Switch manager not available");
        return std::nullopt;

    } catch (const std::exception& e) {
        setLastError("Get switch info exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::getSwitchIndex(const std::string& name) -> std::optional<uint32_t> {
    try {
        if (switch_manager_) {
            return switch_manager_->getSwitchIndex(name);
        }
        
        setLastError("Switch manager not available");
        return std::nullopt;

    } catch (const std::exception& e) {
        setLastError("Get switch index exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::getAllSwitches() -> std::vector<SwitchInfo> {
    try {
        if (switch_manager_) {
            return switch_manager_->getAllSwitches();
        }
        
        setLastError("Switch manager not available");
        return {};

    } catch (const std::exception& e) {
        setLastError("Get all switches exception: " + std::string(e.what()));
        return {};
    }
}

// =========================================================================
// AtomSwitch Interface Implementation - Switch Control
// =========================================================================

auto ASCOMSwitchController::setSwitchState(uint32_t index, SwitchState state) -> bool {
    if (!isConnected()) {
        setLastError("Not connected to device");
        return false;
    }

    try {
        if (switch_manager_) {
            bool result = switch_manager_->setSwitchState(index, state);
            logOperation("setSwitchState", result);
            return result;
        }
        
        setLastError("Switch manager not available");
        return false;

    } catch (const std::exception& e) {
        setLastError("Set switch state exception: " + std::string(e.what()));
        logOperation("setSwitchState", false);
        return false;
    }
}

auto ASCOMSwitchController::setSwitchState(const std::string& name, SwitchState state) -> bool {
    if (!isConnected()) {
        setLastError("Not connected to device");
        return false;
    }

    try {
        if (switch_manager_) {
            bool result = switch_manager_->setSwitchState(name, state);
            logOperation("setSwitchState", result);
            return result;
        }
        
        setLastError("Switch manager not available");
        return false;

    } catch (const std::exception& e) {
        setLastError("Set switch state exception: " + std::string(e.what()));
        logOperation("setSwitchState", false);
        return false;
    }
}

auto ASCOMSwitchController::getSwitchState(uint32_t index) -> std::optional<SwitchState> {
    try {
        if (switch_manager_) {
            return switch_manager_->getSwitchState(index);
        }
        
        setLastError("Switch manager not available");
        return std::nullopt;

    } catch (const std::exception& e) {
        setLastError("Get switch state exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::getSwitchState(const std::string& name) -> std::optional<SwitchState> {
    try {
        if (switch_manager_) {
            return switch_manager_->getSwitchState(name);
        }
        
        setLastError("Switch manager not available");
        return std::nullopt;

    } catch (const std::exception& e) {
        setLastError("Get switch state exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::toggleSwitch(uint32_t index) -> bool {
    if (!isConnected()) {
        setLastError("Not connected to device");
        return false;
    }

    try {
        if (switch_manager_) {
            bool result = switch_manager_->toggleSwitch(index);
            logOperation("toggleSwitch", result);
            return result;
        }
        
        setLastError("Switch manager not available");
        return false;

    } catch (const std::exception& e) {
        setLastError("Toggle switch exception: " + std::string(e.what()));
        logOperation("toggleSwitch", false);
        return false;
    }
}

auto ASCOMSwitchController::toggleSwitch(const std::string& name) -> bool {
    if (!isConnected()) {
        setLastError("Not connected to device");
        return false;
    }

    try {
        if (switch_manager_) {
            bool result = switch_manager_->toggleSwitch(name);
            logOperation("toggleSwitch", result);
            return result;
        }
        
        setLastError("Switch manager not available");
        return false;

    } catch (const std::exception& e) {
        setLastError("Toggle switch exception: " + std::string(e.what()));
        logOperation("toggleSwitch", false);
        return false;
    }
}

auto ASCOMSwitchController::setAllSwitches(SwitchState state) -> bool {
    if (!isConnected()) {
        setLastError("Not connected to device");
        return false;
    }

    try {
        if (switch_manager_) {
            bool result = switch_manager_->setAllSwitches(state);
            logOperation("setAllSwitches", result);
            return result;
        }
        
        setLastError("Switch manager not available");
        return false;

    } catch (const std::exception& e) {
        setLastError("Set all switches exception: " + std::string(e.what()));
        logOperation("setAllSwitches", false);
        return false;
    }
}

// =========================================================================
// Internal Helper Methods
// =========================================================================

auto ASCOMSwitchController::validateConfiguration() const -> bool {
    // Basic validation logic
    return hardware_interface_ && switch_manager_ && group_manager_ && 
           timer_manager_ && power_manager_ && state_manager_;
}

auto ASCOMSwitchController::initializeComponents() -> bool {
    try {
        // Create hardware interface first
        hardware_interface_ = std::make_shared<components::HardwareInterface>();
        if (!hardware_interface_->initialize()) {
            spdlog::error("Failed to initialize hardware interface");
            return false;
        }

        // Create switch manager
        switch_manager_ = std::make_shared<components::SwitchManager>(hardware_interface_);
        if (!switch_manager_->initialize()) {
            spdlog::error("Failed to initialize switch manager");
            return false;
        }

        // Create group manager
        group_manager_ = std::make_shared<components::GroupManager>(switch_manager_);
        if (!group_manager_->initialize()) {
            spdlog::error("Failed to initialize group manager");
            return false;
        }

        // Create timer manager
        timer_manager_ = std::make_shared<components::TimerManager>(switch_manager_);
        if (!timer_manager_->initialize()) {
            spdlog::error("Failed to initialize timer manager");
            return false;
        }

        // Create power manager
        power_manager_ = std::make_shared<components::PowerManager>(switch_manager_);
        if (!power_manager_->initialize()) {
            spdlog::error("Failed to initialize power manager");
            return false;
        }

        // Create state manager
        state_manager_ = std::make_shared<components::StateManager>(
            switch_manager_, group_manager_, power_manager_);
        if (!state_manager_->initialize()) {
            spdlog::error("Failed to initialize state manager");
            return false;
        }

        spdlog::info("All components initialized successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Component initialization exception: {}", e.what());
        return false;
    }
}

auto ASCOMSwitchController::cleanupComponents() -> void {
    try {
        if (state_manager_) {
            state_manager_->destroy();
            state_manager_.reset();
        }

        if (power_manager_) {
            power_manager_->destroy();
            power_manager_.reset();
        }

        if (timer_manager_) {
            timer_manager_->destroy();
            timer_manager_.reset();
        }

        if (group_manager_) {
            group_manager_->destroy();
            group_manager_.reset();
        }

        if (switch_manager_) {
            switch_manager_->destroy();
            switch_manager_.reset();
        }

        if (hardware_interface_) {
            hardware_interface_->destroy();
            hardware_interface_.reset();
        }

        spdlog::info("All components cleaned up");

    } catch (const std::exception& e) {
        spdlog::error("Component cleanup exception: {}", e.what());
    }
}

auto ASCOMSwitchController::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("Controller error: {}", error);
}

auto ASCOMSwitchController::logOperation(const std::string& operation, bool success) const -> void {
    if (verbose_logging_.load()) {
        if (success) {
            spdlog::debug("Operation '{}' completed successfully", operation);
        } else {
            spdlog::warn("Operation '{}' failed", operation);
        }
    }
}

auto ASCOMSwitchController::notifyComponentsOfConnection(bool connected) -> void {
    // Placeholder for component notification logic
    spdlog::debug("Notifying components of connection state: {}", connected);
}

auto ASCOMSwitchController::synchronizeComponentStates() -> bool {
    try {
        // Synchronize switch states
        if (switch_manager_) {
            switch_manager_->refreshSwitchStates();
        }

        // Load saved state if available
        if (state_manager_) {
            state_manager_->loadState();
        }

        return true;

    } catch (const std::exception& e) {
        spdlog::error("State synchronization failed: {}", e.what());
        return false;
    }
}

// Placeholder implementations for other required methods
auto ASCOMSwitchController::setSwitchStates(const std::vector<std::pair<uint32_t, SwitchState>>& states) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::setSwitchStates(const std::vector<std::pair<std::string, SwitchState>>& states) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::getAllSwitchStates() -> std::vector<std::pair<uint32_t, SwitchState>> {
    // Implementation needed
    return {};
}

auto ASCOMSwitchController::addGroup(const SwitchGroup& group) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::removeGroup(const std::string& name) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::getGroupCount() -> uint32_t {
    // Implementation needed
    return 0;
}

auto ASCOMSwitchController::getGroupInfo(const std::string& name) -> std::optional<SwitchGroup> {
    // Implementation needed
    return std::nullopt;
}

auto ASCOMSwitchController::getAllGroups() -> std::vector<SwitchGroup> {
    // Implementation needed
    return {};
}

auto ASCOMSwitchController::addSwitchToGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::removeSwitchFromGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::setGroupState(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::setGroupAllOff(const std::string& groupName) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::getGroupStates(const std::string& groupName) -> std::vector<std::pair<uint32_t, SwitchState>> {
    // Implementation needed
    return {};
}

auto ASCOMSwitchController::setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::setSwitchTimer(const std::string& name, uint32_t durationMs) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::cancelSwitchTimer(uint32_t index) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::cancelSwitchTimer(const std::string& name) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::getRemainingTime(uint32_t index) -> std::optional<uint32_t> {
    // Implementation needed
    return std::nullopt;
}

auto ASCOMSwitchController::getRemainingTime(const std::string& name) -> std::optional<uint32_t> {
    // Implementation needed
    return std::nullopt;
}

auto ASCOMSwitchController::getTotalPowerConsumption() -> double {
    // Implementation needed
    return 0.0;
}

auto ASCOMSwitchController::getSwitchPowerConsumption(uint32_t index) -> std::optional<double> {
    // Implementation needed
    return std::nullopt;
}

auto ASCOMSwitchController::getSwitchPowerConsumption(const std::string& name) -> std::optional<double> {
    // Implementation needed
    return std::nullopt;
}

auto ASCOMSwitchController::setPowerLimit(double maxWatts) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::getPowerLimit() -> double {
    // Implementation needed
    return 0.0;
}

auto ASCOMSwitchController::saveState() -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::loadState() -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::resetToDefaults() -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::enableSafetyMode(bool enable) -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::isSafetyModeEnabled() -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::setEmergencyStop() -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::clearEmergencyStop() -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::isEmergencyStopActive() -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::getSwitchOperationCount(uint32_t index) -> uint64_t {
    // Implementation needed
    return 0;
}

auto ASCOMSwitchController::getSwitchOperationCount(const std::string& name) -> uint64_t {
    // Implementation needed
    return 0;
}

auto ASCOMSwitchController::getTotalOperationCount() -> uint64_t {
    // Implementation needed
    return 0;
}

auto ASCOMSwitchController::getSwitchUptime(uint32_t index) -> uint64_t {
    // Implementation needed
    return 0;
}

auto ASCOMSwitchController::getSwitchUptime(const std::string& name) -> uint64_t {
    // Implementation needed
    return 0;
}

auto ASCOMSwitchController::resetStatistics() -> bool {
    // Implementation needed
    return false;
}

auto ASCOMSwitchController::getASCOMDriverInfo() -> std::optional<std::string> {
    try {
        if (hardware_interface_) {
            return hardware_interface_->getDriverInfo();
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        setLastError("Get driver info exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::getASCOMVersion() -> std::optional<std::string> {
    try {
        if (hardware_interface_) {
            return hardware_interface_->getDriverVersion();
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        setLastError("Get driver version exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::getASCOMInterfaceVersion() -> std::optional<int> {
    try {
        if (hardware_interface_) {
            return hardware_interface_->getInterfaceVersion();
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        setLastError("Get interface version exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::setASCOMClientID(const std::string &clientId) -> bool {
    try {
        if (hardware_interface_) {
            return hardware_interface_->setClientID(clientId);
        }
        setLastError("Hardware interface not available");
        return false;
    } catch (const std::exception& e) {
        setLastError("Set client ID exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchController::getASCOMClientID() -> std::optional<std::string> {
    try {
        if (hardware_interface_) {
            return hardware_interface_->getClientID();
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        setLastError("Get client ID exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

auto ASCOMSwitchController::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto ASCOMSwitchController::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

auto ASCOMSwitchController::enableVerboseLogging(bool enable) -> void {
    verbose_logging_.store(enable);
    spdlog::info("Verbose logging {}", enable ? "enabled" : "disabled");
}

auto ASCOMSwitchController::isVerboseLoggingEnabled() const -> bool {
    return verbose_logging_.load();
}

// Component access methods for testing
auto ASCOMSwitchController::getHardwareInterface() const -> std::shared_ptr<components::HardwareInterface> {
    return hardware_interface_;
}

auto ASCOMSwitchController::getSwitchManager() const -> std::shared_ptr<components::SwitchManager> {
    return switch_manager_;
}

auto ASCOMSwitchController::getGroupManager() const -> std::shared_ptr<components::GroupManager> {
    return group_manager_;
}

auto ASCOMSwitchController::getTimerManager() const -> std::shared_ptr<components::TimerManager> {
    return timer_manager_;
}

auto ASCOMSwitchController::getPowerManager() const -> std::shared_ptr<components::PowerManager> {
    return power_manager_;
}

auto ASCOMSwitchController::getStateManager() const -> std::shared_ptr<components::StateManager> {
    return state_manager_;
}

} // namespace lithium::device::ascom::sw
