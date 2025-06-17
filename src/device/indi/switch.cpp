/*
 * switch.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: INDI Switch Client Implementation

*************************************************/

#include "switch.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

INDISwitch::INDISwitch(std::string name) : AtomSwitch(std::move(name)) {
    setSwitchCapabilities(SwitchCapabilities{
        .canToggle = true,
        .canSetAll = false,
        .hasGroups = true,
        .hasStateFeedback = true,
        .canSaveState = false,
        .hasTimer = true,
        .type = SwitchType::RADIO,
        .maxSwitches = 32,
        .maxGroups = 8
    });
}

auto INDISwitch::initialize() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (is_initialized_.load()) {
        logWarning("Switch already initialized");
        return true;
    }
    
    try {
        setServer("localhost", 7624);
        
        // Start timer thread
        timer_thread_running_ = true;
        timer_thread_ = std::thread(&INDISwitch::timerThreadFunction, this);
        
        is_initialized_ = true;
        logInfo("Switch initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to initialize switch: " + std::string(ex.what()));
        return false;
    }
}

auto INDISwitch::destroy() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!is_initialized_.load()) {
        return true;
    }
    
    try {
        // Stop timer thread
        timer_thread_running_ = false;
        if (timer_thread_.joinable()) {
            timer_thread_.join();
        }
        
        if (is_connected_.load()) {
            disconnect();
        }
        
        disconnectServer();
        
        is_initialized_ = false;
        logInfo("Switch destroyed successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to destroy switch: " + std::string(ex.what()));
        return false;
    }
}

auto INDISwitch::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!is_initialized_.load()) {
        logError("Switch not initialized");
        return false;
    }
    
    if (is_connected_.load()) {
        logWarning("Switch already connected");
        return true;
    }
    
    device_name_ = deviceName;
    
    // Connect to INDI server
    if (!connectServer()) {
        logError("Failed to connect to INDI server");
        return false;
    }
    
    // Wait for server connection
    if (!waitForConnection(timeout)) {
        logError("Timeout waiting for server connection");
        disconnectServer();
        return false;
    }
    
    // Wait for device
    for (int retry = 0; retry < maxRetry; ++retry) {
        base_device_ = getDevice(device_name_.c_str());
        if (base_device_.isValid()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    if (!base_device_.isValid()) {
        logError("Device not found: " + device_name_);
        disconnectServer();
        return false;
    }
    
    // Connect device
    base_device_.getDriverExec();
    
    // Wait for connection property and set it to connect
    if (!waitForProperty("CONNECTION", timeout)) {
        logError("Connection property not found");
        disconnectServer();
        return false;
    }
    
    auto connectionProp = base_device_.getProperty("CONNECTION");
    if (!connectionProp.isValid()) {
        logError("Invalid connection property");
        disconnectServer();
        return false;
    }
    
    auto connectSwitch = connectionProp.getSwitch();
    if (!connectSwitch.isValid()) {
        logError("Invalid connection switch");
        disconnectServer();
        return false;
    }
    
    connectSwitch.reset();
    connectSwitch.findWidgetByName("CONNECT")->setState(ISS_ON);
    connectSwitch.findWidgetByName("DISCONNECT")->setState(ISS_OFF);
    sendNewProperty(connectSwitch);
    
    // Wait for connection
    for (int i = 0; i < timeout * 10; ++i) {
        if (base_device_.isConnected()) {
            is_connected_ = true;
            setupPropertyMappings();
            synchronizeWithDevice();
            logInfo("Switch connected successfully: " + device_name_);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    logError("Timeout waiting for device connection");
    disconnectServer();
    return false;
}

auto INDISwitch::disconnect() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!is_connected_.load()) {
        return true;
    }
    
    try {
        if (base_device_.isValid()) {
            auto connectionProp = base_device_.getProperty("CONNECTION");
            if (connectionProp.isValid()) {
                auto connectSwitch = connectionProp.getSwitch();
                if (connectSwitch.isValid()) {
                    connectSwitch.reset();
                    connectSwitch.findWidgetByName("CONNECT")->setState(ISS_OFF);
                    connectSwitch.findWidgetByName("DISCONNECT")->setState(ISS_ON);
                    sendNewProperty(connectSwitch);
                }
            }
        }
        
        disconnectServer();
        is_connected_ = false;
        
        logInfo("Switch disconnected successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to disconnect switch: " + std::string(ex.what()));
        return false;
    }
}

auto INDISwitch::reconnect(int timeout, int maxRetry) -> bool {
    disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return connect(device_name_, timeout, maxRetry);
}

auto INDISwitch::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;
    
    if (!server_connected_.load()) {
        logError("Server not connected for scanning");
        return devices;
    }
    
    auto deviceList = getDevices();
    for (const auto& device : deviceList) {
        if (device.isValid()) {
            devices.emplace_back(device.getDeviceName());
        }
    }
    
    return devices;
}

