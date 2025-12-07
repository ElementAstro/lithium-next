/*
 * device_types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device type definitions and utilities for unified device management

*************************************************/

#ifndef LITHIUM_DEVICE_SERVICE_DEVICE_TYPES_HPP
#define LITHIUM_DEVICE_SERVICE_DEVICE_TYPES_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::device {

/**
 * @brief Standard device types supported by the system
 */
enum class DeviceType {
    Unknown = 0,
    Camera,
    Telescope,
    Focuser,
    FilterWheel,
    Dome,
    Rotator,
    Weather,
    GPS,
    Guider,
    AuxiliaryDevice,
    SafetyMonitor,
    Switch,
    CoverCalibrator,
    ObservingConditions,
    Video
};

/**
 * @brief Convert DeviceType to string
 */
inline auto deviceTypeToString(DeviceType type) -> std::string {
    switch (type) {
        case DeviceType::Camera:
            return "Camera";
        case DeviceType::Telescope:
            return "Telescope";
        case DeviceType::Focuser:
            return "Focuser";
        case DeviceType::FilterWheel:
            return "FilterWheel";
        case DeviceType::Dome:
            return "Dome";
        case DeviceType::Rotator:
            return "Rotator";
        case DeviceType::Weather:
            return "Weather";
        case DeviceType::GPS:
            return "GPS";
        case DeviceType::Guider:
            return "Guider";
        case DeviceType::AuxiliaryDevice:
            return "AuxiliaryDevice";
        case DeviceType::SafetyMonitor:
            return "SafetyMonitor";
        case DeviceType::Switch:
            return "Switch";
        case DeviceType::CoverCalibrator:
            return "CoverCalibrator";
        case DeviceType::ObservingConditions:
            return "ObservingConditions";
        case DeviceType::Video:
            return "Video";
        default:
            return "Unknown";
    }
}

/**
 * @brief Convert string to DeviceType
 */
inline auto stringToDeviceType(const std::string& str) -> DeviceType {
    static const std::unordered_map<std::string, DeviceType> typeMap = {
        {"Camera", DeviceType::Camera},
        {"camera", DeviceType::Camera},
        {"CCD", DeviceType::Camera},
        {"ccd", DeviceType::Camera},
        {"Telescope", DeviceType::Telescope},
        {"telescope", DeviceType::Telescope},
        {"Mount", DeviceType::Telescope},
        {"mount", DeviceType::Telescope},
        {"Focuser", DeviceType::Focuser},
        {"focuser", DeviceType::Focuser},
        {"FilterWheel", DeviceType::FilterWheel},
        {"filterwheel", DeviceType::FilterWheel},
        {"Filter Wheel", DeviceType::FilterWheel},
        {"Dome", DeviceType::Dome},
        {"dome", DeviceType::Dome},
        {"Rotator", DeviceType::Rotator},
        {"rotator", DeviceType::Rotator},
        {"Weather", DeviceType::Weather},
        {"weather", DeviceType::Weather},
        {"GPS", DeviceType::GPS},
        {"gps", DeviceType::GPS},
        {"Guider", DeviceType::Guider},
        {"guider", DeviceType::Guider},
        {"AuxiliaryDevice", DeviceType::AuxiliaryDevice},
        {"Auxiliary", DeviceType::AuxiliaryDevice},
        {"SafetyMonitor", DeviceType::SafetyMonitor},
        {"Switch", DeviceType::Switch},
        {"switch", DeviceType::Switch},
        {"CoverCalibrator", DeviceType::CoverCalibrator},
        {"ObservingConditions", DeviceType::ObservingConditions},
        {"Video", DeviceType::Video},
        {"video", DeviceType::Video}};

    auto it = typeMap.find(str);
    if (it != typeMap.end()) {
        return it->second;
    }
    return DeviceType::Unknown;
}

/**
 * @brief Get all supported device types
 */
inline auto getAllDeviceTypes() -> std::vector<DeviceType> {
    return {DeviceType::Camera,
            DeviceType::Telescope,
            DeviceType::Focuser,
            DeviceType::FilterWheel,
            DeviceType::Dome,
            DeviceType::Rotator,
            DeviceType::Weather,
            DeviceType::GPS,
            DeviceType::Guider,
            DeviceType::AuxiliaryDevice,
            DeviceType::SafetyMonitor,
            DeviceType::Switch,
            DeviceType::CoverCalibrator,
            DeviceType::ObservingConditions,
            DeviceType::Video};
}

/**
 * @brief Check if device type is supported
 */
inline auto isDeviceTypeSupported(DeviceType type) -> bool {
    return type != DeviceType::Unknown;
}

/**
 * @brief INDI interface flags to device types mapping
 */
