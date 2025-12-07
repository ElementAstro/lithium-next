/*
 * indigo_device_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Device Base Class - Implementation

**************************************************/

#include "indigo_device_base.hpp"

#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// INDIGODeviceBase Implementation
// ============================================================================

class INDIGODeviceBase::Impl {
public:
    explicit Impl(const std::string& deviceName, const std::string& deviceType)
        : indigoDeviceName_(deviceName), deviceType_(deviceType) {
        connectionInfo_.deviceName = deviceName;
    }

    ~Impl() {
        if (client_ && isConnected()) {
            disconnect();
        }
    }

    auto connect(const json& params) -> DeviceResult<bool> {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Parse connection parameters
        if (params.contains("host")) {
            connectionInfo_.host = params["host"].get<std::string>();
        }
        if (params.contains("port")) {
            connectionInfo_.port = params["port"].get<int>();
        }
        if (params.contains("deviceName")) {
            connectionInfo_.deviceName = params["deviceName"].get<std::string>();
            indigoDeviceName_ = connectionInfo_.deviceName;
        }

        // Create client if not provided
        if (!client_) {
            INDIGOClient::Config config;
            config.host = connectionInfo_.host;
            config.port = connectionInfo_.port;
            config.connectTimeout = connectionInfo_.connectTimeout;
            config.commandTimeout = connectionInfo_.commandTimeout;
            config.autoReconnect = connectionInfo_.autoReconnect;
            client_ = std::make_shared<INDIGOClient>(config);
        }

        // Connect to INDIGO server
        if (!client_->isConnected()) {
            auto result = client_->connect(connectionInfo_.host,
                                           connectionInfo_.port);
            if (!result.has_value()) {
                status_.serverConnected = false;
                return result;
            }
        }

        status_.serverConnected = true;

        // Set up callbacks
        setupCallbacks();

        // Connect to specific device
        auto deviceResult = client_->connectDevice(indigoDeviceName_);
        if (!deviceResult.has_value()) {
            return deviceResult;
        }

        // Wait for device connection
        lock.unlock();
        auto waitResult = waitForConnection(connectionInfo_.connectTimeout);
        lock.lock();

        if (!waitResult.has_value()) {
            return waitResult;
        }

        status_.deviceConnected = true;
        status_.lastUpdate = std::chrono::system_clock::now();

        LOG_F(INFO, "INDIGO[{}]: Connected successfully", indigoDeviceName_);
        return DeviceResult<bool>(true);
    }

    auto disconnect() -> DeviceResult<bool> {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        if (!client_) {
            return DeviceResult<bool>(true);
        }

        if (status_.deviceConnected) {
            auto result = client_->disconnectDevice(indigoDeviceName_);
            if (!result.has_value()) {
                LOG_F(WARNING, "INDIGO[{}]: Device disconnect warning: {}",
                      indigoDeviceName_, result.error().message);
            }
        }

        status_.deviceConnected = false;
        status_.serverConnected = false;
        status_.lastUpdate = std::chrono::system_clock::now();

        // Clear cached properties
        cachedProperties_.clear();

        LOG_F(INFO, "INDIGO[{}]: Disconnected", indigoDeviceName_);
        return DeviceResult<bool>(true);
    }

    [[nodiscard]] auto isConnected() const -> bool {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return status_.serverConnected && status_.deviceConnected;
    }

    auto reconnect() -> DeviceResult<bool> {
        auto disconnectResult = disconnect();
        if (!disconnectResult.has_value()) {
            return disconnectResult;
        }

        json params;
        params["host"] = connectionInfo_.host;
        params["port"] = connectionInfo_.port;
        params["deviceName"] = connectionInfo_.deviceName;

        return connect(params);
    }