auto INDISwitch::isConnected() const -> bool {
    return is_connected_.load() && base_device_.isValid() && base_device_.isConnected();
}

auto INDISwitch::watchAdditionalProperty() -> bool {
    // Watch for switch-specific properties
    watchDevice(device_name_.c_str());
    return true;
}

// Switch management implementations
auto INDISwitch::addSwitch(const SwitchInfo& switchInfo) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (switches_.size() >= switch_capabilities_.maxSwitches) {
        logError("Maximum number of switches reached");
        return false;
    }
    
    // Check for duplicate names
    if (switch_name_to_index_.find(switchInfo.name) != switch_name_to_index_.end()) {
        logError("Switch with name '" + switchInfo.name + "' already exists");
        return false;
    }
    
    uint32_t index = static_cast<uint32_t>(switches_.size());
    SwitchInfo newSwitch = switchInfo;
    newSwitch.index = index;
    
    switches_.push_back(newSwitch);
    switch_name_to_index_[switchInfo.name] = index;
    
    // Initialize statistics
    if (switch_operation_counts_.size() <= index) {
        switch_operation_counts_.resize(index + 1, 0);
        switch_on_times_.resize(index + 1);
        switch_uptimes_.resize(index + 1, 0);
    }
    
    logInfo("Added switch: " + switchInfo.name + " at index " + std::to_string(index));
    return true;
}

auto INDISwitch::removeSwitch(uint32_t index) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isValidSwitchIndex(index)) {
        logError("Invalid switch index: " + std::to_string(index));
        return false;
    }
    
    std::string switchName = switches_[index].name;
    
    // Remove from name mapping
    switch_name_to_index_.erase(switchName);
    
    // Remove from switches
    switches_.erase(switches_.begin() + index);
    
    // Update indices in mapping
    for (auto& pair : switch_name_to_index_) {
        if (pair.second > index) {
            pair.second--;
        }
    }
    
    // Update switches indices
    for (size_t i = index; i < switches_.size(); ++i) {
        switches_[i].index = static_cast<uint32_t>(i);
    }
    
    logInfo("Removed switch: " + switchName + " from index " + std::to_string(index));
    return true;
}

auto INDISwitch::removeSwitch(const std::string& name) -> bool {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        logError("Switch not found: " + name);
        return false;
    }
    return removeSwitch(*indexOpt);
}

auto INDISwitch::getSwitchCount() -> uint32_t {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return static_cast<uint32_t>(switches_.size());
}

auto INDISwitch::getSwitchInfo(uint32_t index) -> std::optional<SwitchInfo> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isValidSwitchIndex(index)) {
        return std::nullopt;
    }
    
    return switches_[index];
}

auto INDISwitch::getSwitchInfo(const std::string& name) -> std::optional<SwitchInfo> {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        return std::nullopt;
    }
    return getSwitchInfo(*indexOpt);
}

auto INDISwitch::getSwitchIndex(const std::string& name) -> std::optional<uint32_t> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    auto it = switch_name_to_index_.find(name);
    if (it == switch_name_to_index_.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

auto INDISwitch::getAllSwitches() -> std::vector<SwitchInfo> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return switches_;
}

