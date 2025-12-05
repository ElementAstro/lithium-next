/*
 * manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device Manager Public Interface Implementation

**************************************************/

#include "manager.hpp"
#include "manager_impl.hpp"

#include "device/service/backend_registry.hpp"
#include "device/service/device_factory.hpp"
#include "device/service/device_types.hpp"

#include "atom/log/spdlog_logger.hpp"

#include <algorithm>

namespace lithium {

// ==================== DeviceManager ====================

DeviceManager::DeviceManager() : pimpl_(std::make_unique<DeviceManagerImpl>()) {
    LOG_INFO("DeviceManager: Created");
}

DeviceManager::~DeviceManager() {
    stopHealthMonitor();
    LOG_INFO("DeviceManager: Destroyed");
}

auto DeviceManager::createShared() -> std::shared_ptr<DeviceManager> {
    return std::make_shared<DeviceManager>();
}

// ==================== Device Registration ====================

void DeviceManager::addDevice(const std::string& type,
                              std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl_->mtx);

    pimpl_->devices[type].push_back(device);

    // Set as primary if first device of this type
    if (pimpl_->primaryDevices.find(type) == pimpl_->primaryDevices.end()) {
        pimpl_->primaryDevices[type] = device;
    }

    // Initialize device state
    pimpl_->deviceStates[device->getName()] = DeviceState{};
    pimpl_->updateDeviceState(device->getName(), device->isConnected());

    LOG_INFO("DeviceManager: Added device {} of type {}", device->getName(),
             type);

    // Emit event
    DeviceEvent event;
    event.type = DeviceEventType::StateChanged;
    event.deviceName = device->getName();
    event.deviceType = type;
    event.message = "Device added";
    event.timestamp = std::chrono::system_clock::now();
    pimpl_->emitEvent(event);
}

void DeviceManager::addDeviceWithMetadata(const std::string& type,
                                          std::shared_ptr<AtomDriver> device,
                                          const DeviceMetadata& metadata) {
    addDevice(type, device);

    std::unique_lock lock(pimpl_->mtx);
    pimpl_->deviceMetadata[device->getName()] = metadata;
}

void DeviceManager::removeDevice(const std::string& type,
                                 std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl_->mtx);

    auto& deviceList = pimpl_->devices[type];
    auto it = std::find(deviceList.begin(), deviceList.end(), device);
    if (it != deviceList.end()) {
        device->destroy();
        deviceList.erase(it);

        // Update primary device if needed
        if (pimpl_->primaryDevices[type] == device) {
            pimpl_->primaryDevices[type] =
                deviceList.empty() ? nullptr : deviceList.front();
        }

        // Remove state and metadata
        pimpl_->deviceStates.erase(device->getName());
        pimpl_->deviceMetadata.erase(device->getName());

        LOG_INFO("DeviceManager: Removed device {} of type {}",
                 device->getName(), type);
    }
}

void DeviceManager::removeDeviceByName(const std::string& name) {
    std::unique_lock lock(pimpl_->mtx);

    for (auto& [type, deviceList] : pimpl_->devices) {
        auto it = std::find_if(
            deviceList.begin(), deviceList.end(),
            [&name](const auto& d) { return d->getName() == name; });
        if (it != deviceList.end()) {
            (*it)->destroy();
            deviceList.erase(it);

            if (pimpl_->primaryDevices[type] &&
                pimpl_->primaryDevices[type]->getName() == name) {
                pimpl_->primaryDevices[type] =
                    deviceList.empty() ? nullptr : deviceList.front();
            }

            pimpl_->deviceStates.erase(name);
            pimpl_->deviceMetadata.erase(name);

            LOG_INFO("DeviceManager: Removed device {} by name", name);
            return;
        }
    }
}

void DeviceManager::removeAllDevicesOfType(const std::string& type) {
    std::unique_lock lock(pimpl_->mtx);

    auto& deviceList = pimpl_->devices[type];
    for (auto& device : deviceList) {
        device->destroy();
        pimpl_->deviceStates.erase(device->getName());
        pimpl_->deviceMetadata.erase(device->getName());
    }
    deviceList.clear();
    pimpl_->primaryDevices.erase(type);

    LOG_INFO("DeviceManager: Removed all devices of type {}", type);
}

