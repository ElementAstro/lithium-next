/*
 * device_type_registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Dynamic device type registry for extensible device management

**************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_DEVICE_TYPE_REGISTRY_HPP
#define LITHIUM_DEVICE_SERVICE_DEVICE_TYPE_REGISTRY_HPP

#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"
#include "device/common/device_result.hpp"

namespace lithium::device {

/**
 * @brief Extended device capability flags
 */
struct DeviceCapabilities {
    bool canConnect{true};
    bool canDisconnect{true};
    bool canAbort{false};
    bool canPark{false};
    bool canHome{false};
    bool canSync{false};
    bool canSlew{false};
    bool canTrack{false};
    bool canGuide{false};
    bool canCool{false};
    bool canFocus{false};
    bool canRotate{false};
    bool hasShutter{false};
    bool hasTemperature{false};
    bool hasPosition{false};

    // Extended capabilities
    bool supportsAsync{true};       // Supports async operations
    bool supportsEvents{true};      // Supports event notifications
    bool supportsProperties{true};  // Supports property get/set
    bool supportsBatch{false};      // Supports batch operations

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> DeviceCapabilities;

    bool operator==(const DeviceCapabilities&) const = default;
};

/**
 * @brief Information about a device type
 */
struct DeviceTypeInfo {
    std::string typeName;        // Unique type name (e.g., "INDICamera")
    std::string category;        // Category (e.g., "Camera", "Telescope")
    std::string displayName;     // Human-readable name
    std::string description;     // Type description
    std::string pluginName;      // Source plugin name (empty if built-in)
    std::string version;         // Type version
    DeviceCapabilities capabilities;
    nlohmann::json propertySchema;  // JSON Schema for device properties
    nlohmann::json metadata;        // Additional metadata

    // Type priority (higher = preferred when multiple types match)
    int priority{0};

    // Whether this type is enabled
    bool enabled{true};

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> DeviceTypeInfo;

    bool operator==(const DeviceTypeInfo& other) const {
        return typeName == other.typeName;
    }
};

/**
 * @brief Category information
 */
struct DeviceCategoryInfo {
    std::string categoryName;    // Category identifier
    std::string displayName;     // Human-readable name
    std::string description;     // Category description
    std::string iconName;        // Icon identifier
    int sortOrder{0};            // Display sort order
    bool isBuiltIn{false};       // Is this a built-in category

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> DeviceCategoryInfo;
};

/**
 * @brief Type registration event
 */
struct TypeRegistrationEvent {
    enum class Action { Registered, Unregistered, Updated, Enabled, Disabled };

    Action action;
    std::string typeName;
    std::string category;
    std::string pluginName;
};

/**
 * @brief Type registration callback
 */
using TypeRegistrationCallback = std::function<void(const TypeRegistrationEvent&)>;
using TypeRegistrationCallbackId = uint64_t;

/**
 * @brief Dynamic device type registry
 *
 * Allows runtime registration and query of device types.
 * Supports plugin-based type extension.
 */
class DeviceTypeRegistry {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> DeviceTypeRegistry&;

    // Disable copy
    DeviceTypeRegistry(const DeviceTypeRegistry&) = delete;
    DeviceTypeRegistry& operator=(const DeviceTypeRegistry&) = delete;

    // ==================== Type Registration ====================

    /**
     * @brief Register a new device type
     * @param info Type information
     * @return Success or error
     */
    auto registerType(const DeviceTypeInfo& info) -> DeviceResult<bool>;

    /**
     * @brief Register a new device type from plugin
     * @param info Type information
     * @param pluginName Source plugin name
     * @return Success or error
     */
    auto registerTypeFromPlugin(const DeviceTypeInfo& info,
                                const std::string& pluginName)
        -> DeviceResult<bool>;

    /**
     * @brief Unregister a device type
     * @param typeName Type name to unregister
     * @return Success or error
     */
    auto unregisterType(const std::string& typeName) -> DeviceResult<bool>;

    /**
     * @brief Unregister all types from a plugin
     * @param pluginName Plugin name
     * @return Number of types unregistered
     */
    auto unregisterPluginTypes(const std::string& pluginName) -> size_t;

    /**
     * @brief Update an existing type registration
     * @param info Updated type information
     * @return Success or error
     */
    auto updateType(const DeviceTypeInfo& info) -> DeviceResult<bool>;

    // ==================== Type Query ====================

    /**
     * @brief Check if a type is registered
     * @param typeName Type name
     * @return true if registered
     */
    [[nodiscard]] auto hasType(const std::string& typeName) const -> bool;

    /**
     * @brief Get type information
     * @param typeName Type name
     * @return Type info or nullopt
     */
    [[nodiscard]] auto getTypeInfo(const std::string& typeName) const
        -> std::optional<DeviceTypeInfo>;

