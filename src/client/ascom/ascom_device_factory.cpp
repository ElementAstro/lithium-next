/*
 * ascom_device_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Device Factory Implementation

**************************************************/

#include "ascom_device_factory.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client::ascom {

// ==================== ASCOMDeviceFactory ====================

ASCOMDeviceFactory& ASCOMDeviceFactory::getInstance() {
    static ASCOMDeviceFactory instance;
    return instance;
}

ASCOMDeviceFactory::ASCOMDeviceFactory() {
    registerDefaultCreators();
}

void ASCOMDeviceFactory::registerDefaultCreators() {
    creators_[ASCOMDeviceType::Camera] = [](const std::string& name, int num) {
        return std::make_shared<ASCOMCamera>(name, num);
    };

    creators_[ASCOMDeviceType::Focuser] = [](const std::string& name, int num) {
        return std::make_shared<ASCOMFocuser>(name, num);
    };

    creators_[ASCOMDeviceType::FilterWheel] = [](const std::string& name, int num) {
        return std::make_shared<ASCOMFilterWheel>(name, num);
    };

    creators_[ASCOMDeviceType::Telescope] = [](const std::string& name, int num) {
        return std::make_shared<ASCOMTelescope>(name, num);
    };

    creators_[ASCOMDeviceType::Rotator] = [](const std::string& name, int num) {
        return std::make_shared<ASCOMRotator>(name, num);
    };

    creators_[ASCOMDeviceType::Dome] = [](const std::string& name, int num) {
        return std::make_shared<ASCOMDome>(name, num);
    };

    creators_[ASCOMDeviceType::ObservingConditions] = [](const std::string& name, int num) {
        return std::make_shared<ASCOMObservingConditions>(name, num);
    };
}

auto ASCOMDeviceFactory::createDevice(ASCOMDeviceType type, const std::string& name,
                                      int deviceNumber) -> std::shared_ptr<ASCOMDeviceBase> {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = creators_.find(type);
    if (it != creators_.end()) {
        spdlog::debug("Creating ASCOM device: type={}, name={}, number={}",
                      deviceTypeToString(type), name, deviceNumber);
        return it->second(name, deviceNumber);
    }

    spdlog::error("Unknown ASCOM device type: {}", static_cast<int>(type));
    return nullptr;
}

auto ASCOMDeviceFactory::createDevice(const std::string& typeStr, const std::string& name,
                                      int deviceNumber) -> std::shared_ptr<ASCOMDeviceBase> {
    return createDevice(stringToDeviceType(typeStr), name, deviceNumber);
}

auto ASCOMDeviceFactory::createCamera(const std::string& name, int deviceNumber)
    -> std::shared_ptr<ASCOMCamera> {
    return std::make_shared<ASCOMCamera>(name, deviceNumber);
}

auto ASCOMDeviceFactory::createFocuser(const std::string& name, int deviceNumber)
    -> std::shared_ptr<ASCOMFocuser> {
    return std::make_shared<ASCOMFocuser>(name, deviceNumber);
}

auto ASCOMDeviceFactory::createFilterWheel(const std::string& name, int deviceNumber)
    -> std::shared_ptr<ASCOMFilterWheel> {
    return std::make_shared<ASCOMFilterWheel>(name, deviceNumber);
}

auto ASCOMDeviceFactory::createTelescope(const std::string& name, int deviceNumber)
    -> std::shared_ptr<ASCOMTelescope> {
    return std::make_shared<ASCOMTelescope>(name, deviceNumber);
}

auto ASCOMDeviceFactory::createRotator(const std::string& name, int deviceNumber)
    -> std::shared_ptr<ASCOMRotator> {
    return std::make_shared<ASCOMRotator>(name, deviceNumber);
}

auto ASCOMDeviceFactory::createDome(const std::string& name, int deviceNumber)
    -> std::shared_ptr<ASCOMDome> {
    return std::make_shared<ASCOMDome>(name, deviceNumber);
}

auto ASCOMDeviceFactory::createObservingConditions(const std::string& name, int deviceNumber)
    -> std::shared_ptr<ASCOMObservingConditions> {
    return std::make_shared<ASCOMObservingConditions>(name, deviceNumber);
}

void ASCOMDeviceFactory::registerCreator(ASCOMDeviceType type, DeviceCreator creator) {
    std::lock_guard<std::mutex> lock(mutex_);
    creators_[type] = std::move(creator);
    spdlog::debug("Registered custom creator for ASCOM type: {}",
                  deviceTypeToString(type));
}

auto ASCOMDeviceFactory::isSupported(ASCOMDeviceType type) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return creators_.find(type) != creators_.end();
}

auto ASCOMDeviceFactory::getSupportedTypes() const -> std::vector<ASCOMDeviceType> {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ASCOMDeviceType> types;
    types.reserve(creators_.size());
    for (const auto& [type, _] : creators_) {
        types.push_back(type);
    }
    return types;
}

// ==================== ASCOMDeviceManager ====================

ASCOMDeviceManager::~ASCOMDeviceManager() {
    disconnectAll();
    clear();
}

