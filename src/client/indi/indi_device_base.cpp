/*
 * indi_device_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Device Base Class Implementation

**************************************************/

#include "indi_device_base.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDIDeviceBase::INDIDeviceBase(std::string name) : name_(std::move(name)) {
    LOG_DEBUG("INDIDeviceBase created: {}", name_);
}

INDIDeviceBase::~INDIDeviceBase() {
    if (connectionState_.load() == ConnectionState::Connected) {
        disconnect();
    }
    LOG_DEBUG("INDIDeviceBase destroyed: {}", name_);
}

// ==================== Lifecycle ====================

auto INDIDeviceBase::initialize() -> bool {
    if (initialized_.load()) {
        LOG_WARN("Device {} already initialized", name_);
        return true;
    }

    LOG_INFO("Initializing device: {}", name_);
    initialized_.store(true);
    return true;
}

auto INDIDeviceBase::destroy() -> bool {
    if (!initialized_.load()) {
        return true;
    }

    LOG_INFO("Destroying device: {}", name_);

    if (isConnected()) {
        disconnect();
    }

    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        properties_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        eventCallback_ = nullptr;
        propertyCallbacks_.clear();
    }

    initialized_.store(false);
    return true;
}

auto INDIDeviceBase::connect(const std::string& deviceName, int timeout,
                             int maxRetry) -> bool {
    if (connectionState_.load() == ConnectionState::Connected) {
        LOG_WARN("Device {} already connected", deviceName_);
        return true;
    }

    deviceName_ = deviceName;
    setConnectionState(ConnectionState::Connecting);

    LOG_INFO("Connecting to device: {}", deviceName_);

    // Subclasses should override this to implement actual connection logic
    // This base implementation just sets the state
    setConnectionState(ConnectionState::Connected);

    DeviceEvent event;
    event.type = DeviceEventType::Connected;
    event.deviceName = deviceName_;
    event.message = "Device connected";
    event.timestamp = std::chrono::system_clock::now();
    emitEvent(event);

    return true;
}

auto INDIDeviceBase::disconnect() -> bool {
    if (connectionState_.load() != ConnectionState::Connected) {
        return true;
    }

    setConnectionState(ConnectionState::Disconnecting);
    LOG_INFO("Disconnecting from device: {}", deviceName_);

    // Subclasses should override this to implement actual disconnection logic
    setConnectionState(ConnectionState::Disconnected);

    DeviceEvent event;
    event.type = DeviceEventType::Disconnected;
    event.deviceName = deviceName_;
    event.message = "Device disconnected";
    event.timestamp = std::chrono::system_clock::now();
    emitEvent(event);

    return true;
}

auto INDIDeviceBase::isConnected() const -> bool {
    return connectionState_.load() == ConnectionState::Connected;
}

auto INDIDeviceBase::getConnectionState() const -> ConnectionState {
    return connectionState_.load();
}

auto INDIDeviceBase::scan() -> std::vector<std::string> {
    // Subclasses should override this
    return {};
}

// ==================== Device Information ====================

auto INDIDeviceBase::getName() const -> const std::string& { return name_; }

auto INDIDeviceBase::getDeviceName() const -> const std::string& {
    return deviceName_;
}

auto INDIDeviceBase::getDriverInfo() const -> const DriverInfo& {
    return driverInfo_;
}

auto INDIDeviceBase::getStatus() const -> json {
    json status;
    status["name"] = name_;
    status["deviceName"] = deviceName_;
    status["connected"] = isConnected();
    status["initialized"] = initialized_.load();
    status["type"] = getDeviceType();

    status["driver"] = {{"name", driverInfo_.name},
                        {"exec", driverInfo_.exec},
                        {"version", driverInfo_.version},
                        {"interface", driverInfo_.interfaceStr}};

    // Add properties summary
    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        status["propertyCount"] = properties_.size();
    }

    return status;
}

// ==================== Property Management ====================

auto INDIDeviceBase::getProperties() const
    -> std::unordered_map<std::string, INDIProperty> {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_;
}

auto INDIDeviceBase::getProperty(const std::string& propertyName) const
    -> std::optional<INDIProperty> {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    auto it = properties_.find(propertyName);
    if (it != properties_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto INDIDeviceBase::setNumberProperty(const std::string& propertyName,
                                       const std::string& elementName,
                                       double value) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set property: device not connected");
        return false;
    }

    LOG_DEBUG("Setting number property {}.{} = {}", propertyName, elementName,
              value);

    // Subclasses should override this to send the property to INDI server
    return true;
}

auto INDIDeviceBase::setTextProperty(const std::string& propertyName,
                                     const std::string& elementName,
                                     const std::string& value) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set property: device not connected");
        return false;
    }

    LOG_DEBUG("Setting text property {}.{} = {}", propertyName, elementName,
              value);

    // Subclasses should override this to send the property to INDI server
    return true;
}

