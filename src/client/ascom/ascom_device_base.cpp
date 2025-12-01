/*
 * ascom_device_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Device Base Class Implementation

**************************************************/

#include "ascom_device_base.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client::ascom {

// ==================== Constructor/Destructor ====================

ASCOMDeviceBase::ASCOMDeviceBase(std::string name, ASCOMDeviceType deviceType,
                                 int deviceNumber)
    : name_(std::move(name)),
      deviceType_(deviceType),
      deviceNumber_(deviceNumber) {
    spdlog::debug("ASCOMDeviceBase created: {} (type={}, number={})", name_,
                  deviceTypeToString(deviceType_), deviceNumber_);
}

ASCOMDeviceBase::~ASCOMDeviceBase() {
    if (isConnected()) {
        disconnect();
    }
    spdlog::debug("ASCOMDeviceBase destroyed: {}", name_);
}

// ==================== Connection ====================

void ASCOMDeviceBase::setClient(std::shared_ptr<AlpacaClient> client) {
    std::lock_guard<std::mutex> lock(mutex_);
    client_ = std::move(client);
}

auto ASCOMDeviceBase::connect(int timeout) -> bool {
    if (!client_) {
        setError("No Alpaca client configured");
        return false;
    }

    if (!client_->isConnected()) {
        setError("Alpaca client not connected to server");
        return false;
    }

    state_.store(DeviceState::Connecting);
    clearError();

    // Set timeout
    client_->setTimeout(timeout);

    // Connect to device
    if (!client_->connectDevice(deviceType_, deviceNumber_)) {
        setError("Failed to connect to device");
        state_.store(DeviceState::Error);
        return false;
    }

    state_.store(DeviceState::Connected);
    emitEvent(DeviceEventType::Connected, "", "Device connected");
    spdlog::info("ASCOM device connected: {}", name_);

    return true;
}

auto ASCOMDeviceBase::disconnect() -> bool {
    if (!client_) {
        return true;
    }

    state_.store(DeviceState::Disconnecting);

    if (!client_->disconnectDevice(deviceType_, deviceNumber_)) {
        spdlog::warn("Failed to disconnect device: {}", name_);
    }

    state_.store(DeviceState::Disconnected);
    emitEvent(DeviceEventType::Disconnected, "", "Device disconnected");
    spdlog::info("ASCOM device disconnected: {}", name_);

    return true;
}

auto ASCOMDeviceBase::isConnected() const -> bool {
    if (!client_ || !client_->isConnected()) {
        return false;
    }
    return client_->isDeviceConnected(deviceType_, deviceNumber_);
}

// ==================== Device Info ====================

auto ASCOMDeviceBase::getDescription() const -> std::string {
    if (!client_) return "";
    return client_->getDeviceDescription(deviceType_, deviceNumber_);
}

auto ASCOMDeviceBase::getDriverInfo() const -> std::string {
    if (!client_) return "";
    return client_->getDriverInfo(deviceType_, deviceNumber_);
}

auto ASCOMDeviceBase::getDriverVersion() const -> std::string {
    if (!client_) return "";
    return client_->getDriverVersion(deviceType_, deviceNumber_);
}

auto ASCOMDeviceBase::getInterfaceVersion() const -> int {
    if (!client_) return 0;
    return client_->getInterfaceVersion(deviceType_, deviceNumber_);
}

auto ASCOMDeviceBase::getSupportedActions() const -> std::vector<std::string> {
    if (!client_) return {};
    return client_->getSupportedActions(deviceType_, deviceNumber_);
}

// ==================== Property Access ====================

auto ASCOMDeviceBase::getProperty(
    const std::string& property,
    const std::unordered_map<std::string, std::string>& params) -> AlpacaResponse {
    if (!client_) {
        AlpacaResponse resp;
        resp.errorNumber = ASCOMErrorCode::NotConnected;
        resp.errorMessage = "No client configured";
        return resp;
    }
    return client_->get(deviceType_, deviceNumber_, property, params);
}

auto ASCOMDeviceBase::setProperty(
    const std::string& property,
    const std::unordered_map<std::string, std::string>& params) -> AlpacaResponse {
    if (!client_) {
        AlpacaResponse resp;
        resp.errorNumber = ASCOMErrorCode::NotConnected;
        resp.errorMessage = "No client configured";
        return resp;
    }
    return client_->put(deviceType_, deviceNumber_, property, params);
}

auto ASCOMDeviceBase::executeAction(const std::string& action,
                                    const std::string& parameters) -> std::string {
    if (!client_) return "";
    return client_->action(deviceType_, deviceNumber_, action, parameters);
}

