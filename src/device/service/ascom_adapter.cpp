/*
 * ascom_adapter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: ASCOM protocol adapter implementation

*************************************************/

#include "ascom_adapter.hpp"
#include "client/ascom/ascom_client.hpp"

namespace lithium::device {

// ==================== ASCOMClientAdapter Implementation ====================

ASCOMClientAdapter::ASCOMClientAdapter(
    std::shared_ptr<lithium::client::ASCOMClient> client)
    : client_(std::move(client)), ownsClient_(false) {
    LOG_INFO("ASCOMClientAdapter created with existing client");
}

ASCOMClientAdapter::ASCOMClientAdapter()
    : client_(std::make_shared<lithium::client::ASCOMClient>("ascom_adapter")),
      ownsClient_(true) {
    LOG_INFO("ASCOMClientAdapter created with new client");
    client_->initialize();
}

ASCOMClientAdapter::~ASCOMClientAdapter() {
    if (ownsClient_ && client_) {
        client_->destroy();
    }
    LOG_INFO("ASCOMClientAdapter destroyed");
}

auto ASCOMClientAdapter::connectServer(const std::string& host, int port)
    -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        LOG_ERROR("ASCOMClientAdapter: No client available");
        return false;
    }

    std::string target = host + ":" + std::to_string(port);
    if (!client_->connect(target)) {
        LOG_ERROR("ASCOMClientAdapter: Failed to connect to %s",
                  target.c_str());
        return false;
    }

    LOG_INFO("ASCOMClientAdapter: Connected to server %s:%d", host.c_str(),
             port);
    return true;
}

auto ASCOMClientAdapter::disconnectServer() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        return true;
    }

    client_->disconnect();
    LOG_INFO("ASCOMClientAdapter: Disconnected from server");
    return true;
}

auto ASCOMClientAdapter::isServerConnected() const -> bool {
    if (!client_) {
        return false;
    }
    return client_->isConnected();
}

auto ASCOMClientAdapter::getDevices() -> std::vector<ASCOMDeviceInfo> {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ASCOMDeviceInfo> result;

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

auto ASCOMClientAdapter::getDevice(const std::string& deviceName)
    -> std::optional<ASCOMDeviceInfo> {
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

auto ASCOMClientAdapter::connectDevice(const std::string& deviceName) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        return false;
    }

    bool result = client_->connectDevice(deviceName);

    if (result && eventCallback_) {
        ASCOMEvent event;
        event.type = ASCOMEventType::DEVICE_CONNECTED;
        event.deviceName = deviceName;
        event.message = "Device connected";
        event.timestamp = std::chrono::system_clock::now();
        eventCallback_(event);
    }

    return result;
}

auto ASCOMClientAdapter::disconnectDevice(const std::string& deviceName)
    -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        return true;
    }

    bool result = client_->disconnectDevice(deviceName);

    if (result && eventCallback_) {
        ASCOMEvent event;
        event.type = ASCOMEventType::DEVICE_DISCONNECTED;
        event.deviceName = deviceName;
        event.message = "Device disconnected";
        event.timestamp = std::chrono::system_clock::now();
        eventCallback_(event);
    }

    return result;
}

auto ASCOMClientAdapter::getProperty(const std::string& deviceName,
                                     const std::string& propertyName)
    -> std::optional<ASCOMPropertyValue> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        return std::nullopt;
    }

    std::string value = client_->getProperty(deviceName, propertyName, "");
    if (value.empty()) {
        return std::nullopt;
    }

    ASCOMPropertyValue prop;
    prop.name = propertyName;

    // Try to determine type from value
    if (value == "true" || value == "false") {
        prop.type = ASCOMPropertyType::BOOLEAN;
        prop.boolValue = (value == "true");
    } else {
        try {
            prop.numberValue = std::stod(value);
            prop.type = ASCOMPropertyType::NUMBER;
        } catch (...) {
            prop.type = ASCOMPropertyType::STRING;
            prop.stringValue = value;
        }
    }

    return prop;
}

