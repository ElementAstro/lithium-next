/*
 * property_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Rotator Property Manager Component Implementation

*************************************************/

#include "property_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <thread>

namespace lithium::device::ascom::rotator::components {

PropertyManager::PropertyManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware) {
    spdlog::debug("PropertyManager constructor called");
}

PropertyManager::~PropertyManager() {
    spdlog::debug("PropertyManager destructor called");
    destroy();
}

auto PropertyManager::initialize() -> bool {
    spdlog::info("Initializing Property Manager");

    if (!hardware_) {
        setLastError("Hardware interface not available");
        return false;
    }

    clearLastError();

    // Register standard ASCOM rotator properties
    registerStandardProperties();

    spdlog::info("Property Manager initialized successfully");
    return true;
}

auto PropertyManager::destroy() -> bool {
    spdlog::info("Destroying Property Manager");

    stopPropertyMonitoring();
    clearPropertyCache();

    return true;
}

auto PropertyManager::getProperty(const std::string& name) -> std::optional<PropertyValue> {
    if (!validatePropertyAccess(name, false)) {
        return std::nullopt;
    }

    // Check cache first
    if (isCacheValid(name)) {
        std::shared_lock<std::shared_mutex> lock(property_mutex_);
        auto it = property_cache_.find(name);
        if (it != property_cache_.end() && it->second.valid) {
            return it->second.value;
        }
    }

    // Load from hardware
    auto value = loadPropertyFromHardware(name);
    if (value) {
        updatePropertyCache(name, *value);
    }

    return value;
}

auto PropertyManager::setProperty(const std::string& name, const PropertyValue& value) -> bool {
    if (!validatePropertyAccess(name, true)) {
        return false;
    }

    // Validate the value
    if (!validateProperty(name, value)) {
        setLastError("Invalid property value for: " + name);
        return false;
    }

    // Save to hardware
    if (!savePropertyToHardware(name, value)) {
        return false;
    }

    // Update cache
    updatePropertyCache(name, value);

    // Notify callbacks
    notifyPropertyChange(name, value);

    return true;
}

auto PropertyManager::hasProperty(const std::string& name) -> bool {
    std::shared_lock<std::shared_mutex> lock(property_mutex_);
    return property_registry_.find(name) != property_registry_.end();
}

auto PropertyManager::getPropertyMetadata(const std::string& name) -> std::optional<PropertyMetadata> {
    std::shared_lock<std::shared_mutex> lock(property_mutex_);
    auto it = property_registry_.find(name);
    if (it != property_registry_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto PropertyManager::getBoolProperty(const std::string& name) -> std::optional<bool> {
    auto value = getProperty(name);
    if (value && std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value);
    }
    return std::nullopt;
}

auto PropertyManager::getDoubleProperty(const std::string& name) -> std::optional<double> {
    auto value = getProperty(name);
    if (value && std::holds_alternative<double>(*value)) {
        return std::get<double>(*value);
    }
    return std::nullopt;
}

auto PropertyManager::getStringProperty(const std::string& name) -> std::optional<std::string> {
    auto value = getProperty(name);
    if (value && std::holds_alternative<std::string>(*value)) {
        return std::get<std::string>(*value);
    }
    return std::nullopt;
}

auto PropertyManager::setBoolProperty(const std::string& name, bool value) -> bool {
    return setProperty(name, PropertyValue{value});
}

auto PropertyManager::setDoubleProperty(const std::string& name, double value) -> bool {
    return setProperty(name, PropertyValue{value});
}

auto PropertyManager::setStringProperty(const std::string& name, const std::string& value) -> bool {
    return setProperty(name, PropertyValue{value});
}

auto PropertyManager::validateProperty(const std::string& name, const PropertyValue& value) -> bool {
    auto metadata = getPropertyMetadata(name);
    if (!metadata) {
        return false;
    }

    // Check if property is writable
    if (!metadata->writable) {
        setLastError("Property is read-only: " + name);
        return false;
    }

    // Type validation happens implicitly through variant
    // Additional range validation could be added here

    return true;
}

auto PropertyManager::enablePropertyCaching(const std::string& name, std::chrono::milliseconds duration) -> bool {
    std::unique_lock<std::shared_mutex> lock(property_mutex_);
    auto it = property_registry_.find(name);
    if (it != property_registry_.end()) {
        it->second.cached = true;
        it->second.cache_duration = duration;
        return true;
    }
    return false;
}

auto PropertyManager::disablePropertyCaching(const std::string& name) -> bool {
    std::unique_lock<std::shared_mutex> lock(property_mutex_);
    auto it = property_registry_.find(name);
    if (it != property_registry_.end()) {
        it->second.cached = false;
        // Remove from cache
        property_cache_.erase(name);
        return true;
    }
    return false;
}

auto PropertyManager::clearPropertyCache(const std::string& name) -> void {
    std::unique_lock<std::shared_mutex> lock(property_mutex_);
    if (name.empty()) {
        property_cache_.clear();
    } else {
        property_cache_.erase(name);
    }
}

auto PropertyManager::updateDeviceCapabilities() -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(capabilities_mutex_);

    bool success = queryDeviceCapabilities();
    if (success) {
        capabilities_loaded_.store(true);
    }

    return success;
}