inline auto indiInterfaceToDeviceTypes(uint32_t interfaces)
    -> std::vector<DeviceType> {
    std::vector<DeviceType> types;

    // INDI interface flags (from indicom.h)
    constexpr uint32_t INDI_TELESCOPE = 1 << 0;
    constexpr uint32_t INDI_CCD = 1 << 1;
    constexpr uint32_t INDI_GUIDER = 1 << 2;
    constexpr uint32_t INDI_FOCUSER = 1 << 3;
    constexpr uint32_t INDI_FILTER = 1 << 4;
    constexpr uint32_t INDI_DOME = 1 << 5;
    constexpr uint32_t INDI_GPS = 1 << 6;
    constexpr uint32_t INDI_WEATHER = 1 << 7;
    constexpr uint32_t INDI_AO = 1 << 8;
    constexpr uint32_t INDI_DUSTCAP = 1 << 9;
    constexpr uint32_t INDI_LIGHTBOX = 1 << 10;
    constexpr uint32_t INDI_DETECTOR = 1 << 11;
    constexpr uint32_t INDI_ROTATOR = 1 << 12;
    constexpr uint32_t INDI_SPECTROGRAPH = 1 << 13;
    constexpr uint32_t INDI_AUX = 1 << 15;

    if (interfaces & INDI_TELESCOPE)
        types.push_back(DeviceType::Telescope);
    if (interfaces & INDI_CCD)
        types.push_back(DeviceType::Camera);
    if (interfaces & INDI_GUIDER)
        types.push_back(DeviceType::Guider);
    if (interfaces & INDI_FOCUSER)
        types.push_back(DeviceType::Focuser);
    if (interfaces & INDI_FILTER)
        types.push_back(DeviceType::FilterWheel);
    if (interfaces & INDI_DOME)
        types.push_back(DeviceType::Dome);
    if (interfaces & INDI_GPS)
        types.push_back(DeviceType::GPS);
    if (interfaces & INDI_WEATHER)
        types.push_back(DeviceType::Weather);
    if (interfaces & INDI_DUSTCAP)
        types.push_back(DeviceType::CoverCalibrator);
    if (interfaces & INDI_LIGHTBOX)
        types.push_back(DeviceType::CoverCalibrator);
    if (interfaces & INDI_ROTATOR)
        types.push_back(DeviceType::Rotator);
    if (interfaces & INDI_AUX)
        types.push_back(DeviceType::AuxiliaryDevice);

    return types;
}

/**
 * @brief ASCOM device type string to DeviceType mapping
 */
inline auto ascomDeviceTypeToDeviceType(const std::string& ascomType)
    -> DeviceType {
    static const std::unordered_map<std::string, DeviceType> typeMap = {
        {"Camera", DeviceType::Camera},
        {"Telescope", DeviceType::Telescope},
        {"Focuser", DeviceType::Focuser},
        {"FilterWheel", DeviceType::FilterWheel},
        {"Dome", DeviceType::Dome},
        {"Rotator", DeviceType::Rotator},
        {"SafetyMonitor", DeviceType::SafetyMonitor},
        {"Switch", DeviceType::Switch},
        {"CoverCalibrator", DeviceType::CoverCalibrator},
        {"ObservingConditions", DeviceType::ObservingConditions},
        {"Video", DeviceType::Video}};

    auto it = typeMap.find(ascomType);
    if (it != typeMap.end()) {
        return it->second;
    }
    return DeviceType::Unknown;
}

/**
 * @brief Device capability flags
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

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j;
        j["canConnect"] = canConnect;
        j["canDisconnect"] = canDisconnect;
        j["canAbort"] = canAbort;
        j["canPark"] = canPark;
        j["canHome"] = canHome;
        j["canSync"] = canSync;
        j["canSlew"] = canSlew;
        j["canTrack"] = canTrack;
        j["canGuide"] = canGuide;
        j["canCool"] = canCool;
        j["canFocus"] = canFocus;
        j["canRotate"] = canRotate;
        j["hasShutter"] = hasShutter;
        j["hasTemperature"] = hasTemperature;
        j["hasPosition"] = hasPosition;
        return j;
    }
};

/**
 * @brief Get default capabilities for a device type
 */
inline auto getDefaultCapabilities(DeviceType type) -> DeviceCapabilities {
    DeviceCapabilities caps;

    switch (type) {
        case DeviceType::Camera:
            caps.canAbort = true;
            caps.canCool = true;
            caps.hasTemperature = true;
            break;
        case DeviceType::Telescope:
            caps.canAbort = true;
            caps.canPark = true;
            caps.canHome = true;
            caps.canSync = true;
            caps.canSlew = true;
            caps.canTrack = true;
            caps.canGuide = true;
            caps.hasPosition = true;
            break;
        case DeviceType::Focuser:
            caps.canAbort = true;
            caps.canFocus = true;
            caps.hasPosition = true;
            caps.hasTemperature = true;
            break;
        case DeviceType::FilterWheel:
            caps.hasPosition = true;
            break;
        case DeviceType::Dome:
            caps.canAbort = true;
            caps.canPark = true;
            caps.canHome = true;
            caps.hasShutter = true;
            caps.hasPosition = true;
            break;
        case DeviceType::Rotator:
            caps.canAbort = true;
            caps.canRotate = true;
            caps.hasPosition = true;
            break;
        case DeviceType::Guider:
            caps.canGuide = true;
            break;
        default:
            break;
    }

    return caps;
}

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_SERVICE_DEVICE_TYPES_HPP
