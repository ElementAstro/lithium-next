/*
 * indi_device_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Device Factory Implementation

**************************************************/

#include "indi_device_factory.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== INDIDeviceFactory ====================

auto INDIDeviceFactory::getInstance() -> INDIDeviceFactory& {
    static INDIDeviceFactory instance;
    return instance;
}

INDIDeviceFactory::INDIDeviceFactory() { registerDefaultCreators(); }

void INDIDeviceFactory::registerDefaultCreators() {
    // Register camera creator
    creators_[DeviceType::Camera] = [](const std::string& name) {
        return std::make_shared<INDICamera>(name);
    };

    // Register focuser creator
    creators_[DeviceType::Focuser] = [](const std::string& name) {
        return std::make_shared<INDIFocuser>(name);
    };

    // Register filterwheel creator
    creators_[DeviceType::FilterWheel] = [](const std::string& name) {
        return std::make_shared<INDIFilterWheel>(name);
    };

    // Register telescope creator
    creators_[DeviceType::Telescope] = [](const std::string& name) {
        return std::make_shared<INDITelescope>(name);
    };

    // Register rotator creator
    creators_[DeviceType::Rotator] = [](const std::string& name) {
        return std::make_shared<INDIRotator>(name);
    };

    // Register dome creator
    creators_[DeviceType::Dome] = [](const std::string& name) {
        return std::make_shared<INDIDome>(name);
    };

    // Register weather creator
    creators_[DeviceType::Weather] = [](const std::string& name) {
        return std::make_shared<INDIWeather>(name);
    };

    // Register GPS creator
    creators_[DeviceType::GPS] = [](const std::string& name) {
        return std::make_shared<INDIGPS>(name);
    };

    // Register type mappings
    typeMap_["Camera"] = DeviceType::Camera;
    typeMap_["CCD"] = DeviceType::Camera;
    typeMap_["Focuser"] = DeviceType::Focuser;
    typeMap_["FilterWheel"] = DeviceType::FilterWheel;
    typeMap_["Filter Wheel"] = DeviceType::FilterWheel;
    typeMap_["Telescope"] = DeviceType::Telescope;
    typeMap_["Mount"] = DeviceType::Telescope;
    typeMap_["Rotator"] = DeviceType::Rotator;
    typeMap_["Dome"] = DeviceType::Dome;
    typeMap_["Weather"] = DeviceType::Weather;
    typeMap_["Weather Station"] = DeviceType::Weather;
    typeMap_["GPS"] = DeviceType::GPS;
}

auto INDIDeviceFactory::createDevice(DeviceType type, const std::string& name)
    -> std::shared_ptr<INDIDeviceBase> {
    auto it = creators_.find(type);
    if (it != creators_.end()) {
        LOG_DEBUG("Creating device: type={}, name={}", deviceTypeToString(type),
                  name);
        return it->second(name);
    }
    LOG_ERROR("Unknown device type: {}", static_cast<int>(type));
    return nullptr;
}

auto INDIDeviceFactory::createDevice(const std::string& typeStr,
                                     const std::string& name)
    -> std::shared_ptr<INDIDeviceBase> {
    auto it = typeMap_.find(typeStr);
    if (it != typeMap_.end()) {
        return createDevice(it->second, name);
    }
    LOG_ERROR("Unknown device type string: {}", typeStr);
    return nullptr;
}

auto INDIDeviceFactory::createCamera(const std::string& name)
    -> std::shared_ptr<INDICamera> {
    return std::make_shared<INDICamera>(name);
}

auto INDIDeviceFactory::createFocuser(const std::string& name)
    -> std::shared_ptr<INDIFocuser> {
    return std::make_shared<INDIFocuser>(name);
}

auto INDIDeviceFactory::createFilterWheel(const std::string& name)
    -> std::shared_ptr<INDIFilterWheel> {
    return std::make_shared<INDIFilterWheel>(name);
}

auto INDIDeviceFactory::createTelescope(const std::string& name)
    -> std::shared_ptr<INDITelescope> {
    return std::make_shared<INDITelescope>(name);
}

auto INDIDeviceFactory::createRotator(const std::string& name)
    -> std::shared_ptr<INDIRotator> {
    return std::make_shared<INDIRotator>(name);
}

auto INDIDeviceFactory::createDome(const std::string& name)
    -> std::shared_ptr<INDIDome> {
    return std::make_shared<INDIDome>(name);
}

auto INDIDeviceFactory::createWeather(const std::string& name)
    -> std::shared_ptr<INDIWeather> {
    return std::make_shared<INDIWeather>(name);
}

auto INDIDeviceFactory::createGPS(const std::string& name)
    -> std::shared_ptr<INDIGPS> {
    return std::make_shared<INDIGPS>(name);
}

void INDIDeviceFactory::registerCreator(DeviceType type,
                                        DeviceCreator creator) {
    creators_[type] = std::move(creator);
    LOG_DEBUG("Registered custom creator for type: {}",
              deviceTypeToString(type));
}

void INDIDeviceFactory::registerCreator(const std::string& typeStr,
                                        DeviceCreator creator) {
    auto type = deviceTypeFromString(typeStr);
    if (type == DeviceType::Unknown) {
        // Create a new type mapping
        type = static_cast<DeviceType>(static_cast<int>(DeviceType::Unknown) +
                                       typeMap_.size() + 1);
        typeMap_[typeStr] = type;
    }
    registerCreator(type, std::move(creator));
}

auto INDIDeviceFactory::isSupported(DeviceType type) const -> bool {
    return creators_.find(type) != creators_.end();
}

auto INDIDeviceFactory::getSupportedTypes() const -> std::vector<DeviceType> {
    std::vector<DeviceType> types;
    types.reserve(creators_.size());
    for (const auto& [type, _] : creators_) {
        types.push_back(type);
    }
    return types;
}

