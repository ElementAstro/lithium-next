/*
 * property_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_CLIENT_INDI_MANAGER_PROPERTY_UTILS_HPP
#define LITHIUM_CLIENT_INDI_MANAGER_PROPERTY_UTILS_HPP

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

namespace lithium::client::indi_manager {

enum class PropertyType : uint8_t { Number, Text, Switch, Light, Blob, Unknown };

enum class PropertyState : uint8_t { Idle, Ok, Busy, Alert };

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

enum class SwitchRule : uint8_t { OneOfMany, AtMostOne, AnyOfMany };

struct NumberElement {
    std::string name;
    std::string label;
    double value{0.0};
    double min{0.0};
    double max{0.0};
    double step{0.0};
    std::string format;
};

struct TextElement {
    std::string name;
    std::string label;
    std::string value;
};

struct SwitchElement {
    std::string name;
    std::string label;
    bool on{false};
};

struct LightElement {
    std::string name;
    std::string label;
    PropertyState state{PropertyState::Idle};
};

struct BlobElement {
    std::string name;
    std::string label;
    std::string format;
    std::vector<uint8_t> data;
    size_t size{0};
};

using PropertyElement =
    std::variant<NumberElement, TextElement, SwitchElement, LightElement,
                 BlobElement>;

struct Property {
    std::string device;
    std::string name;
    std::string label;
    std::string group;
    PropertyType type{PropertyType::Unknown};
    PropertyState state{PropertyState::Idle};
    std::string permission;
    std::string timestamp;
    std::vector<PropertyElement> elements;
    SwitchRule rule{SwitchRule::OneOfMany};

    [[nodiscard]] auto getElement(std::string_view elemName) const
        -> std::optional<PropertyElement>;
    [[nodiscard]] auto getNumber(std::string_view elemName) const
        -> std::optional<double>;
    [[nodiscard]] auto getText(std::string_view elemName) const
        -> std::optional<std::string>;
    [[nodiscard]] auto getSwitch(std::string_view elemName) const
        -> std::optional<bool>;
    [[nodiscard]] auto isWritable() const noexcept -> bool {
        return permission.find('w') != std::string::npos;
    }
    [[nodiscard]] auto isReadable() const noexcept -> bool {
        return permission.find('r') != std::string::npos;
    }
};

struct PropertyChange {
    std::string device;
    std::string property;
    std::string element;
    std::variant<double, std::string, bool> value;
};

class PropertyUtils {
public:
    [[nodiscard]] static auto parsePropertyName(std::string_view fullName)
        -> std::pair<std::string, std::string>;
    [[nodiscard]] static auto buildPropertyName(std::string_view property,
                                                std::string_view element)
        -> std::string;
    [[nodiscard]] static auto fromPropertyValue(const PropertyValue& pv)
        -> Property;
    [[nodiscard]] static auto toPropertyValue(const Property& prop)
        -> PropertyValue;
    [[nodiscard]] static auto formatNumber(double value,
                                           std::string_view format)
        -> std::string;
    [[nodiscard]] static auto parseSexagesimal(std::string_view str)
        -> std::optional<double>;
    [[nodiscard]] static auto formatSexagesimal(double value, int precision = 2)
        -> std::string;
};

class PropertyWatcher {
public:
    using Callback = std::function<void(const Property&)>;

    void watch(std::string_view device, std::string_view property,
               Callback callback);
    void watchDevice(std::string_view device, Callback callback);
    void unwatch(std::string_view device, std::string_view property);
    void unwatchDevice(std::string_view device);
    void notify(const Property& property);

private:
    struct WatchEntry {
        std::string device;
        std::string property;
        Callback callback;
    };

    std::vector<WatchEntry> watches_;
    std::mutex mutex_;
};

}  // namespace lithium::client::indi_manager

#endif  // LITHIUM_CLIENT_INDI_MANAGER_PROPERTY_UTILS_HPP