auto ASCOMDeviceManager::addDevice(std::shared_ptr<ASCOMDeviceBase> device) -> bool {
    if (!device) return false;

    std::lock_guard<std::mutex> lock(mutex_);
    const auto& name = device->getName();

    if (devices_.find(name) != devices_.end()) {
        spdlog::warn("Device already exists: {}", name);
        return false;
    }

    devices_[name] = std::move(device);
    spdlog::debug("Added ASCOM device: {}", name);
    return true;
}

auto ASCOMDeviceManager::removeDevice(const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = devices_.find(name);
    if (it == devices_.end()) {
        return false;
    }

    if (it->second->isConnected()) {
        it->second->disconnect();
    }

    devices_.erase(it);
    spdlog::debug("Removed ASCOM device: {}", name);
    return true;
}

auto ASCOMDeviceManager::getDevice(const std::string& name)
    -> std::shared_ptr<ASCOMDeviceBase> {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = devices_.find(name);
    if (it != devices_.end()) {
        return it->second;
    }
    return nullptr;
}

auto ASCOMDeviceManager::getAllDevices() const
    -> std::vector<std::shared_ptr<ASCOMDeviceBase>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMDeviceBase>> result;
    result.reserve(devices_.size());
    for (const auto& [_, device] : devices_) {
        result.push_back(device);
    }
    return result;
}

auto ASCOMDeviceManager::getDevicesByType(ASCOMDeviceType type) const
    -> std::vector<std::shared_ptr<ASCOMDeviceBase>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMDeviceBase>> result;
    for (const auto& [_, device] : devices_) {
        if (device->getASCOMDeviceType() == type) {
            result.push_back(device);
        }
    }
    return result;
}

auto ASCOMDeviceManager::getCameras() const -> std::vector<std::shared_ptr<ASCOMCamera>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMCamera>> result;
    for (const auto& [_, device] : devices_) {
        if (auto camera = std::dynamic_pointer_cast<ASCOMCamera>(device)) {
            result.push_back(camera);
        }
    }
    return result;
}

auto ASCOMDeviceManager::getFocusers() const -> std::vector<std::shared_ptr<ASCOMFocuser>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMFocuser>> result;
    for (const auto& [_, device] : devices_) {
        if (auto focuser = std::dynamic_pointer_cast<ASCOMFocuser>(device)) {
            result.push_back(focuser);
        }
    }
    return result;
}

auto ASCOMDeviceManager::getFilterWheels() const
    -> std::vector<std::shared_ptr<ASCOMFilterWheel>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMFilterWheel>> result;
    for (const auto& [_, device] : devices_) {
        if (auto fw = std::dynamic_pointer_cast<ASCOMFilterWheel>(device)) {
            result.push_back(fw);
        }
    }
    return result;
}

auto ASCOMDeviceManager::getTelescopes() const
    -> std::vector<std::shared_ptr<ASCOMTelescope>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMTelescope>> result;
    for (const auto& [_, device] : devices_) {
        if (auto telescope = std::dynamic_pointer_cast<ASCOMTelescope>(device)) {
            result.push_back(telescope);
        }
    }
    return result;
}

auto ASCOMDeviceManager::getRotators() const -> std::vector<std::shared_ptr<ASCOMRotator>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMRotator>> result;
    for (const auto& [_, device] : devices_) {
        if (auto rotator = std::dynamic_pointer_cast<ASCOMRotator>(device)) {
            result.push_back(rotator);
        }
    }
    return result;
}

auto ASCOMDeviceManager::getDomes() const -> std::vector<std::shared_ptr<ASCOMDome>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMDome>> result;
    for (const auto& [_, device] : devices_) {
        if (auto dome = std::dynamic_pointer_cast<ASCOMDome>(device)) {
            result.push_back(dome);
        }
    }
    return result;
}

auto ASCOMDeviceManager::getObservingConditions() const
    -> std::vector<std::shared_ptr<ASCOMObservingConditions>> {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<ASCOMObservingConditions>> result;
    for (const auto& [_, device] : devices_) {
        if (auto oc = std::dynamic_pointer_cast<ASCOMObservingConditions>(device)) {
            result.push_back(oc);
        }
    }
    return result;
}

auto ASCOMDeviceManager::connectAll(std::shared_ptr<AlpacaClient> client) -> int {
    std::lock_guard<std::mutex> lock(mutex_);

    int connected = 0;
    for (auto& [_, device] : devices_) {
        device->setClient(client);
        if (device->connect()) {
            ++connected;
        }
    }

    spdlog::info("Connected {} of {} ASCOM devices", connected, devices_.size());
    return connected;
}

auto ASCOMDeviceManager::disconnectAll() -> int {
    std::lock_guard<std::mutex> lock(mutex_);

    int disconnected = 0;
    for (auto& [_, device] : devices_) {
        if (device->isConnected()) {
            device->disconnect();
            ++disconnected;
        }
    }

    spdlog::info("Disconnected {} ASCOM devices", disconnected);
    return disconnected;
}

auto ASCOMDeviceManager::hasDevice(const std::string& name) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.find(name) != devices_.end();
}

auto ASCOMDeviceManager::getDeviceCount() const -> size_t {
    std::lock_guard<std::mutex> lock(mutex_);
    return devices_.size();
}

void ASCOMDeviceManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    devices_.clear();
    spdlog::debug("Cleared all ASCOM devices");
}

}  // namespace lithium::client::ascom