auto PropertyManager::getDeviceCapabilities() -> DeviceCapabilities {
    if (!capabilities_loaded_.load()) {
        updateDeviceCapabilities();
    }

    std::lock_guard<std::mutex> lock(capabilities_mutex_);
    return capabilities_;
}

auto PropertyManager::isConnected() -> bool {
    auto connected = getBoolProperty("connected");
    return connected && *connected;
}

auto PropertyManager::getPosition() -> std::optional<double> {
    return getDoubleProperty("position");
}

auto PropertyManager::isMoving() -> bool {
    auto moving = getBoolProperty("ismoving");
    return moving && *moving;
}

auto PropertyManager::canReverse() -> bool {
    auto canRev = getBoolProperty("canreverse");
    return canRev && *canRev;
}

auto PropertyManager::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto PropertyManager::clearLastError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private helper methods

auto PropertyManager::registerStandardProperties() -> void {
    std::unique_lock<std::shared_mutex> lock(property_mutex_);

    // Connection properties
    property_registry_["connected"] = PropertyMetadata{
        .name = "connected",
        .description = "Device connection status",
        .readable = true,
        .writable = true,
        .default_value = false,
        .cached = true,
        .cache_duration = std::chrono::milliseconds(1000)
    };

    // Position properties
    property_registry_["position"] = PropertyMetadata{
        .name = "position",
        .description = "Current rotator position in degrees",
        .readable = true,
        .writable = true,
        .min_value = 0.0,
        .max_value = 360.0,
        .default_value = 0.0,
        .cached = true,
        .cache_duration = std::chrono::milliseconds(500)
    };

    property_registry_["mechanicalposition"] = PropertyMetadata{
        .name = "mechanicalposition",
        .description = "Mechanical position of the rotator",
        .readable = true,
        .writable = false,
        .min_value = 0.0,
        .max_value = 360.0,
        .default_value = 0.0,
        .cached = true,
        .cache_duration = std::chrono::milliseconds(500)
    };

    // Movement properties
    property_registry_["ismoving"] = PropertyMetadata{
        .name = "ismoving",
        .description = "Whether the rotator is currently moving",
        .readable = true,
        .writable = false,
        .default_value = false,
        .cached = true,
        .cache_duration = std::chrono::milliseconds(200)
    };

    // Capability properties
    property_registry_["canreverse"] = PropertyMetadata{
        .name = "canreverse",
        .description = "Whether the rotator can be reversed",
        .readable = true,
        .writable = false,
        .default_value = false,
        .cached = true,
        .cache_duration = std::chrono::milliseconds(10000)
    };

    property_registry_["reverse"] = PropertyMetadata{
        .name = "reverse",
        .description = "Rotator reverse state",
        .readable = true,
        .writable = true,
        .default_value = false,
        .cached = true,
        .cache_duration = std::chrono::milliseconds(5000)
    };

    // Device information
    property_registry_["description"] = PropertyMetadata{
        .name = "description",
        .description = "Device description",
        .readable = true,
        .writable = false,
        .default_value = std::string("ASCOM Rotator"),
        .cached = true,
        .cache_duration = std::chrono::milliseconds(60000)
    };

    property_registry_["driverinfo"] = PropertyMetadata{
        .name = "driverinfo",
        .description = "Driver information",
        .readable = true,
        .writable = false,
        .default_value = std::string(""),
        .cached = true,
        .cache_duration = std::chrono::milliseconds(60000)
    };

    property_registry_["driverversion"] = PropertyMetadata{
        .name = "driverversion",
        .description = "Driver version",
        .readable = true,
        .writable = false,
        .default_value = std::string(""),
        .cached = true,
        .cache_duration = std::chrono::milliseconds(60000)
    };
}

