#include "manager.hpp"

#include "atom/log/spdlog_logger.hpp"

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace lithium {

class DeviceManager::Impl {
public:
    std::unordered_map<std::string, std::vector<std::shared_ptr<AtomDriver>>>
        devices;
    std::unordered_map<std::string, std::shared_ptr<AtomDriver>> primaryDevices;
    std::unordered_map<std::string, DeviceMetadata> deviceMetadata;
    std::unordered_map<std::string, DeviceState> deviceStates;
    DeviceEventCallback eventCallback;
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

    std::string findDeviceType(const std::string& name) const {
        for (const auto& [type, deviceList] : devices) {
            for (const auto& device : deviceList) {
                if (device->getName() == name) {
                    return type;
                }
            }
        }
        return "";
    }

    void emitEvent(DeviceEventType event, const std::string& deviceType,
                   const std::string& deviceName, const json& data = {}) {
        if (eventCallback) {
            try {
                eventCallback(event, deviceType, deviceName, data);
            } catch (const std::exception& e) {
                LOG_ERROR( "DeviceManager: Event callback error: %s",
                      e.what());
            }
        }
    }

    void updateDeviceState(const std::string& name, bool connected) {
        auto& state = deviceStates[name];
        state.isConnected = connected;
        state.lastActivity = std::chrono::system_clock::now();
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
    LOG_INFO( "Added device {} of type {}", device->getName(), type);

    if (pimpl->primaryDevices.find(type) == pimpl->primaryDevices.end()) {
        pimpl->primaryDevices[type] = device;
        LOG_INFO( "Primary device for {} set to {}", type, device->getName());
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
            LOG_ERROR( "Failed to destroy device {}", device->getName());
        }
        LOG_INFO( "Removed device {} of type {}", device->getName(), type);
        if (pimpl->primaryDevices[type] == device) {
            if (!vec.empty()) {
                pimpl->primaryDevices[type] = vec.front();
                LOG_INFO( "Primary device for {} set to {}", type,
                      vec.front()->getName());
            } else {
                pimpl->primaryDevices.erase(type);
                LOG_INFO( "No primary device for {} as the list is empty",
                      type);
            }
        }
    } else {
        LOG_WARN( "Attempted to remove device {} of non-existent type {}",
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
            LOG_INFO( "Primary device for {} set to {}", type,
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
    LOG_WARN( "No primary device found for type {}", type);
    return nullptr;
}

void DeviceManager::connectAllDevices() {
    std::shared_lock lock(pimpl->mtx);
    for (auto& [type, vec] : pimpl->devices) {
        for (auto& device : vec) {
            try {
                device->connect("7624");
                LOG_INFO( "Connected device {} of type {}", device->getName(),
                      type);
            } catch (const DeviceNotFoundException& e) {
                LOG_ERROR( "Failed to connect device {}: {}",
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
                LOG_INFO( "Disconnected device {} of type {}",
                      device->getName(), type);
            } catch (const DeviceNotFoundException& e) {
                LOG_ERROR( "Failed to disconnect device {}: {}",
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
    LOG_WARN( "No devices found for type {}", type);
    return {};
}

void DeviceManager::connectDevicesByType(const std::string& type) {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        for (auto& device : it->second) {
            try {
                device->connect("7624");
                LOG_INFO( "Connected device {} of type {}", device->getName(),
                      type);
            } catch (const DeviceNotFoundException& e) {
                LOG_ERROR( "Failed to connect device {}: {}",
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
                LOG_INFO( "Disconnected device {} of type {}",
                      device->getName(), type);
            } catch (const DeviceNotFoundException& e) {
                LOG_ERROR( "Failed to disconnect device {}: {}",
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
        LOG_WARN( "No device found with name {}", name);
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
        LOG_INFO( "Connected device {}", name);
    } catch (const DeviceNotFoundException& e) {
        LOG_ERROR( "Failed to connect device {}: {}", name, e.what());
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
        LOG_INFO( "Disconnected device {}", name);
    } catch (const DeviceNotFoundException& e) {
        LOG_ERROR( "Failed to disconnect device {}: {}", name, e.what());
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
            LOG_INFO( "Removed device {} of type {}", name, type);

            if (pimpl->primaryDevices[type] == device) {
                if (!deviceList.empty()) {
                    pimpl->primaryDevices[type] = deviceList.front();
                    LOG_INFO( "Primary device for {} set to {}", type,
                          deviceList.front()->getName());
                } else {
                    pimpl->primaryDevices.erase(type);
                    LOG_INFO( "No primary device for {} as the list is empty",
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
        LOG_ERROR( "Failed to initialize device {}", name);
        return false;
    }
    LOG_INFO( "Initialized device {}", name);
    return true;
}

bool DeviceManager::destroyDevice(const std::string& name) {
    std::shared_lock lock(pimpl->mtx);
    auto device = pimpl->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found");
    }

    if (!device->destroy()) {
        LOG_ERROR( "Failed to destroy device {}", name);
        return false;
    }
    LOG_INFO( "Destroyed device {}", name);
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

// ========== Enhanced Device Management Implementation ==========

void DeviceManager::addDeviceWithMetadata(const std::string& type,
                                          std::shared_ptr<AtomDriver> device,
                                          const DeviceMetadata& metadata) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->devices[type].push_back(device);
    pimpl->deviceMetadata[device->getName()] = metadata;

    DeviceState state;
    state.isConnected = device->isConnected();
    state.isInitialized = true;
    state.lastActivity = std::chrono::system_clock::now();
    pimpl->deviceStates[device->getName()] = state;

    LOG_INFO( "Added device {} of type {} with metadata", device->getName(),
          type);

    if (pimpl->primaryDevices.find(type) == pimpl->primaryDevices.end()) {
        pimpl->primaryDevices[type] = device;
        LOG_INFO( "Primary device for {} set to {}", type, device->getName());
    }

    pimpl->emitEvent(DeviceEventType::DEVICE_ADDED, type, device->getName(),
                     metadata.toJson());
}

std::optional<DeviceMetadata> DeviceManager::getDeviceMetadata(
    const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->deviceMetadata.find(name);
    if (it != pimpl->deviceMetadata.end()) {
        return it->second;
    }
    return std::nullopt;
}

void DeviceManager::updateDeviceMetadata(const std::string& name,
                                         const DeviceMetadata& metadata) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->deviceMetadata[name] = metadata;
    LOG_INFO( "Updated metadata for device {}", name);
}

std::optional<DeviceState> DeviceManager::getDeviceState(
    const std::string& name) const {
    std::shared_lock lock(pimpl->mtx);
    auto it = pimpl->deviceStates.find(name);
    if (it != pimpl->deviceStates.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<std::pair<std::shared_ptr<AtomDriver>, DeviceState>>
DeviceManager::getDevicesWithState(const std::string& type) const {
    std::shared_lock lock(pimpl->mtx);
    std::vector<std::pair<std::shared_ptr<AtomDriver>, DeviceState>> result;

    auto it = pimpl->devices.find(type);
    if (it != pimpl->devices.end()) {
        for (const auto& device : it->second) {
            DeviceState state;
            auto stateIt = pimpl->deviceStates.find(device->getName());
            if (stateIt != pimpl->deviceStates.end()) {
                state = stateIt->second;
            }
            result.emplace_back(device, state);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AtomDriver>> DeviceManager::findDevicesByDriver(
    const std::string& driverName) const {
    std::shared_lock lock(pimpl->mtx);
    std::vector<std::shared_ptr<AtomDriver>> result;

    for (const auto& [type, deviceList] : pimpl->devices) {
        for (const auto& device : deviceList) {
            auto metaIt = pimpl->deviceMetadata.find(device->getName());
            if (metaIt != pimpl->deviceMetadata.end() &&
                metaIt->second.driverName == driverName) {
                result.push_back(device);
            }
        }
    }
    return result;
}

std::shared_ptr<AtomDriver> DeviceManager::getDeviceById(
    const std::string& deviceId) const {
    std::shared_lock lock(pimpl->mtx);

    for (const auto& [name, meta] : pimpl->deviceMetadata) {
        if (meta.deviceId == deviceId) {
            return pimpl->findDeviceByName(name);
        }
    }
    return nullptr;
}

void DeviceManager::registerEventCallback(DeviceEventCallback callback) {
    std::unique_lock lock(pimpl->mtx);
    pimpl->eventCallback = std::move(callback);
    LOG_INFO( "DeviceManager: Event callback registered");
}

void DeviceManager::unregisterEventCallback() {
    std::unique_lock lock(pimpl->mtx);
    pimpl->eventCallback = nullptr;
    LOG_INFO( "DeviceManager: Event callback unregistered");
}

json DeviceManager::exportConfiguration() const {
    std::shared_lock lock(pimpl->mtx);

    json config;
    config["version"] = "1.0";

    json devicesJson = json::array();
    for (const auto& [type, deviceList] : pimpl->devices) {
        for (const auto& device : deviceList) {
            json deviceJson;
            deviceJson["type"] = type;
            deviceJson["name"] = device->getName();

            auto metaIt = pimpl->deviceMetadata.find(device->getName());
            if (metaIt != pimpl->deviceMetadata.end()) {
                deviceJson["metadata"] = metaIt->second.toJson();
            }

            auto primaryIt = pimpl->primaryDevices.find(type);
            deviceJson["isPrimary"] =
                (primaryIt != pimpl->primaryDevices.end() &&
                 primaryIt->second == device);

            devicesJson.push_back(deviceJson);
        }
    }
    config["devices"] = devicesJson;

    return config;
}

void DeviceManager::importConfiguration(const json& config) {
    if (!config.contains("devices") || !config["devices"].is_array()) {
        LOG_WARN( "DeviceManager: Invalid configuration format");
        return;
    }

    for (const auto& deviceJson : config["devices"]) {
        if (!deviceJson.contains("name") || !deviceJson.contains("metadata")) {
            continue;
        }

        std::string name = deviceJson["name"];
        auto metadata = DeviceMetadata::fromJson(deviceJson["metadata"]);

        std::unique_lock lock(pimpl->mtx);
        pimpl->deviceMetadata[name] = metadata;
    }

    LOG_INFO( "DeviceManager: Configuration imported");
}

json DeviceManager::getStatus() const {
    std::shared_lock lock(pimpl->mtx);

    json status;
    status["totalDevices"] = 0;
    status["connectedDevices"] = 0;

    json typesJson = json::object();
    int total = 0;
    int connected = 0;

    for (const auto& [type, deviceList] : pimpl->devices) {
        json typeInfo;
        typeInfo["count"] = deviceList.size();
        total += static_cast<int>(deviceList.size());

        int typeConnected = 0;
        for (const auto& device : deviceList) {
            if (device->isConnected()) {
                typeConnected++;
                connected++;
            }
        }
        typeInfo["connected"] = typeConnected;

        auto primaryIt = pimpl->primaryDevices.find(type);
        if (primaryIt != pimpl->primaryDevices.end()) {
            typeInfo["primary"] = primaryIt->second->getName();
        }

        typesJson[type] = typeInfo;
    }

    status["totalDevices"] = total;
    status["connectedDevices"] = connected;
    status["deviceTypes"] = typesJson;

    return status;
}

std::vector<DeviceMetadata> DeviceManager::discoverDevices(
    const std::string& backend) {
    LOG_INFO( "DeviceManager: Discovering devices via %s", backend.c_str());

    std::vector<DeviceMetadata> discovered;

    // TODO: Integrate with INDI/ASCOM discovery
    // This is a placeholder implementation

    return discovered;
}

void DeviceManager::refreshDevices() {
    LOG_INFO( "DeviceManager: Refreshing device list");

    std::shared_lock lock(pimpl->mtx);
    for (const auto& [type, deviceList] : pimpl->devices) {
        for (const auto& device : deviceList) {
            pimpl->updateDeviceState(device->getName(), device->isConnected());
        }
    }
}

}  // namespace lithium