// ==================== Convenience Property Getters ====================

auto ASCOMDeviceBase::getBoolProperty(const std::string& property) const
    -> std::optional<bool> {
    if (!client_) return std::nullopt;

    auto response = client_->get(deviceType_, deviceNumber_, property);
    if (response.isSuccess() && response.value.is_boolean()) {
        return response.value.get<bool>();
    }
    return std::nullopt;
}

auto ASCOMDeviceBase::getIntProperty(const std::string& property) const
    -> std::optional<int> {
    if (!client_) return std::nullopt;

    auto response = client_->get(deviceType_, deviceNumber_, property);
    if (response.isSuccess() && response.value.is_number_integer()) {
        return response.value.get<int>();
    }
    return std::nullopt;
}

auto ASCOMDeviceBase::getDoubleProperty(const std::string& property) const
    -> std::optional<double> {
    if (!client_) return std::nullopt;

    auto response = client_->get(deviceType_, deviceNumber_, property);
    if (response.isSuccess() && response.value.is_number()) {
        return response.value.get<double>();
    }
    return std::nullopt;
}

auto ASCOMDeviceBase::getStringProperty(const std::string& property) const
    -> std::optional<std::string> {
    if (!client_) return std::nullopt;

    auto response = client_->get(deviceType_, deviceNumber_, property);
    if (response.isSuccess() && response.value.is_string()) {
        return response.value.get<std::string>();
    }
    return std::nullopt;
}

// ==================== Convenience Property Setters ====================

auto ASCOMDeviceBase::setBoolProperty(const std::string& property, bool value)
    -> bool {
    if (!client_) return false;

    auto response = client_->put(deviceType_, deviceNumber_, property,
                                 {{property, value ? "true" : "false"}});
    return response.isSuccess();
}

auto ASCOMDeviceBase::setIntProperty(const std::string& property, int value)
    -> bool {
    if (!client_) return false;

    auto response = client_->put(deviceType_, deviceNumber_, property,
                                 {{property, std::to_string(value)}});
    return response.isSuccess();
}

auto ASCOMDeviceBase::setDoubleProperty(const std::string& property, double value)
    -> bool {
    if (!client_) return false;

    auto response = client_->put(deviceType_, deviceNumber_, property,
                                 {{property, std::to_string(value)}});
    return response.isSuccess();
}

auto ASCOMDeviceBase::setStringProperty(const std::string& property,
                                        const std::string& value) -> bool {
    if (!client_) return false;

    auto response =
        client_->put(deviceType_, deviceNumber_, property, {{property, value}});
    return response.isSuccess();
}

// ==================== Events ====================

void ASCOMDeviceBase::registerEventCallback(DeviceEventCallback callback) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    eventCallback_ = std::move(callback);
}

void ASCOMDeviceBase::unregisterEventCallback() {
    std::lock_guard<std::mutex> lock(eventMutex_);
    eventCallback_ = nullptr;
}

// ==================== Status ====================

auto ASCOMDeviceBase::getStatus() const -> json {
    json status;

    status["name"] = name_;
    status["type"] = getDeviceType();
    status["deviceNumber"] = deviceNumber_;
    status["state"] = deviceStateToString(state_.load());
    status["connected"] = isConnected();

    if (!lastError_.empty()) {
        status["lastError"] = lastError_;
    }

    if (isConnected()) {
        status["description"] = getDescription();
        status["driverInfo"] = getDriverInfo();
        status["driverVersion"] = getDriverVersion();
        status["interfaceVersion"] = getInterfaceVersion();
    }

    return status;
}

// ==================== Protected Methods ====================

void ASCOMDeviceBase::emitEvent(DeviceEventType type, const std::string& property,
                                const std::string& message, const json& data) {
    std::lock_guard<std::mutex> lock(eventMutex_);
    if (eventCallback_) {
        DeviceEvent event;
        event.type = type;
        event.deviceName = name_;
        event.propertyName = property;
        event.message = message;
        event.data = data;
        event.timestamp = std::chrono::system_clock::now();
        eventCallback_(event);
    }
}

void ASCOMDeviceBase::setError(const std::string& message) {
    lastError_ = message;
    state_.store(DeviceState::Error);
    emitEvent(DeviceEventType::Error, "", message);
    spdlog::error("ASCOM device error ({}): {}", name_, message);
}

void ASCOMDeviceBase::clearError() {
    lastError_.clear();
}

}  // namespace lithium::client::ascom
