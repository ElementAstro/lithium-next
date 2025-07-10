/*
 * main.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Rotator Modular Integration Header

This file provides the main integration points for the modular ASCOM rotator
implementation, including entry points, factory methods, and public API.

*************************************************/

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "controller.hpp"

// Forward declarations
namespace lithium::device::ascom::rotator::components {
    class HardwareInterface;
    enum class ConnectionType;
}

namespace lithium::device::ascom::rotator {

/**
 * @brief Main ASCOM Rotator Integration Class
 * 
 * This class provides the primary integration interface for the modular
 * ASCOM rotator system. It encapsulates the controller and provides
 * simplified access to rotator functionality.
 */
class ASCOMRotatorMain {
public:
    // Configuration structure for rotator initialization
    struct RotatorInitConfig {
        std::string device_name;
        std::string client_id;
        components::ConnectionType connection_type;
        
        // Connection settings
        std::string alpaca_host;
        int alpaca_port;
        int alpaca_device_number;
        std::string com_prog_id;
        
        // Feature flags
        bool enable_monitoring;
        bool enable_presets;
        bool enable_backlash_compensation;
        bool enable_position_limits;
        
        // Performance settings
        int position_update_interval_ms;
        int property_cache_duration_ms;
        int movement_timeout_ms;
        
        // Constructor with default values
        RotatorInitConfig() 
            : device_name("Default ASCOM Rotator")
            , client_id("Lithium-Next")
            , connection_type(components::ConnectionType::ALPACA_REST)
            , alpaca_host("localhost")
            , alpaca_port(11111)
            , alpaca_device_number(0)
            , enable_monitoring(true)
            , enable_presets(true)
            , enable_backlash_compensation(false)
            , enable_position_limits(false)
            , position_update_interval_ms(500)
            , property_cache_duration_ms(5000)
            , movement_timeout_ms(30000)
        {}
    };

    explicit ASCOMRotatorMain(const std::string& name = "ASCOM Rotator");
    ~ASCOMRotatorMain();

    // Factory methods
    static auto createRotator(const std::string& name, 
                             const RotatorInitConfig& config = {}) 
        -> std::shared_ptr<ASCOMRotatorMain>;
    static auto createRotatorWithController(const std::string& name,
                                           std::shared_ptr<ASCOMRotatorController> controller)
        -> std::shared_ptr<ASCOMRotatorMain>;

    // Lifecycle management
    auto initialize(const RotatorInitConfig& config = {}) -> bool;
    auto destroy() -> bool;
    auto isInitialized() const -> bool;

    // Connection management
    auto connect(const std::string& deviceIdentifier) -> bool;
    auto connectWithConfig(const std::string& deviceIdentifier, 
                          const RotatorInitConfig& config) -> bool;
    auto disconnect() -> bool;
    auto reconnect() -> bool;
    auto isConnected() const -> bool;

    // Device discovery
    auto scanDevices() -> std::vector<std::string>;
    auto getAvailableDevices() -> std::map<std::string, std::string>; // name -> description

    // Basic rotator operations
    auto getCurrentPosition() -> std::optional<double>;
    auto moveToAngle(double angle) -> bool;
    auto rotateByAngle(double angle) -> bool;
    auto syncPosition(double angle) -> bool;
    auto abortMove() -> bool;
    auto isMoving() const -> bool;

    // Configuration and settings
    auto setSpeed(double speed) -> bool;
    auto getSpeed() -> std::optional<double>;
    auto setReversed(bool reversed) -> bool;
    auto isReversed() -> bool;
    auto enableBacklashCompensation(bool enable) -> bool;
    auto setBacklashAmount(double amount) -> bool;

    // Preset management (simplified interface)
    auto saveCurrentAsPreset(int slot, const std::string& name = "") -> bool;
    auto moveToPreset(int slot) -> bool;
    auto deletePreset(int slot) -> bool;
    auto getPresetNames() -> std::map<int, std::string>;

    // Status and information
    auto getStatus() -> RotatorStatus;
    auto getLastError() -> std::string;
    auto clearLastError() -> void;
    auto getDeviceInfo() -> std::map<std::string, std::string>;

    // Event handling (simplified)
    auto onPositionChanged(std::function<void(double)> callback) -> void;
    auto onMovementStarted(std::function<void()> callback) -> void;
    auto onMovementCompleted(std::function<void()> callback) -> void;
    auto onError(std::function<void(const std::string&)> callback) -> void;

