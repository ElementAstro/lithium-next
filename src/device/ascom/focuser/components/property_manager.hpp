/*
 * property_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Focuser Property Manager Component

This component handles ASCOM property management, caching,
and validation for focuser devices.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace lithium::device::ascom::focuser::components {

// Forward declaration
class HardwareInterface;

/**
 * @brief Property Manager for ASCOM Focuser
 *
 * This component manages ASCOM property operations:
 * - Property caching and validation
 * - Property change notifications
 * - Property synchronization with hardware
 * - Property access control and validation
 */
class PropertyManager {
public:
    // Property value types
    using PropertyValue = std::variant<bool, int, double, std::string>;
    
    // Property metadata
    struct PropertyMetadata {
        std::string name;
        std::string description;
        std::string unit;
        PropertyValue defaultValue;
        PropertyValue minValue;
        PropertyValue maxValue;
        bool readOnly = false;
        bool cached = true;
        std::chrono::milliseconds cacheTimeout{5000};
        std::chrono::steady_clock::time_point lastUpdate;
        bool isValid = false;
    };

    // Property cache entry
    struct PropertyCacheEntry {
        PropertyValue value;
        std::chrono::steady_clock::time_point timestamp;
        bool isValid = false;
        bool isDirty = false;
        int accessCount = 0;
        std::chrono::steady_clock::time_point lastAccess;
    };

    // Property access statistics
    struct PropertyStats {
        int totalReads = 0;
        int totalWrites = 0;
        int cacheHits = 0;
        int cacheMisses = 0;
        int validationErrors = 0;
        int hardwareErrors = 0;
        std::chrono::steady_clock::time_point lastAccess;
        std::chrono::milliseconds averageReadTime{0};
        std::chrono::milliseconds averageWriteTime{0};
    };

    // Property manager configuration
    struct PropertyConfig {
        bool enableCaching = true;
        bool enableValidation = true;
        bool enableNotifications = true;
        std::chrono::milliseconds defaultCacheTimeout{5000};
        std::chrono::milliseconds propertyUpdateInterval{1000};
        int maxCacheSize = 100;
        bool strictValidation = false;
        bool logPropertyAccess = false;
    };

    // Constructor and destructor
    explicit PropertyManager(std::shared_ptr<HardwareInterface> hardware);
    ~PropertyManager();

    // Non-copyable and non-movable
    PropertyManager(const PropertyManager&) = delete;
    PropertyManager& operator=(const PropertyManager&) = delete;
    PropertyManager(PropertyManager&&) = delete;
    PropertyManager& operator=(PropertyManager&&) = delete;

    // =========================================================================
    // Initialization and Configuration
    // =========================================================================

    /**
     * @brief Initialize the property manager
     */
    auto initialize() -> bool;

    /**
     * @brief Destroy the property manager
     */
    auto destroy() -> bool;

    /**
     * @brief Set property configuration
     */
    auto setPropertyConfig(const PropertyConfig& config) -> void;

    /**
     * @brief Get property configuration
     */
    auto getPropertyConfig() const -> PropertyConfig;

    // =========================================================================
    // Property Registration and Metadata
    // =========================================================================

    /**
     * @brief Register property with metadata
     */
    auto registerProperty(const std::string& name, const PropertyMetadata& metadata) -> bool;

    /**
     * @brief Unregister property
     */
    auto unregisterProperty(const std::string& name) -> bool;

    /**
     * @brief Get property metadata
     */
    auto getPropertyMetadata(const std::string& name) -> std::optional<PropertyMetadata>;

    /**
     * @brief Get all registered properties
     */
    auto getRegisteredProperties() -> std::vector<std::string>;

    /**
     * @brief Check if property is registered
     */
    auto isPropertyRegistered(const std::string& name) -> bool;

    /**
     * @brief Set property metadata
     */
    auto setPropertyMetadata(const std::string& name, const PropertyMetadata& metadata) -> bool;

    // =========================================================================
    // Property Access
    // =========================================================================

    /**
     * @brief Get property value
     */
    auto getProperty(const std::string& name) -> std::optional<PropertyValue>;

    /**
     * @brief Set property value
     */
    auto setProperty(const std::string& name, const PropertyValue& value) -> bool;

    /**
     * @brief Get property value with type checking
     */
    template<typename T>
    auto getPropertyAs(const std::string& name) -> std::optional<T>;

    /**
     * @brief Set property value with type checking
     */
    template<typename T>
    auto setPropertyAs(const std::string& name, const T& value) -> bool;