auto INDIDeviceBase::setSwitchProperty(const std::string& propertyName,
                                       const std::string& elementName,
                                       bool value) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set property: device not connected");
        return false;
    }

    LOG_DEBUG("Setting switch property {}.{} = {}", propertyName, elementName,
              value ? "ON" : "OFF");

    // Subclasses should override this to send the property to INDI server
    return true;
}

auto INDIDeviceBase::waitForPropertyState(const std::string& propertyName,
                                          PropertyState targetState,
                                          std::chrono::milliseconds timeout)
    -> bool {
    auto startTime = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - startTime < timeout) {
        auto prop = getProperty(propertyName);
        if (prop && prop->state == targetState) {
            return true;
        }

        std::unique_lock<std::mutex> lock(stateMutex_);
        stateCondition_.wait_for(lock, std::chrono::milliseconds(100));
    }

    LOG_WARN("Timeout waiting for property {} to reach state {}", propertyName,
             std::string(propertyStateToString(targetState)));
    return false;
}

// ==================== Event System ====================

void INDIDeviceBase::registerEventCallback(DeviceEventCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    eventCallback_ = std::move(callback);
}

void INDIDeviceBase::unregisterEventCallback() {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    eventCallback_ = nullptr;
}

void INDIDeviceBase::watchProperty(const std::string& propertyName,
                                   PropertyCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    propertyCallbacks_[propertyName] = std::move(callback);
}

void INDIDeviceBase::unwatchProperty(const std::string& propertyName) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    propertyCallbacks_.erase(propertyName);
}

// ==================== Internal Property Handling ====================

void INDIDeviceBase::onPropertyDefined(const INDIProperty& property) {
    updatePropertyCache(property);

    DeviceEvent event;
    event.type = DeviceEventType::PropertyDefined;
    event.deviceName = deviceName_;
    event.propertyName = property.name;
    event.data = property.toJson();
    event.timestamp = std::chrono::system_clock::now();
    emitEvent(event);

    // Notify property watchers
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        auto it = propertyCallbacks_.find(property.name);
        if (it != propertyCallbacks_.end() && it->second) {
            it->second(property);
        }
    }
}

void INDIDeviceBase::onPropertyUpdated(const INDIProperty& property) {
    updatePropertyCache(property);

    DeviceEvent event;
    event.type = DeviceEventType::PropertyUpdated;
    event.deviceName = deviceName_;
    event.propertyName = property.name;
    event.data = property.toJson();
    event.timestamp = std::chrono::system_clock::now();
    emitEvent(event);

    // Notify property watchers
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        auto it = propertyCallbacks_.find(property.name);
        if (it != propertyCallbacks_.end() && it->second) {
            it->second(property);
        }
    }

    // Signal state change for waiters
    stateCondition_.notify_all();
}

void INDIDeviceBase::onPropertyDeleted(const std::string& propertyName) {
    {
        std::lock_guard<std::mutex> lock(propertiesMutex_);
        properties_.erase(propertyName);
    }

    DeviceEvent event;
    event.type = DeviceEventType::PropertyDeleted;
    event.deviceName = deviceName_;
    event.propertyName = propertyName;
    event.timestamp = std::chrono::system_clock::now();
    emitEvent(event);
}

void INDIDeviceBase::onMessage(const std::string& message) {
    LOG_INFO("[{}] {}", deviceName_, message);

    DeviceEvent event;
    event.type = DeviceEventType::MessageReceived;
    event.deviceName = deviceName_;
    event.message = message;
    event.timestamp = std::chrono::system_clock::now();
    emitEvent(event);
}

void INDIDeviceBase::onBlobReceived(const INDIProperty& property) {
    updatePropertyCache(property);

    DeviceEvent event;
    event.type = DeviceEventType::BlobReceived;
    event.deviceName = deviceName_;
    event.propertyName = property.name;
    event.data = property.toJson();
    event.timestamp = std::chrono::system_clock::now();
    emitEvent(event);
}

void INDIDeviceBase::emitEvent(const DeviceEvent& event) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (eventCallback_) {
        eventCallback_(event);
    }
}

void INDIDeviceBase::updatePropertyCache(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    properties_[property.name] = property;
}

void INDIDeviceBase::setConnectionState(ConnectionState state) {
    connectionState_.store(state);
    stateCondition_.notify_all();
}

void INDIDeviceBase::log(std::string_view level, std::string_view message) const {
    if (level == "debug") {
        LOG_DEBUG("[{}] {}", name_, message);
    } else if (level == "info") {
        LOG_INFO("[{}] {}", name_, message);
    } else if (level == "warn") {
        LOG_WARN("[{}] {}", name_, message);
    } else if (level == "error") {
        LOG_ERROR("[{}] {}", name_, message);
    }
}

}  // namespace lithium::client::indi
