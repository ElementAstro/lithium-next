/*
 * device_component_adapter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device component adapter implementation

**************************************************/

#include "device_component_adapter.hpp"

#include <loguru.hpp>

namespace lithium::device {

// ============================================================================
// State Conversion Functions
// ============================================================================

auto deviceComponentStateToString(DeviceComponentState state) -> std::string {
    switch (state) {
        case DeviceComponentState::Created:
            return "Created";
        case DeviceComponentState::Initialized:
            return "Initialized";
        case DeviceComponentState::Connecting:
            return "Connecting";
        case DeviceComponentState::Connected:
            return "Connected";
        case DeviceComponentState::Paused:
            return "Paused";
        case DeviceComponentState::Disconnecting:
            return "Disconnecting";
        case DeviceComponentState::Disconnected:
            return "Disconnected";
        case DeviceComponentState::Error:
            return "Error";
        case DeviceComponentState::Disabled:
            return "Disabled";
        default:
            return "Unknown";
    }
}

auto toComponentState(DeviceComponentState state) -> ComponentState {
    switch (state) {
        case DeviceComponentState::Created:
            return ComponentState::Created;
        case DeviceComponentState::Initialized:
        case DeviceComponentState::Disconnected:
            return ComponentState::Initialized;
        case DeviceComponentState::Connecting:
        case DeviceComponentState::Connected:
            return ComponentState::Running;
        case DeviceComponentState::Paused:
            return ComponentState::Paused;
        case DeviceComponentState::Disconnecting:
            return ComponentState::Stopped;
        case DeviceComponentState::Error:
            return ComponentState::Error;
        case DeviceComponentState::Disabled:
            return ComponentState::Disabled;
        default:
            return ComponentState::Error;
    }
}

auto fromComponentState(ComponentState state) -> DeviceComponentState {
    switch (state) {
        case ComponentState::Created:
            return DeviceComponentState::Created;
        case ComponentState::Initialized:
            return DeviceComponentState::Initialized;
        case ComponentState::Running:
            return DeviceComponentState::Connected;
        case ComponentState::Paused:
            return DeviceComponentState::Paused;
        case ComponentState::Stopped:
            return DeviceComponentState::Disconnected;
        case ComponentState::Error:
            return DeviceComponentState::Error;
        case ComponentState::Disabled:
            return DeviceComponentState::Disabled;
        case ComponentState::Unloading:
            return DeviceComponentState::Disconnecting;
        default:
            return DeviceComponentState::Error;
    }
}

// ============================================================================
// DeviceAdapterConfig Implementation
// ============================================================================

auto DeviceAdapterConfig::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["connectionPort"] = connectionPort;
    j["connectionTimeout"] = connectionTimeout;
    j["maxRetries"] = maxRetries;
    j["autoConnect"] = autoConnect;
    j["autoReconnect"] = autoReconnect;
    j["reconnectDelay"] = reconnectDelay;
    j["deviceConfig"] = deviceConfig;
    return j;
}

auto DeviceAdapterConfig::fromJson(const nlohmann::json& j)
    -> DeviceAdapterConfig {
    DeviceAdapterConfig config;

    if (j.contains("connectionPort")) {
        config.connectionPort = j["connectionPort"].get<std::string>();
    }
    if (j.contains("connectionTimeout")) {
        config.connectionTimeout = j["connectionTimeout"].get<int>();
    }
    if (j.contains("maxRetries")) {
        config.maxRetries = j["maxRetries"].get<int>();
    }
    if (j.contains("autoConnect")) {
        config.autoConnect = j["autoConnect"].get<bool>();
    }
    if (j.contains("autoReconnect")) {
        config.autoReconnect = j["autoReconnect"].get<bool>();
    }
    if (j.contains("reconnectDelay")) {
        config.reconnectDelay = j["reconnectDelay"].get<int>();
    }
    if (j.contains("deviceConfig")) {
        config.deviceConfig = j["deviceConfig"];
    }

    return config;
}

// ============================================================================
// DeviceComponentAdapter Implementation
// ============================================================================

