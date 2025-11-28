/*
 * indi_adapter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI protocol adapter implementation

*************************************************/

#include "indi_adapter.hpp"
#include "client/indi/indi_client.hpp"

#include <thread>

namespace lithium::device {

// ==================== INDIClientAdapter Implementation ====================

INDIClientAdapter::INDIClientAdapter(std::shared_ptr<lithium::client::INDIClient> client)
    : client_(std::move(client)), ownsClient_(false) {
    LOG_INFO( "INDIClientAdapter created with existing client");
}

INDIClientAdapter::INDIClientAdapter()
    : client_(std::make_shared<lithium::client::INDIClient>("indi_adapter")),
      ownsClient_(true) {
    LOG_INFO( "INDIClientAdapter created with new client");
    client_->initialize();
}

INDIClientAdapter::~INDIClientAdapter() {
    if (ownsClient_ && client_) {
        client_->destroy();
    }
    LOG_INFO( "INDIClientAdapter destroyed");
}

auto INDIClientAdapter::connectServer(const std::string& host, int port) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        LOG_ERROR( "INDIClientAdapter: No client available");
        return false;
    }
    
    std::string target = host + ":" + std::to_string(port);
    if (!client_->connect(target)) {
        LOG_ERROR( "INDIClientAdapter: Failed to connect to %s", target.c_str());
        return false;
    }
    
    if (!client_->startServer()) {
        LOG_WARN( "INDIClientAdapter: Server may already be running");
    }
    
    LOG_INFO( "INDIClientAdapter: Connected to server %s:%d", host.c_str(), port);
    return true;
}

auto INDIClientAdapter::disconnectServer() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        return true;
    }
    
    client_->disconnect();
    LOG_INFO( "INDIClientAdapter: Disconnected from server");
    return true;
}

auto INDIClientAdapter::isServerConnected() const -> bool {
    if (!client_) {
        return false;
    }
    return client_->isConnected();
}

auto INDIClientAdapter::getDevices() -> std::vector<INDIDeviceInfo> {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<INDIDeviceInfo> result;
    
    if (!client_) {
        return result;
    }
    
    auto devices = client_->getDevices();
    result.reserve(devices.size());
    
    for (const auto& dev : devices) {
        result.push_back(convertDeviceInfo(dev));
    }
    
    return result;
}

auto INDIClientAdapter::getDevice(const std::string& deviceName)
    -> std::optional<INDIDeviceInfo> {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        return std::nullopt;
    }
    
    auto deviceOpt = client_->getDevice(deviceName);
    if (!deviceOpt) {
        return std::nullopt;
    }
    
    return convertDeviceInfo(*deviceOpt);
}

auto INDIClientAdapter::connectDevice(const std::string& deviceName) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        return false;
    }
    
    bool result = client_->connectDevice(deviceName);
    
    if (result && eventCallback_) {
        INDIEvent event;
        event.type = INDIEventType::DEVICE_CONNECTED;
        event.deviceName = deviceName;
        event.message = "Device connected";
        event.timestamp = std::chrono::system_clock::now();
        eventCallback_(event);
    }
    
    return result;
}

auto INDIClientAdapter::disconnectDevice(const std::string& deviceName) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        return true;
    }
    
    bool result = client_->disconnectDevice(deviceName);
    
    if (result && eventCallback_) {
        INDIEvent event;
        event.type = INDIEventType::DEVICE_DISCONNECTED;
        event.deviceName = deviceName;
        event.message = "Device disconnected";
        event.timestamp = std::chrono::system_clock::now();
        eventCallback_(event);
    }
    
    return result;
}

auto INDIClientAdapter::getProperty(const std::string& deviceName,
                                     const std::string& propertyName)
    -> std::optional<INDIPropertyValue> {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        return std::nullopt;
    }
    
    auto deviceOpt = client_->getDevice(deviceName);
    if (!deviceOpt) {
        return std::nullopt;
    }
    
    auto it = deviceOpt->properties.find(propertyName);
    if (it != deviceOpt->properties.end()) {
        return convertPropertyValue(it->second);
    }
    
    return std::nullopt;
}

auto INDIClientAdapter::setNumberProperty(const std::string& deviceName,
                                           const std::string& propertyName,
                                           const std::string& elementName,
                                           double value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        return false;
    }
    
    return client_->setNumberProperty(deviceName, propertyName, elementName, value);
}

auto INDIClientAdapter::setSwitchProperty(const std::string& deviceName,
                                           const std::string& propertyName,
                                           const std::string& elementName,
                                           bool value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        return false;
    }
    
    return client_->setSwitchProperty(deviceName, propertyName, elementName, value);
}

auto INDIClientAdapter::setTextProperty(const std::string& deviceName,
                                         const std::string& propertyName,
                                         const std::string& elementName,
                                         const std::string& value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        return false;
    }
    
    return client_->setTextProperty(deviceName, propertyName, elementName, value);
}

