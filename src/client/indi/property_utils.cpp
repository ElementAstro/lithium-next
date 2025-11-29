/*
 * property_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-29

Description: INDI property utilities implementation

*************************************************/

#include "property_utils.hpp"

#include <cmath>
#include <cstdio>
#include <regex>
#include <sstream>

namespace lithium::client::indi {

// ==================== Property Implementation ====================

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
    if (!elem) {
        return std::nullopt;
    }
    if (auto* num = std::get_if<NumberElement>(&*elem)) {
        return num->value;
    }
    return std::nullopt;
}

auto Property::getText(std::string_view elemName) const
    -> std::optional<std::string> {
    auto elem = getElement(elemName);
    if (!elem) {
        return std::nullopt;
    }
    if (auto* txt = std::get_if<TextElement>(&*elem)) {
        return txt->value;
    }
    return std::nullopt;
}

auto Property::getSwitch(std::string_view elemName) const
    -> std::optional<bool> {
    auto elem = getElement(elemName);
    if (!elem) {
        return std::nullopt;
    }
    if (auto* sw = std::get_if<SwitchElement>(&*elem)) {
        return sw->on;
    }
    return std::nullopt;
}

// ==================== PropertyUtils Implementation ====================

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
    prop.state = propertyStateFromString(pv.state);

    switch (pv.type) {
        case PropertyValue::Type::Number:
            prop.type = PropertyType::Number;
            {
                NumberElement elem;
                elem.name = pv.name;
                elem.value = pv.numberValue;
                elem.min = pv.numberMin;
                elem.max = pv.numberMax;
                elem.step = pv.numberStep;
                prop.elements.push_back(elem);
            }
            break;
        case PropertyValue::Type::Text:
            prop.type = PropertyType::Text;
            {
                TextElement elem;
                elem.name = pv.name;
                elem.value = pv.textValue;
                prop.elements.push_back(elem);
            }
            break;
        case PropertyValue::Type::Switch:
            prop.type = PropertyType::Switch;
            {
                SwitchElement elem;
                elem.name = pv.name;
                elem.on = pv.switchValue;
                prop.elements.push_back(elem);
            }
            break;
        case PropertyValue::Type::Light:
            prop.type = PropertyType::Light;
            {
                LightElement elem;
                elem.name = pv.name;
                elem.state = propertyStateFromString(pv.state);
                prop.elements.push_back(elem);
            }
            break;
        case PropertyValue::Type::Blob:
            prop.type = PropertyType::Blob;
            {
                BlobElement elem;
                elem.name = pv.name;
                elem.format = pv.blobFormat;
                elem.data = pv.blobData;
                elem.size = pv.blobData.size();
                prop.elements.push_back(elem);
            }
            break;
        default:
            prop.type = PropertyType::Unknown;
            break;
    }

    return prop;
}

auto PropertyUtils::toPropertyValue(const Property& prop) -> PropertyValue {
    PropertyValue pv;
    pv.name = prop.name;
    pv.label = prop.label;
    pv.group = prop.group;
    pv.state = std::string(propertyStateToString(prop.state));

    switch (prop.type) {
        case PropertyType::Number:
            pv.type = PropertyValue::Type::Number;
            if (!prop.elements.empty()) {
                if (auto* num =
                        std::get_if<NumberElement>(&prop.elements[0])) {
                    pv.numberValue = num->value;
                    pv.numberMin = num->min;
                    pv.numberMax = num->max;
                    pv.numberStep = num->step;
                }
            }
            break;
        case PropertyType::Text:
            pv.type = PropertyValue::Type::Text;
            if (!prop.elements.empty()) {
                if (auto* txt = std::get_if<TextElement>(&prop.elements[0])) {
                    pv.textValue = txt->value;
                }
            }
            break;
        case PropertyType::Switch:
            pv.type = PropertyValue::Type::Switch;
            if (!prop.elements.empty()) {
                if (auto* sw = std::get_if<SwitchElement>(&prop.elements[0])) {
                    pv.switchValue = sw->on;
                }
            }
            break;
        case PropertyType::Light:
            pv.type = PropertyValue::Type::Light;
            break;
        case PropertyType::Blob:
            pv.type = PropertyValue::Type::Blob;
            if (!prop.elements.empty()) {
                if (auto* blob = std::get_if<BlobElement>(&prop.elements[0])) {
                    pv.blobFormat = blob->format;
                    pv.blobData = blob->data;
                }
            }
            break;
        default:
            pv.type = PropertyValue::Type::Unknown;
            break;
    }

    return pv;
}

auto PropertyUtils::formatNumber(double value, std::string_view format)
    -> std::string {
    if (format.empty()) {
        return std::to_string(value);
    }

    // Check for sexagesimal format
    if (format.find('m') != std::string_view::npos ||
        format.find('s') != std::string_view::npos) {
        return formatSexagesimal(value);
    }

    // Standard printf format
    char buf[64];
    std::snprintf(buf, sizeof(buf), std::string(format).c_str(), value);
    return buf;
}

auto PropertyUtils::parseSexagesimal(std::string_view str)
    -> std::optional<double> {
    // Format: DD:MM:SS.ss or DD MM SS.ss or DD°MM'SS.ss"
    std::regex re(R"(([+-]?\d+)[:\s°]+(\d+)[:\s']+([0-9.]+))");
    std::smatch match;
    std::string text(str);

    if (std::regex_search(text, match, re)) {
        try {
            double d = std::stod(match[1].str());
            double m = std::stod(match[2].str());
            double s = std::stod(match[3].str());
            double sign = (d < 0 || str.find('-') != std::string_view::npos)
                              ? -1.0
                              : 1.0;
            return sign * (std::abs(d) + m / 60.0 + s / 3600.0);
        } catch (...) {
            return std::nullopt;
        }
    }

    // Try plain number
    try {
        return std::stod(text);
    } catch (...) {
        return std::nullopt;
    }
}

auto PropertyUtils::formatSexagesimal(double value, int precision)
    -> std::string {
    bool negative = value < 0;
    value = std::abs(value);

    int d = static_cast<int>(value);
    double minRem = (value - d) * 60.0;
    int m = static_cast<int>(minRem);
    double s = (minRem - m) * 60.0;

    char buf[32];
    if (precision == 0) {
        std::snprintf(buf, sizeof(buf), "%s%d:%02d:%02.0f", negative ? "-" : "",
                      d, m, s);
    } else {
        std::snprintf(buf, sizeof(buf), "%s%d:%02d:%0*.*f", negative ? "-" : "",
                      d, m, precision + 3, precision, s);
    }
    return buf;
}

// ==================== PropertyWatcher Implementation ====================

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
    std::erase_if(watches_, [&](const WatchEntry& entry) {
        return entry.device == device && entry.property == property;
    });
}

void PropertyWatcher::unwatchDevice(std::string_view device) {
    std::lock_guard lock(mutex_);
    std::erase_if(watches_,
                  [&](const WatchEntry& entry) { return entry.device == device; });
}

void PropertyWatcher::notify(const Property& property) {
    std::lock_guard lock(mutex_);
    for (const auto& entry : watches_) {
        if (entry.device == property.device) {
            if (entry.property.empty() || entry.property == property.name) {
                if (entry.callback) {
                    entry.callback(property);
                }
            }
        }
    }
}

}  // namespace lithium::client::indi