auto ASCOMClientAdapter::setProperty(const std::string& deviceName,
                                     const std::string& propertyName,
                                     const json& value) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        return false;
    }

    std::string valueStr;
    if (value.is_string()) {
        valueStr = value.get<std::string>();
    } else if (value.is_number()) {
        valueStr = std::to_string(value.get<double>());
    } else if (value.is_boolean()) {
        valueStr = value.get<bool>() ? "true" : "false";
    } else {
        valueStr = value.dump();
    }

    return client_->setProperty(deviceName, propertyName, "", valueStr);
}

auto ASCOMClientAdapter::executeAction(const std::string& deviceName,
                                       const std::string& action,
                                       const std::string& parameters)
    -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        return "";
    }

    return client_->executeAction(deviceName, action, parameters);
}

auto ASCOMClientAdapter::getSupportedActions(const std::string& deviceName)
    -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        return {};
    }

    return client_->getSupportedActions(deviceName);
}

void ASCOMClientAdapter::registerEventCallback(ASCOMEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = std::move(callback);

    // Also register with the underlying client for server events
    if (client_) {
        client_->registerServerEventCallback(
            [this](const lithium::client::ServerEvent& event) {
                if (!eventCallback_)
                    return;

                ASCOMEvent ascomEvent;
                ascomEvent.timestamp = event.timestamp;
                ascomEvent.message = event.message;
                ascomEvent.deviceName = event.source;

                switch (event.type) {
                    case lithium::client::ServerEventType::DeviceConnected:
                        ascomEvent.type = ASCOMEventType::DEVICE_CONNECTED;
                        break;
                    case lithium::client::ServerEventType::DeviceDisconnected:
                        ascomEvent.type = ASCOMEventType::DEVICE_DISCONNECTED;
                        break;
                    case lithium::client::ServerEventType::PropertyUpdated:
                        ascomEvent.type = ASCOMEventType::PROPERTY_CHANGED;
                        break;
                    case lithium::client::ServerEventType::ServerStarted:
                        ascomEvent.type = ASCOMEventType::SERVER_CONNECTED;
                        break;
                    case lithium::client::ServerEventType::ServerStopped:
                        ascomEvent.type = ASCOMEventType::SERVER_DISCONNECTED;
                        break;
                    case lithium::client::ServerEventType::ServerError:
                    case lithium::client::ServerEventType::DriverError:
                        ascomEvent.type = ASCOMEventType::ERROR;
                        break;
                    default:
                        ascomEvent.type = ASCOMEventType::PROPERTY_CHANGED;
                        break;
                }

                eventCallback_(ascomEvent);
            });
    }
}

void ASCOMClientAdapter::unregisterEventCallback() {
    std::lock_guard<std::mutex> lock(mutex_);
    eventCallback_ = nullptr;

    if (client_) {
        client_->unregisterServerEventCallback();
    }
}

auto ASCOMClientAdapter::getServerInfo() -> json {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!client_) {
        json info;
        info["connected"] = false;
        return info;
    }

    return client_->getServerStatus();
}

ASCOMDeviceInfo ASCOMClientAdapter::convertDeviceInfo(
    const lithium::client::DeviceInfo& info) const {
    ASCOMDeviceInfo result;
    result.name = info.name;
    result.deviceType = info.interfaceString;
    result.uniqueId = info.id;
    result.driverInfo = info.driver;
    result.driverVersion = info.driverVersion;
    result.isConnected = info.connected;
    result.lastUpdate = info.lastUpdate;

    // Extract device number from metadata if available
    auto it = info.metadata.find("deviceNumber");
    if (it != info.metadata.end()) {
        try {
            result.deviceNumber = std::stoi(it->second);
        } catch (...) {
            result.deviceNumber = 0;
        }
    }

    return result;
}

// ==================== Factory Implementation ====================

auto ASCOMAdapterFactory::createAdapter() -> std::shared_ptr<ASCOMAdapter> {
    return std::make_shared<ASCOMClientAdapter>();
}

auto ASCOMAdapterFactory::createAdapter(
    std::shared_ptr<lithium::client::ASCOMClient> client)
    -> std::shared_ptr<ASCOMAdapter> {
    return std::make_shared<ASCOMClientAdapter>(std::move(client));
}

}  // namespace lithium::device