// ==================== Device Access ====================

auto DeviceManager::getDevices() const
    -> std::unordered_map<std::string,
                          std::vector<std::shared_ptr<AtomDriver>>> {
    std::shared_lock lock(pimpl_->mtx);
    return pimpl_->devices;
}

auto DeviceManager::getDevicesByType(const std::string& type) const
    -> std::vector<std::shared_ptr<AtomDriver>> {
    std::shared_lock lock(pimpl_->mtx);
    auto it = pimpl_->devices.find(type);
    if (it != pimpl_->devices.end()) {
        return it->second;
    }
    return {};
}

auto DeviceManager::getDeviceByName(const std::string& name) const
    -> std::shared_ptr<AtomDriver> {
    std::shared_lock lock(pimpl_->mtx);
    return pimpl_->findDeviceByName(name);
}

auto DeviceManager::getPrimaryDevice(const std::string& type) const
    -> std::shared_ptr<AtomDriver> {
    std::shared_lock lock(pimpl_->mtx);
    auto it = pimpl_->primaryDevices.find(type);
    if (it != pimpl_->primaryDevices.end()) {
        return it->second;
    }
    return nullptr;
}

void DeviceManager::setPrimaryDevice(const std::string& type,
                                     std::shared_ptr<AtomDriver> device) {
    std::unique_lock lock(pimpl_->mtx);
    pimpl_->primaryDevices[type] = device;
    LOG_INFO("DeviceManager: Set primary device for type {} to {}", type,
             device ? device->getName() : "null");
}

auto DeviceManager::getDeviceType(const std::string& name) const
    -> std::string {
    std::shared_lock lock(pimpl_->mtx);
    return pimpl_->findDeviceType(name);
}

// ==================== Device Metadata ====================

auto DeviceManager::getDeviceMetadata(const std::string& name) const
    -> std::optional<DeviceMetadata> {
    std::shared_lock lock(pimpl_->mtx);
    auto it = pimpl_->deviceMetadata.find(name);
    if (it != pimpl_->deviceMetadata.end()) {
        return it->second;
    }
    return std::nullopt;
}

void DeviceManager::updateDeviceMetadata(const std::string& name,
                                         const DeviceMetadata& metadata) {
    std::unique_lock lock(pimpl_->mtx);
    pimpl_->deviceMetadata[name] = metadata;
}

auto DeviceManager::getDeviceState(const std::string& name) const
    -> DeviceState {
    std::shared_lock lock(pimpl_->mtx);
    auto it = pimpl_->deviceStates.find(name);
    if (it != pimpl_->deviceStates.end()) {
        return it->second;
    }
    return DeviceState{};
}

// ==================== Device Connection ====================

bool DeviceManager::connectDeviceByName(const std::string& name, int timeout) {
    auto device = getDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found: {}", name);
    }

    pimpl_->statistics.totalConnections++;

    auto config = pimpl_->getRetryConfig(name);
    int attempts = 0;

    while (attempts <= config.maxRetries) {
        try {
            if (device->connect("", timeout, 1)) {
                pimpl_->statistics.successfulConnections++;
                pimpl_->updateDeviceState(name, true);

                DeviceEvent event;
                event.type = DeviceEventType::Connected;
                event.deviceName = name;
                event.deviceType = getDeviceType(name);
                event.timestamp = std::chrono::system_clock::now();
                pimpl_->emitEvent(event);

                LOG_INFO("DeviceManager: Connected to device {}", name);
                return true;
            }
        } catch (const std::exception& e) {
            LOG_WARN("DeviceManager: Connection attempt {} failed for {}: {}",
                     attempts + 1, name, e.what());
        }

        attempts++;
        if (attempts <= config.maxRetries) {
            auto delay = pimpl_->calculateRetryDelay(config, attempts);
            std::this_thread::sleep_for(delay);
            pimpl_->statistics.totalRetries++;
        }
    }

    pimpl_->statistics.failedConnections++;
    LOG_ERROR("DeviceManager: Failed to connect to device {}", name);
    return false;
}

