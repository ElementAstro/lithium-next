#include "manager.hpp"

#include <spdlog/spdlog.h>

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace lithium {

class DeviceManager::Impl {
public:
    std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>>
        devices;
    std::unordered_map<std::string, std::shared_ptr<AtomDriver>> primaryDevices;
    mutable std::shared_mutex mtx;

    std::shared_ptr<AtomDriver> findDeviceByName(
        const std::string& name) const {
        for (const auto& [type, deviceList] : devices) {
            for (const auto& device : deviceList) {
                if (device->getName() == name) {
                    return device;
                }
            }
        }
        return nullptr;
    }
};

// 构造和析构函数
DeviceManager::DeviceManager() : pimpl(std::make_unique<Impl>()) {}
DeviceManager::~DeviceManager() = default;

void DeviceManager::addDevice(const std::string& type,
                              std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->devices[type].push_back(device);
    device->setName(device->getName());
    LOG_F(INFO, "Added device {} of type {}", device->getName(), type);

    if (pimpl->primaryDevices.find(type) == pimpl->primaryDevices.end()) {
        pimpl->primaryDevices[type] = device;
        LOG_F(INFO, "Primary device for {} set to {}", type, device->getName());
    }
}

void DeviceManager::removeDevice(const std::string& type,
                                 std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), device), vec.end());
        if (device->destroy()) {
            LOG_F(ERROR, "Failed to destroy device {}", device->getName());
        }
        LOG_F(INFO, "Removed device {} of type {}", device->getName(), type);
        if (pimpl->primaryDevices[type] == device) {
            if (!vec.empty()) {
                pimpl->primaryDevices[type] = vec.front();
                LOG_F(INFO, "Primary device for {} set to {}", type,
                      vec.front()->getName());
            } else {
                pimpl->primaryDevices.erase(type);
                LOG_F(INFO, "No primary device for {} as the list is empty",
                      type);
            }
        }
    } else {
        LOG_F(WARNING, "Attempted to remove device {} of non-existent type {}",
              device->getName(), type);
    }
}

void DeviceManager::setPrimaryDevice(const std::string& type,
                                     std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        if (std::find(it->second.begin(), it->second.end(), device) !=
            it->second.end()) {
            pimpl->primaryDevices[type] = device;
            LOG_F(INFO, "Primary device for {} set to {}", type,
                  device->getName());
        } else {
            THROW_DEVICE_NOT_FOUND("Device not found");
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }
}

std::shared_ptr<AtomDriver> DeviceManager::getPrimaryDevice(
    const std::string& type) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->primaryDevices.find(type);
    if (it != pimpl->primaryDevices.end()) {
        return it->second;
    }
    LOG_F(WARNING, "No primary device found for type {}", type);
    return nullptr;
}

void DeviceManager::connectAllDevices() {
    std::shared_lock lock(pimpl->mtx);
    for (auto& [type, vec] : pimpl->devices) {
        for (auto& device : vec) {
            try {
                device->connect("7624");
                LOG_F(INFO, "Connected device {} of type {}", device->getName(),
                      type);
            } catch (const DeviceNotFoundException& e) {
                LOG_F(ERROR, "Failed to connect device {}: {}",
                      device->getName(), e.what());
            }
        }
    }
}

void DeviceManager::disconnectAllDevices() {
    std::shared_lock lock(pimpl->mtx);
    for (auto& [type, vec] : pimpl->devices) {
        for (auto& device : vec) {
            try {
                device->disconnect();
                LOG_F(INFO, "Disconnected device {} of type {}",
                      device->getName(), type);
            } catch (const DeviceNotFoundException& e) {
                LOG_F(ERROR, "Failed to disconnect device {}: {}",
                      device->getName(), e.what());
            }
        }
    }
}

std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>>
DeviceManager::getDevices() const {
    std::shared_lock lock(pimpl->mtx);
    return pimpl->devices;
}

