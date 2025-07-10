/*
 * main.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Main Implementation

*************************************************/

#include "main.hpp"

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace lithium::device::ascom::sw {

ASCOMSwitchMain::ASCOMSwitchMain(const SwitchConfig& config) : config_(config) {
    spdlog::info("ASCOMSwitchMain created with device: {}", config_.deviceName);
}

ASCOMSwitchMain::ASCOMSwitchMain() {
    // Default configuration
    SwitchConfig defaultConfig;
    config_ = defaultConfig;
    spdlog::info("ASCOMSwitchMain created with default configuration");
}

ASCOMSwitchMain::~ASCOMSwitchMain() {
    spdlog::info("ASCOMSwitchMain destructor called");
    destroy();
}

// =========================================================================
// Lifecycle Management
// =========================================================================

auto ASCOMSwitchMain::initialize() -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (initialized_.load()) {
        spdlog::warn("Switch main already initialized");
        return true;
    }

    spdlog::info("Initializing ASCOM Switch Main");

    try {
        // Create controller
        controller_ = std::make_shared<ASCOMSwitchController>(config_.deviceName);
        
        if (!controller_->initialize()) {
            setLastError("Failed to initialize controller");
            return false;
        }

        // Apply configuration
        if (!applyConfig(config_)) {
            setLastError("Failed to apply configuration");
            return false;
        }

        initialized_.store(true);
        notifyStatus("ASCOM Switch Main initialized successfully");
        return true;

    } catch (const std::exception& e) {
        setLastError("Initialization exception: " + std::string(e.what()));
        spdlog::error("Initialization failed: {}", e.what());
        return false;
    }
}

auto ASCOMSwitchMain::destroy() -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!initialized_.load()) {
        return true;
    }

    spdlog::info("Destroying ASCOM Switch Main");

    try {
        disconnect();
        
        if (controller_) {
            controller_->destroy();
            controller_.reset();
        }

        initialized_.store(false);
        notifyStatus("ASCOM Switch Main destroyed successfully");
        return true;

    } catch (const std::exception& e) {
        setLastError("Destruction exception: " + std::string(e.what()));
        spdlog::error("Destruction failed: {}", e.what());
        return false;
    }
}

auto ASCOMSwitchMain::isInitialized() const -> bool {
    return initialized_.load();
}

// =========================================================================
// Device Management
// =========================================================================