bool DeviceManager::disconnectDeviceByName(const std::string& name) {
    auto device = getDeviceByName(name);
    if (!device) {
        return true;  // Already disconnected
    }

    if (device->disconnect()) {
        pimpl_->updateDeviceState(name, false);

        DeviceEvent event;
        event.type = DeviceEventType::Disconnected;
        event.deviceName = name;
        event.deviceType = getDeviceType(name);
        event.timestamp = std::chrono::system_clock::now();
        pimpl_->emitEvent(event);

        LOG_INFO("DeviceManager: Disconnected from device {}", name);
        return true;
    }

    return false;
}

auto DeviceManager::connectDeviceAsync(const std::string& name, int timeout)
    -> std::future<bool> {
    return std::async(std::launch::async, [this, name, timeout]() {
        return connectDeviceByName(name, timeout);
    });
}

auto DeviceManager::disconnectDeviceAsync(const std::string& name)
    -> std::future<bool> {
    return std::async(std::launch::async,
                      [this, name]() { return disconnectDeviceByName(name); });
}

// ==================== Event System ====================

auto DeviceManager::subscribeToEvents(DeviceEventCallback callback)
    -> EventCallbackId {
    std::lock_guard<std::mutex> lock(pimpl_->eventMutex);
    EventCallbackId id = pimpl_->nextCallbackId++;
    pimpl_->eventSubscriptions.push_back({id, std::move(callback), {}});
    return id;
}

auto DeviceManager::subscribeToEvents(
    DeviceEventCallback callback,
    const std::vector<DeviceEventType>& eventTypes) -> EventCallbackId {
    std::lock_guard<std::mutex> lock(pimpl_->eventMutex);
    EventCallbackId id = pimpl_->nextCallbackId++;

    std::unordered_set<int> typeSet;
    for (auto type : eventTypes) {
        typeSet.insert(static_cast<int>(type));
    }

    pimpl_->eventSubscriptions.push_back({id, std::move(callback), typeSet});
    return id;
}

void DeviceManager::unsubscribeFromEvents(EventCallbackId callbackId) {
    std::lock_guard<std::mutex> lock(pimpl_->eventMutex);
    auto it = std::remove_if(pimpl_->eventSubscriptions.begin(),
                             pimpl_->eventSubscriptions.end(),
                             [callbackId](const EventSubscription& sub) {
                                 return sub.id == callbackId;
                             });
    pimpl_->eventSubscriptions.erase(it, pimpl_->eventSubscriptions.end());
}

void DeviceManager::setEventCallback(DeviceEventCallback callback) {
    std::lock_guard<std::mutex> lock(pimpl_->eventMutex);
    pimpl_->legacyEventCallback = std::move(callback);
}

// ==================== Backend Management ====================

bool DeviceManager::connectBackend(const std::string& backend,
                                   const std::string& host, int port) {
    LOG_INFO("DeviceManager: Connecting to {} backend at {}:{}", backend, host,
             port);

    auto& registry = device::BackendRegistry::getInstance();
    auto backendPtr = registry.getBackend(backend);

    if (!backendPtr) {
        LOG_ERROR("DeviceManager: Backend {} not found", backend);
        THROW_BACKEND_NOT_FOUND("Backend not found: {}", backend);
    }

    device::BackendConfig config;
    config.host = host;
    config.port = port;

    bool result = backendPtr->connectServer(config);

    if (result) {
        LOG_INFO("DeviceManager: Connected to {} backend", backend);
    } else {
        LOG_ERROR("DeviceManager: Failed to connect to {} backend", backend);
    }

    return result;
}

bool DeviceManager::disconnectBackend(const std::string& backend) {
    LOG_INFO("DeviceManager: Disconnecting from {} backend", backend);

    auto& registry = device::BackendRegistry::getInstance();
    auto backendPtr = registry.getBackend(backend);

    if (!backendPtr) {
        return true;
    }

    return backendPtr->disconnectServer();
}

bool DeviceManager::isBackendConnected(const std::string& backend) const {
    auto& registry = device::BackendRegistry::getInstance();
    auto backendPtr = registry.getBackend(backend);

    if (!backendPtr) {
        return false;
    }

    return backendPtr->isServerConnected();
}