// Switch control implementations
auto INDISwitch::setSwitchState(uint32_t index, SwitchState state) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isConnected()) {
        logError("Device not connected");
        return false;
    }
    
    if (!isValidSwitchIndex(index)) {
        logError("Invalid switch index: " + std::to_string(index));
        return false;
    }
    
    const auto& switchInfo = switches_[index];
    auto property = findSwitchProperty(switchInfo.name);
    
    if (!property.isValid()) {
        logError("Switch property not found for: " + switchInfo.name);
        return false;
    }
    
    property.reset();
    auto widget = property.findWidgetByName(switchInfo.name.c_str());
    if (!widget) {
        logError("Switch widget not found: " + switchInfo.name);
        return false;
    }
    
    widget->setState(createINDIState(state));
    sendNewProperty(property);
    
    // Update local state
    switches_[index].state = state;
    updateStatistics(index, state);
    notifySwitchStateChange(index, state);
    
    logInfo("Set switch " + switchInfo.name + " to " + (state == SwitchState::ON ? "ON" : "OFF"));
    return true;
}

auto INDISwitch::setSwitchState(const std::string& name, SwitchState state) -> bool {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        logError("Switch not found: " + name);
        return false;
    }
    return setSwitchState(*indexOpt, state);
}

auto INDISwitch::getSwitchState(uint32_t index) -> std::optional<SwitchState> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isValidSwitchIndex(index)) {
        return std::nullopt;
    }
    
    return switches_[index].state;
}

auto INDISwitch::getSwitchState(const std::string& name) -> std::optional<SwitchState> {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        return std::nullopt;
    }
    return getSwitchState(*indexOpt);
}

auto INDISwitch::toggleSwitch(uint32_t index) -> bool {
    auto currentState = getSwitchState(index);
    if (!currentState) {
        return false;
    }
    
    SwitchState newState = (*currentState == SwitchState::ON) ? SwitchState::OFF : SwitchState::ON;
    return setSwitchState(index, newState);
}

auto INDISwitch::toggleSwitch(const std::string& name) -> bool {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        return false;
    }
    return toggleSwitch(*indexOpt);
}

auto INDISwitch::setAllSwitches(SwitchState state) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    bool success = true;
    for (uint32_t i = 0; i < switches_.size(); ++i) {
        if (!setSwitchState(i, state)) {
            success = false;
        }
    }
    
    return success;
}

// Continue implementing remaining methods...
// For brevity, I'll implement the key remaining virtual methods

// Timer functionality implementation
auto INDISwitch::setSwitchTimer(uint32_t index, uint32_t durationMs) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isValidSwitchIndex(index)) {
        return false;
    }
    
    switches_[index].hasTimer = true;
    switches_[index].timerDuration = durationMs;
    switches_[index].timerStart = std::chrono::steady_clock::now();
    
    logInfo("Set timer for switch " + switches_[index].name + ": " + std::to_string(durationMs) + "ms");
    return true;
}

auto INDISwitch::setSwitchTimer(const std::string& name, uint32_t durationMs) -> bool {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) return false;
    return setSwitchTimer(*indexOpt, durationMs);
}

// Power monitoring stub implementations  
auto INDISwitch::getTotalPowerConsumption() -> double {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return total_power_consumption_;
}

// Statistics implementations
auto INDISwitch::getSwitchOperationCount(uint32_t index) -> uint64_t {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    if (index < switch_operation_counts_.size()) {
        return switch_operation_counts_[index];
    }
    return 0;
}

auto INDISwitch::getTotalOperationCount() -> uint64_t {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return total_operation_count_;
}

// INDI BaseClient virtual method implementations
void INDISwitch::newDevice(INDI::BaseDevice baseDevice) {
    logInfo("New device: " + std::string(baseDevice.getDeviceName()));
}

void INDISwitch::removeDevice(INDI::BaseDevice baseDevice) {
    logInfo("Device removed: " + std::string(baseDevice.getDeviceName()));
}

void INDISwitch::newProperty(INDI::Property property) {
    if (property.getType() == INDI_SWITCH) {
        handleSwitchProperty(property.getSwitch());
    }
}

void INDISwitch::updateProperty(INDI::Property property) {
    if (property.getType() == INDI_SWITCH) {
        updateSwitchFromProperty(property.getSwitch());
    }
}

void INDISwitch::removeProperty(INDI::Property property) {
    logInfo("Property removed: " + std::string(property.getName()));
}

void INDISwitch::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    // Handle device messages
}