auto ASCOMSwitchMain::connect(const std::string& deviceName) -> bool {
    if (!initialized_.load()) {
        setLastError("Not initialized");
        return false;
    }

    if (connected_.load()) {
        spdlog::warn("Already connected, disconnecting first");
        disconnect();
    }

    spdlog::info("Connecting to device: {}", deviceName);

    try {
        if (!controller_->connect(deviceName, config_.connectionTimeout, config_.maxRetries)) {
            setLastError("Controller connection failed: " + controller_->getLastError());
            notifyError("Failed to connect to device: " + deviceName);
            return false;
        }

        connected_.store(true);
        notifyStatus("Connected to device: " + deviceName);
        return true;

    } catch (const std::exception& e) {
        setLastError("Connection exception: " + std::string(e.what()));
        spdlog::error("Connection failed: {}", e.what());
        notifyError("Connection exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::disconnect() -> bool {
    if (!connected_.load()) {
        return true;
    }

    spdlog::info("Disconnecting from device");

    try {
        if (controller_) {
            controller_->disconnect();
        }

        connected_.store(false);
        notifyStatus("Disconnected from device");
        return true;

    } catch (const std::exception& e) {
        setLastError("Disconnection exception: " + std::string(e.what()));
        spdlog::error("Disconnection failed: {}", e.what());
        notifyError("Disconnection exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::isConnected() const -> bool {
    return connected_.load();
}

auto ASCOMSwitchMain::scan() -> std::vector<std::string> {
    if (!initialized_.load()) {
        setLastError("Not initialized");
        return {};
    }

    try {
        return controller_->scan();
    } catch (const std::exception& e) {
        setLastError("Scan exception: " + std::string(e.what()));
        spdlog::error("Scan failed: {}", e.what());
        return {};
    }
}

auto ASCOMSwitchMain::getDeviceInfo() -> std::optional<std::string> {
    if (!isConnected()) {
        setLastError("Not connected");
        return std::nullopt;
    }

    try {
        return controller_->getASCOMDriverInfo();
    } catch (const std::exception& e) {
        setLastError("Get device info exception: " + std::string(e.what()));
        return std::nullopt;
    }
}

// =========================================================================
// Configuration Management
// =========================================================================

auto ASCOMSwitchMain::updateConfig(const SwitchConfig& config) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    if (!validateConfig(config)) {
        setLastError("Invalid configuration");
        return false;
    }

    try {
        config_ = config;
        
        if (initialized_.load()) {
            return applyConfig(config_);
        }
        
        return true;

    } catch (const std::exception& e) {
        setLastError("Update config exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::getConfig() const -> SwitchConfig {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

auto ASCOMSwitchMain::saveConfigToFile(const std::string& filename) -> bool {
    try {
        auto jsonStr = configToJson(config_);
        std::ofstream file(filename);
        file << jsonStr;
        return file.good();

    } catch (const std::exception& e) {
        setLastError("Save config exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::loadConfigFromFile(const std::string& filename) -> bool {
    try {
        std::ifstream file(filename);
        std::string jsonStr((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        auto config = jsonToConfig(jsonStr);
        if (!config) {
            setLastError("Failed to parse configuration file");
            return false;
        }

        return updateConfig(*config);

    } catch (const std::exception& e) {
        setLastError("Load config exception: " + std::string(e.what()));
        return false;
    }
}

// =========================================================================
// Controller Access
// =========================================================================

auto ASCOMSwitchMain::getController() -> std::shared_ptr<ASCOMSwitchController> {
    return controller_;
}

auto ASCOMSwitchMain::getController() const -> std::shared_ptr<const ASCOMSwitchController> {
    return controller_;
}

// =========================================================================
// Simplified Switch Operations
// =========================================================================

auto ASCOMSwitchMain::turnOn(uint32_t index) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool result = controller_->setSwitchState(index, SwitchState::ON);
        if (result) {
            // Get switch name for notification
            auto info = controller_->getSwitchInfo(index);
            if (info) {
                notifySwitchChange(info->name, true);
            }
        }
        return result;

    } catch (const std::exception& e) {
        setLastError("Turn on exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::turnOn(const std::string& name) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool result = controller_->setSwitchState(name, SwitchState::ON);
        if (result) {
            notifySwitchChange(name, true);
        }
        return result;

    } catch (const std::exception& e) {
        setLastError("Turn on exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::turnOff(uint32_t index) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool result = controller_->setSwitchState(index, SwitchState::OFF);
        if (result) {
            // Get switch name for notification
            auto info = controller_->getSwitchInfo(index);
            if (info) {
                notifySwitchChange(info->name, false);
            }
        }
        return result;

    } catch (const std::exception& e) {
        setLastError("Turn off exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::turnOff(const std::string& name) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool result = controller_->setSwitchState(name, SwitchState::OFF);
        if (result) {
            notifySwitchChange(name, false);
        }
        return result;

    } catch (const std::exception& e) {
        setLastError("Turn off exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::toggle(uint32_t index) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool result = controller_->toggleSwitch(index);
        if (result) {
            // Get current state for notification
            auto state = controller_->getSwitchState(index);
            auto info = controller_->getSwitchInfo(index);
            if (state && info) {
                notifySwitchChange(info->name, *state == SwitchState::ON);
            }
        }
        return result;

    } catch (const std::exception& e) {
        setLastError("Toggle exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::toggle(const std::string& name) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool result = controller_->toggleSwitch(name);
        if (result) {
            // Get current state for notification
            auto state = controller_->getSwitchState(name);
            if (state) {
                notifySwitchChange(name, *state == SwitchState::ON);
            }
        }
        return result;

    } catch (const std::exception& e) {
        setLastError("Toggle exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::isOn(uint32_t index) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        auto state = controller_->getSwitchState(index);
        return state && (*state == SwitchState::ON);

    } catch (const std::exception& e) {
        setLastError("Is on exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::isOn(const std::string& name) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        auto state = controller_->getSwitchState(name);
        return state && (*state == SwitchState::ON);

    } catch (const std::exception& e) {
        setLastError("Is on exception: " + std::string(e.what()));
        return false;
    }
}

// =========================================================================
// Batch Operations
// =========================================================================

auto ASCOMSwitchMain::turnAllOn() -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool result = controller_->setAllSwitches(SwitchState::ON);
        if (result) {
            notifyStatus("All switches turned on");
        }
        return result;

    } catch (const std::exception& e) {
        setLastError("Turn all on exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::turnAllOff() -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool result = controller_->setAllSwitches(SwitchState::OFF);
        if (result) {
            notifyStatus("All switches turned off");
        }
        return result;

    } catch (const std::exception& e) {
        setLastError("Turn all off exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::getStatus() -> std::vector<std::pair<std::string, bool>> {
    if (!isConnected()) {
        setLastError("Not connected");
        return {};
    }

    try {
        std::vector<std::pair<std::string, bool>> status;
        auto switches = controller_->getAllSwitches();
        
        for (const auto& sw : switches) {
            auto state = controller_->getSwitchState(sw.name);
            bool isOn = state && (*state == SwitchState::ON);
            status.emplace_back(sw.name, isOn);
        }
        
        return status;

    } catch (const std::exception& e) {
        setLastError("Get status exception: " + std::string(e.what()));
        return {};
    }
}

auto ASCOMSwitchMain::setMultiple(const std::vector<std::pair<std::string, bool>>& switches) -> bool {
    if (!isConnected()) {
        setLastError("Not connected");
        return false;
    }

    try {
        bool allSuccess = true;
        
        for (const auto& [name, state] : switches) {
            SwitchState switchState = state ? SwitchState::ON : SwitchState::OFF;
            if (!controller_->setSwitchState(name, switchState)) {
                allSuccess = false;
                spdlog::warn("Failed to set switch '{}' to {}", name, state ? "ON" : "OFF");
            } else {
                notifySwitchChange(name, state);
            }
        }
        
        return allSuccess;

    } catch (const std::exception& e) {
        setLastError("Set multiple exception: " + std::string(e.what()));
        return false;
    }
}

// =========================================================================
// Error Handling and Diagnostics
// =========================================================================

auto ASCOMSwitchMain::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto ASCOMSwitchMain::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

auto ASCOMSwitchMain::performSelfTest() -> bool {
    if (!initialized_.load()) {
        setLastError("Not initialized");
        return false;
    }

    try {
        // Basic self-test logic
        if (!controller_) {
            setLastError("Controller not available");
            return false;
        }

        // Add more comprehensive self-test logic here
        notifyStatus("Self-test completed successfully");
        return true;

    } catch (const std::exception& e) {
        setLastError("Self-test exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::getDiagnosticInfo() -> std::string {
    try {
        json diag;
        diag["initialized"] = initialized_.load();
        diag["connected"] = connected_.load();
        diag["device_name"] = config_.deviceName;
        diag["client_id"] = config_.clientId;
        
        if (controller_) {
            diag["switch_count"] = controller_->getSwitchCount();
            diag["ascom_version"] = controller_->getASCOMVersion().value_or("Unknown");
            diag["driver_info"] = controller_->getASCOMDriverInfo().value_or("Unknown");
        }
        
        return diag.dump(2);

    } catch (const std::exception& e) {
        return "Diagnostic info exception: " + std::string(e.what());
    }
}

// =========================================================================
// Event Callbacks
// =========================================================================

void ASCOMSwitchMain::setStatusCallback(StatusCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    status_callback_ = std::move(callback);
}

void ASCOMSwitchMain::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = std::move(callback);
}

void ASCOMSwitchMain::setSwitchChangeCallback(SwitchChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    switch_change_callback_ = std::move(callback);
}

// =========================================================================
// Factory Methods
// =========================================================================

auto ASCOMSwitchMain::createInstance(const SwitchConfig& config) -> std::unique_ptr<ASCOMSwitchMain> {
    return std::make_unique<ASCOMSwitchMain>(config);
}

auto ASCOMSwitchMain::createInstance() -> std::unique_ptr<ASCOMSwitchMain> {
    return std::make_unique<ASCOMSwitchMain>();
}

auto ASCOMSwitchMain::createShared(const SwitchConfig& config) -> std::shared_ptr<ASCOMSwitchMain> {
    return std::make_shared<ASCOMSwitchMain>(config);
}

auto ASCOMSwitchMain::createShared() -> std::shared_ptr<ASCOMSwitchMain> {
    return std::make_shared<ASCOMSwitchMain>();
}

// =========================================================================
// Internal Methods
// =========================================================================

auto ASCOMSwitchMain::validateConfig(const SwitchConfig& config) -> bool {
    if (config.deviceName.empty()) {
        setLastError("Device name cannot be empty");
        return false;
    }
    
    if (config.connectionTimeout <= 0) {
        setLastError("Connection timeout must be positive");
        return false;
    }
    
    if (config.maxRetries < 0) {
        setLastError("Max retries cannot be negative");
        return false;
    }
    
    return true;
}

auto ASCOMSwitchMain::applyConfig(const SwitchConfig& config) -> bool {
    if (!controller_) {
        return false;
    }

    try {
        // Apply configuration to controller
        controller_->setASCOMClientID(config.clientId);
        controller_->enableVerboseLogging(config.enableVerboseLogging);
        
        // Apply other configuration settings
        // ... additional config application logic
        
        return true;

    } catch (const std::exception& e) {
        setLastError("Apply config exception: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMSwitchMain::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("ASCOMSwitchMain error: {}", error);
}

auto ASCOMSwitchMain::notifyStatus(const std::string& message) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (status_callback_) {
        status_callback_(message);
    }
}

auto ASCOMSwitchMain::notifyError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

auto ASCOMSwitchMain::notifySwitchChange(const std::string& switchName, bool state) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (switch_change_callback_) {
        switch_change_callback_(switchName, state);
    }
}

auto ASCOMSwitchMain::configToJson(const SwitchConfig& config) -> std::string {
    json j;
    j["deviceName"] = config.deviceName;
    j["clientId"] = config.clientId;
    j["connectionTimeout"] = config.connectionTimeout;
    j["maxRetries"] = config.maxRetries;
    j["enableVerboseLogging"] = config.enableVerboseLogging;
    j["enableAutoSave"] = config.enableAutoSave;
    j["autoSaveInterval"] = config.autoSaveInterval;
    j["enablePowerMonitoring"] = config.enablePowerMonitoring;
    j["powerLimit"] = config.powerLimit;
    j["enableSafetyMode"] = config.enableSafetyMode;
    return j.dump(2);
}

auto ASCOMSwitchMain::jsonToConfig(const std::string& jsonStr) -> std::optional<SwitchConfig> {
    try {
        json j = json::parse(jsonStr);
        
        SwitchConfig config;
        config.deviceName = j.value("deviceName", "Default ASCOM Switch");
        config.clientId = j.value("clientId", "Lithium-Next");
        config.connectionTimeout = j.value("connectionTimeout", 5000);
        config.maxRetries = j.value("maxRetries", 3);
        config.enableVerboseLogging = j.value("enableVerboseLogging", false);
        config.enableAutoSave = j.value("enableAutoSave", true);
        config.autoSaveInterval = j.value("autoSaveInterval", 300);
        config.enablePowerMonitoring = j.value("enablePowerMonitoring", true);
        config.powerLimit = j.value("powerLimit", 1000.0);
        config.enableSafetyMode = j.value("enableSafetyMode", true);
        
        return config;

    } catch (const std::exception& e) {
        spdlog::error("JSON to config exception: {}", e.what());
        return std::nullopt;
    }
}

// =========================================================================
// Utility Functions
// =========================================================================

auto discoverASCOMSwitches() -> std::vector<std::string> {
    try {
        // Create temporary controller for discovery
        auto controller = std::make_shared<ASCOMSwitchController>("Discovery");
        if (controller->initialize()) {
            return controller->scan();
        }
        return {};

    } catch (const std::exception& e) {
        spdlog::error("Discover switches exception: {}", e.what());
        return {};
    }
}

auto validateDeviceName(const std::string& deviceName) -> bool {
    return !deviceName.empty() && deviceName.length() < 256;
}

auto getDriverInfo(const std::string& deviceName) -> std::optional<std::string> {
    try {
        auto switchMain = ASCOMSwitchMain::createInstance();
        if (switchMain->initialize() && switchMain->connect(deviceName)) {
            auto info = switchMain->getDeviceInfo();
            switchMain->disconnect();
            return info;
        }
        return std::nullopt;

    } catch (const std::exception& e) {
        spdlog::error("Get driver info exception: {}", e.what());
        return std::nullopt;
    }
}

auto isDeviceAvailable(const std::string& deviceName) -> bool {
    try {
        auto switches = discoverASCOMSwitches();
        return std::find(switches.begin(), switches.end(), deviceName) != switches.end();

    } catch (const std::exception& e) {
        spdlog::error("Is device available exception: {}", e.what());
        return false;
    }
}

} // namespace lithium::device::ascom::sw
