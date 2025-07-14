/*
 * property_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Rotator Property Manager Component

This component manages ASCOM properties, device capabilities,
and configuration settings with caching and validation.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace lithium::device::ascom::rotator::components {

// Forward declaration
class HardwareInterface;

/**
 * @brief ASCOM property value types
 */
using PropertyValue = std::variant<bool, int, double, std::string>;

/**
 * @brief Property metadata
 */
struct PropertyMetadata {
    std::string name;
    std::string description;
    bool readable{true};
    bool writable{false};
    PropertyValue min_value{0};
    PropertyValue max_value{0};
    PropertyValue default_value{0};
    std::chrono::steady_clock::time_point last_updated;
    bool cached{false};
    std::chrono::milliseconds cache_duration{5000}; // 5 seconds default
};

/**
 * @brief Property cache entry
 */
struct PropertyCacheEntry {
    PropertyValue value;
    std::chrono::steady_clock::time_point timestamp;
    bool valid{false};
};

/**
 * @brief Device capabilities structure
 */
struct DeviceCapabilities {
    // Basic capabilities
    bool can_reverse{false};
    bool can_sync{true};
    bool can_abort{true};
    bool can_set_position{true};

    // Movement capabilities
    bool has_variable_speed{false};
    bool has_acceleration_control{false};
    bool supports_homing{false};
    bool supports_presets{false};

    // Hardware features
    bool has_temperature_sensor{false};
    bool has_position_feedback{true};
    bool supports_backlash_compensation{false};

    // Position limits
    double step_size{1.0};
    double min_position{0.0};
    double max_position{360.0};
    double position_tolerance{0.1};

    // Speed limits
    double min_speed{0.1};
    double max_speed{50.0};
    double default_speed{10.0};

    // Interface information
    std::string interface_version{"2"};
    std::string driver_version;
    std::string driver_info;
    std::string device_description;
};

/**
 * @brief Property Manager for ASCOM Rotator
 *
 * This component manages all ASCOM properties, providing caching,
 * validation, and type-safe access to device properties and capabilities.
 */
class PropertyManager {
public:
    explicit PropertyManager(std::shared_ptr<HardwareInterface> hardware);
    ~PropertyManager();

    // Lifecycle management
    auto initialize() -> bool;
    auto destroy() -> bool;

    // Property access
    auto getProperty(const std::string& name) -> std::optional<PropertyValue>;
    auto setProperty(const std::string& name, const PropertyValue& value) -> bool;
    auto hasProperty(const std::string& name) -> bool;
    auto getPropertyMetadata(const std::string& name) -> std::optional<PropertyMetadata>;

    // Typed property access
    auto getBoolProperty(const std::string& name) -> std::optional<bool>;
    auto getIntProperty(const std::string& name) -> std::optional<int>;
    auto getDoubleProperty(const std::string& name) -> std::optional<double>;
    auto getStringProperty(const std::string& name) -> std::optional<std::string>;

    auto setBoolProperty(const std::string& name, bool value) -> bool;
    auto setIntProperty(const std::string& name, int value) -> bool;
    auto setDoubleProperty(const std::string& name, double value) -> bool;
    auto setStringProperty(const std::string& name, const std::string& value) -> bool;

    // Property validation
    auto validateProperty(const std::string& name, const PropertyValue& value) -> bool;
    auto getPropertyConstraints(const std::string& name) -> std::pair<PropertyValue, PropertyValue>; // min, max

    // Cache management
    auto enablePropertyCaching(const std::string& name, std::chrono::milliseconds duration) -> bool;
    auto disablePropertyCaching(const std::string& name) -> bool;
    auto clearPropertyCache(const std::string& name = "") -> void;
    auto refreshProperty(const std::string& name) -> bool;
    auto refreshAllProperties() -> bool;

    // Device capabilities
    auto getDeviceCapabilities() -> DeviceCapabilities;
    auto updateDeviceCapabilities() -> bool;
    auto hasCapability(const std::string& capability) -> bool;

    // Standard ASCOM properties
    auto isConnected() -> bool;
    auto getPosition() -> std::optional<double>;
    auto getMechanicalPosition() -> std::optional<double>;
    auto isMoving() -> bool;
    auto canReverse() -> bool;
    auto isReversed() -> bool;
    auto getStepSize() -> double;
    auto getTemperature() -> std::optional<double>;

    // Property change notifications
    auto setPropertyChangeCallback(const std::string& name,
                                  std::function<void(const PropertyValue&)> callback) -> void;
    auto removePropertyChangeCallback(const std::string& name) -> void;
    auto notifyPropertyChange(const std::string& name, const PropertyValue& value) -> void;

    // Property monitoring
    auto startPropertyMonitoring(const std::vector<std::string>& properties,
                                int interval_ms = 1000) -> bool;
    auto stopPropertyMonitoring() -> bool;
    auto addMonitoredProperty(const std::string& name) -> bool;
    auto removeMonitoredProperty(const std::string& name) -> bool;

    // Configuration and settings
    auto savePropertyConfiguration(const std::string& filename) -> bool;
    auto loadPropertyConfiguration(const std::string& filename) -> bool;
    auto exportPropertyValues() -> std::unordered_map<std::string, PropertyValue>;
    auto importPropertyValues(const std::unordered_map<std::string, PropertyValue>& values) -> bool;

    // Error handling
    auto getLastError() const -> std::string;
    auto clearLastError() -> void;

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // Property registry
    std::unordered_map<std::string, PropertyMetadata> property_registry_;
    std::unordered_map<std::string, PropertyCacheEntry> property_cache_;
    mutable std::shared_mutex property_mutex_;

    // Device capabilities
    DeviceCapabilities capabilities_;
    std::atomic<bool> capabilities_loaded_{false};
    mutable std::mutex capabilities_mutex_;

    // Property change callbacks
    std::unordered_map<std::string, std::function<void(const PropertyValue&)>> property_callbacks_;
    mutable std::mutex callback_mutex_;

    // Property monitoring
    std::vector<std::string> monitored_properties_;
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> monitoring_active_{false};
    int monitor_interval_ms_{1000};
    mutable std::mutex monitor_mutex_;

    // Error handling
    std::string last_error_;
    mutable std::mutex error_mutex_;

    // Helper methods
    auto registerStandardProperties() -> void;
    auto loadPropertyFromHardware(const std::string& name) -> std::optional<PropertyValue>;
    auto savePropertyToHardware(const std::string& name, const PropertyValue& value) -> bool;
    auto parsePropertyValue(const std::string& str_value, PropertyMetadata& metadata) -> PropertyValue;
    auto propertyValueToString(const PropertyValue& value) -> std::string;
    auto isCacheValid(const std::string& name) -> bool;
    auto updatePropertyCache(const std::string& name, const PropertyValue& value) -> void;
    auto setLastError(const std::string& error) -> void;
    auto propertyMonitoringLoop() -> void;
    auto queryDeviceCapabilities() -> bool;
    auto validatePropertyAccess(const std::string& name, bool write_access = false) -> bool;

    // Property conversion helpers
    template<typename T>
    auto getTypedProperty(const std::string& name) -> std::optional<T>;

    template<typename T>
    auto setTypedProperty(const std::string& name, const T& value) -> bool;
};

} // namespace lithium::device::ascom::rotator::components