void INDISwitch::serverConnected() {
    server_connected_ = true;
    logInfo("Server connected");
}

void INDISwitch::serverDisconnected(int exit_code) {
    server_connected_ = false;
    is_connected_ = false;
    logInfo("Server disconnected with code: " + std::to_string(exit_code));
}

// Private helper method implementations
void INDISwitch::timerThreadFunction() {
    while (timer_thread_running_.load()) {
        processTimers();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

auto INDISwitch::findSwitchProperty(const std::string& switchName) -> INDI::PropertySwitch {
    if (!base_device_.isValid()) {
        return INDI::PropertySwitch();
    }
    
    // Try to find property by switch name or mapped property
    auto it = property_mappings_.find(switchName);
    std::string propertyName = (it != property_mappings_.end()) ? it->second : switchName;
    
    auto property = base_device_.getProperty(propertyName.c_str());
    if (property.isValid() && property.getType() == INDI_SWITCH) {
        return property.getSwitch();
    }
    
    return INDI::PropertySwitch();
}

auto INDISwitch::createINDIState(SwitchState state) -> ISState {
    return (state == SwitchState::ON) ? ISS_ON : ISS_OFF;
}

auto INDISwitch::parseINDIState(ISState state) -> SwitchState {
    return (state == ISS_ON) ? SwitchState::ON : SwitchState::OFF;
}

void INDISwitch::updateSwitchFromProperty(const INDI::PropertySwitch& property) {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    // Update switch states from INDI property
    for (int i = 0; i < property.count(); ++i) {
        auto widget = property.at(i);
        std::string switchName = widget->getName();
        
        auto indexOpt = getSwitchIndex(switchName);
        if (indexOpt) {
            SwitchState newState = parseINDIState(widget->getState());
            switches_[*indexOpt].state = newState;
            notifySwitchStateChange(*indexOpt, newState);
        }
    }
}

void INDISwitch::handleSwitchProperty(const INDI::PropertySwitch& property) {
    logInfo("New switch property: " + std::string(property.getName()));
    updateSwitchFromProperty(property);
}

void INDISwitch::setupPropertyMappings() {
    // Setup mapping between switch names and INDI properties
    // This would typically be configured based on the specific device
}

void INDISwitch::synchronizeWithDevice() {
    // Synchronize local switch states with device
    if (!isConnected()) return;
    
    for (const auto& switchInfo : switches_) {
        auto property = findSwitchProperty(switchInfo.name);
        if (property.isValid()) {
            updateSwitchFromProperty(property);
        }
    }
}

auto INDISwitch::waitForConnection(int timeout) -> bool {
    for (int i = 0; i < timeout * 10; ++i) {
        if (server_connected_.load()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

auto INDISwitch::waitForProperty(const std::string& propertyName, int timeout) -> bool {
    for (int i = 0; i < timeout * 10; ++i) {
        if (base_device_.isValid()) {
            auto property = base_device_.getProperty(propertyName.c_str());
            if (property.isValid()) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

void INDISwitch::logInfo(const std::string& message) {
    spdlog::info("[INDISwitch::{}] {}", getName(), message);
}

void INDISwitch::logWarning(const std::string& message) {
    spdlog::warn("[INDISwitch::{}] {}", getName(), message);
}

void INDISwitch::updatePowerConsumption() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    double totalPower = 0.0;
    for (const auto& switchInfo : switches_) {
        if (switchInfo.state == SwitchState::ON) {
            totalPower += switchInfo.powerConsumption;
        }
    }
    
    total_power_consumption_ = totalPower;
    
    // Check power limit
    bool limitExceeded = totalPower > power_limit_;
    
    if (limitExceeded) {
        spdlog::warn("[INDISwitch::{}] Power limit exceeded: {:.2f}W > {:.2f}W", 
            getName(), totalPower, power_limit_);
        
        if (safety_mode_enabled_) {
            spdlog::critical("[INDISwitch::{}] Safety mode: turning OFF all switches due to power limit", getName());
            setAllSwitches(SwitchState::OFF);
        }
    }
    
    notifyPowerEvent(totalPower, limitExceeded);
}

void INDISwitch::updateStatistics(uint32_t index, SwitchState state) {
    if (index >= switch_operation_counts_.size()) {
        switch_operation_counts_.resize(index + 1, 0);
        switch_on_times_.resize(index + 1);
        switch_uptimes_.resize(index + 1, 0);
    }
    
    switch_operation_counts_[index]++;
    total_operation_count_++;
    
    auto now = std::chrono::steady_clock::now();
    
    if (state == SwitchState::ON) {
        switch_on_times_[index] = now;
    } else if (state == SwitchState::OFF) {
        // Add session time to total uptime
        if (index < switch_on_times_.size()) {
            auto sessionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - switch_on_times_[index]).count();
            switch_uptimes_[index] += static_cast<uint64_t>(sessionTime);
        }
    }
}

void INDISwitch::processTimers() {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (uint32_t i = 0; i < switches_.size(); ++i) {
        auto& switchInfo = switches_[i];
        
        if (switchInfo.hasTimer && switchInfo.state == SwitchState::ON) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - switchInfo.timerStart).count();
            
            if (elapsed >= switchInfo.timerDuration) {
                // Timer expired, turn off switch
                switchInfo.state = SwitchState::OFF;
                switchInfo.hasTimer = false;
                
                // Update INDI property if connected
                if (isConnected()) {
                    auto property = findSwitchProperty(switchInfo.name);
                    if (property.isValid()) {
                        property.reset();
                        auto widget = property.findWidgetByName(switchInfo.name.c_str());
                        if (widget) {
                            widget->setState(ISS_OFF);
                            sendNewProperty(property);
                        }
                    }
                }
                
                updateStatistics(i, SwitchState::OFF);
                notifySwitchStateChange(i, SwitchState::OFF);
                notifyTimerEvent(i, true);
                
                spdlog::info("[INDISwitch::{}] Timer expired for switch: {}", getName(), switchInfo.name);
            }
        }
    }
}

// Stub implementations for remaining methods to satisfy interface
auto INDISwitch::setSwitchStates(const std::vector<std::pair<uint32_t, SwitchState>>& states) -> bool {
    bool success = true;
    for (const auto& pair : states) {
        if (!setSwitchState(pair.first, pair.second)) {
            success = false;
        }
    }
    return success;
}

auto INDISwitch::setSwitchStates(const std::vector<std::pair<std::string, SwitchState>>& states) -> bool {
    bool success = true;
    for (const auto& pair : states) {
        if (!setSwitchState(pair.first, pair.second)) {
            success = false;
        }
    }
    return success;
}

auto INDISwitch::getAllSwitchStates() -> std::vector<std::pair<uint32_t, SwitchState>> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    std::vector<std::pair<uint32_t, SwitchState>> states;
    
    for (uint32_t i = 0; i < switches_.size(); ++i) {
        states.emplace_back(i, switches_[i].state);
    }
    
    return states;
}

// Group management implementations
auto INDISwitch::addGroup(const SwitchGroup& group) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (groups_.size() >= switch_capabilities_.maxGroups) {
        spdlog::error("[INDISwitch::{}] Maximum number of groups reached", getName());
        return false;
    }
    
    // Check for duplicate names
    if (group_name_to_index_.find(group.name) != group_name_to_index_.end()) {
        spdlog::error("[INDISwitch::{}] Group with name '{}' already exists", getName(), group.name);
        return false;
    }
    
    uint32_t index = static_cast<uint32_t>(groups_.size());
    SwitchGroup newGroup = group;
    
    groups_.push_back(newGroup);
    group_name_to_index_[group.name] = index;
    
    spdlog::info("[INDISwitch::{}] Added group: {} at index {}", getName(), group.name, index);
    return true;
}

auto INDISwitch::removeGroup(const std::string& name) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    auto it = group_name_to_index_.find(name);
    if (it == group_name_to_index_.end()) {
        spdlog::error("[INDISwitch::{}] Group not found: {}", getName(), name);
        return false;
    }
    
    uint32_t index = it->second;
    
    // Remove from name mapping
    group_name_to_index_.erase(name);
    
    // Remove from groups
    groups_.erase(groups_.begin() + index);
    
    // Update indices in mapping
    for (auto& pair : group_name_to_index_) {
        if (pair.second > index) {
            pair.second--;
        }
    }
    
    spdlog::info("[INDISwitch::{}] Removed group: {} from index {}", getName(), name, index);
    return true;
}

auto INDISwitch::getGroupCount() -> uint32_t {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return static_cast<uint32_t>(groups_.size());
}

auto INDISwitch::getGroupInfo(const std::string& name) -> std::optional<SwitchGroup> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    auto it = group_name_to_index_.find(name);
    if (it == group_name_to_index_.end()) {
        return std::nullopt;
    }
    
    return groups_[it->second];
}

