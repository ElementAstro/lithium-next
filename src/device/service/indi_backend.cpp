/*
 * indi_backend.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI device backend implementation

*************************************************/

#include "indi_backend.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::device {

// ==================== INDIBackend Implementation ====================

INDIBackend::INDIBackend(std::shared_ptr<INDIAdapter> adapter)
    : adapter_(std::move(adapter)) {
    LOG_INFO("INDIBackend created with existing adapter");
}

INDIBackend::INDIBackend()
    : adapter_(INDIAdapterFactory::createAdapter()) {
    LOG_INFO("INDIBackend created with new adapter");
}

INDIBackend::~INDIBackend() {
    if (adapter_ && adapter_->isServerConnected()) {
        adapter_->disconnectServer();
    }
    LOG_INFO("INDIBackend destroyed");
}

// ==================== Server Connection ====================

auto INDIBackend::connectServer(const BackendConfig& config) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        LOG_ERROR("INDIBackend: No adapter available");
        return false;
    }

    config_ = config;
    int port = config.port > 0 ? config.port : 7624;  // INDI default port

    bool result = adapter_->connectServer(config.host, port);

    if (result) {
        LOG_INFO("INDIBackend: Connected to server {}:{}", config.host, port);

        // Emit server connected event
        BackendEvent event;
        event.type = BackendEventType::SERVER_CONNECTED;
        event.backendName = "INDI";
        event.message = "Connected to INDI server";
        event.timestamp = std::chrono::system_clock::now();
        emitEvent(event);
    } else {
        LOG_ERROR("INDIBackend: Failed to connect to server {}:{}", config.host, port);
    }

    return result;
}

auto INDIBackend::disconnectServer() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return true;
    }

    bool result = adapter_->disconnectServer();
    cachedDevices_.clear();

    if (result) {
        LOG_INFO("INDIBackend: Disconnected from server");

        BackendEvent event;
        event.type = BackendEventType::SERVER_DISCONNECTED;
        event.backendName = "INDI";
        event.message = "Disconnected from INDI server";
        event.timestamp = std::chrono::system_clock::now();
        emitEvent(event);
    }

    return result;
}

auto INDIBackend::isServerConnected() const -> bool {
    if (!adapter_) {
        return false;
    }
    return adapter_->isServerConnected();
}

auto INDIBackend::getServerStatus() const -> json {
    std::lock_guard<std::mutex> lock(mutex_);

    json status;
    status["backend"] = "INDI";
    status["connected"] = isServerConnected();
    status["host"] = config_.host;
    status["port"] = config_.port > 0 ? config_.port : 7624;
    status["deviceCount"] = cachedDevices_.size();

    if (adapter_) {
        status["serverInfo"] = adapter_->getServerInfo();
    }

    return status;
}

// ==================== Device Discovery ====================

auto INDIBackend::discoverDevices(int timeout) -> std::vector<DiscoveredDevice> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_ || !adapter_->isServerConnected()) {
        LOG_WARN("INDIBackend: Cannot discover devices - not connected to server");
        return {};
    }

    // Get devices from adapter
    auto indiDevices = adapter_->getDevices();

    cachedDevices_.clear();
    cachedDevices_.reserve(indiDevices.size());

    for (const auto& dev : indiDevices) {
        cachedDevices_.push_back(convertToDiscoveredDevice(dev));
    }

    LOG_INFO("INDIBackend: Discovered {} devices", cachedDevices_.size());
    return cachedDevices_;
}

auto INDIBackend::getDevices() -> std::vector<DiscoveredDevice> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (cachedDevices_.empty() && adapter_ && adapter_->isServerConnected()) {
        // Refresh cache if empty
        auto indiDevices = adapter_->getDevices();
        cachedDevices_.reserve(indiDevices.size());
        for (const auto& dev : indiDevices) {
            cachedDevices_.push_back(convertToDiscoveredDevice(dev));
        }
    }

    return cachedDevices_;
}

auto INDIBackend::getDevice(const std::string& deviceId)
    -> std::optional<DiscoveredDevice> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return std::nullopt;
    }

    auto deviceOpt = adapter_->getDevice(deviceId);
    if (!deviceOpt) {
        return std::nullopt;
    }

    return convertToDiscoveredDevice(*deviceOpt);
}

