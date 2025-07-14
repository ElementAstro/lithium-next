/*
 * device_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Device Factory for creating different device types

*************************************************/

#pragma once

#include "template/device.hpp"
#include "template/camera.hpp"
#include "template/telescope.hpp"
#include "template/focuser.hpp"
#include "template/filterwheel.hpp"
#include "template/rotator.hpp"
#include "template/dome.hpp"
#include "template/guider.hpp"
#include "template/weather.hpp"
#include "template/safety_monitor.hpp"
#include "template/adaptive_optics.hpp"

// Mock implementations
#include "template/mock/mock_camera.hpp"
#include "template/mock/mock_telescope.hpp"
#include "template/mock/mock_focuser.hpp"
#include "template/mock/mock_filterwheel.hpp"
#include "template/mock/mock_rotator.hpp"
#include "template/mock/mock_dome.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

enum class DeviceType {
    CAMERA,
    TELESCOPE,
    FOCUSER,
    FILTERWHEEL,
    ROTATOR,
    DOME,
    GUIDER,
    WEATHER_STATION,
    SAFETY_MONITOR,
    ADAPTIVE_OPTICS,
    UNKNOWN
};

enum class DeviceBackend {
    MOCK,
    INDI,
    ASCOM,
    NATIVE
};

class DeviceFactory {
public:
    static DeviceFactory& getInstance() {
        static DeviceFactory instance;
        return instance;
    }

    // Factory methods for creating devices
    std::unique_ptr<AtomCamera> createCamera(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomTelescope> createTelescope(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomFocuser> createFocuser(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomFilterWheel> createFilterWheel(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomRotator> createRotator(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);
    std::unique_ptr<AtomDome> createDome(const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);

    // Generic device creation
    std::unique_ptr<AtomDriver> createDevice(DeviceType type, const std::string& name, DeviceBackend backend = DeviceBackend::MOCK);

    // Device type utilities
    static DeviceType stringToDeviceType(const std::string& typeStr);
    static std::string deviceTypeToString(DeviceType type);
    static DeviceBackend stringToBackend(const std::string& backendStr);
    static std::string backendToString(DeviceBackend backend);

    // Available device backends
    std::vector<DeviceBackend> getAvailableBackends(DeviceType type) const;
    bool isBackendAvailable(DeviceType type, DeviceBackend backend) const;

    // Device discovery
    struct DeviceInfo {
        std::string name;
        DeviceType type;
        DeviceBackend backend;
        std::string description;
        std::string version;
    };

    std::vector<DeviceInfo> discoverDevices(DeviceType type = DeviceType::UNKNOWN, DeviceBackend backend = DeviceBackend::MOCK) const;

    // Registry for custom device creators
    using DeviceCreator = std::function<std::unique_ptr<AtomDriver>(const std::string&)>;
    void registerDeviceCreator(DeviceType type, DeviceBackend backend, DeviceCreator creator);

private:
    DeviceFactory() = default;
    ~DeviceFactory() = default;

    // Disable copy and assignment
    DeviceFactory(const DeviceFactory&) = delete;
    DeviceFactory& operator=(const DeviceFactory&) = delete;

    // Registry of custom device creators
    std::unordered_map<std::string, DeviceCreator> device_creators_;

    // Helper methods
    std::string makeRegistryKey(DeviceType type, DeviceBackend backend) const;

    // Backend availability checking
    bool isINDIAvailable() const;
    bool isASCOMAvailable() const;
};

// Inline implementations
inline DeviceType DeviceFactory::stringToDeviceType(const std::string& typeStr) {
    if (typeStr == "camera") return DeviceType::CAMERA;
    if (typeStr == "telescope") return DeviceType::TELESCOPE;
    if (typeStr == "focuser") return DeviceType::FOCUSER;
    if (typeStr == "filterwheel") return DeviceType::FILTERWHEEL;
    if (typeStr == "rotator") return DeviceType::ROTATOR;
    if (typeStr == "dome") return DeviceType::DOME;
    if (typeStr == "guider") return DeviceType::GUIDER;
    if (typeStr == "weather") return DeviceType::WEATHER_STATION;
    if (typeStr == "safety") return DeviceType::SAFETY_MONITOR;
    if (typeStr == "ao") return DeviceType::ADAPTIVE_OPTICS;
    return DeviceType::UNKNOWN;
}

inline std::string DeviceFactory::deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::CAMERA: return "camera";
        case DeviceType::TELESCOPE: return "telescope";
        case DeviceType::FOCUSER: return "focuser";
        case DeviceType::FILTERWHEEL: return "filterwheel";
        case DeviceType::ROTATOR: return "rotator";
        case DeviceType::DOME: return "dome";
        case DeviceType::GUIDER: return "guider";
        case DeviceType::WEATHER_STATION: return "weather";
        case DeviceType::SAFETY_MONITOR: return "safety";
        case DeviceType::ADAPTIVE_OPTICS: return "ao";
        default: return "unknown";
    }
}

inline DeviceBackend DeviceFactory::stringToBackend(const std::string& backendStr) {
    if (backendStr == "mock") return DeviceBackend::MOCK;
    if (backendStr == "indi") return DeviceBackend::INDI;
    if (backendStr == "ascom") return DeviceBackend::ASCOM;
    if (backendStr == "native") return DeviceBackend::NATIVE;
    return DeviceBackend::MOCK;
}

inline std::string DeviceFactory::backendToString(DeviceBackend backend) {
    switch (backend) {
        case DeviceBackend::MOCK: return "mock";
        case DeviceBackend::INDI: return "indi";
        case DeviceBackend::ASCOM: return "ascom";
        case DeviceBackend::NATIVE: return "native";
        default: return "mock";
    }
}

inline std::string DeviceFactory::makeRegistryKey(DeviceType type, DeviceBackend backend) const {
    return deviceTypeToString(type) + "_" + backendToString(backend);
}