std::vector<std::shared_ptr<AtomDriver>> DeviceManager::findDevicesByType(
    const std::string& type) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        return it->second;
    }
    LOG_F(WARNING, "No devices found for type {}", type);
    return {};
}

void DeviceManager::connectDevicesByType(const std::string& type) {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        for (auto& device : it->second) {
            try {
                device->connect("7624");
                LOG_F(INFO, "Connected device {} of type {}", device->getName(),
                      type);
            } catch (const DeviceNotFoundException& e) {
                LOG_F(ERROR, "Failed to connect device {}: {}",
                      device->getName(), e.what());
            }
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }
}

void DeviceManager::disconnectDevicesByType(const std::string& type) {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        for (auto& device : it->second) {
            try {
                device->disconnect();
                LOG_F(INFO, "Disconnected device {} of type {}",
                      device->getName(), type);
            } catch (const DeviceNotFoundException& e) {
                LOG_F(ERROR, "Failed to disconnect device {}: {}",
                      device->getName(), e.what());
            }
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }
}

std::shared_ptr<AtomDriver> DeviceManager::findDeviceByName(
    const std::string& name) const {
    return pimpl->findDeviceByName(name);
}

std::shared_ptr<AtomDriver> DeviceManager::getDeviceByName(
    const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        LOG_F(WARNING, "No device found with name {}", name);
    }
    return device;
}

void DeviceManager::connectDeviceByName(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    try {
        device->connect("7624");
        LOG_F(INFO, "Connected device {}", name);
    } catch (const DeviceNotFoundException& e) {
        LOG_F(ERROR, "Failed to connect device {}: {}", name, e.what());
        throw;
    }
}

void DeviceManager::disconnectDeviceByName(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    try {
        device->disconnect();
        LOG_F(INFO, "Disconnected device {}", name);
    } catch (const DeviceNotFoundException& e) {
        LOG_F(ERROR, "Failed to disconnect device {}: {}", name, e.what());
        throw;
    }
}

void DeviceManager::removeDeviceByName(const std::string& name) {
    std::unique_lock lock(pimpl->mtx);
    for (auto& [type, deviceList] : pimpl->devices) {
        auto it = std::find_if(
            deviceList.begin(), deviceList.end(),
            [&name](const auto& device) { return device->getName() == name; });

        if (it != deviceList.end()) {
            auto device = *it;
            deviceList.erase(it);
            LOG_F(INFO, "Removed device {} of type {}", name, type);

            if (pimpl->primaryDevices[type] == device) {
                if (!deviceList.empty()) {
                    pimpl->primaryDevices[type] = deviceList.front();
                    LOG_F(INFO, "Primary device for {} set to {}", type,
                          deviceList.front()->getName());
                } else {
                    pimpl->primaryDevices.erase(type);
                    LOG_F(INFO, "No primary device for {} as the list is empty",
                          type);
                }
            }
            return;
        }
    }
    THROW_DEVICE_NOT_FOUND("Device not found");
}

bool DeviceManager::initializeDevice(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }

    if (!device->initialize()) {
        LOG_F(ERROR, "Failed to initialize device {}", name);
        return false;
    }
    LOG_F(INFO, "Initialized device {}", name);
    return true;
}

bool DeviceManager::destroyDevice(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }

    if (!device->destroy()) {
        LOG_F(ERROR, "Failed to destroy device {}", name);
        return false;
    }
    LOG_F(INFO, "Destroyed device {}", name);
    return true;
}

std::vector<std::string> DeviceManager::scanDevices(const std::string& type) {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it == pimpl->devices.end()) {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found");
    }

    std::vector<std::string> ports;
    for (const auto& device : it->second) {
        auto devicePorts = device->scan();
        ports.insert(ports.end(), devicePorts.begin(), devicePorts.end());
    }
    return ports;
}

bool DeviceManager::isDeviceConnected(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    return device->isConnected();
}

std::string DeviceManager::getDeviceType(const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }
    return device->getType();
}

}  // namespace lithium
