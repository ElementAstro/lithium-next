/*
 * property_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: INDI property utilities with modern C++ features

*************************************************/

#pragma once

#include "../common/server_client.hpp"

#include <concepts>
#include <expected>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace lithium::client::indi {

/**
 * @brief INDI property types
 */
enum class PropertyType : uint8_t {
    Number,
    Text,
    Switch,
    Light,
    Blob,
    Unknown
};

/**
 * @brief INDI property state
 */
enum class PropertyState : uint8_t { Idle, Ok, Busy, Alert };

/**
 * @brief Convert property state to string
 */
[[nodiscard]] constexpr auto propertyStateToString(PropertyState state) noexcept
    -> std::string_view {
    switch (state) {
        case PropertyState::Idle:
            return "Idle";
        case PropertyState::Ok:
            return "Ok";
        case PropertyState::Busy:
            return "Busy";
        case PropertyState::Alert:
            return "Alert";
    }
    return "Unknown";
}

/**
 * @brief Parse property state from string
 */
[[nodiscard]] constexpr auto propertyStateFromString(
    std::string_view state) noexcept -> PropertyState {
    if (state == "Idle")
        return PropertyState::Idle;
    if (state == "Ok")
        return PropertyState::Ok;
    if (state == "Busy")
        return PropertyState::Busy;
    if (state == "Alert")
        return PropertyState::Alert;
    return PropertyState::Idle;
}

/**
 * @brief INDI switch rule
 */
enum class SwitchRule : uint8_t { OneOfMany, AtMostOne, AnyOfMany };

/**
 * @brief Number property element
 */
struct NumberElement {
    std::string name;
    std::string label;
    double value{0.0};
    double min{0.0};
    double max{0.0};
    double step{0.0};
    std::string format;
};

/**
 * @brief Text property element
 */
struct TextElement {
    std::string name;
    std::string label;
    std::string value;
};

/**
 * @brief Switch property element
 */
struct SwitchElement {
    std::string name;
    std::string label;
    bool on{false};
};

/**
 * @brief Light property element
 */
struct LightElement {
    std::string name;
    std::string label;
    PropertyState state{PropertyState::Idle};
};

/**
 * @brief BLOB property element
 */
struct BlobElement {
    std::string name;
    std::string label;
    std::string format;
    std::vector<uint8_t> data;
    size_t size{0};
};

/**
 * @brief Generic property value
 */
using PropertyElement =
    std::variant<NumberElement, TextElement, SwitchElement, LightElement,
                 BlobElement>;

/**
 * @brief INDI property
 */
struct Property {
    std::string device;
    std::string name;
    std::string label;
    std::string group;
    PropertyType type{PropertyType::Unknown};
    PropertyState state{PropertyState::Idle};
    std::string permission;  // ro, wo, rw
    std::string timestamp;
    std::vector<PropertyElement> elements;

    // For switch properties
    SwitchRule rule{SwitchRule::OneOfMany};

    /**
     * @brief Get element by name
     */
    [[nodiscard]] auto getElement(std::string_view elemName) const
        -> std::optional<PropertyElement>;

    /**
     * @brief Get number value
     */
    [[nodiscard]] auto getNumber(std::string_view elemName) const
        -> std::optional<double>;

    /**
     * @brief Get text value
     */
    [[nodiscard]] auto getText(std::string_view elemName) const
        -> std::optional<std::string>;

    /**
     * @brief Get switch state
     */
    [[nodiscard]] auto getSwitch(std::string_view elemName) const
        -> std::optional<bool>;

    /**
     * @brief Check if property is writable
     */
    [[nodiscard]] auto isWritable() const noexcept -> bool {
        return permission.find('w') != std::string::npos;
    }

    /**
     * @brief Check if property is readable
     */
    [[nodiscard]] auto isReadable() const noexcept -> bool {
        return permission.find('r') != std::string::npos;
    }
};

/**
 * @brief Property change request
 */
struct PropertyChange {
    std::string device;
    std::string property;
    std::string element;
    std::variant<double, std::string, bool> value;
};

/**
 * @brief Property utilities
 */
class PropertyUtils {
public:
    /**
     * @brief Parse property name.element format
     * @param fullName Full property name (e.g., "CCD_EXPOSURE.CCD_EXPOSURE_VALUE")
     * @return Pair of property name and element name
     */
    [[nodiscard]] static auto parsePropertyName(std::string_view fullName)
        -> std::pair<std::string, std::string>;

    /**
     * @brief Build property name.element format
     */
    [[nodiscard]] static auto buildPropertyName(std::string_view property,
                                                std::string_view element)
        -> std::string;

    /**
     * @brief Convert PropertyValue to Property
     */
    [[nodiscard]] static auto fromPropertyValue(const PropertyValue& pv)
        -> Property;

    /**
     * @brief Convert Property to PropertyValue
     */
    [[nodiscard]] static auto toPropertyValue(const Property& prop)
        -> PropertyValue;

    /**
     * @brief Format number value according to INDI format string
     */
    [[nodiscard]] static auto formatNumber(double value,
                                           std::string_view format)
        -> std::string;

    /**
     * @brief Parse INDI sexagesimal format
     */
    [[nodiscard]] static auto parseSexagesimal(std::string_view str)
        -> std::optional<double>;

    /**
     * @brief Format as sexagesimal
     */
    [[nodiscard]] static auto formatSexagesimal(double value, int precision = 2)
        -> std::string;
};

/**
 * @brief Property watcher for monitoring property changes
 */
class PropertyWatcher {
public:
    using Callback = std::function<void(const Property&)>;

    /**
     * @brief Watch a specific property
     */
    void watch(std::string_view device, std::string_view property,
               Callback callback);

    /**
     * @brief Watch all properties of a device
     */
    void watchDevice(std::string_view device, Callback callback);

    /**
     * @brief Stop watching a property
     */
    void unwatch(std::string_view device, std::string_view property);

    /**
     * @brief Stop watching a device
     */
    void unwatchDevice(std::string_view device);

    /**
     * @brief Notify property change
     */
    void notify(const Property& property);

private:
    struct WatchEntry {
        std::string device;
        std::string property;  // Empty for device-wide watch
        Callback callback;
    };

    std::vector<WatchEntry> watches_;
    std::mutex mutex_;
};

}  // namespace lithium::client::indi