json DeviceManager::getBackendStatus() const {
    auto& registry = device::BackendRegistry::getInstance();
    return registry.getStatus();
}

// ==================== Device Discovery ====================

auto DeviceManager::discoverDevices(const std::string& backend)
    -> std::vector<DeviceMetadata> {
    LOG_INFO("DeviceManager: Discovering devices via {}", backend);

    std::vector<DeviceMetadata> discovered;
    auto& registry = device::BackendRegistry::getInstance();

    std::vector<device::DiscoveredDevice> backendDevices;

    if (backend.empty() || backend == "ALL") {
        backendDevices = registry.discoverAllDevices();
    } else {
        backendDevices = registry.discoverDevices(backend);
    }

    discovered.reserve(backendDevices.size());
    for (const auto& dev : backendDevices) {
        DeviceMetadata meta;
        meta.deviceId = dev.deviceId;
        meta.displayName = dev.displayName;
        meta.driverName = dev.driverName;
        meta.driverVersion = dev.driverVersion;
        meta.connectionString = dev.connectionString;
        meta.priority = dev.priority;
        meta.autoConnect = false;
        meta.customProperties = dev.customProperties;
        meta.customProperties["deviceType"] = dev.deviceType;
        meta.customProperties["isConnected"] = dev.isConnected;
        discovered.push_back(meta);
    }

    LOG_INFO("DeviceManager: Discovered {} devices via {}", discovered.size(),
             backend);
    return discovered;
}

int DeviceManager::discoverAndRegisterDevices(const std::string& backend,
                                              bool autoConnect) {
    LOG_INFO("DeviceManager: Discovering and registering devices from {}",
             backend);

    auto discovered = discoverDevices(backend);
    int registered = 0;

    auto& factory = device::DeviceFactory::getInstance();

    for (const auto& meta : discovered) {
        if (pimpl_->findDeviceByName(meta.displayName)) {
            LOG_DEBUG("DeviceManager: Device {} already registered, skipping",
                      meta.displayName);
            continue;
        }

        device::DiscoveredDevice devInfo;
        devInfo.deviceId = meta.deviceId;
        devInfo.displayName = meta.displayName;
        devInfo.deviceType =
            meta.customProperties.value("deviceType", "Unknown");
        devInfo.driverName = meta.driverName;
        devInfo.driverVersion = meta.driverVersion;
        devInfo.connectionString = meta.connectionString;
        devInfo.customProperties = meta.customProperties;

        auto device = factory.createDevice(devInfo);
        if (device) {
            std::string deviceType = devInfo.deviceType;
            if (deviceType.empty() || deviceType == "Unknown") {
                deviceType = "generic";
            }

            addDeviceWithMetadata(deviceType, device, meta);
            registered++;

            LOG_INFO("DeviceManager: Registered device {} ({})",
                     meta.displayName, deviceType);

            if (autoConnect) {
                try {
                    connectDeviceByName(meta.displayName);
                } catch (const std::exception& e) {
                    LOG_WARN("DeviceManager: Failed to auto-connect {}: {}",
                             meta.displayName, e.what());
                }
            }
        } else {
            LOG_WARN("DeviceManager: No factory for device {} (type: {})",
                     meta.displayName, devInfo.deviceType);
        }
    }

    LOG_INFO("DeviceManager: Registered {} devices from {}", registered,
             backend);
    return registered;
}

void DeviceManager::refreshDevices() {
    LOG_INFO("DeviceManager: Refreshing device list");

    auto& registry = device::BackendRegistry::getInstance();
    registry.refreshAllDevices();

    std::shared_lock lock(pimpl_->mtx);
    for (const auto& [type, deviceList] : pimpl_->devices) {
        for (const auto& device : deviceList) {
            pimpl_->updateDeviceState(device->getName(), device->isConnected());
        }
    }
}

// ==================== Health Monitoring ====================

void DeviceManager::startHealthMonitor(std::chrono::seconds interval) {
    pimpl_->healthCheckInterval = interval;
    pimpl_->startHealthMonitorInternal();
    LOG_INFO("DeviceManager: Health monitor started with {}s interval",
             interval.count());
}

void DeviceManager::stopHealthMonitor() {
    pimpl_->stopHealthMonitorInternal();
    LOG_INFO("DeviceManager: Health monitor stopped");
}