auto INDISwitch::getAllGroups() -> std::vector<SwitchGroup> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return groups_;
}

auto INDISwitch::addSwitchToGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isValidSwitchIndex(switchIndex)) {
        spdlog::error("[INDISwitch::{}] Invalid switch index: {}", getName(), switchIndex);
        return false;
    }
    
    auto it = group_name_to_index_.find(groupName);
    if (it == group_name_to_index_.end()) {
        spdlog::error("[INDISwitch::{}] Group not found: {}", getName(), groupName);
        return false;
    }
    
    uint32_t groupIndex = it->second;
    auto& group = groups_[groupIndex];
    
    // Check if switch is already in group
    if (std::find(group.switchIndices.begin(), group.switchIndices.end(), switchIndex) != group.switchIndices.end()) {
        spdlog::warn("[INDISwitch::{}] Switch {} already in group {}", getName(), switchIndex, groupName);
        return true;
    }
    
    group.switchIndices.push_back(switchIndex);
    switches_[switchIndex].group = groupName;
    
    spdlog::info("[INDISwitch::{}] Added switch {} to group {}", getName(), switchIndex, groupName);
    return true;
}

auto INDISwitch::removeSwitchFromGroup(const std::string& groupName, uint32_t switchIndex) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    auto it = group_name_to_index_.find(groupName);
    if (it == group_name_to_index_.end()) {
        spdlog::error("[INDISwitch::{}] Group not found: {}", getName(), groupName);
        return false;
    }
    
    uint32_t groupIndex = it->second;
    auto& group = groups_[groupIndex];
    
    auto switchIt = std::find(group.switchIndices.begin(), group.switchIndices.end(), switchIndex);
    if (switchIt == group.switchIndices.end()) {
        spdlog::warn("[INDISwitch::{}] Switch {} not found in group {}", getName(), switchIndex, groupName);
        return true;
    }
    
    group.switchIndices.erase(switchIt);
    if (isValidSwitchIndex(switchIndex)) {
        switches_[switchIndex].group.clear();
    }
    
    spdlog::info("[INDISwitch::{}] Removed switch {} from group {}", getName(), switchIndex, groupName);
    return true;
}

