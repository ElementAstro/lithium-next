/*
 * indigo_device_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "indigo_device_factory.hpp"

#include "indigo_camera.hpp"
#include "indigo_focuser.hpp"
#include "indigo_filterwheel.hpp"
#include "indigo_telescope.hpp"
#include "indigo_dome.hpp"
#include "indigo_rotator.hpp"
#include "indigo_weather.hpp"
#include "indigo_gps.hpp"

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

INDIGODeviceFactory::INDIGODeviceFactory() {
    registerDefaultCreators();
}

auto INDIGODeviceFactory::getInstance() -> INDIGODeviceFactory& {
    static INDIGODeviceFactory instance;
    return instance;
}

void INDIGODeviceFactory::registerDefaultCreators() {
    // Camera
    creators_["Camera"] = [](const std::string& name, std::shared_ptr<INDIGOClient> client)
        -> std::shared_ptr<INDIGODeviceBase> {
        auto device = std::make_shared<INDIGOCamera>(name);
        if (client) device->setClient(client);
        return device;
    };

    // Focuser
    creators_["Focuser"] = [](const std::string& name, std::shared_ptr<INDIGOClient> client)
        -> std::shared_ptr<INDIGODeviceBase> {
        auto device = std::make_shared<INDIGOFocuser>(name);
        if (client) device->setClient(client);
        return device;
    };

    // FilterWheel
    creators_["FilterWheel"] = [](const std::string& name, std::shared_ptr<INDIGOClient> client)
        -> std::shared_ptr<INDIGODeviceBase> {
        auto device = std::make_shared<INDIGOFilterWheel>(name);
        if (client) device->setClient(client);
        return device;
    };

    // Telescope/Mount
    creators_["Telescope"] = [](const std::string& name, std::shared_ptr<INDIGOClient> client)
        -> std::shared_ptr<INDIGODeviceBase> {
        auto device = std::make_shared<INDIGOTelescope>(name);
        if (client) device->setClient(client);
        return device;
    };
    creators_["Mount"] = creators_["Telescope"];

    // Dome
    creators_["Dome"] = [](const std::string& name, std::shared_ptr<INDIGOClient> client)
        -> std::shared_ptr<INDIGODeviceBase> {
        auto device = std::make_shared<INDIGODome>(name);
        if (client) device->setClient(client);
        return device;
    };

    // Rotator
    creators_["Rotator"] = [](const std::string& name, std::shared_ptr<INDIGOClient> client)
        -> std::shared_ptr<INDIGODeviceBase> {
        auto device = std::make_shared<INDIGORotator>(name);
        if (client) device->setClient(client);
        return device;
    };

    // Weather
    creators_["Weather"] = [](const std::string& name, std::shared_ptr<INDIGOClient> client)
        -> std::shared_ptr<INDIGODeviceBase> {
        auto device = std::make_shared<INDIGOWeather>(name);
        if (client) device->setClient(client);
        return device;
    };

    // GPS
    creators_["GPS"] = [](const std::string& name, std::shared_ptr<INDIGOClient> client)
        -> std::shared_ptr<INDIGODeviceBase> {
        auto device = std::make_shared<INDIGOGPS>(name);
        if (client) device->setClient(client);
        return device;
    };

    LOG_F(INFO, "INDIGO DeviceFactory: Registered {} device creators", creators_.size());
}

auto INDIGODeviceFactory::createDevice(INDIGODeviceType type, const std::string& deviceName,
                                       std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGODeviceBase>> {
    return createDevice(deviceTypeToString(type), deviceName, client);
}

auto INDIGODeviceFactory::createDevice(const std::string& typeName, const std::string& deviceName,
                                       std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGODeviceBase>> {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = creators_.find(typeName);
    if (it == creators_.end()) {
        return DeviceError{DeviceErrorCode::InvalidDeviceType,
                           "Unknown device type: " + typeName};
    }

    try {
        auto device = it->second(deviceName, client);
        LOG_F(INFO, "INDIGO DeviceFactory: Created {} device '{}'", typeName, deviceName);
        return device;
    } catch (const std::exception& e) {
        return DeviceError{DeviceErrorCode::CreationFailed,
                           std::string("Failed to create device: ") + e.what()};
    }
}

auto INDIGODeviceFactory::createDevice(const DiscoveredDevice& device,
                                       std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGODeviceBase>> {
    auto type = inferDeviceType(device.interfaces);
    return createDevice(type, device.name, client);
}

auto INDIGODeviceFactory::createCamera(const std::string& deviceName,
                                       std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGOCamera>> {
    auto result = createDevice("Camera", deviceName, client);
    if (!result.has_value()) {
        return DeviceError{result.error()};
    }
    return std::dynamic_pointer_cast<INDIGOCamera>(result.value());
}

auto INDIGODeviceFactory::createFocuser(const std::string& deviceName,
                                        std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGOFocuser>> {
    auto result = createDevice("Focuser", deviceName, client);
    if (!result.has_value()) {
        return DeviceError{result.error()};
    }
    return std::dynamic_pointer_cast<INDIGOFocuser>(result.value());
}

auto INDIGODeviceFactory::createFilterWheel(const std::string& deviceName,
                                            std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGOFilterWheel>> {
    auto result = createDevice("FilterWheel", deviceName, client);
    if (!result.has_value()) {
        return DeviceError{result.error()};
    }
    return std::dynamic_pointer_cast<INDIGOFilterWheel>(result.value());
}

auto INDIGODeviceFactory::createTelescope(const std::string& deviceName,
                                          std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGOTelescope>> {
    auto result = createDevice("Telescope", deviceName, client);
    if (!result.has_value()) {
        return DeviceError{result.error()};
    }
    return std::dynamic_pointer_cast<INDIGOTelescope>(result.value());
}

auto INDIGODeviceFactory::createDome(const std::string& deviceName,
                                     std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGODome>> {
    auto result = createDevice("Dome", deviceName, client);
    if (!result.has_value()) {
        return DeviceError{result.error()};
    }
    return std::dynamic_pointer_cast<INDIGODome>(result.value());
}

auto INDIGODeviceFactory::createRotator(const std::string& deviceName,
                                        std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGORotator>> {
    auto result = createDevice("Rotator", deviceName, client);
    if (!result.has_value()) {
        return DeviceError{result.error()};
    }
    return std::dynamic_pointer_cast<INDIGORotator>(result.value());
}

auto INDIGODeviceFactory::createWeather(const std::string& deviceName,
                                        std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGOWeather>> {
    auto result = createDevice("Weather", deviceName, client);
    if (!result.has_value()) {
        return DeviceError{result.error()};
    }
    return std::dynamic_pointer_cast<INDIGOWeather>(result.value());
}

auto INDIGODeviceFactory::createGPS(const std::string& deviceName,
                                    std::shared_ptr<INDIGOClient> client)
    -> DeviceResult<std::shared_ptr<INDIGOGPS>> {
    auto result = createDevice("GPS", deviceName, client);
    if (!result.has_value()) {
        return DeviceError{result.error()};
    }
    return std::dynamic_pointer_cast<INDIGOGPS>(result.value());
}

void INDIGODeviceFactory::registerCreator(const std::string& typeName, DeviceCreator creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    creators_[typeName] = std::move(creator);
    LOG_F(INFO, "INDIGO DeviceFactory: Registered creator for '{}'", typeName);
}

void INDIGODeviceFactory::unregisterCreator(const std::string& typeName) {
    std::lock_guard<std::mutex> lock(mutex_);
    creators_.erase(typeName);
}

auto INDIGODeviceFactory::isTypeSupported(const std::string& typeName) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return creators_.find(typeName) != creators_.end();
}

auto INDIGODeviceFactory::getSupportedTypes() const -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> types;
    types.reserve(creators_.size());
    for (const auto& [name, _] : creators_) {
        types.push_back(name);
    }
    return types;
}

auto INDIGODeviceFactory::inferDeviceType(DeviceInterface interfaces) -> INDIGODeviceType {
    // Priority order for interface detection
    if (hasInterface(interfaces, DeviceInterface::CCD)) return INDIGODeviceType::Camera;
    if (hasInterface(interfaces, DeviceInterface::Mount)) return INDIGODeviceType::Telescope;
    if (hasInterface(interfaces, DeviceInterface::Focuser)) return INDIGODeviceType::Focuser;
    if (hasInterface(interfaces, DeviceInterface::FilterWheel)) return INDIGODeviceType::FilterWheel;
    if (hasInterface(interfaces, DeviceInterface::Dome)) return INDIGODeviceType::Dome;
    if (hasInterface(interfaces, DeviceInterface::Rotator)) return INDIGODeviceType::Rotator;
    if (hasInterface(interfaces, DeviceInterface::Weather)) return INDIGODeviceType::Weather;
    if (hasInterface(interfaces, DeviceInterface::GPS)) return INDIGODeviceType::GPS;
    if (hasInterface(interfaces, DeviceInterface::Guider)) return INDIGODeviceType::Guider;
    if (hasInterface(interfaces, DeviceInterface::AO)) return INDIGODeviceType::AO;
    if (hasInterface(interfaces, DeviceInterface::Dustcap)) return INDIGODeviceType::Dustcap;
    if (hasInterface(interfaces, DeviceInterface::Lightbox)) return INDIGODeviceType::Lightbox;

    return INDIGODeviceType::Unknown;
}

auto INDIGODeviceFactory::deviceTypeToString(INDIGODeviceType type) -> std::string {
    switch (type) {
        case INDIGODeviceType::Camera: return "Camera";
        case INDIGODeviceType::Focuser: return "Focuser";
        case INDIGODeviceType::FilterWheel: return "FilterWheel";
        case INDIGODeviceType::Telescope: return "Telescope";
        case INDIGODeviceType::Dome: return "Dome";
        case INDIGODeviceType::Rotator: return "Rotator";
        case INDIGODeviceType::Weather: return "Weather";
        case INDIGODeviceType::GPS: return "GPS";
        case INDIGODeviceType::Guider: return "Guider";
        case INDIGODeviceType::AO: return "AO";
        case INDIGODeviceType::Dustcap: return "Dustcap";
        case INDIGODeviceType::Lightbox: return "Lightbox";
        case INDIGODeviceType::Detector: return "Detector";
        case INDIGODeviceType::Spectrograph: return "Spectrograph";
        case INDIGODeviceType::Aux: return "Aux";
        default: return "Unknown";
    }
}

auto INDIGODeviceFactory::deviceTypeFromString(const std::string& typeName) -> INDIGODeviceType {
    if (typeName == "Camera" || typeName == "CCD") return INDIGODeviceType::Camera;
    if (typeName == "Focuser") return INDIGODeviceType::Focuser;
    if (typeName == "FilterWheel" || typeName == "Wheel") return INDIGODeviceType::FilterWheel;
    if (typeName == "Telescope" || typeName == "Mount") return INDIGODeviceType::Telescope;
    if (typeName == "Dome") return INDIGODeviceType::Dome;
    if (typeName == "Rotator") return INDIGODeviceType::Rotator;
    if (typeName == "Weather") return INDIGODeviceType::Weather;
    if (typeName == "GPS") return INDIGODeviceType::GPS;
    if (typeName == "Guider") return INDIGODeviceType::Guider;
    if (typeName == "AO") return INDIGODeviceType::AO;
    if (typeName == "Dustcap") return INDIGODeviceType::Dustcap;
    if (typeName == "Lightbox") return INDIGODeviceType::Lightbox;
    if (typeName == "Detector") return INDIGODeviceType::Detector;
    if (typeName == "Spectrograph") return INDIGODeviceType::Spectrograph;
    if (typeName == "Aux") return INDIGODeviceType::Aux;
    return INDIGODeviceType::Unknown;
}

auto INDIGODeviceFactory::getOrCreateClient(const std::string& host, int port)
    -> std::shared_ptr<INDIGOClient> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string key = host + ":" + std::to_string(port);
    auto it = clientPool_.find(key);
    if (it != clientPool_.end() && it->second) {
        return it->second;
    }

    INDIGOClient::Config config;
    config.host = host;
    config.port = port;
    auto client = std::make_shared<INDIGOClient>(config);
    clientPool_[key] = client;

    LOG_F(INFO, "INDIGO DeviceFactory: Created client for {}:{}", host, port);
    return client;
}

void INDIGODeviceFactory::releaseClient(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = host + ":" + std::to_string(port);
    clientPool_.erase(key);
}

void INDIGODeviceFactory::clearClientPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    clientPool_.clear();
    LOG_F(INFO, "INDIGO DeviceFactory: Cleared client pool");
}

}  // namespace lithium::client::indigo