    void setConnectionInfo(const INDIGOConnectionInfo& info) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        connectionInfo_ = info;
        indigoDeviceName_ = info.deviceName;
    }

    [[nodiscard]] auto getConnectionInfo() const -> const INDIGOConnectionInfo& {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return connectionInfo_;
    }

    [[nodiscard]] auto getDeviceStatus() const -> INDIGODeviceStatus {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return status_;
    }

    [[nodiscard]] auto getClient() const -> std::shared_ptr<INDIGOClient> {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return client_;
    }

    void setClient(std::shared_ptr<INDIGOClient> client) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        client_ = std::move(client);
    }

    auto getProperty(const std::string& propertyName) -> DeviceResult<Property> {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (!client_) {
            return DeviceError{DeviceErrorCode::NotConnected, "Client not initialized"};
        }

        // Check cache first
        auto it = cachedProperties_.find(propertyName);
        if (it != cachedProperties_.end()) {
            return it->second;
        }

        // Fetch from client
        lock.unlock();
        auto result = client_->getProperty(indigoDeviceName_, propertyName);
        lock.lock();

        if (result.has_value()) {
            cachedProperties_[propertyName] = result.value();
        }

        return result;
    }

    auto getAllProperties() -> DeviceResult<std::vector<Property>> {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (!client_) {
            return DeviceError{DeviceErrorCode::NotConnected, "Client not initialized"};
        }

        return client_->getDeviceProperties(indigoDeviceName_);
    }

    auto setTextProperty(const std::string& propertyName,
                         const std::string& elementName,
                         const std::string& value) -> DeviceResult<bool> {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (!client_) {
            return DeviceError{DeviceErrorCode::NotConnected, "Client not initialized"};
        }

        return client_->setTextProperty(indigoDeviceName_, propertyName,
                                        {{elementName, value}});
    }

    auto setNumberProperty(const std::string& propertyName,
                           const std::string& elementName,
                           double value) -> DeviceResult<bool> {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (!client_) {
            return DeviceError{DeviceErrorCode::NotConnected, "Client not initialized"};
        }

        return client_->setNumberProperty(indigoDeviceName_, propertyName,
                                          {{elementName, value}});
    }

    auto setSwitchProperty(const std::string& propertyName,
                           const std::string& elementName,
                           bool value) -> DeviceResult<bool> {
        return setSwitchProperty(propertyName, {{elementName, value}});
    }

    auto setSwitchProperty(
        const std::string& propertyName,
        const std::vector<std::pair<std::string, bool>>& elements)
        -> DeviceResult<bool> {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (!client_) {
            return DeviceError{DeviceErrorCode::NotConnected, "Client not initialized"};
        }

        return client_->setSwitchProperty(indigoDeviceName_, propertyName,
                                          elements);
    }

    auto getTextValue(const std::string& propertyName,
                      const std::string& elementName) -> DeviceResult<std::string> {
        auto propResult = getProperty(propertyName);
        if (!propResult.has_value()) {
            return DeviceError{propResult.error()};
        }

        const auto& prop = propResult.value();
        if (prop.type != PropertyType::Text) {
            return DeviceError{DeviceErrorCode::InvalidPropertyType,
                               "Property is not a text property"};
        }

        for (const auto& elem : prop.textElements) {
            if (elem.name == elementName) {
                return elem.value;
            }
        }

        return DeviceError{DeviceErrorCode::ElementNotFound,
                           "Element not found: " + elementName};
    }

    auto getNumberValue(const std::string& propertyName,
                        const std::string& elementName) -> DeviceResult<double> {
        auto propResult = getProperty(propertyName);
        if (!propResult.has_value()) {
            return DeviceError{propResult.error()};
        }

        const auto& prop = propResult.value();
        if (prop.type != PropertyType::Number) {
            return DeviceError{DeviceErrorCode::InvalidPropertyType,
                               "Property is not a number property"};
        }

        for (const auto& elem : prop.numberElements) {
            if (elem.name == elementName) {
                return elem.value;
            }
        }

        return DeviceError{DeviceErrorCode::ElementNotFound,
                           "Element not found: " + elementName};
    }

    auto getSwitchValue(const std::string& propertyName,
                        const std::string& elementName) -> DeviceResult<bool> {
        auto propResult = getProperty(propertyName);
        if (!propResult.has_value()) {
            return DeviceError{propResult.error()};
        }

        const auto& prop = propResult.value();
        if (prop.type != PropertyType::Switch) {
            return DeviceError{DeviceErrorCode::InvalidPropertyType,
                               "Property is not a switch property"};
        }

        for (const auto& elem : prop.switchElements) {
            if (elem.name == elementName) {
                return elem.value;
            }
        }

        return DeviceError{DeviceErrorCode::ElementNotFound,
                           "Element not found: " + elementName};
    }

    auto getActiveSwitchName(const std::string& propertyName)
        -> DeviceResult<std::string> {
        auto propResult = getProperty(propertyName);
        if (!propResult.has_value()) {
            return DeviceError{propResult.error()};
        }

        const auto& prop = propResult.value();
        if (prop.type != PropertyType::Switch) {
            return DeviceError{DeviceErrorCode::InvalidPropertyType,
                               "Property is not a switch property"};
        }

        for (const auto& elem : prop.switchElements) {
            if (elem.value) {
                return elem.name;
            }
        }

        return DeviceError{DeviceErrorCode::ElementNotFound,
                           "No active switch found"};
    }

    void onPropertyUpdate(const std::string& propertyName,
                          INDIGODeviceBase::PropertyCallback callback) {
        std::unique_lock<std::shared_mutex> lock(callbackMutex_);
        propertyCallbacks_[propertyName].push_back(std::move(callback));
    }

    void onConnectionStateChange(INDIGODeviceBase::ConnectionStateCallback callback) {
        std::unique_lock<std::shared_mutex> lock(callbackMutex_);
        connectionCallbacks_.push_back(std::move(callback));
    }

    auto waitForPropertyState(const std::string& propertyName,
                              PropertyState targetState,
                              std::chrono::milliseconds timeout)
        -> DeviceResult<bool> {
        auto startTime = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - startTime < timeout) {
            auto propResult = getProperty(propertyName);
            if (propResult.has_value() && propResult.value().state == targetState) {
                return DeviceResult<bool>(true);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return DeviceError{DeviceErrorCode::Timeout,
                           "Timeout waiting for property state"};
    }

    auto waitForNumberValue(const std::string& propertyName,
                            const std::string& elementName,
                            double expectedValue, double tolerance,
                            std::chrono::milliseconds timeout)
        -> DeviceResult<bool> {
        auto startTime = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - startTime < timeout) {
            auto valueResult = getNumberValue(propertyName, elementName);
            if (valueResult.has_value()) {
                double diff = std::abs(valueResult.value() - expectedValue);
                if (diff <= tolerance) {
                    return DeviceResult<bool>(true);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return DeviceError{DeviceErrorCode::Timeout,
                           "Timeout waiting for number value"};
    }

    [[nodiscard]] auto getDeviceInfo() const -> json {
        json info;
        info["deviceName"] = indigoDeviceName_;
        info["deviceType"] = deviceType_;
        info["host"] = connectionInfo_.host;
        info["port"] = connectionInfo_.port;
        info["connected"] = isConnected();

        // Get INFO property values
        auto implPtr = const_cast<Impl*>(this);
        auto driverResult = implPtr->getTextValue("INFO", "DEVICE_DRIVER");
        if (driverResult.has_value()) {
            info["driver"] = driverResult.value();
        }

        auto versionResult = implPtr->getTextValue("INFO", "DEVICE_VERSION");
        if (versionResult.has_value()) {
            info["version"] = versionResult.value();
        }

        auto ifaceResult = implPtr->getTextValue("INFO", "DEVICE_INTERFACE");
        if (ifaceResult.has_value()) {
            info["interfaces"] = ifaceResult.value();
        }

        return info;
    }

    [[nodiscard]] auto getDriverName() const -> std::string {
        auto implPtr = const_cast<Impl*>(this);
        auto result = implPtr->getTextValue("INFO", "DEVICE_DRIVER");
        return result.value_or("");
    }

    [[nodiscard]] auto getDriverVersion() const -> std::string {
        auto implPtr = const_cast<Impl*>(this);
        auto result = implPtr->getTextValue("INFO", "DEVICE_VERSION");
        return result.value_or("");
    }

    [[nodiscard]] auto getDeviceInterfaces() const -> DeviceInterface {
        auto implPtr = const_cast<Impl*>(this);
        auto result = implPtr->getTextValue("INFO", "DEVICE_INTERFACE");
        if (!result.has_value()) {
            return DeviceInterface::None;
        }
        try {
            return static_cast<DeviceInterface>(std::stoul(result.value()));
        } catch (...) {
            return DeviceInterface::None;
        }
    }

    auto enableBlob(bool enable, bool urlMode) -> DeviceResult<bool> {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (!client_) {
            return DeviceError{DeviceErrorCode::NotConnected, "Client not initialized"};
        }

        return client_->enableBlob(indigoDeviceName_, enable, urlMode);
    }

    auto fetchBlob(const std::string& url) -> DeviceResult<std::vector<uint8_t>> {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (!client_) {
            return DeviceError{DeviceErrorCode::NotConnected, "Client not initialized"};
        }

        return client_->fetchBlobUrl(url);
    }

    [[nodiscard]] auto getINDIGODeviceName() const -> const std::string& {
        return indigoDeviceName_;
    }

    [[nodiscard]] auto isServerConnected() const -> bool {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return status_.serverConnected;
    }

    // Callback registration for derived class hooks
    std::function<void()> onConnectedCallback;
    std::function<void()> onDisconnectedCallback;
    std::function<void(const Property&)> onPropertyUpdatedCallback;

private:
    void setupCallbacks() {
        if (!client_)
            return;

        // Property update callback
        client_->setPropertyUpdateCallback([this](const Property& prop) {
            if (prop.device != indigoDeviceName_)
                return;

            // Update cache
            {
                std::unique_lock<std::shared_mutex> lock(mutex_);
                cachedProperties_[prop.name] = prop;
                status_.lastUpdate = std::chrono::system_clock::now();
            }

            // Call registered callbacks
            {
                std::shared_lock<std::shared_mutex> lock(callbackMutex_);

                // Specific property callbacks
                auto it = propertyCallbacks_.find(prop.name);
                if (it != propertyCallbacks_.end()) {
                    for (const auto& cb : it->second) {
                        cb(prop);
                    }
                }

                // All-property callbacks
                auto allIt = propertyCallbacks_.find("");
                if (allIt != propertyCallbacks_.end()) {
                    for (const auto& cb : allIt->second) {
                        cb(prop);
                    }
                }
            }

            // Notify derived class
            if (onPropertyUpdatedCallback) {
                onPropertyUpdatedCallback(prop);
            }
        });

        // Device callback
        client_->setDeviceCallback([this](const std::string& device, bool attached) {
            if (device != indigoDeviceName_)
                return;

            if (attached) {
                {
                    std::unique_lock<std::shared_mutex> lock(mutex_);
                    status_.deviceConnected = true;
                }
                connectionCv_.notify_all();

                if (onConnectedCallback) {
                    onConnectedCallback();
                }

                // Notify connection callbacks
                std::shared_lock<std::shared_mutex> lock(callbackMutex_);
                for (const auto& cb : connectionCallbacks_) {
                    cb(true);
                }
            } else {
                {
                    std::unique_lock<std::shared_mutex> lock(mutex_);
                    status_.deviceConnected = false;
                }

                if (onDisconnectedCallback) {
                    onDisconnectedCallback();
                }

                // Notify connection callbacks
                std::shared_lock<std::shared_mutex> lock(callbackMutex_);
                for (const auto& cb : connectionCallbacks_) {
                    cb(false);
                }
            }
        });

        // Connection callback
        client_->setConnectionCallback([this](bool connected, const std::string& msg) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            status_.serverConnected = connected;
            status_.message = msg;

            if (!connected) {
                status_.deviceConnected = false;
            }
        });
    }

    auto waitForConnection(std::chrono::seconds timeout) -> DeviceResult<bool> {
        std::unique_lock<std::mutex> lock(connectionMutex_);
        bool result = connectionCv_.wait_for(lock, timeout, [this] {
            std::shared_lock<std::shared_mutex> slock(mutex_);
            return status_.deviceConnected;
        });

        if (!result) {
            return DeviceError{DeviceErrorCode::Timeout,
                               "Timeout waiting for device connection"};
        }

        return DeviceResult<bool>(true);
    }

    std::string indigoDeviceName_;
    std::string deviceType_;

    INDIGOConnectionInfo connectionInfo_;
    INDIGODeviceStatus status_;

    std::shared_ptr<INDIGOClient> client_;

    mutable std::shared_mutex mutex_;
    mutable std::shared_mutex callbackMutex_;

    std::mutex connectionMutex_;
    std::condition_variable connectionCv_;

    std::unordered_map<std::string, Property> cachedProperties_;

    std::unordered_map<std::string,
                       std::vector<INDIGODeviceBase::PropertyCallback>>
        propertyCallbacks_;
    std::vector<INDIGODeviceBase::ConnectionStateCallback> connectionCallbacks_;
};

// ============================================================================
// INDIGODeviceBase public interface
// ============================================================================

INDIGODeviceBase::INDIGODeviceBase(const std::string& deviceName,
                                   const std::string& deviceType)
    : AtomDriver(deviceName), impl_(std::make_unique<Impl>(deviceName, deviceType)) {
    // Set up derived class callback hooks
    impl_->onConnectedCallback = [this]() { onConnected(); };
    impl_->onDisconnectedCallback = [this]() { onDisconnected(); };
    impl_->onPropertyUpdatedCallback = [this](const Property& p) {
        onPropertyUpdated(p);
    };
}

INDIGODeviceBase::~INDIGODeviceBase() = default;

INDIGODeviceBase::INDIGODeviceBase(INDIGODeviceBase&&) noexcept = default;
INDIGODeviceBase& INDIGODeviceBase::operator=(INDIGODeviceBase&&) noexcept = default;

auto INDIGODeviceBase::connect(const json& params) -> DeviceResult<bool> {
    return impl_->connect(params);
}

auto INDIGODeviceBase::disconnect() -> DeviceResult<bool> {
    return impl_->disconnect();
}

auto INDIGODeviceBase::isConnected() const -> bool {
    return impl_->isConnected();
}

auto INDIGODeviceBase::reconnect() -> DeviceResult<bool> {
    return impl_->reconnect();
}

void INDIGODeviceBase::setConnectionInfo(const INDIGOConnectionInfo& info) {
    impl_->setConnectionInfo(info);
}

auto INDIGODeviceBase::getConnectionInfo() const -> const INDIGOConnectionInfo& {
    return impl_->getConnectionInfo();
}

auto INDIGODeviceBase::getDeviceStatus() const -> INDIGODeviceStatus {
    return impl_->getDeviceStatus();
}

auto INDIGODeviceBase::getClient() const -> std::shared_ptr<INDIGOClient> {
    return impl_->getClient();
}

void INDIGODeviceBase::setClient(std::shared_ptr<INDIGOClient> client) {
    impl_->setClient(std::move(client));
}

auto INDIGODeviceBase::getProperty(const std::string& propertyName)
    -> DeviceResult<Property> {
    return impl_->getProperty(propertyName);
}

auto INDIGODeviceBase::getAllProperties() -> DeviceResult<std::vector<Property>> {
    return impl_->getAllProperties();
}

auto INDIGODeviceBase::setTextProperty(const std::string& propertyName,
                                       const std::string& elementName,
                                       const std::string& value)
    -> DeviceResult<bool> {
    return impl_->setTextProperty(propertyName, elementName, value);
}

auto INDIGODeviceBase::setNumberProperty(const std::string& propertyName,
                                         const std::string& elementName,
                                         double value) -> DeviceResult<bool> {
    return impl_->setNumberProperty(propertyName, elementName, value);
}

auto INDIGODeviceBase::setSwitchProperty(const std::string& propertyName,
                                         const std::string& elementName,
                                         bool value) -> DeviceResult<bool> {
    return impl_->setSwitchProperty(propertyName, elementName, value);
}

auto INDIGODeviceBase::setSwitchProperty(
    const std::string& propertyName,
    const std::vector<std::pair<std::string, bool>>& elements)
    -> DeviceResult<bool> {
    return impl_->setSwitchProperty(propertyName, elements);
}

auto INDIGODeviceBase::getTextValue(const std::string& propertyName,
                                    const std::string& elementName)
    -> DeviceResult<std::string> {
    return impl_->getTextValue(propertyName, elementName);
}

auto INDIGODeviceBase::getNumberValue(const std::string& propertyName,
                                      const std::string& elementName)
    -> DeviceResult<double> {
    return impl_->getNumberValue(propertyName, elementName);
}

auto INDIGODeviceBase::getSwitchValue(const std::string& propertyName,
                                      const std::string& elementName)
    -> DeviceResult<bool> {
    return impl_->getSwitchValue(propertyName, elementName);
}

auto INDIGODeviceBase::getActiveSwitchName(const std::string& propertyName)
    -> DeviceResult<std::string> {
    return impl_->getActiveSwitchName(propertyName);
}

void INDIGODeviceBase::onPropertyUpdate(const std::string& propertyName,
                                        PropertyCallback callback) {
    impl_->onPropertyUpdate(propertyName, std::move(callback));
}

void INDIGODeviceBase::onConnectionStateChange(ConnectionStateCallback callback) {
    impl_->onConnectionStateChange(std::move(callback));
}

auto INDIGODeviceBase::waitForPropertyState(const std::string& propertyName,
                                            PropertyState targetState,
                                            std::chrono::milliseconds timeout)
    -> DeviceResult<bool> {
    return impl_->waitForPropertyState(propertyName, targetState, timeout);
}

auto INDIGODeviceBase::waitForNumberValue(const std::string& propertyName,
                                          const std::string& elementName,
                                          double expectedValue, double tolerance,
                                          std::chrono::milliseconds timeout)
    -> DeviceResult<bool> {
    return impl_->waitForNumberValue(propertyName, elementName, expectedValue,
                                     tolerance, timeout);
}

auto INDIGODeviceBase::getDeviceInfo() const -> json {
    return impl_->getDeviceInfo();
}

auto INDIGODeviceBase::getDriverName() const -> std::string {
    return impl_->getDriverName();
}

auto INDIGODeviceBase::getDriverVersion() const -> std::string {
    return impl_->getDriverVersion();
}

auto INDIGODeviceBase::getDeviceInterfaces() const -> DeviceInterface {
    return impl_->getDeviceInterfaces();
}

auto INDIGODeviceBase::enableBlob(bool enable, bool urlMode) -> DeviceResult<bool> {
    return impl_->enableBlob(enable, urlMode);
}

auto INDIGODeviceBase::fetchBlob(const std::string& url)
    -> DeviceResult<std::vector<uint8_t>> {
    return impl_->fetchBlob(url);
}

void INDIGODeviceBase::onConnected() {
    // Default implementation - derived classes can override
    LOG_F(INFO, "INDIGO[{}]: Device connected callback", getINDIGODeviceName());
}

void INDIGODeviceBase::onDisconnected() {
    // Default implementation - derived classes can override
    LOG_F(INFO, "INDIGO[{}]: Device disconnected callback", getINDIGODeviceName());
}

void INDIGODeviceBase::onPropertyUpdated(const Property& property) {
    // Default implementation - derived classes can override
    LOG_F(DEBUG, "INDIGO[{}]: Property {} updated",
          getINDIGODeviceName(), property.name);
}

auto INDIGODeviceBase::getINDIGODeviceName() const -> const std::string& {
    return impl_->getINDIGODeviceName();
}

auto INDIGODeviceBase::isServerConnected() const -> bool {
    return impl_->isServerConnected();
}

void INDIGODeviceBase::logDevice(int level, const std::string& message) const {
    switch (level) {
        case 0:  // Error
            LOG_F(ERROR, "INDIGO[{}]: {}", getINDIGODeviceName(), message);
            break;
        case 1:  // Warning
            LOG_F(WARNING, "INDIGO[{}]: {}", getINDIGODeviceName(), message);
            break;
        case 2:  // Info
            LOG_F(INFO, "INDIGO[{}]: {}", getINDIGODeviceName(), message);
            break;
        default:  // Debug
            LOG_F(INFO, "INDIGO[{}]: {}", getINDIGODeviceName(), message);
            break;
    }
}

}  // namespace lithium::client::indigo