auto INDISwitch::setGroupState(const std::string& groupName, uint32_t switchIndex, SwitchState state) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        spdlog::error("[INDISwitch::{}] Group not found: {}", getName(), groupName);
        return false;
    }
    
    // Check if switch is in group
    if (std::find(groupInfo->switchIndices.begin(), groupInfo->switchIndices.end(), switchIndex) == groupInfo->switchIndices.end()) {
        spdlog::error("[INDISwitch::{}] Switch {} not in group {}", getName(), switchIndex, groupName);
        return false;
    }
    
    // Handle exclusive groups
    if (groupInfo->exclusive && state == SwitchState::ON) {
        // Turn off all other switches in the group
        for (uint32_t idx : groupInfo->switchIndices) {
            if (idx != switchIndex) {
                setSwitchState(idx, SwitchState::OFF);
            }
        }
    }
    
    // Set the target switch state
    bool result = setSwitchState(switchIndex, state);
    
    if (result) {
        notifyGroupStateChange(groupName, switchIndex, state);
    }
    
    return result;
}

auto INDISwitch::setGroupAllOff(const std::string& groupName) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        spdlog::error("[INDISwitch::{}] Group not found: {}", getName(), groupName);
        return false;
    }
    
    bool success = true;
    for (uint32_t switchIndex : groupInfo->switchIndices) {
        if (!setSwitchState(switchIndex, SwitchState::OFF)) {
            success = false;
        }
    }
    
    spdlog::info("[INDISwitch::{}] Set all switches OFF in group: {}", getName(), groupName);
    return success;
}