DeviceComponentAdapter::DeviceComponentAdapter(std::shared_ptr<AtomDriver> device,
                                               const std::string& name)
    : device_(std::move(device)),
      name_(name.empty() ? (device_ ? device_->getName() : "unknown") : name),
      createdAt_(std::chrono::system_clock::now()) {}

DeviceComponentAdapter::DeviceComponentAdapter(
    std::shared_ptr<AtomDriver> device,
    const DeviceAdapterConfig& config,
    const std::string& name)
    : device_(std::move(device)),
      name_(name.empty() ? (device_ ? device_->getName() : "unknown") : name),
      config_(config),
      createdAt_(std::chrono::system_clock::now()) {}

DeviceComponentAdapter::~DeviceComponentAdapter() {
    // Ensure device is disconnected and destroyed
    if (device_) {
        try {
            if (device_->isConnected()) {
                device_->disconnect();
            }
            device_->destroy();
        } catch (...) {
            // Ignore exceptions during destruction
        }
    }
}

auto DeviceComponentAdapter::initialize() -> bool {
    std::unique_lock lock(mutex_);

    if (!device_) {
        setLastError("Device is null");
        setState(DeviceComponentState::Error);
        return false;
    }

    try {
        if (!device_->initialize()) {
            setLastError("Device initialization failed");
            setState(DeviceComponentState::Error);
            return false;
        }

        setState(DeviceComponentState::Initialized);
        LOG_F(INFO, "Device component '%s' initialized", name_.c_str());

        // Auto-connect if configured
        if (config_.autoConnect && !config_.connectionPort.empty()) {
            lock.unlock();
            auto result = connect(config_.connectionPort,
                                  config_.connectionTimeout);
            if (!result) {
                LOG_F(WARNING, "Auto-connect failed for '%s': %s",
                      name_.c_str(), result.error().message.c_str());
            }
        }

        return true;
    } catch (const std::exception& e) {
        setLastError(std::string("Initialization exception: ") + e.what());
        setState(DeviceComponentState::Error);
        return false;
    }
}

auto DeviceComponentAdapter::destroy() -> bool {
    std::unique_lock lock(mutex_);

    if (!device_) {
        return true;
    }

    try {
        // Disconnect first if connected
        if (device_->isConnected()) {
            lock.unlock();
            disconnect();
            lock.lock();
        }

        if (!device_->destroy()) {
            setLastError("Device destruction failed");
            return false;
        }

        setState(DeviceComponentState::Created);
        LOG_F(INFO, "Device component '%s' destroyed", name_.c_str());
        return true;
    } catch (const std::exception& e) {
        setLastError(std::string("Destruction exception: ") + e.what());
        return false;
    }
}

auto DeviceComponentAdapter::getName() const -> std::string {
    return name_;
}

auto DeviceComponentAdapter::start() -> bool {
    auto result = connect();
    return result.has_value() && *result;
}

auto DeviceComponentAdapter::stop() -> bool {
    auto result = disconnect();
    return result.has_value() && *result;
}

auto DeviceComponentAdapter::pause() -> bool {
    std::unique_lock lock(mutex_);

    if (state_ != DeviceComponentState::Connected) {
        setLastError("Cannot pause: device not connected");
        return false;
    }

    // Device pause is application-specific
    // For now, we just update the state
    setState(DeviceComponentState::Paused);
    LOG_F(INFO, "Device component '%s' paused", name_.c_str());
    return true;
}

auto DeviceComponentAdapter::resume() -> bool {
    std::unique_lock lock(mutex_);

    if (state_ != DeviceComponentState::Paused) {
        setLastError("Cannot resume: device not paused");
        return false;
    }

    setState(DeviceComponentState::Connected);
    LOG_F(INFO, "Device component '%s' resumed", name_.c_str());
    return true;
}

auto DeviceComponentAdapter::isHealthy() const -> bool {
    std::shared_lock lock(mutex_);
    return device_ != nullptr &&
           (state_ == DeviceComponentState::Connected ||
            state_ == DeviceComponentState::Initialized ||
            state_ == DeviceComponentState::Paused);
}