    // Advanced access
    auto getController() -> std::shared_ptr<ASCOMRotatorController>;
    auto setController(std::shared_ptr<ASCOMRotatorController> controller) -> void;

    // Configuration persistence
    auto saveConfiguration(const std::string& filename) -> bool;
    auto loadConfiguration(const std::string& filename) -> bool;
    auto getDefaultConfigPath() -> std::string;

private:
    std::string name_;
    std::shared_ptr<ASCOMRotatorController> controller_;
    RotatorInitConfig current_config_;
    std::atomic<bool> initialized_{false};
    mutable std::mutex mutex_;

    // Simplified event callbacks
    std::function<void(double)> position_changed_callback_;
    std::function<void()> movement_started_callback_;
    std::function<void()> movement_completed_callback_;
    std::function<void(const std::string&)> error_callback_;

    // Helper methods
    auto setupCallbacks() -> void;
    auto removeCallbacks() -> void;
    auto createDefaultController() -> std::shared_ptr<ASCOMRotatorController>;
    auto validateConfig(const RotatorInitConfig& config) -> bool;
};

/**
 * @brief Global registry for ASCOM Rotator instances
 */
class ASCOMRotatorRegistry {
public:
    static auto getInstance() -> ASCOMRotatorRegistry&;

    auto registerRotator(const std::string& name, 
                        std::shared_ptr<ASCOMRotatorMain> rotator) -> bool;
    auto unregisterRotator(const std::string& name) -> bool;
    auto getRotator(const std::string& name) -> std::shared_ptr<ASCOMRotatorMain>;
    auto getAllRotators() -> std::map<std::string, std::shared_ptr<ASCOMRotatorMain>>;
    auto getRotatorNames() -> std::vector<std::string>;
    auto clear() -> void;

private:
    ASCOMRotatorRegistry() = default;
    std::map<std::string, std::shared_ptr<ASCOMRotatorMain>> rotators_;
    mutable std::shared_mutex registry_mutex_;
};

/**
 * @brief Utility functions for ASCOM Rotator operations
 */
namespace utils {

    /**
     * @brief Create a quick rotator instance with minimal configuration
     */
    auto createQuickRotator(const std::string& device_identifier = "localhost:11111/0") 
        -> std::shared_ptr<ASCOMRotatorMain>;

    /**
     * @brief Auto-discover and connect to the first available rotator
     */
    auto autoConnectRotator() -> std::shared_ptr<ASCOMRotatorMain>;

    /**
     * @brief Convert angle between different coordinate systems
     */
    auto normalizeAngle(double angle) -> double;
    auto angleDifference(double angle1, double angle2) -> double;
    auto shortestRotationPath(double from_angle, double to_angle) -> std::pair<double, bool>; // distance, clockwise

    /**
     * @brief Validate rotator configuration
     */
    auto validateRotatorConfig(const ASCOMRotatorMain::RotatorInitConfig& config) -> bool;

    /**
     * @brief Get default configuration for different connection types
     */
    auto getDefaultAlpacaConfig() -> ASCOMRotatorMain::RotatorInitConfig;
    auto getDefaultCOMConfig(const std::string& prog_id) -> ASCOMRotatorMain::RotatorInitConfig;

    /**
     * @brief Configuration file helpers
     */
    auto getConfigDirectory() -> std::string;
    auto getDefaultConfigFile(const std::string& rotator_name) -> std::string;
    auto ensureConfigDirectory() -> bool;

} // namespace utils

/**
 * @brief Exception classes for ASCOM Rotator operations
 */
class ASCOMRotatorException : public std::runtime_error {
public:
    explicit ASCOMRotatorException(const std::string& message) 
        : std::runtime_error("ASCOM Rotator Error: " + message) {}
};

class ASCOMRotatorConnectionException : public ASCOMRotatorException {
public:
    explicit ASCOMRotatorConnectionException(const std::string& message)
        : ASCOMRotatorException("Connection Error: " + message) {}
};

class ASCOMRotatorMovementException : public ASCOMRotatorException {
public:
    explicit ASCOMRotatorMovementException(const std::string& message)
        : ASCOMRotatorException("Movement Error: " + message) {}
};

class ASCOMRotatorConfigurationException : public ASCOMRotatorException {
public:
    explicit ASCOMRotatorConfigurationException(const std::string& message)
        : ASCOMRotatorException("Configuration Error: " + message) {}
};

} // namespace lithium::device::ascom::rotator