    /**
     * @brief Get all registered types
     * @return Vector of type info
     */
    [[nodiscard]] auto getAllTypes() const -> std::vector<DeviceTypeInfo>;

    /**
     * @brief Get types by category
     * @param category Category name
     * @return Vector of type info
     */
    [[nodiscard]] auto getTypesByCategory(const std::string& category) const
        -> std::vector<DeviceTypeInfo>;

    /**
     * @brief Get types from a plugin
     * @param pluginName Plugin name
     * @return Vector of type info
     */
    [[nodiscard]] auto getPluginTypes(const std::string& pluginName) const
        -> std::vector<DeviceTypeInfo>;

    /**
     * @brief Get enabled types only
     * @return Vector of enabled type info
     */
    [[nodiscard]] auto getEnabledTypes() const -> std::vector<DeviceTypeInfo>;

    /**
     * @brief Get type names
     * @return Vector of type names
     */
    [[nodiscard]] auto getTypeNames() const -> std::vector<std::string>;

    // ==================== Category Management ====================

    /**
     * @brief Register a new category
     * @param info Category information
     * @return Success or error
     */
    auto registerCategory(const DeviceCategoryInfo& info) -> DeviceResult<bool>;

    /**
     * @brief Get category information
     * @param categoryName Category name
     * @return Category info or nullopt
     */
    [[nodiscard]] auto getCategoryInfo(const std::string& categoryName) const
        -> std::optional<DeviceCategoryInfo>;

    /**
     * @brief Get all categories
     * @return Vector of category info
     */
    [[nodiscard]] auto getAllCategories() const
        -> std::vector<DeviceCategoryInfo>;

    /**
     * @brief Check if category exists
     * @param categoryName Category name
     * @return true if exists
     */
    [[nodiscard]] auto hasCategory(const std::string& categoryName) const
        -> bool;

    // ==================== Type State ====================

    /**
     * @brief Enable a device type
     * @param typeName Type name
     * @return Success or error
     */
    auto enableType(const std::string& typeName) -> DeviceResult<bool>;

    /**
     * @brief Disable a device type
     * @param typeName Type name
     * @return Success or error
     */
    auto disableType(const std::string& typeName) -> DeviceResult<bool>;

    /**
     * @brief Check if type is enabled
     * @param typeName Type name
     * @return true if enabled
     */
    [[nodiscard]] auto isTypeEnabled(const std::string& typeName) const -> bool;

    // ==================== Event System ====================

    /**
     * @brief Subscribe to type registration events
     * @param callback Event callback
     * @return Callback ID for unsubscription
     */
    auto subscribe(TypeRegistrationCallback callback)
        -> TypeRegistrationCallbackId;

    /**
     * @brief Unsubscribe from events
     * @param callbackId Callback ID
     */
    void unsubscribe(TypeRegistrationCallbackId callbackId);

    // ==================== Initialization ====================

    /**
     * @brief Initialize with built-in types
     */
    void initializeBuiltInTypes();

    /**
     * @brief Clear all registered types
     */
    void clear();

    /**
     * @brief Get registry statistics
     */
    [[nodiscard]] auto getStatistics() const -> nlohmann::json;

private:
    DeviceTypeRegistry();
    ~DeviceTypeRegistry() = default;

    /**
     * @brief Notify subscribers of registration event
     */
    void notifySubscribers(const TypeRegistrationEvent& event);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, DeviceTypeInfo> types_;
    std::unordered_map<std::string, DeviceCategoryInfo> categories_;
    std::unordered_map<TypeRegistrationCallbackId, TypeRegistrationCallback>
        subscribers_;
    TypeRegistrationCallbackId nextCallbackId_{1};
};

// ============================================================================
// Built-in category constants
// ============================================================================

namespace categories {
inline constexpr const char* CAMERA = "Camera";
inline constexpr const char* TELESCOPE = "Telescope";
inline constexpr const char* FOCUSER = "Focuser";
inline constexpr const char* FILTERWHEEL = "FilterWheel";
inline constexpr const char* DOME = "Dome";
inline constexpr const char* ROTATOR = "Rotator";
inline constexpr const char* WEATHER = "Weather";
inline constexpr const char* GPS = "GPS";
inline constexpr const char* GUIDER = "Guider";
inline constexpr const char* AUXILIARY = "AuxiliaryDevice";
inline constexpr const char* SAFETY_MONITOR = "SafetyMonitor";
inline constexpr const char* SWITCH = "Switch";
inline constexpr const char* COVER_CALIBRATOR = "CoverCalibrator";
inline constexpr const char* OBSERVING_CONDITIONS = "ObservingConditions";
inline constexpr const char* VIDEO = "Video";
}  // namespace categories

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_DEVICE_TYPE_REGISTRY_HPP