auto DeviceComponentAdapter::getDevice() const -> std::shared_ptr<AtomDriver> {
    std::shared_lock lock(mutex_);
    return device_;
}

auto DeviceComponentAdapter::getDeviceUUID() const -> std::string {
    std::shared_lock lock(mutex_);
    return device_ ? device_->getUUID() : "";
}

auto DeviceComponentAdapter::getDeviceType() const -> std::string {
    std::shared_lock lock(mutex_);
    return device_ ? device_->getType() : "";
}

auto DeviceComponentAdapter::isConnected() const -> bool {
    std::shared_lock lock(mutex_);
    return device_ && device_->isConnected();
}

auto DeviceComponentAdapter::getDeviceState() const -> DeviceComponentState {
    std::shared_lock lock(mutex_);
    return state_;
}

auto DeviceComponentAdapter::connect(const std::string& port, int timeout)
    -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    if (!device_) {
        return std::unexpected(
            error::connectionFailed("", "Device is null"));
    }

    if (device_->isConnected()) {
        return true;  // Already connected
    }

    std::string connectionPort =
        port.empty() ? config_.connectionPort : port;
    int connectionTimeout =
        timeout > 0 ? timeout : config_.connectionTimeout;

    if (connectionPort.empty()) {
        return std::unexpected(
            error::invalidArgument("port", "Connection port not specified"));
    }

    setState(DeviceComponentState::Connecting);

    // Release lock during potentially long operation
    lock.unlock();

    int retries = 0;
    while (retries < config_.maxRetries) {
        try {
            if (device_->connect(connectionPort, connectionTimeout,
                                 config_.maxRetries)) {
                lock.lock();
                setState(DeviceComponentState::Connected);
                connectCount_++;
                lastConnectedAt_ = std::chrono::system_clock::now();
                LOG_F(INFO, "Device '%s' connected to '%s'", name_.c_str(),
                      connectionPort.c_str());
                return true;
            }
        } catch (const std::exception& e) {
            LOG_F(WARNING, "Connection attempt %d failed for '%s': %s",
                  retries + 1, name_.c_str(), e.what());
        }
        retries++;
    }

    lock.lock();
    handleConnectionError("Connection failed after " +
                          std::to_string(config_.maxRetries) + " retries");
    return std::unexpected(
        error::connectionFailed(name_, lastError_));
}

auto DeviceComponentAdapter::disconnect() -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    if (!device_) {
        return std::unexpected(
            error::operationFailed("disconnect", "Device is null"));
    }

    if (!device_->isConnected()) {
        setState(DeviceComponentState::Disconnected);
        return true;  // Already disconnected
    }

    setState(DeviceComponentState::Disconnecting);

    // Release lock during operation
    lock.unlock();

    try {
        if (!device_->disconnect()) {
            lock.lock();
            setLastError("Disconnect failed");
            setState(DeviceComponentState::Error);
            return std::unexpected(
                error::operationFailed("disconnect", lastError_));
        }

        lock.lock();
        setState(DeviceComponentState::Disconnected);
        disconnectCount_++;
        LOG_F(INFO, "Device '%s' disconnected", name_.c_str());
        return true;
    } catch (const std::exception& e) {
        lock.lock();
        handleConnectionError(std::string("Disconnect exception: ") +
                              e.what());
        return std::unexpected(
            error::operationFailed("disconnect", lastError_));
    }
}

auto DeviceComponentAdapter::scan() -> std::vector<std::string> {
    std::shared_lock lock(mutex_);

    if (!device_) {
        return {};
    }

    try {
        return device_->scan();
    } catch (const std::exception& e) {
        LOG_F(WARNING, "Device scan failed: %s", e.what());
        return {};
    }
}

void DeviceComponentAdapter::updateConfig(const DeviceAdapterConfig& config) {
    std::unique_lock lock(mutex_);
    config_ = config;
}