auto PropertyManager::loadPropertyFromHardware(const std::string& name) -> std::optional<PropertyValue> {
    if (!hardware_ || !hardware_->isConnected()) {
        return std::nullopt;
    }

    auto response = hardware_->getProperty(name);
    if (!response) {
        return std::nullopt;
    }

    // Parse the response based on property metadata
    auto metadata = getPropertyMetadata(name);
    if (!metadata) {
        // Try to parse as string by default
        return PropertyValue{*response};
    }

    return parsePropertyValue(*response, *metadata);
}

auto PropertyManager::savePropertyToHardware(const std::string& name, const PropertyValue& value) -> bool {
    if (!hardware_ || !hardware_->isConnected()) {
        setLastError("Hardware not connected");
        return false;
    }

    std::string str_value = propertyValueToString(value);
    return hardware_->setProperty(name, str_value);
}

auto PropertyManager::parsePropertyValue(const std::string& str_value, PropertyMetadata& metadata) -> PropertyValue {
    // Simple parsing based on the default value type
    if (std::holds_alternative<bool>(metadata.default_value)) {
        return PropertyValue{str_value == "true" || str_value == "1"};
    } else if (std::holds_alternative<int>(metadata.default_value)) {
        try {
            return PropertyValue{std::stoi(str_value)};
        } catch (...) {
            return metadata.default_value;
        }
    } else if (std::holds_alternative<double>(metadata.default_value)) {
        try {
            return PropertyValue{std::stod(str_value)};
        } catch (...) {
            return metadata.default_value;
        }
    } else {
        return PropertyValue{str_value};
    }
}

auto PropertyManager::propertyValueToString(const PropertyValue& value) -> std::string {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(v);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        }
        return "";
    }, value);
}

auto PropertyManager::isCacheValid(const std::string& name) -> bool {
    std::shared_lock<std::shared_mutex> lock(property_mutex_);

    auto reg_it = property_registry_.find(name);
    if (reg_it == property_registry_.end() || !reg_it->second.cached) {
        return false;
    }

    auto cache_it = property_cache_.find(name);
    if (cache_it == property_cache_.end() || !cache_it->second.valid) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto age = now - cache_it->second.timestamp;

    return age < reg_it->second.cache_duration;
}

auto PropertyManager::updatePropertyCache(const std::string& name, const PropertyValue& value) -> void {
    std::unique_lock<std::shared_mutex> lock(property_mutex_);

    property_cache_[name] = PropertyCacheEntry{
        .value = value,
        .timestamp = std::chrono::steady_clock::now(),
        .valid = true
    };
}

auto PropertyManager::setLastError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("PropertyManager error: {}", error);
}

auto PropertyManager::notifyPropertyChange(const std::string& name, const PropertyValue& value) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    auto it = property_callbacks_.find(name);
    if (it != property_callbacks_.end() && it->second) {
        it->second(value);
    }
}

auto PropertyManager::queryDeviceCapabilities() -> bool {
    // Query basic capabilities
    auto canReverse = getBoolProperty("canreverse");
    if (canReverse) {
        capabilities_.can_reverse = *canReverse;
    }

    auto description = getStringProperty("description");
    if (description) {
        capabilities_.device_description = *description;
    }

    auto driverInfo = getStringProperty("driverinfo");
    if (driverInfo) {
        capabilities_.driver_info = *driverInfo;
    }

    auto driverVersion = getStringProperty("driverversion");
    if (driverVersion) {
        capabilities_.driver_version = *driverVersion;
    }

    return true;
}

auto PropertyManager::validatePropertyAccess(const std::string& name, bool write_access) -> bool {
    auto metadata = getPropertyMetadata(name);
    if (!metadata) {
        setLastError("Unknown property: " + name);
        return false;
    }

    if (write_access && !metadata->writable) {
        setLastError("Property is read-only: " + name);
        return false;
    }

    if (!write_access && !metadata->readable) {
        setLastError("Property is write-only: " + name);
        return false;
    }

    return true;
}

} // namespace lithium::device::ascom::rotator::components
