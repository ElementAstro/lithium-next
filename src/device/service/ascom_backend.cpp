/*
 * ascom_backend.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM device backend implementation

*************************************************/

#include "ascom_backend.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "client/ascom/ascom_client.hpp"

namespace lithium::device {

// ==================== ASCOMBackend Implementation ====================

ASCOMBackend::ASCOMBackend(std::shared_ptr<ASCOMAdapter> adapter)
    : adapter_(std::move(adapter)) {
    LOG_INFO("ASCOMBackend created with existing adapter");
}

ASCOMBackend::ASCOMBackend()
    : adapter_(ASCOMAdapterFactory::createAdapter()) {
    LOG_INFO("ASCOMBackend created with new adapter");
}

ASCOMBackend::~ASCOMBackend() {
    if (adapter_ && adapter_->isServerConnected()) {
        adapter_->disconnectServer();
    }
    LOG_INFO("ASCOMBackend destroyed");
}

// ==================== Server Connection ====================

auto ASCOMBackend::connectServer(const BackendConfig& config) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        LOG_ERROR("ASCOMBackend: No adapter available");
        return false;
    }

    config_ = config;
    int port = config.port > 0 ? config.port : 11111;  // ASCOM Alpaca default port

    bool result = adapter_->connectServer(config.host, port);

    if (result) {
        LOG_INFO("ASCOMBackend: Connected to server {}:{}", config.host, port);

        // Emit server connected event
        BackendEvent event;
        event.type = BackendEventType::SERVER_CONNECTED;
        event.backendName = "ASCOM";
        event.message = "Connected to ASCOM Alpaca server";
        event.timestamp = std::chrono::system_clock::now();
        emitEvent(event);
    } else {
        LOG_ERROR("ASCOMBackend: Failed to connect to server {}:{}", config.host, port);
    }

    return result;
}

auto ASCOMBackend::disconnectServer() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return true;
    }

    bool result = adapter_->disconnectServer();
    cachedDevices_.clear();

    if (result) {
        LOG_INFO("ASCOMBackend: Disconnected from server");

        BackendEvent event;
        event.type = BackendEventType::SERVER_DISCONNECTED;
        event.backendName = "ASCOM";
        event.message = "Disconnected from ASCOM Alpaca server";
        event.timestamp = std::chrono::system_clock::now();
        emitEvent(event);
    }

    return result;
}

auto ASCOMBackend::isServerConnected() const -> bool {
    if (!adapter_) {
        return false;
    }
    return adapter_->isServerConnected();
}

auto ASCOMBackend::getServerStatus() const -> json {
    std::lock_guard<std::mutex> lock(mutex_);

    json status;
    status["backend"] = "ASCOM";
    status["connected"] = isServerConnected();
    status["host"] = config_.host;
    status["port"] = config_.port > 0 ? config_.port : 11111;
    status["deviceCount"] = cachedDevices_.size();

    if (adapter_) {
        status["serverInfo"] = adapter_->getServerInfo();
    }

    return status;
}

// ==================== Device Discovery ====================

auto ASCOMBackend::discoverDevices(int timeout) -> std::vector<DiscoveredDevice> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_ || !adapter_->isServerConnected()) {
        LOG_WARN("ASCOMBackend: Cannot discover devices - not connected to server");
        return {};
    }

    // Get devices from adapter
    auto ascomDevices = adapter_->getDevices();

    cachedDevices_.clear();
    cachedDevices_.reserve(ascomDevices.size());

    for (const auto& dev : ascomDevices) {
        cachedDevices_.push_back(convertToDiscoveredDevice(dev));
    }

    LOG_INFO("ASCOMBackend: Discovered {} devices", cachedDevices_.size());
    return cachedDevices_;
}

auto ASCOMBackend::getDevices() -> std::vector<DiscoveredDevice> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (cachedDevices_.empty() && adapter_ && adapter_->isServerConnected()) {
        // Refresh cache if empty
        auto ascomDevices = adapter_->getDevices();
        cachedDevices_.reserve(ascomDevices.size());
        for (const auto& dev : ascomDevices) {
            cachedDevices_.push_back(convertToDiscoveredDevice(dev));
        }
    }

    return cachedDevices_;
}

auto ASCOMBackend::getDevice(const std::string& deviceId)
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

auto ASCOMBackend::refreshDevices() -> int {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_ || !adapter_->isServerConnected()) {
        return 0;
    }

    auto ascomDevices = adapter_->getDevices();

    cachedDevices_.clear();
    cachedDevices_.reserve(ascomDevices.size());

    for (const auto& dev : ascomDevices) {
        cachedDevices_.push_back(convertToDiscoveredDevice(dev));
    }

    LOG_INFO("ASCOMBackend: Refreshed device list, found {} devices", cachedDevices_.size());
    return static_cast<int>(cachedDevices_.size());
}

// ==================== Device Connection ====================