auto INDISwitch::getGroupStates(const std::string& groupName) -> std::vector<std::pair<uint32_t, SwitchState>> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    std::vector<std::pair<uint32_t, SwitchState>> states;
    
    auto groupInfo = getGroupInfo(groupName);
    if (!groupInfo) {
        spdlog::error("[INDISwitch::{}] Group not found: {}", getName(), groupName);
        return states;
    }
    
    for (uint32_t switchIndex : groupInfo->switchIndices) {
        auto state = getSwitchState(switchIndex);
        if (state) {
            states.emplace_back(switchIndex, *state);
        }
    }
    
    return states;
}

// Timer functionality implementations
auto INDISwitch::cancelSwitchTimer(uint32_t index) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isValidSwitchIndex(index)) {
        spdlog::error("[INDISwitch::{}] Invalid switch index: {}", getName(), index);
        return false;
    }
    
    switches_[index].hasTimer = false;
    switches_[index].timerDuration = 0;
    
    spdlog::info("[INDISwitch::{}] Cancelled timer for switch: {}", getName(), switches_[index].name);
    return true;
}

auto INDISwitch::cancelSwitchTimer(const std::string& name) -> bool {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        spdlog::error("[INDISwitch::{}] Switch not found: {}", getName(), name);
        return false;
    }
    return cancelSwitchTimer(*indexOpt);
}

auto INDISwitch::getRemainingTime(uint32_t index) -> std::optional<uint32_t> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isValidSwitchIndex(index)) {
        return std::nullopt;
    }
    
    const auto& switchInfo = switches_[index];
    if (!switchInfo.hasTimer) {
        return std::nullopt;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - switchInfo.timerStart).count();
    
    if (elapsed >= switchInfo.timerDuration) {
        return 0;
    }
    
    return static_cast<uint32_t>(switchInfo.timerDuration - elapsed);
}

auto INDISwitch::getRemainingTime(const std::string& name) -> std::optional<uint32_t> {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        return std::nullopt;
    }
    return getRemainingTime(*indexOpt);
}

// Power monitoring implementations
auto INDISwitch::getSwitchPowerConsumption(uint32_t index) -> std::optional<double> {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (!isValidSwitchIndex(index)) {
        return std::nullopt;
    }
    
    const auto& switchInfo = switches_[index];
    return (switchInfo.state == SwitchState::ON) ? switchInfo.powerConsumption : 0.0;
}

auto INDISwitch::getSwitchPowerConsumption(const std::string& name) -> std::optional<double> {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        return std::nullopt;
    }
    return getSwitchPowerConsumption(*indexOpt);
}

auto INDISwitch::setPowerLimit(double maxWatts) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (maxWatts <= 0.0) {
        spdlog::error("[INDISwitch::{}] Invalid power limit: {}", getName(), maxWatts);
        return false;
    }
    
    power_limit_ = maxWatts;
    spdlog::info("[INDISwitch::{}] Set power limit to: {} watts", getName(), maxWatts);
    
    // Check if current consumption exceeds new limit
    updatePowerConsumption();
    
    return true;
}

auto INDISwitch::getPowerLimit() -> double {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    return power_limit_;
}

// State persistence implementations
auto INDISwitch::saveState() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    try {
        // In a real implementation, this would save to a config file or database
        spdlog::info("[INDISwitch::{}] Saving switch states to persistent storage", getName());
        
        // For now, just log the current state
        for (const auto& switchInfo : switches_) {
            spdlog::debug("[INDISwitch::{}] Switch {}: state={}, power={}", 
                getName(), switchInfo.name, 
                (switchInfo.state == SwitchState::ON ? "ON" : "OFF"),
                switchInfo.powerConsumption);
        }
        
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("[INDISwitch::{}] Failed to save state: {}", getName(), ex.what());
        return false;
    }
}

auto INDISwitch::loadState() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    try {
        // In a real implementation, this would load from a config file or database
        spdlog::info("[INDISwitch::{}] Loading switch states from persistent storage", getName());
        
        // For now, just set all switches to OFF
        for (auto& switchInfo : switches_) {
            switchInfo.state = SwitchState::OFF;
        }
        
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("[INDISwitch::{}] Failed to load state: {}", getName(), ex.what());
        return false;
    }
}