    /**
     * @brief Get multiple properties
     */
    auto getProperties(const std::vector<std::string>& names) -> std::unordered_map<std::string, PropertyValue>;

    /**
     * @brief Set multiple properties
     */
    auto setProperties(const std::unordered_map<std::string, PropertyValue>& properties) -> bool;

    // =========================================================================
    // Property Validation
    // =========================================================================

    /**
     * @brief Validate property value
     */
    auto validateProperty(const std::string& name, const PropertyValue& value) -> bool;

    /**
     * @brief Get property validation error
     */
    auto getValidationError(const std::string& name) -> std::string;

    /**
     * @brief Set property validator
     */
    auto setPropertyValidator(const std::string& name, 
                             std::function<bool(const PropertyValue&)> validator) -> bool;

    /**
     * @brief Clear property validator
     */
    auto clearPropertyValidator(const std::string& name) -> bool;

    // =========================================================================
    // Property Caching
    // =========================================================================

    /**
     * @brief Enable/disable property caching
     */
    auto enablePropertyCaching(bool enable) -> void;

    /**
     * @brief Check if property caching is enabled
     */
    auto isPropertyCachingEnabled() -> bool;

    /**
     * @brief Clear property cache
     */
    auto clearPropertyCache() -> void;

    /**
     * @brief Clear specific property from cache
     */
    auto clearPropertyFromCache(const std::string& name) -> void;

    /**
     * @brief Get cache statistics
     */
    auto getCacheStats() -> std::unordered_map<std::string, PropertyStats>;

    /**
     * @brief Get cache hit rate
     */
    auto getCacheHitRate() -> double;

    /**
     * @brief Set cache timeout for property
     */
    auto setCacheTimeout(const std::string& name, std::chrono::milliseconds timeout) -> bool;

    // =========================================================================
    // Property Synchronization
    // =========================================================================

    /**
     * @brief Synchronize property with hardware
     */
    auto synchronizeProperty(const std::string& name) -> bool;

    /**
     * @brief Synchronize all properties with hardware
     */
    auto synchronizeAllProperties() -> bool;

    /**
     * @brief Get property from hardware (bypass cache)
     */
    auto getPropertyFromHardware(const std::string& name) -> std::optional<PropertyValue>;

    /**
     * @brief Set property to hardware (bypass cache)
     */
    auto setPropertyToHardware(const std::string& name, const PropertyValue& value) -> bool;

    /**
     * @brief Check if property is synchronized
     */
    auto isPropertySynchronized(const std::string& name) -> bool;

    /**
     * @brief Mark property as dirty (needs synchronization)
     */
    auto markPropertyDirty(const std::string& name) -> void;

    // =========================================================================
    // Property Monitoring and Notifications
    // =========================================================================

    /**
     * @brief Start property monitoring
     */
    auto startMonitoring() -> bool;

    /**
     * @brief Stop property monitoring
     */
    auto stopMonitoring() -> bool;

    /**
     * @brief Check if monitoring is active
     */
    auto isMonitoring() -> bool;

    /**
     * @brief Add property to monitoring list
     */
    auto addPropertyToMonitoring(const std::string& name) -> bool;

    /**
     * @brief Remove property from monitoring list
     */
    auto removePropertyFromMonitoring(const std::string& name) -> bool;

    /**
     * @brief Get monitored properties
     */
    auto getMonitoredProperties() -> std::vector<std::string>;

    // =========================================================================
    // Standard ASCOM Focuser Properties
    // =========================================================================

    /**
     * @brief Register standard ASCOM focuser properties
     */
    auto registerStandardProperties() -> bool;

    // Standard property getters/setters
    auto getAbsolute() -> bool;
    auto getIsMoving() -> bool;
    auto getPosition() -> int;
    auto getMaxStep() -> int;
    auto getMaxIncrement() -> int;
    auto getStepSize() -> double;
    auto getTempCompAvailable() -> bool;
    auto getTempComp() -> bool;
    auto setTempComp(bool value) -> bool;
    auto getTemperature() -> double;
    auto getConnected() -> bool;
    auto setConnected(bool value) -> bool;

    // =========================================================================
    // Callbacks and Events
    // =========================================================================

    using PropertyChangeCallback = std::function<void(const std::string& name, const PropertyValue& oldValue, const PropertyValue& newValue)>;
    using PropertyErrorCallback = std::function<void(const std::string& name, const std::string& error)>;
    using PropertyValidationCallback = std::function<void(const std::string& name, const PropertyValue& value, bool isValid)>;