json DeviceManager::checkAllDevicesHealth() {
    json report;
    report["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    json devices = json::array();
    std::shared_lock lock(pimpl_->mtx);

    for (const auto& [type, deviceList] : pimpl_->devices) {
        for (const auto& device : deviceList) {
            json deviceInfo;
            deviceInfo["name"] = device->getName();
            deviceInfo["type"] = type;

            auto it = pimpl_->deviceStates.find(device->getName());
            if (it != pimpl_->deviceStates.end()) {
                deviceInfo["state"] = it->second.toJson();
            }

            devices.push_back(deviceInfo);
        }
    }

    report["devices"] = devices;
    return report;
}

float DeviceManager::getDeviceHealth(const std::string& name) const {
    std::shared_lock lock(pimpl_->mtx);
    auto it = pimpl_->deviceStates.find(name);
    if (it != pimpl_->deviceStates.end()) {
        return it->second.healthScore;
    }
    return 1.0f;
}

auto DeviceManager::getUnhealthyDevices(float threshold) const
    -> std::vector<std::string> {
    std::vector<std::string> unhealthy;
    std::shared_lock lock(pimpl_->mtx);

    for (const auto& [name, state] : pimpl_->deviceStates) {
        if (state.healthScore < threshold) {
            unhealthy.push_back(name);
        }
    }

    return unhealthy;
}

// ==================== Device Validation ====================

bool DeviceManager::isDeviceValid(const std::string& name) const {
    std::shared_lock lock(pimpl_->mtx);
    return pimpl_->findDeviceByName(name) != nullptr;
}

void DeviceManager::setDeviceRetryConfig(const std::string& name,
                                         const DeviceRetryConfig& config) {
    std::unique_lock lock(pimpl_->mtx);
    pimpl_->retryConfigs[name] = config;
    LOG_INFO("DeviceManager: Set retry config for device {}", name);
}

auto DeviceManager::getDeviceRetryConfig(const std::string& name) const
    -> DeviceRetryConfig {
    std::shared_lock lock(pimpl_->mtx);
    return pimpl_->getRetryConfig(name);
}

void DeviceManager::abortDeviceOperation(const std::string& name) {
    std::unique_lock lock(pimpl_->mtx);
    auto it = pimpl_->deviceStates.find(name);
    if (it != pimpl_->deviceStates.end()) {
        it->second.isBusy = false;
    }
    LOG_INFO("DeviceManager: Aborted operation for device {}", name);
}

// ==================== Additional Methods ====================

void DeviceManager::connectAllDevices() {
    std::shared_lock lock(pimpl_->mtx);
    for (auto& [type, vec] : pimpl_->devices) {
        for (auto& device : vec) {
            try {
                device->connect("7624");
                pimpl_->updateDeviceState(device->getName(), true);
                LOG_INFO("DeviceManager: Connected device {} of type {}",
                         device->getName(), type);
            } catch (const std::exception& e) {
                LOG_ERROR("DeviceManager: Failed to connect device {}: {}",
                          device->getName(), e.what());
            }
        }
    }
}

void DeviceManager::disconnectAllDevices() {
    std::shared_lock lock(pimpl_->mtx);
    for (auto& [type, vec] : pimpl_->devices) {
        for (auto& device : vec) {
            try {
                device->disconnect();
                pimpl_->updateDeviceState(device->getName(), false);
                LOG_INFO("DeviceManager: Disconnected device {} of type {}",
                         device->getName(), type);
            } catch (const std::exception& e) {
                LOG_ERROR("DeviceManager: Failed to disconnect device {}: {}",
                          device->getName(), e.what());
            }
        }
    }
}

void DeviceManager::connectDevicesByType(const std::string& type) {
    std::shared_lock lock(pimpl_->mtx);
    auto it = pimpl_->devices.find(type);
    if (it != pimpl_->devices.end()) {
        for (auto& device : it->second) {
            try {
                device->connect("7624");
                pimpl_->updateDeviceState(device->getName(), true);
                LOG_INFO("DeviceManager: Connected device {} of type {}",
                         device->getName(), type);
            } catch (const std::exception& e) {
                LOG_ERROR("DeviceManager: Failed to connect device {}: {}",
                          device->getName(), e.what());
            }
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found: {}", type);
    }
}

void DeviceManager::disconnectDevicesByType(const std::string& type) {
    std::shared_lock lock(pimpl_->mtx);
    auto it = pimpl_->devices.find(type);
    if (it != pimpl_->devices.end()) {
        for (auto& device : it->second) {
            try {
                device->disconnect();
                pimpl_->updateDeviceState(device->getName(), false);
                LOG_INFO("DeviceManager: Disconnected device {} of type {}",
                         device->getName(), type);
            } catch (const std::exception& e) {
                LOG_ERROR("DeviceManager: Failed to disconnect device {}: {}",
                          device->getName(), e.what());
            }
        }
    } else {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found: {}", type);
    }
}

std::shared_ptr<AtomDriver> DeviceManager::findDeviceByName(
    const std::string& name) const {
    std::shared_lock lock(pimpl_->mtx);
    return pimpl_->findDeviceByName(name);
}

bool DeviceManager::isDeviceConnected(const std::string& name) const {
    std::shared_lock lock(pimpl_->mtx);
    auto device = pimpl_->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found: {}", name);
    }
    return device->isConnected();
}

bool DeviceManager::initializeDevice(const std::string& name) {
    std::shared_lock lock(pimpl_->mtx);
    auto device = pimpl_->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found: {}", name);
    }

    if (!device->initialize()) {
        LOG_ERROR("DeviceManager: Failed to initialize device {}", name);
        return false;
    }
    LOG_INFO("DeviceManager: Initialized device {}", name);
    return true;
}

