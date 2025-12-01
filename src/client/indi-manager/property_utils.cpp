/*
 * property_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "property_utils.hpp"

#include <cmath>
#include <cstdio>
#include <regex>
#include <sstream>

namespace lithium::client::indi_manager {

auto Property::getElement(std::string_view elemName) const
    -> std::optional<PropertyElement> {
    for (const auto& elem : elements) {
        bool found = std::visit(
            [&elemName](const auto& e) { return e.name == elemName; }, elem);
        if (found) {
            return elem;
        }
    }
    return std::nullopt;
}

auto Property::getNumber(std::string_view elemName) const
    -> std::optional<double> {
    auto elem = getElement(elemName);
    if (!elem)
        return std::nullopt;

    if (auto* num = std::get_if<NumberElement>(&*elem)) {
        return num->value;
    }
    return std::nullopt;
}

auto Property::getText(std::string_view elemName) const
    -> std::optional<std::string> {
    auto elem = getElement(elemName);
    if (!elem)
        return std::nullopt;

    if (auto* txt = std::get_if<TextElement>(&*elem)) {
        return txt->value;
    }
    return std::nullopt;
}

auto Property::getSwitch(std::string_view elemName) const
    -> std::optional<bool> {
    auto elem = getElement(elemName);
    if (!elem)
        return std::nullopt;

    if (auto* sw = std::get_if<SwitchElement>(&*elem)) {
        return sw->on;
    }
    return std::nullopt;
}

auto PropertyUtils::parsePropertyName(std::string_view fullName)
    -> std::pair<std::string, std::string> {
    auto dotPos = fullName.find('.');
    if (dotPos == std::string_view::npos) {
        return {std::string(fullName), ""};
    }
    return {std::string(fullName.substr(0, dotPos)),
            std::string(fullName.substr(dotPos + 1))};
}

auto PropertyUtils::buildPropertyName(std::string_view property,
                                      std::string_view element) -> std::string {
    if (element.empty()) {
        return std::string(property);
    }
    return std::string(property) + "." + std::string(element);
}

auto PropertyUtils::fromPropertyValue(const PropertyValue& pv) -> Property {
    Property prop;
    prop.name = pv.name;
    prop.label = pv.label;
    prop.group = pv.group;
    prop.permission = pv.permission;
    prop.timestamp = pv.timestamp;

    // Map type
    if (pv.type == "Number") {
        prop.type = PropertyType::Number;
    } else if (pv.type == "Text") {
        prop.type = PropertyType::Text;
    } else if (pv.type == "Switch") {
        prop.type = PropertyType::Switch;
    } else if (pv.type == "Light") {
        prop.type = PropertyType::Light;
    } else if (pv.type == "BLOB") {
        prop.type = PropertyType::Blob;
    }

    // Map state
    prop.state = propertyStateFromString(pv.state);

    return prop;
}

auto PropertyUtils::toPropertyValue(const Property& prop) -> PropertyValue {
    PropertyValue pv;
    pv.name = prop.name;
    pv.label = prop.label;
    pv.group = prop.group;
    pv.permission = prop.permission;
    pv.timestamp = prop.timestamp;

    switch (prop.type) {
        case PropertyType::Number:
            pv.type = "Number";
            break;
        case PropertyType::Text:
            pv.type = "Text";
            break;
        case PropertyType::Switch:
            pv.type = "Switch";
            break;
        case PropertyType::Light:
            pv.type = "Light";
            break;
        case PropertyType::Blob:
            pv.type = "BLOB";
            break;
        default:
            pv.type = "Unknown";
    }

    pv.state = std::string(propertyStateToString(prop.state));

    return pv;
}

auto PropertyUtils::formatNumber(double value, std::string_view format)
    -> std::string {
    if (format.empty() || format[0] != '%') {
        return std::to_string(value);
    }

    // Check for sexagesimal format
    if (format.find('m') != std::string_view::npos) {
        return formatSexagesimal(value);
    }

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), std::string(format).c_str(), value);
    return buffer;
}

auto PropertyUtils::parseSexagesimal(std::string_view str)
    -> std::optional<double> {
    // Parse formats: DD:MM:SS, DD:MM, DD MM SS, etc.
    std::regex sexPattern(R"(([+-]?\d+)[:\s]+(\d+)(?:[:\s]+(\d+(?:\.\d+)?))?)");
    std::string strCopy(str);
    std::smatch match;

    if (std::regex_match(strCopy, match, sexPattern)) {
        double degrees = std::stod(match[1].str());
        double minutes = std::stod(match[2].str());
        double seconds = match[3].matched ? std::stod(match[3].str()) : 0.0;

        double sign = (degrees < 0 || str[0] == '-') ? -1.0 : 1.0;
        degrees = std::abs(degrees);

        return sign * (degrees + minutes / 60.0 + seconds / 3600.0);
    }

    // Try plain number
    try {
        return std::stod(strCopy);
    } catch (...) {
        return std::nullopt;
    }
}

auto PropertyUtils::formatSexagesimal(double value, int precision)
    -> std::string {
    bool negative = value < 0;
    value = std::abs(value);

    int degrees = static_cast<int>(value);
    double remainder = (value - degrees) * 60.0;
    int minutes = static_cast<int>(remainder);
    double seconds = (remainder - minutes) * 60.0;

    std::ostringstream oss;
    if (negative)
        oss << "-";
    oss << degrees << ":" << std::setw(2) << std::setfill('0') << minutes << ":"
        << std::fixed << std::setprecision(precision) << seconds;

    return oss.str();
}

void PropertyWatcher::watch(std::string_view device, std::string_view property,
                            Callback callback) {
    std::lock_guard lock(mutex_);
    watches_.push_back(
        {std::string(device), std::string(property), std::move(callback)});
}

void PropertyWatcher::watchDevice(std::string_view device, Callback callback) {
    std::lock_guard lock(mutex_);
    watches_.push_back({std::string(device), "", std::move(callback)});
}

void PropertyWatcher::unwatch(std::string_view device,
                              std::string_view property) {
    std::lock_guard lock(mutex_);
    watches_.erase(
        std::remove_if(watches_.begin(), watches_.end(),
                       [&](const WatchEntry& e) {
                           return e.device == device && e.property == property;
                       }),
        watches_.end());
}

void PropertyWatcher::unwatchDevice(std::string_view device) {
    std::lock_guard lock(mutex_);
    watches_.erase(std::remove_if(watches_.begin(), watches_.end(),
                                  [&](const WatchEntry& e) {
                                      return e.device == device;
                                  }),
                   watches_.end());
}

void PropertyWatcher::notify(const Property& property) {
    std::lock_guard lock(mutex_);
    for (const auto& watch : watches_) {
        if (watch.device == property.device) {
            if (watch.property.empty() || watch.property == property.name) {
                watch.callback(property);
            }
        }
    }
}

}  // namespace lithium::client::indi_manager