auto INDIBackend::refreshDevices() -> int {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_ || !adapter_->isServerConnected()) {
        return 0;
    }

    auto indiDevices = adapter_->getDevices();

    cachedDevices_.clear();
    cachedDevices_.reserve(indiDevices.size());

    for (const auto& dev : indiDevices) {
        cachedDevices_.push_back(convertToDiscoveredDevice(dev));
    }

    LOG_INFO("INDIBackend: Refreshed device list, found {} devices", cachedDevices_.size());
    return static_cast<int>(cachedDevices_.size());
}

// ==================== Device Connection ====================

auto INDIBackend::connectDevice(const std::string& deviceId) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return false;
    }

    bool result = adapter_->connectDevice(deviceId);

    if (result) {
        LOG_INFO("INDIBackend: Connected to device {}", deviceId);

        // Update cached device state
        for (auto& dev : cachedDevices_) {
            if (dev.deviceId == deviceId) {
                dev.isConnected = true;
                break;
            }
        }

        BackendEvent event;
        event.type = BackendEventType::DEVICE_CONNECTED;
        event.backendName = "INDI";
        event.deviceId = deviceId;
        event.message = "Device connected";
        event.timestamp = std::chrono::system_clock::now();
        emitEvent(event);
    }

    return result;
}

auto INDIBackend::disconnectDevice(const std::string& deviceId) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return true;
    }

    bool result = adapter_->disconnectDevice(deviceId);

    if (result) {
        LOG_INFO("INDIBackend: Disconnected from device {}", deviceId);

        // Update cached device state
        for (auto& dev : cachedDevices_) {
            if (dev.deviceId == deviceId) {
                dev.isConnected = false;
                break;
            }
        }

        BackendEvent event;
        event.type = BackendEventType::DEVICE_DISCONNECTED;
        event.backendName = "INDI";
        event.deviceId = deviceId;
        event.message = "Device disconnected";
        event.timestamp = std::chrono::system_clock::now();
        emitEvent(event);
    }

    return result;
}

auto INDIBackend::isDeviceConnected(const std::string& deviceId) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& dev : cachedDevices_) {
        if (dev.deviceId == deviceId) {
            return dev.isConnected;
        }
    }

    // Check from adapter if not in cache
    if (adapter_) {
        auto deviceOpt = adapter_->getDevice(deviceId);
        if (deviceOpt) {
            return deviceOpt->isConnected;
        }
    }

    return false;
}

// ==================== Property Access ====================

auto INDIBackend::getProperty(const std::string& deviceId,
                              const std::string& propertyName)
    -> std::optional<json> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return std::nullopt;
    }

    auto propOpt = adapter_->getProperty(deviceId, propertyName);
    if (!propOpt) {
        return std::nullopt;
    }

    return convertPropertyToJson(*propOpt);
}

auto INDIBackend::setProperty(const std::string& deviceId,
                              const std::string& propertyName,
                              const json& value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return false;
    }

    // Determine property type and set accordingly
    if (value.is_object()) {
        std::string type = value.value("type", "");
        std::string element = value.value("element", "");

        if (type == "number" && value.contains("value")) {
            return adapter_->setNumberProperty(deviceId, propertyName, element,
                                               value["value"].get<double>());
        } else if (type == "switch" && value.contains("value")) {
            return adapter_->setSwitchProperty(deviceId, propertyName, element,
                                               value["value"].get<bool>());
        } else if (type == "text" && value.contains("value")) {
            return adapter_->setTextProperty(deviceId, propertyName, element,
                                             value["value"].get<std::string>());
        }
    }

    LOG_WARN("INDIBackend: Unsupported property value format for {}.{}",
             deviceId, propertyName);
    return false;
}

auto INDIBackend::getAllProperties(const std::string& deviceId)
    -> std::unordered_map<std::string, json> {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, json> result;

    if (!adapter_) {
        return result;
    }

    auto deviceOpt = adapter_->getDevice(deviceId);
    if (!deviceOpt) {
        return result;
    }

    for (const auto& [propName, propValue] : deviceOpt->properties) {
        result[propName] = convertPropertyToJson(propValue);
    }

    return result;
}