bool DeviceManager::destroyDevice(const std::string& name) {
    std::shared_lock lock(pimpl_->mtx);
    auto device = pimpl_->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found: {}", name);
    }

    if (!device->destroy()) {
        LOG_ERROR("DeviceManager: Failed to destroy device {}", name);
        return false;
    }
    LOG_INFO("DeviceManager: Destroyed device {}", name);
    return true;
}

std::vector<std::string> DeviceManager::scanDevices(const std::string& type) {
    std::shared_lock lock(pimpl_->mtx);
    auto it = pimpl_->devices.find(type);
    if (it == pimpl_->devices.end()) {
        THROW_DEVICE_TYPE_NOT_FOUND("Device type not found: {}", type);
    }

    std::vector<std::string> ports;
    for (const auto& device : it->second) {
        auto devicePorts = device->scan();
        ports.insert(ports.end(), devicePorts.begin(), devicePorts.end());
    }
    return ports;
}

void DeviceManager::resetDevice(const std::string& name) {
    std::unique_lock lock(pimpl_->mtx);
    auto device = pimpl_->findDeviceByName(name);
    if (!device) {
        THROW_DEVICE_NOT_FOUND("Device not found: {}", name);
    }

    auto& state = pimpl_->deviceStates[name];
    state.consecutiveErrors = 0;
    state.healthScore = 1.0f;
    state.isBusy = false;
    state.lastError.clear();
    state.lastActivity = std::chrono::system_clock::now();

    LOG_INFO("DeviceManager: Reset device {}", name);

    DeviceEvent event;
    event.type = DeviceEventType::StateChanged;
    event.deviceName = name;
    event.deviceType = pimpl_->findDeviceType(name);
    event.message = "Device reset";
    event.timestamp = std::chrono::system_clock::now();
    pimpl_->emitEvent(event);
}

void DeviceManager::updateDeviceHealth(const std::string& name,
                                       bool operationSuccess) {
    std::unique_lock lock(pimpl_->mtx);

    auto& state = pimpl_->deviceStates[name];
    state.totalOperations++;
    state.lastActivity = std::chrono::system_clock::now();

    if (operationSuccess) {
        state.consecutiveErrors = 0;
        state.healthScore = std::min(1.0f, state.healthScore + 0.1f);
    } else {
        state.failedOperations++;
        state.consecutiveErrors++;
        float penalty = 0.1f * state.consecutiveErrors;
        state.healthScore = std::max(0.0f, state.healthScore - penalty);
    }

    pimpl_->statistics.totalOperations++;
    if (operationSuccess) {
        pimpl_->statistics.successfulOperations++;
    } else {
        pimpl_->statistics.failedOperations++;
    }
}

