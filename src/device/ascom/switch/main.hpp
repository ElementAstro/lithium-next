/*
 * main.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Switch Modular Integration Header

This file provides the main integration points for the modular ASCOM switch
implementation, including entry points, factory methods, and public API.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <optional>
#include <atomic>

#include "controller.hpp"

namespace lithium::device::ascom::sw {

/**
 * @brief Main ASCOM Switch Integration Class
 * 
 * This class provides the primary integration interface for the modular
 * ASCOM switch system. It encapsulates the controller and provides
 * simplified access to switch functionality.
 */
class ASCOMSwitchMain {
public:
    // Configuration structure for switch initialization
    struct SwitchConfig {
        std::string deviceName = "Default ASCOM Switch";
        std::string clientId = "Lithium-Next";
        int connectionTimeout = 5000;
        int maxRetries = 3;
        bool enableVerboseLogging = false;
        bool enableAutoSave = true;
        uint32_t autoSaveInterval = 300; // seconds
        bool enablePowerMonitoring = true;
        double powerLimit = 1000.0; // watts
        bool enableSafetyMode = true;
    };

    explicit ASCOMSwitchMain(const SwitchConfig& config);
    explicit ASCOMSwitchMain();
    ~ASCOMSwitchMain();

    // Non-copyable and non-movable
    ASCOMSwitchMain(const ASCOMSwitchMain&) = delete;
    ASCOMSwitchMain& operator=(const ASCOMSwitchMain&) = delete;
    ASCOMSwitchMain(ASCOMSwitchMain&&) = delete;
    ASCOMSwitchMain& operator=(ASCOMSwitchMain&&) = delete;

    // =========================================================================
    // Lifecycle Management
    // =========================================================================

    auto initialize() -> bool;
    auto destroy() -> bool;
    auto isInitialized() const -> bool;

    // =========================================================================
    // Device Management
    // =========================================================================

    auto connect(const std::string& deviceName) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;
    auto scan() -> std::vector<std::string>;
    auto getDeviceInfo() -> std::optional<std::string>;

    // =========================================================================
    // Configuration Management
    // =========================================================================

    auto updateConfig(const SwitchConfig& config) -> bool;
    auto getConfig() const -> SwitchConfig;
    auto saveConfigToFile(const std::string& filename) -> bool;
    auto loadConfigFromFile(const std::string& filename) -> bool;

    // =========================================================================
    // Controller Access
    // =========================================================================

    auto getController() -> std::shared_ptr<ASCOMSwitchController>;
    auto getController() const -> std::shared_ptr<const ASCOMSwitchController>;

    // =========================================================================
    // Simplified Switch Operations
    // =========================================================================

    auto turnOn(uint32_t index) -> bool;
    auto turnOn(const std::string& name) -> bool;
    auto turnOff(uint32_t index) -> bool;
    auto turnOff(const std::string& name) -> bool;
    auto toggle(uint32_t index) -> bool;
    auto toggle(const std::string& name) -> bool;
    auto isOn(uint32_t index) -> bool;
    auto isOn(const std::string& name) -> bool;

    // =========================================================================
    // Batch Operations
    // =========================================================================

    auto turnAllOn() -> bool;
    auto turnAllOff() -> bool;
    auto getStatus() -> std::vector<std::pair<std::string, bool>>;
    auto setMultiple(const std::vector<std::pair<std::string, bool>>& switches) -> bool;

    // =========================================================================
    // Error Handling and Diagnostics
    // =========================================================================

    auto getLastError() const -> std::string;
    auto clearLastError() -> void;
    auto performSelfTest() -> bool;
    auto getDiagnosticInfo() -> std::string;

    // =========================================================================
    // Event Callbacks
    // =========================================================================

    using StatusCallback = std::function<void(const std::string& message)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    using SwitchChangeCallback = std::function<void(const std::string& switchName, bool state)>;

    void setStatusCallback(StatusCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setSwitchChangeCallback(SwitchChangeCallback callback);

    // =========================================================================
    // Factory Methods
    // =========================================================================

    static auto createInstance(const SwitchConfig& config) -> std::unique_ptr<ASCOMSwitchMain>;
    static auto createInstance() -> std::unique_ptr<ASCOMSwitchMain>;
    static auto createShared(const SwitchConfig& config) -> std::shared_ptr<ASCOMSwitchMain>;
    static auto createShared() -> std::shared_ptr<ASCOMSwitchMain>;

private:
    // Configuration
    SwitchConfig config_;
    mutable std::mutex config_mutex_;

    // Controller instance
    std::shared_ptr<ASCOMSwitchController> controller_;

    // State tracking
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};

    // Error handling
    mutable std::string last_error_;
    mutable std::mutex error_mutex_;

    // Callbacks
    StatusCallback status_callback_;
    ErrorCallback error_callback_;
    SwitchChangeCallback switch_change_callback_;
    std::mutex callback_mutex_;

    // =========================================================================
    // Internal Methods
    // =========================================================================

    auto validateConfig(const SwitchConfig& config) -> bool;
    auto applyConfig(const SwitchConfig& config) -> bool;
    auto setLastError(const std::string& error) const -> void;
    auto notifyStatus(const std::string& message) -> void;
    auto notifyError(const std::string& error) -> void;
    auto notifySwitchChange(const std::string& switchName, bool state) -> void;

    // Configuration helpers
    auto configToJson(const SwitchConfig& config) -> std::string;
    auto jsonToConfig(const std::string& json) -> std::optional<SwitchConfig>;
};

// =========================================================================
// Utility Functions
// =========================================================================

/**
 * @brief Discover available ASCOM switch devices
 */
auto discoverASCOMSwitches() -> std::vector<std::string>;

/**
 * @brief Validate ASCOM switch device name
 */
auto validateDeviceName(const std::string& deviceName) -> bool;

/**
 * @brief Get ASCOM switch driver information
 */
auto getDriverInfo(const std::string& deviceName) -> std::optional<std::string>;

/**
 * @brief Check if ASCOM switch device is available
 */
auto isDeviceAvailable(const std::string& deviceName) -> bool;

// =========================================================================
// Exception Classes
// =========================================================================

class ASCOMSwitchMainException : public std::runtime_error {
public:
    explicit ASCOMSwitchMainException(const std::string& message) : std::runtime_error(message) {}
};

class ConfigurationException : public ASCOMSwitchMainException {
public:
    explicit ConfigurationException(const std::string& message) 
        : ASCOMSwitchMainException("Configuration error: " + message) {}
};

class InitializationException : public ASCOMSwitchMainException {
public:
    explicit InitializationException(const std::string& message) 
        : ASCOMSwitchMainException("Initialization error: " + message) {}
};

} // namespace lithium::device::ascom::sw