    /**
     * @brief Set property change callback
     */
    auto setPropertyChangeCallback(PropertyChangeCallback callback) -> void;

    /**
     * @brief Set property error callback
     */
    auto setPropertyErrorCallback(PropertyErrorCallback callback) -> void;

    /**
     * @brief Set property validation callback
     */
    auto setPropertyValidationCallback(PropertyValidationCallback callback) -> void;

    // =========================================================================
    // Statistics and Debugging
    // =========================================================================

    /**
     * @brief Get property statistics
     */
    auto getPropertyStats() -> std::unordered_map<std::string, PropertyStats>;

    /**
     * @brief Reset property statistics
     */
    auto resetPropertyStats() -> void;

    /**
     * @brief Get property access history
     */
    auto getPropertyAccessHistory(const std::string& name) -> std::vector<std::chrono::steady_clock::time_point>;

    /**
     * @brief Export property data to JSON
     */
    auto exportPropertyData() -> std::string;

    /**
     * @brief Import property data from JSON
     */
    auto importPropertyData(const std::string& json) -> bool;

private:
    // Hardware interface reference
    std::shared_ptr<HardwareInterface> hardware_;
    
    // Configuration
    PropertyConfig config_;
    
    // Property storage
    std::unordered_map<std::string, PropertyMetadata> property_metadata_;
    std::unordered_map<std::string, PropertyCacheEntry> property_cache_;
    std::unordered_map<std::string, PropertyStats> property_stats_;
    std::unordered_map<std::string, std::function<bool(const PropertyValue&)>> property_validators_;
    
    // Monitoring
    std::vector<std::string> monitored_properties_;
    std::thread monitoring_thread_;
    std::atomic<bool> monitoring_active_{false};
    
    // Synchronization
    mutable std::mutex metadata_mutex_;
    mutable std::mutex cache_mutex_;
    mutable std::mutex stats_mutex_;
    mutable std::mutex config_mutex_;
    mutable std::mutex monitoring_mutex_;
    
    // Callbacks
    PropertyChangeCallback property_change_callback_;
    PropertyErrorCallback property_error_callback_;
    PropertyValidationCallback property_validation_callback_;
    
    // Private methods
    auto getCachedProperty(const std::string& name) -> std::optional<PropertyValue>;
    auto setCachedProperty(const std::string& name, const PropertyValue& value) -> void;
    auto isCacheValid(const std::string& name) -> bool;
    auto updatePropertyCache(const std::string& name, const PropertyValue& value) -> void;
    auto updatePropertyStats(const std::string& name, bool isRead, bool isWrite, 
                           std::chrono::milliseconds duration, bool success) -> void;
    
    auto monitoringLoop() -> void;
    auto checkPropertyChanges() -> void;
    auto validatePropertyValue(const std::string& name, const PropertyValue& value) -> bool;
    
    // Notification methods
    auto notifyPropertyChange(const std::string& name, const PropertyValue& oldValue, 
                             const PropertyValue& newValue) -> void;
    auto notifyPropertyError(const std::string& name, const std::string& error) -> void;
    auto notifyPropertyValidation(const std::string& name, const PropertyValue& value, bool isValid) -> void;
    
    // Utility methods
    auto propertyValueToString(const PropertyValue& value) -> std::string;
    auto stringToPropertyValue(const std::string& str, const PropertyValue& defaultValue) -> PropertyValue;
    auto comparePropertyValues(const PropertyValue& a, const PropertyValue& b) -> bool;
    auto clampPropertyValue(const PropertyValue& value, const PropertyValue& min, const PropertyValue& max) -> PropertyValue;
    
    // Standard property helpers
    auto initializeStandardProperty(const std::string& name, const PropertyValue& defaultValue,
                                   const std::string& description = "", const std::string& unit = "",
                                   bool readOnly = false) -> void;
    auto getStandardPropertyValue(const std::string& name) -> std::optional<PropertyValue>;
    auto setStandardPropertyValue(const std::string& name, const PropertyValue& value) -> bool;
};

// Template implementations
template<typename T>
auto PropertyManager::getPropertyAs(const std::string& name) -> std::optional<T> {
    auto value = getProperty(name);
    if (!value) {
        return std::nullopt;
    }
    
    if (std::holds_alternative<T>(*value)) {
        return std::get<T>(*value);
    }
    
    return std::nullopt;
}

template<typename T>
auto PropertyManager::setPropertyAs(const std::string& name, const T& value) -> bool {
    return setProperty(name, PropertyValue(value));
}

} // namespace lithium::device::ascom::focuser::components