// ==================== INDIDeviceManager ====================

INDIDeviceManager::~INDIDeviceManager() { destroyAll(); }

auto INDIDeviceManager::addDevice(std::shared_ptr<INDIDeviceBase> device)
    -> bool {
    if (!device) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto& name = device->getName();

    if (devices_.find(name) != devices_.end()) {
        LOG_WARN("Device already exists: {}", name);
        return false;
    }

    devices_[name] = std::move(device);
    LOG_DEBUG("Added device: {}", name);
    return true;
}

auto INDIDeviceManager::removeDevice(const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = devices_.find(name);
    if (it == devices_.end()) {
        return false;
    }

    if (it->second->isConnected()) {
        it->second->disconnect();
    }
    it->second->destroy();

    devices_.erase(it);
    LOG_DEBUG("Removed device: {}", name);
    return true;
}

auto INDIDeviceManager::getDevice(const std::string& name) const
    -> std::shared_ptr<INDIDeviceBase> {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = devices_.find(name);
    if (it != devices_.end()) {
        return it->second;
    }
    return nullptr;
}

auto INDIDeviceManager::getDevices() const
    -> std::vector<std::shared_ptr<INDIDeviceBase>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDIDeviceBase>> result;
    result.reserve(devices_.size());
    for (const auto& [_, device] : devices_) {
        result.push_back(device);
    }
    return result;
}

auto INDIDeviceManager::getDevicesByType(DeviceType type) const
    -> std::vector<std::shared_ptr<INDIDeviceBase>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDIDeviceBase>> result;
    std::string typeStr = deviceTypeToString(type);

    for (const auto& [_, device] : devices_) {
        if (device->getDeviceType() == typeStr) {
            result.push_back(device);
        }
    }
    return result;
}

auto INDIDeviceManager::getCameras() const
    -> std::vector<std::shared_ptr<INDICamera>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDICamera>> result;
    for (const auto& [_, device] : devices_) {
        if (auto camera = std::dynamic_pointer_cast<INDICamera>(device)) {
            result.push_back(camera);
        }
    }
    return result;
}

auto INDIDeviceManager::getFocusers() const
    -> std::vector<std::shared_ptr<INDIFocuser>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDIFocuser>> result;
    for (const auto& [_, device] : devices_) {
        if (auto focuser = std::dynamic_pointer_cast<INDIFocuser>(device)) {
            result.push_back(focuser);
        }
    }
    return result;
}

auto INDIDeviceManager::getFilterWheels() const
    -> std::vector<std::shared_ptr<INDIFilterWheel>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDIFilterWheel>> result;
    for (const auto& [_, device] : devices_) {
        if (auto fw = std::dynamic_pointer_cast<INDIFilterWheel>(device)) {
            result.push_back(fw);
        }
    }
    return result;
}

auto INDIDeviceManager::getTelescopes() const
    -> std::vector<std::shared_ptr<INDITelescope>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDITelescope>> result;
    for (const auto& [_, device] : devices_) {
        if (auto telescope = std::dynamic_pointer_cast<INDITelescope>(device)) {
            result.push_back(telescope);
        }
    }
    return result;
}

auto INDIDeviceManager::getRotators() const
    -> std::vector<std::shared_ptr<INDIRotator>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDIRotator>> result;
    for (const auto& [_, device] : devices_) {
        if (auto rotator = std::dynamic_pointer_cast<INDIRotator>(device)) {
            result.push_back(rotator);
        }
    }
    return result;
}

auto INDIDeviceManager::getDomes() const
    -> std::vector<std::shared_ptr<INDIDome>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDIDome>> result;
    for (const auto& [_, device] : devices_) {
        if (auto dome = std::dynamic_pointer_cast<INDIDome>(device)) {
            result.push_back(dome);
        }
    }
    return result;
}

auto INDIDeviceManager::getWeatherStations() const
    -> std::vector<std::shared_ptr<INDIWeather>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDIWeather>> result;
    for (const auto& [_, device] : devices_) {
        if (auto weather = std::dynamic_pointer_cast<INDIWeather>(device)) {
            result.push_back(weather);
        }
    }
    return result;
}

auto INDIDeviceManager::getGPSDevices() const
    -> std::vector<std::shared_ptr<INDIGPS>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<INDIGPS>> result;
    for (const auto& [_, device] : devices_) {
        if (auto gps = std::dynamic_pointer_cast<INDIGPS>(device)) {
            result.push_back(gps);
        }
    }
    return result;
}

auto INDIDeviceManager::hasDevice(const std::string& name) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.find(name) != devices_.end();
}

auto INDIDeviceManager::getDeviceCount() const -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.size();
}

auto INDIDeviceManager::connectAll() -> int {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    for (auto& [name, device] : devices_) {
        if (!device->isConnected()) {
            if (device->connect(name)) {
                ++count;
            }
        } else {
            ++count;
        }
    }
    return count;
}

auto INDIDeviceManager::disconnectAll() -> int {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    for (auto& [_, device] : devices_) {
        if (device->isConnected()) {
            if (device->disconnect()) {
                ++count;
            }
        } else {
            ++count;
        }
    }
    return count;
}

auto INDIDeviceManager::initializeAll() -> int {
    std::lock_guard<std::mutex> lock(mutex_);

    int count = 0;
    for (auto& [_, device] : devices_) {
        if (device->initialize()) {
            ++count;
        }
    }
    return count;
}

void INDIDeviceManager::destroyAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [_, device] : devices_) {
        if (device->isConnected()) {
            device->disconnect();
        }
        device->destroy();
    }
    devices_.clear();
}

void INDIDeviceManager::clear() { destroyAll(); }

}  // namespace lithium::client::indi