// ==================== Event System ====================

void INDIBackend::registerEventCallback(BackendEventCallback callback) {
    eventCallback_ = std::move(callback);

    // Also register with adapter
    if (adapter_) {
        adapter_->registerEventCallback(
            [this](const INDIEvent& event) {
                handleINDIEvent(event);
            });
    }
}

void INDIBackend::unregisterEventCallback() {
    eventCallback_ = nullptr;

    if (adapter_) {
        adapter_->unregisterEventCallback();
    }
}

// ==================== INDI-Specific ====================

auto INDIBackend::setNumberProperty(const std::string& deviceId,
                                    const std::string& propertyName,
                                    const std::string& elementName,
                                    double value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return false;
    }

    return adapter_->setNumberProperty(deviceId, propertyName, elementName, value);
}

auto INDIBackend::setSwitchProperty(const std::string& deviceId,
                                    const std::string& propertyName,
                                    const std::string& elementName,
                                    bool value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return false;
    }

    return adapter_->setSwitchProperty(deviceId, propertyName, elementName, value);
}

auto INDIBackend::setTextProperty(const std::string& deviceId,
                                  const std::string& propertyName,
                                  const std::string& elementName,
                                  const std::string& value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return false;
    }

    return adapter_->setTextProperty(deviceId, propertyName, elementName, value);
}

auto INDIBackend::waitForPropertyState(const std::string& deviceId,
                                       const std::string& propertyName,
                                       INDIPropertyState targetState,
                                       std::chrono::milliseconds timeout) -> bool {
    if (!adapter_) {
        return false;
    }

    return adapter_->waitForPropertyState(deviceId, propertyName, targetState, timeout);
}

// ==================== Private Methods ====================

auto INDIBackend::convertToDiscoveredDevice(const INDIDeviceInfo& info)
    -> DiscoveredDevice {
    DiscoveredDevice dev;
    dev.deviceId = info.name;
    dev.displayName = info.name;
    dev.deviceType = info.driverInterface;
    dev.driverName = info.driverName;
    dev.driverVersion = info.driverVersion;
    dev.connectionString = config_.host + ":" + std::to_string(config_.port > 0 ? config_.port : 7624);
    dev.isConnected = info.isConnected;

    // Add INDI-specific properties
    dev.customProperties["backend"] = "INDI";
    dev.customProperties["interface"] = info.driverInterface;

    return dev;
}

auto INDIBackend::convertPropertyToJson(const INDIPropertyValue& prop) -> json {
    return prop.toJson();
}

void INDIBackend::handleINDIEvent(const INDIEvent& event) {
    if (!eventCallback_) {
        return;
    }

    BackendEvent backendEvent;
    backendEvent.backendName = "INDI";
    backendEvent.deviceId = event.deviceName;
    backendEvent.message = event.message;
    backendEvent.data = event.data;
    backendEvent.timestamp = event.timestamp;

    switch (event.type) {
        case INDIEventType::DEVICE_CONNECTED:
            backendEvent.type = BackendEventType::DEVICE_CONNECTED;
            break;
        case INDIEventType::DEVICE_DISCONNECTED:
            backendEvent.type = BackendEventType::DEVICE_DISCONNECTED;
            break;
        case INDIEventType::PROPERTY_DEFINED:
        case INDIEventType::PROPERTY_UPDATED:
            backendEvent.type = BackendEventType::DEVICE_UPDATED;
            break;
        case INDIEventType::SERVER_CONNECTED:
            backendEvent.type = BackendEventType::SERVER_CONNECTED;
            break;
        case INDIEventType::SERVER_DISCONNECTED:
            backendEvent.type = BackendEventType::SERVER_DISCONNECTED;
            break;
        case INDIEventType::ERROR:
            backendEvent.type = BackendEventType::ERROR;
            break;
        default:
            backendEvent.type = BackendEventType::DEVICE_UPDATED;
            break;
    }

    emitEvent(backendEvent);
}

}  // namespace lithium::device