auto ASCOMBackend::connectDevice(const std::string& deviceId) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return false;
    }

    bool result = adapter_->connectDevice(deviceId);

    if (result) {
        LOG_INFO("ASCOMBackend: Connected to device {}", deviceId);

        // Update cached device state
        for (auto& dev : cachedDevices_) {
            if (dev.deviceId == deviceId) {
                dev.isConnected = true;
                break;
            }
        }

        BackendEvent event;
        event.type = BackendEventType::DEVICE_CONNECTED;
        event.backendName = "ASCOM";
        event.deviceId = deviceId;
        event.message = "Device connected";
        event.timestamp = std::chrono::system_clock::now();
        emitEvent(event);
    }

    return result;
}

auto ASCOMBackend::disconnectDevice(const std::string& deviceId) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return true;
    }

    bool result = adapter_->disconnectDevice(deviceId);

    if (result) {
        LOG_INFO("ASCOMBackend: Disconnected from device {}", deviceId);

        // Update cached device state
        for (auto& dev : cachedDevices_) {
            if (dev.deviceId == deviceId) {
                dev.isConnected = false;
                break;
            }
        }

        BackendEvent event;
        event.type = BackendEventType::DEVICE_DISCONNECTED;
        event.backendName = "ASCOM";
        event.deviceId = deviceId;
        event.message = "Device disconnected";
        event.timestamp = std::chrono::system_clock::now();
        emitEvent(event);
    }

    return result;
}

auto ASCOMBackend::isDeviceConnected(const std::string& deviceId) const -> bool {
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

auto ASCOMBackend::getProperty(const std::string& deviceId,
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

auto ASCOMBackend::setProperty(const std::string& deviceId,
                               const std::string& propertyName,
                               const json& value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return false;
    }

    return adapter_->setProperty(deviceId, propertyName, value);
}

auto ASCOMBackend::getAllProperties(const std::string& deviceId)
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

void ASCOMBackend::registerEventCallback(BackendEventCallback callback) {
    eventCallback_ = std::move(callback);

    // Also register with adapter
    if (adapter_) {
        adapter_->registerEventCallback(
            [this](const ASCOMEvent& event) {
                handleASCOMEvent(event);
            });
    }
}

void ASCOMBackend::unregisterEventCallback() {
    eventCallback_ = nullptr;

    if (adapter_) {
        adapter_->unregisterEventCallback();
    }
}

// ==================== ASCOM-Specific ====================

auto ASCOMBackend::executeAction(const std::string& deviceId,
                                 const std::string& action,
                                 const std::string& parameters) -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return "";
    }

    return adapter_->executeAction(deviceId, action, parameters);
}

auto ASCOMBackend::getSupportedActions(const std::string& deviceId)
    -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!adapter_) {
        return {};
    }

    return adapter_->getSupportedActions(deviceId);
}

auto ASCOMBackend::discoverServers(int timeout) -> std::vector<std::string> {
    return lithium::client::ASCOMClient::discoverServers(timeout);
}

// ==================== Private Methods ====================

auto ASCOMBackend::convertToDiscoveredDevice(const ASCOMDeviceInfo& info)
    -> DiscoveredDevice {
    DiscoveredDevice dev;
    dev.deviceId = info.name;
    dev.displayName = info.name;
    dev.deviceType = info.deviceType;
    dev.driverName = info.driverInfo;
    dev.driverVersion = info.driverVersion;
    dev.connectionString = config_.host + ":" + std::to_string(config_.port > 0 ? config_.port : 11111);
    dev.isConnected = info.isConnected;

    // Add ASCOM-specific properties
    dev.customProperties["backend"] = "ASCOM";
    dev.customProperties["deviceNumber"] = info.deviceNumber;
    dev.customProperties["uniqueId"] = info.uniqueId;

    return dev;
}

auto ASCOMBackend::convertPropertyToJson(const ASCOMPropertyValue& prop) -> json {
    return prop.toJson();
}

void ASCOMBackend::handleASCOMEvent(const ASCOMEvent& event) {
    if (!eventCallback_) {
        return;
    }

    BackendEvent backendEvent;
    backendEvent.backendName = "ASCOM";
    backendEvent.deviceId = event.deviceName;
    backendEvent.message = event.message;
    backendEvent.data = event.data;
    backendEvent.timestamp = event.timestamp;

    switch (event.type) {
        case ASCOMEventType::DEVICE_CONNECTED:
            backendEvent.type = BackendEventType::DEVICE_CONNECTED;
            break;
        case ASCOMEventType::DEVICE_DISCONNECTED:
            backendEvent.type = BackendEventType::DEVICE_DISCONNECTED;
            break;
        case ASCOMEventType::PROPERTY_CHANGED:
            backendEvent.type = BackendEventType::DEVICE_UPDATED;
            break;
        case ASCOMEventType::SERVER_CONNECTED:
            backendEvent.type = BackendEventType::SERVER_CONNECTED;
            break;
        case ASCOMEventType::SERVER_DISCONNECTED:
            backendEvent.type = BackendEventType::SERVER_DISCONNECTED;
            break;
        case ASCOMEventType::ERROR:
            backendEvent.type = BackendEventType::ERROR;
            break;
        default:
            backendEvent.type = BackendEventType::DEVICE_UPDATED;
            break;
    }

    emitEvent(backendEvent);
}

}  // namespace lithium::device