auto DeviceComponentAdapter::getConfig() const -> DeviceAdapterConfig {
    std::shared_lock lock(mutex_);
    return config_;
}

auto DeviceComponentAdapter::getLastError() const -> std::string {
    std::shared_lock lock(mutex_);
    return lastError_;
}

auto DeviceComponentAdapter::saveState() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json state;
    state["name"] = name_;
    state["uuid"] = device_ ? device_->getUUID() : "";
    state["type"] = device_ ? device_->getType() : "";
    state["wasConnected"] = device_ && device_->isConnected();
    state["config"] = config_.toJson();
    state["deviceState"] = deviceComponentStateToString(state_);

    return state;
}

auto DeviceComponentAdapter::restoreState(const nlohmann::json& state)
    -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    try {
        if (state.contains("config")) {
            config_ = DeviceAdapterConfig::fromJson(state["config"]);
        }

        bool wasConnected = state.value("wasConnected", false);

        lock.unlock();

        // Initialize device if needed
        if (state_ == DeviceComponentState::Created) {
            if (!initialize()) {
                return std::unexpected(
                    error::operationFailed("restore", "Initialization failed"));
            }
        }

        // Reconnect if was connected
        if (wasConnected && !config_.connectionPort.empty()) {
            auto result = connect();
            if (!result) {
                LOG_F(WARNING, "State restore: reconnection failed for '%s'",
                      name_.c_str());
                // Not a critical error - device can be manually reconnected
            }
        }

        return true;
    } catch (const std::exception& e) {
        return std::unexpected(error::operationFailed(
            "restore", std::string("Exception: ") + e.what()));
    }
}

auto DeviceComponentAdapter::getInfo() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json info;
    info["name"] = name_;
    info["uuid"] = device_ ? device_->getUUID() : "";
    info["type"] = device_ ? device_->getType() : "";
    info["state"] = deviceComponentStateToString(state_);
    info["componentState"] =
        lithium::componentStateToString(toComponentState(state_));
    info["isConnected"] = device_ && device_->isConnected();
    info["lastError"] = lastError_;
    info["config"] = config_.toJson();

    return info;
}

auto DeviceComponentAdapter::getStatistics() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json stats;
    stats["connectCount"] = connectCount_;
    stats["disconnectCount"] = disconnectCount_;
    stats["errorCount"] = errorCount_;
    stats["createdAt"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            createdAt_.time_since_epoch())
            .count();

    if (lastConnectedAt_.time_since_epoch().count() > 0) {
        stats["lastConnectedAt"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                lastConnectedAt_.time_since_epoch())
                .count();
    }

    if (lastErrorAt_.time_since_epoch().count() > 0) {
        stats["lastErrorAt"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                lastErrorAt_.time_since_epoch())
                .count();
    }

    return stats;
}

void DeviceComponentAdapter::setState(DeviceComponentState state) {
    state_ = state;
}

void DeviceComponentAdapter::setLastError(const std::string& error) {
    lastError_ = error;
    lastErrorAt_ = std::chrono::system_clock::now();
    errorCount_++;
}

void DeviceComponentAdapter::handleConnectionError(const std::string& error) {
    setLastError(error);
    setState(DeviceComponentState::Error);
    LOG_F(ERROR, "Device '%s' connection error: %s", name_.c_str(),
          error.c_str());

    // Try to reconnect if configured
    if (config_.autoReconnect) {
        tryReconnect();
    }
}

void DeviceComponentAdapter::tryReconnect() {
    // Schedule reconnect attempt
    // Note: This would typically use async mechanisms
    // For now, we just log the intent
    LOG_F(INFO, "Device '%s' will attempt reconnection", name_.c_str());
}

// ============================================================================
// Factory Function
// ============================================================================

auto createDeviceAdapter(std::shared_ptr<AtomDriver> device,
                         const DeviceAdapterConfig& config,
                         const std::string& name)
    -> std::shared_ptr<DeviceComponentAdapter> {
    return std::make_shared<DeviceComponentAdapter>(std::move(device), config,
                                                    name);
}

}  // namespace lithium::device