void INDIClientAdapter::registerEventCallback(INDIEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = std::move(callback);
    
    // Also register with the underlying client for server events
    if (client_) {
        client_->registerServerEventCallback([this](const lithium::client::ServerEvent& event) {
            if (!eventCallback_) return;
            
            INDIEvent indiEvent;
            indiEvent.timestamp = event.timestamp;
            indiEvent.message = event.message;
            indiEvent.deviceName = event.source;
            
            switch (event.type) {
                case lithium::client::ServerEventType::DeviceConnected:
                    indiEvent.type = INDIEventType::DEVICE_CONNECTED;
                    break;
                case lithium::client::ServerEventType::DeviceDisconnected:
                    indiEvent.type = INDIEventType::DEVICE_DISCONNECTED;
                    break;
                case lithium::client::ServerEventType::PropertyDefined:
                    indiEvent.type = INDIEventType::PROPERTY_DEFINED;
                    break;
                case lithium::client::ServerEventType::PropertyUpdated:
                    indiEvent.type = INDIEventType::PROPERTY_UPDATED;
                    break;
                case lithium::client::ServerEventType::PropertyDeleted:
                    indiEvent.type = INDIEventType::PROPERTY_DELETED;
                    break;
                case lithium::client::ServerEventType::MessageReceived:
                    indiEvent.type = INDIEventType::MESSAGE_RECEIVED;
                    break;
                case lithium::client::ServerEventType::BlobReceived:
                    indiEvent.type = INDIEventType::BLOB_RECEIVED;
                    break;
                case lithium::client::ServerEventType::ServerStarted:
                    indiEvent.type = INDIEventType::SERVER_CONNECTED;
                    break;
                case lithium::client::ServerEventType::ServerStopped:
                    indiEvent.type = INDIEventType::SERVER_DISCONNECTED;
                    break;
                default:
                    indiEvent.type = INDIEventType::MESSAGE_RECEIVED;
                    break;
            }
            
            eventCallback_(indiEvent);
        });
    }
}

void INDIClientAdapter::unregisterEventCallback() {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = nullptr;
    
    if (client_) {
        client_->unregisterServerEventCallback();
    }
}

auto INDIClientAdapter::waitForPropertyState(const std::string& deviceName,
                                              const std::string& propertyName,
                                              INDIPropertyState targetState,
                                              std::chrono::milliseconds timeout)
    -> bool {
    auto startTime = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - startTime < timeout) {
        auto propOpt = getProperty(deviceName, propertyName);
        if (propOpt && propOpt->state == targetState) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

auto INDIClientAdapter::getServerInfo() -> json {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!client_) {
        json info;
        info["connected"] = false;
        return info;
    }
    
    return client_->getServerStatus();
}

INDIDeviceInfo INDIClientAdapter::convertDeviceInfo(
    const lithium::client::DeviceInfo& info) const {
    INDIDeviceInfo result;
    result.name = info.name;
    result.driverName = info.driver;
    result.driverVersion = info.driverVersion;
    result.driverInterface = info.interfaceString;
    result.isConnected = info.connected;
    result.lastUpdate = info.lastUpdate;
    
    // Convert properties
    for (const auto& [propName, propValue] : info.properties) {
        result.properties[propName] = convertPropertyValue(propValue);
    }
    
    return result;
}

INDIPropertyValue INDIClientAdapter::convertPropertyValue(
    const lithium::client::PropertyValue& prop) const {
    INDIPropertyValue result;
    result.name = prop.name;
    result.label = prop.label;
    
    // Convert type
    switch (prop.type) {
        case lithium::client::PropertyValue::Type::Number:
            result.type = INDIPropertyType::NUMBER;
            result.numberValue = prop.numberValue;
            result.numberMin = prop.numberMin;
            result.numberMax = prop.numberMax;
            result.numberStep = prop.numberStep;
            break;
        case lithium::client::PropertyValue::Type::Text:
            result.type = INDIPropertyType::TEXT;
            result.textValue = prop.textValue;
            break;
        case lithium::client::PropertyValue::Type::Switch:
            result.type = INDIPropertyType::SWITCH;
            result.switchValue = prop.switchValue;
            break;
        case lithium::client::PropertyValue::Type::Light:
            result.type = INDIPropertyType::LIGHT;
            break;
        case lithium::client::PropertyValue::Type::Blob:
            result.type = INDIPropertyType::BLOB;
            result.blobValue = prop.blobData;
            result.format = prop.blobFormat;
            break;
        default:
            result.type = INDIPropertyType::UNKNOWN;
            break;
    }
    
    // Convert state
    if (prop.state == "Idle") {
        result.state = INDIPropertyState::IDLE;
    } else if (prop.state == "Ok") {
        result.state = INDIPropertyState::OK;
    } else if (prop.state == "Busy") {
        result.state = INDIPropertyState::BUSY;
    } else if (prop.state == "Alert") {
        result.state = INDIPropertyState::ALERT;
    } else {
        result.state = INDIPropertyState::UNKNOWN;
    }
    
    return result;
}

// ==================== Factory Implementation ====================

auto INDIAdapterFactory::createAdapter() -> std::shared_ptr<INDIAdapter> {
    return std::make_shared<INDIClientAdapter>();
}

auto INDIAdapterFactory::createAdapter(std::shared_ptr<lithium::client::INDIClient> client)
    -> std::shared_ptr<INDIAdapter> {
    return std::make_shared<INDIClientAdapter>(std::move(client));
}

}  // namespace lithium::device