std::vector<std::pair<std::shared_ptr<AtomDriver>, DeviceState>>
DeviceManager::getDevicesWithState(const std::string& type) const {
    std::shared_lock lock(pimpl_->mtx);
    std::vector<std::pair<std::shared_ptr<AtomDriver>, DeviceState>> result;

    auto it = pimpl_->devices.find(type);
    if (it != pimpl_->devices.end()) {
        for (const auto& device : it->second) {
            DeviceState state;
            auto stateIt = pimpl_->deviceStates.find(device->getName());
            if (stateIt != pimpl_->deviceStates.end()) {
                state = stateIt->second;
            }
            result.emplace_back(device, state);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AtomDriver>> DeviceManager::findDevicesByDriver(
    const std::string& driverName) const {
    std::shared_lock lock(pimpl_->mtx);
    std::vector<std::shared_ptr<AtomDriver>> result;

    for (const auto& [type, deviceList] : pimpl_->devices) {
        for (const auto& device : deviceList) {
            auto metaIt = pimpl_->deviceMetadata.find(device->getName());
            if (metaIt != pimpl_->deviceMetadata.end() &&
                metaIt->second.driverName == driverName) {
                result.push_back(device);
            }
        }
    }
    return result;
}

std::shared_ptr<AtomDriver> DeviceManager::getDeviceById(
    const std::string& deviceId) const {
    std::shared_lock lock(pimpl_->mtx);

    for (const auto& [name, meta] : pimpl_->deviceMetadata) {
        if (meta.deviceId == deviceId) {
            return pimpl_->findDeviceByName(name);
        }
    }
    return nullptr;
}

void DeviceManager::registerEventCallback(DeviceEventCallback callback) {
    std::lock_guard<std::mutex> lock(pimpl_->eventMutex);
    pimpl_->legacyEventCallback = std::move(callback);
    LOG_INFO("DeviceManager: Event callback registered");
}

void DeviceManager::unregisterEventCallback() {
    std::lock_guard<std::mutex> lock(pimpl_->eventMutex);
    pimpl_->legacyEventCallback = nullptr;
    LOG_INFO("DeviceManager: Event callback unregistered");
}

std::vector<DeviceEvent> DeviceManager::getPendingEvents(size_t maxEvents) {
    std::lock_guard<std::mutex> lock(pimpl_->eventMutex);
    std::vector<DeviceEvent> events;
    size_t count = std::min(maxEvents, pimpl_->pendingEvents.size());
    for (size_t i = 0; i < count; ++i) {
        events.push_back(pimpl_->pendingEvents.front());
        pimpl_->pendingEvents.pop_front();
    }
    return events;
}

void DeviceManager::clearPendingEvents() {
    std::lock_guard<std::mutex> lock(pimpl_->eventMutex);
    pimpl_->pendingEvents.clear();
}

json DeviceManager::exportConfiguration() const {
    std::shared_lock lock(pimpl_->mtx);

    json config;
    config["version"] = "1.0";

    json devicesJson = json::array();
    for (const auto& [type, deviceList] : pimpl_->devices) {
        for (const auto& device : deviceList) {
            json deviceJson;
            deviceJson["type"] = type;
            deviceJson["name"] = device->getName();

            auto metaIt = pimpl_->deviceMetadata.find(device->getName());
            if (metaIt != pimpl_->deviceMetadata.end()) {
                deviceJson["metadata"] = metaIt->second.toJson();
            }

            auto primaryIt = pimpl_->primaryDevices.find(type);
            deviceJson["isPrimary"] =
                (primaryIt != pimpl_->primaryDevices.end() &&
                 primaryIt->second == device);

            devicesJson.push_back(deviceJson);
        }
    }
    config["devices"] = devicesJson;

    return config;
}

void DeviceManager::importConfiguration(const json& config) {
    if (!config.contains("devices") || !config["devices"].is_array()) {
        LOG_WARN("DeviceManager: Invalid configuration format");
        return;
    }

    for (const auto& deviceJson : config["devices"]) {
        if (!deviceJson.contains("name") || !deviceJson.contains("metadata")) {
            continue;
        }

        std::string name = deviceJson["name"];
        auto metadata = DeviceMetadata::fromJson(deviceJson["metadata"]);

        std::unique_lock lock(pimpl_->mtx);
        pimpl_->deviceMetadata[name] = metadata;
    }

    LOG_INFO("DeviceManager: Configuration imported");
}

std::future<DeviceOperationResult> DeviceManager::executeWithRetry(
    const std::string& name,
    std::function<bool(std::shared_ptr<AtomDriver>)> operation,
    const std::string& operationName) {
    return std::async(
        std::launch::async,
        [this, name, operation, operationName]() -> DeviceOperationResult {
            DeviceOperationResult result;
            auto startTime = std::chrono::steady_clock::now();

            std::shared_lock lock(pimpl_->mtx);
            auto device = pimpl_->findDeviceByName(name);
            if (!device) {
                result.errorMessage = "Device not found";
                return result;
            }

            auto config = pimpl_->getRetryConfig(name);
            auto deviceType = pimpl_->findDeviceType(name);
            lock.unlock();

            int attempt = 0;
            while (attempt <= config.maxRetries) {
                try {
                    if (operation(device)) {
                        result.success = true;
                        result.retryCount = attempt;
                        break;
                    }
                } catch (const std::exception& e) {
                    result.errorMessage = e.what();
                }

                attempt++;
                if (attempt <= config.maxRetries) {
                    pimpl_->statistics.totalRetries++;
                    auto delay = pimpl_->calculateRetryDelay(config, attempt);
                    std::this_thread::sleep_for(delay);
                }
            }

            auto endTime = std::chrono::steady_clock::now();
            result.duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    endTime - startTime);

            if (!result.success && result.retryCount >= config.maxRetries) {
                LOG_ERROR(
                    "DeviceManager: Operation failed after {} retries: {}",
                    result.retryCount, result.errorMessage);
            }

            return result;
        });
}

std::vector<std::pair<std::string, bool>> DeviceManager::connectDevicesBatch(
    const std::vector<std::string>& names, int timeoutMs) {
    std::vector<std::future<std::pair<std::string, bool>>> futures;

    for (const auto& name : names) {
        futures.push_back(std::async(
            std::launch::async,
            [this, name, timeoutMs]() -> std::pair<std::string, bool> {
                try {
                    return {name, connectDeviceByName(name, timeoutMs)};
                } catch (...) {
                    return {name, false};
                }
            }));
    }

    std::vector<std::pair<std::string, bool>> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    return results;
}

std::vector<std::pair<std::string, bool>> DeviceManager::disconnectDevicesBatch(
    const std::vector<std::string>& names) {
    std::vector<std::future<std::pair<std::string, bool>>> futures;

    for (const auto& name : names) {
        futures.push_back(std::async(
            std::launch::async, [this, name]() -> std::pair<std::string, bool> {
                try {
                    return {name, disconnectDeviceByName(name)};
                } catch (...) {
                    return {name, false};
                }
            }));
    }

    std::vector<std::pair<std::string, bool>> results;
    for (auto& future : futures) {
        results.push_back(future.get());
    }
    return results;
}

// ==================== Status & Statistics ====================

json DeviceManager::getStatus() const {
    json status;
    std::shared_lock lock(pimpl_->mtx);

    int totalDevices = 0;
    int connectedDevices = 0;

    for (const auto& [type, deviceList] : pimpl_->devices) {
        totalDevices += static_cast<int>(deviceList.size());
        for (const auto& device : deviceList) {
            if (device->isConnected()) {
                connectedDevices++;
            }
        }
    }

    status["totalDevices"] = totalDevices;
    status["connectedDevices"] = connectedDevices;
    status["deviceTypes"] = pimpl_->devices.size();
    status["healthMonitorRunning"] = pimpl_->healthMonitorRunning.load();

    return status;
}

json DeviceManager::getStatistics() const {
    return pimpl_->statistics.toJson();
}

void DeviceManager::resetStatistics() {
    pimpl_->statistics.reset();
    LOG_INFO("DeviceManager: Statistics reset");
}

}  // namespace lithium