auto INDISwitch::resetToDefaults() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    try {
        // Reset all switches to OFF
        for (auto& switchInfo : switches_) {
            switchInfo.state = SwitchState::OFF;
            switchInfo.hasTimer = false;
            switchInfo.timerDuration = 0;
        }
        
        // Reset power monitoring
        total_power_consumption_ = 0.0;
        power_limit_ = 1000.0;
        
        // Reset safety
        safety_mode_enabled_ = false;
        emergency_stop_active_ = false;
        
        // Reset statistics
        std::fill(switch_operation_counts_.begin(), switch_operation_counts_.end(), 0);
        std::fill(switch_uptimes_.begin(), switch_uptimes_.end(), 0);
        total_operation_count_ = 0;
        
        spdlog::info("[INDISwitch::{}] Reset all switches to defaults", getName());
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("[INDISwitch::{}] Failed to reset to defaults: {}", getName(), ex.what());
        return false;
    }
}

// Safety features implementations
auto INDISwitch::enableSafetyMode(bool enable) -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    safety_mode_enabled_ = enable;
    
    if (enable) {
        spdlog::info("[INDISwitch::{}] Safety mode ENABLED", getName());
        // In safety mode, automatically turn off all switches if power limit exceeded
        updatePowerConsumption();
    } else {
        spdlog::info("[INDISwitch::{}] Safety mode DISABLED", getName());
    }
    
    return true;
}

auto INDISwitch::isSafetyModeEnabled() -> bool {
    return safety_mode_enabled_;
}

auto INDISwitch::setEmergencyStop() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    emergency_stop_active_ = true;
    
    // Turn off all switches immediately
    for (uint32_t i = 0; i < switches_.size(); ++i) {
        setSwitchState(i, SwitchState::OFF);
    }
    
    spdlog::critical("[INDISwitch::{}] EMERGENCY STOP ACTIVATED - All switches turned OFF", getName());
    notifyEmergencyEvent(true);
    
    return true;
}

auto INDISwitch::clearEmergencyStop() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    emergency_stop_active_ = false;
    
    spdlog::info("[INDISwitch::{}] Emergency stop CLEARED", getName());
    notifyEmergencyEvent(false);
    
    return true;
}

auto INDISwitch::isEmergencyStopActive() -> bool {
    return emergency_stop_active_;
}

// Statistics implementations
auto INDISwitch::getSwitchOperationCount(const std::string& name) -> uint64_t {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        return 0;
    }
    return getSwitchOperationCount(*indexOpt);
}

auto INDISwitch::getSwitchUptime(uint32_t index) -> uint64_t {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    if (index >= switch_uptimes_.size()) {
        return 0;
    }
    
    uint64_t uptime = switch_uptimes_[index];
    
    // Add current session time if switch is ON
    if (isValidSwitchIndex(index) && switches_[index].state == SwitchState::ON) {
        auto now = std::chrono::steady_clock::now();
        auto sessionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - switch_on_times_[index]).count();
        uptime += static_cast<uint64_t>(sessionTime);
    }
    
    return uptime;
}

auto INDISwitch::getSwitchUptime(const std::string& name) -> uint64_t {
    auto indexOpt = getSwitchIndex(name);
    if (!indexOpt) {
        return 0;
    }
    return getSwitchUptime(*indexOpt);
}

auto INDISwitch::resetStatistics() -> bool {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    
    try {
        std::fill(switch_operation_counts_.begin(), switch_operation_counts_.end(), 0);
        std::fill(switch_uptimes_.begin(), switch_uptimes_.end(), 0);
        total_operation_count_ = 0;
        
        // Reset on times for currently ON switches
        auto now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < switches_.size() && i < switch_on_times_.size(); ++i) {
            if (switches_[i].state == SwitchState::ON) {
                switch_on_times_[i] = now;
            }
        }
        
        spdlog::info("[INDISwitch::{}] Statistics reset", getName());
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("[INDISwitch::{}] Failed to reset statistics: {}", getName(), ex.what());
        return false;
    }
}
