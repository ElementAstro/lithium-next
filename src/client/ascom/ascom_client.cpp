/*
 * ascom_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: ASCOM server client implementation

*************************************************/

#include "ascom_client.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client {

ASCOMClient::ASCOMClient(std::string name)
    : ServerClient(std::move(name)) {
    spdlog::info("ASCOMClient created: {}", getName());
}

ASCOMClient::~ASCOMClient() {
    if (isConnected()) {
        disconnect();
    }
    spdlog::debug("ASCOMClient destroyed: {}", getName());
}

bool ASCOMClient::initialize() {
    spdlog::debug("Initializing ASCOMClient");

    // Create Alpaca client
    alpacaClient_ = std::make_unique<ascom::AlpacaClient>(ascomHost_, ascomPort_);

    setState(ClientState::Initialized);
    emitEvent("initialized", "");
    return true;
}

bool ASCOMClient::destroy() {
    spdlog::debug("Destroying ASCOMClient");

    if (isConnected()) {
        disconnect();
    }

    alpacaClient_.reset();
    
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        deviceCache_.clear();
    }

    setState(ClientState::Uninitialized);
    emitEvent("destroyed", "");
    return true;
}

bool ASCOMClient::connect(const std::string& target, int timeout, int /*maxRetry*/) {
    spdlog::debug("Connecting to ASCOM Alpaca server");
    setState(ClientState::Connecting);

    // Parse target (host:port)
    if (!target.empty()) {
        auto colonPos = target.find(':');
        if (colonPos != std::string::npos) {
            ascomHost_ = target.substr(0, colonPos);
            ascomPort_ = std::stoi(target.substr(colonPos + 1));
        } else {
            ascomHost_ = target;
        }
    }

    // Create or reconfigure Alpaca client
    if (!alpacaClient_) {
        alpacaClient_ = std::make_unique<ascom::AlpacaClient>(ascomHost_, ascomPort_);
    } else {
        alpacaClient_->setServer(ascomHost_, ascomPort_);
    }

    if (!alpacaClient_->connect(timeout)) {
        setError(1, "Failed to connect to ASCOM Alpaca server");
        setState(ClientState::Error);
        return false;
    }

    // Refresh device cache
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        deviceCache_ = alpacaClient_->getConfiguredDevices();
    }

    setState(ClientState::Connected);
    emitEvent("connected", ascomHost_ + ":" + std::to_string(ascomPort_));
    
    ServerEvent event;
    event.type = ServerEventType::ServerStarted;
    event.source = "ASCOM";
    event.message = "Connected to Alpaca server";
    event.timestamp = std::chrono::system_clock::now();
    emitServerEvent(event);
    
    return true;
}

bool ASCOMClient::disconnect() {
    spdlog::debug("Disconnecting from ASCOM Alpaca server");
    setState(ClientState::Disconnecting);

    if (alpacaClient_) {
        alpacaClient_->disconnect();
    }

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        deviceCache_.clear();
    }

    setState(ClientState::Disconnected);
    emitEvent("disconnected", "");
    return true;
}

bool ASCOMClient::isConnected() const {
    return alpacaClient_ && alpacaClient_->isConnected();
}

std::vector<std::string> ASCOMClient::scan() {
    return discoverServers(5000);
}

bool ASCOMClient::startServer() {
    // ASCOM Alpaca servers are typically external processes
    // This would start a local Alpaca server if available
    spdlog::warn("ASCOMClient::startServer - External server management not implemented");
    return false;
}

bool ASCOMClient::stopServer() {
    // ASCOM Alpaca servers are typically external processes
    spdlog::warn("ASCOMClient::stopServer - External server management not implemented");
    return false;
}

bool ASCOMClient::isServerRunning() const {
    return isConnected();
}

bool ASCOMClient::isInstalled() const {
    // Check if we can discover any Alpaca servers
    auto servers = discoverServers(1000);
    return !servers.empty();
}

bool ASCOMClient::startDriver(const DriverInfo& driver) {
    // ASCOM drivers are managed by the Alpaca server
    // We just need to connect to the device
    return connectDevice(driver.name);
}

bool ASCOMClient::stopDriver(const std::string& driverName) {
    // Disconnect the device
    return disconnectDevice(driverName);
}

std::unordered_map<std::string, DriverInfo> ASCOMClient::getRunningDrivers() const {
    std::unordered_map<std::string, DriverInfo> result;

    if (!alpacaClient_) {
        return result;
    }

    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (const auto& desc : deviceCache_) {
        if (alpacaClient_->isDeviceConnected(desc.deviceType, desc.deviceNumber)) {
            auto info = ASCOMDriverInfo::fromDescription(desc);
            info.running = true;
            result[desc.deviceName] = info;
        }
    }

    return result;
}

std::vector<DriverInfo> ASCOMClient::getAvailableDrivers() const {
    std::vector<DriverInfo> result;
    
    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (const auto& desc : deviceCache_) {
        result.push_back(ASCOMDriverInfo::fromDescription(desc));
    }
    
    return result;
}

std::vector<DeviceInfo> ASCOMClient::getDevices() const {
    std::vector<DeviceInfo> result;

    if (!alpacaClient_) {
        return result;
    }

    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (const auto& desc : deviceCache_) {
        result.push_back(convertToDeviceInfo(desc));
    }

    return result;
}

std::optional<DeviceInfo> ASCOMClient::getDevice(const std::string& name) const {
    auto devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.name == name) {
            return dev;
        }
    }
    return std::nullopt;
}

bool ASCOMClient::connectDevice(const std::string& deviceName) {
    if (!alpacaClient_) {
        setError(20, "Not connected to ASCOM server");
        return false;
    }

    auto deviceOpt = findDevice(deviceName);
    if (!deviceOpt) {
        setError(21, "Device not found: " + deviceName);
        return false;
    }

    auto [deviceType, deviceNumber] = *deviceOpt;
    bool result = alpacaClient_->connectDevice(deviceType, deviceNumber);
    
    if (result) {
        ServerEvent event;
        event.type = ServerEventType::DeviceConnected;
        event.source = deviceName;
        event.message = "Device connected";
        event.timestamp = std::chrono::system_clock::now();
        emitServerEvent(event);
    }
    
    return result;
}

bool ASCOMClient::disconnectDevice(const std::string& deviceName) {
    if (!alpacaClient_) {
        return true;
    }

    auto deviceOpt = findDevice(deviceName);
    if (!deviceOpt) {
        return false;
    }

    auto [deviceType, deviceNumber] = *deviceOpt;
    bool result = alpacaClient_->disconnectDevice(deviceType, deviceNumber);
    
    if (result) {
        ServerEvent event;
        event.type = ServerEventType::DeviceDisconnected;
        event.source = deviceName;
        event.message = "Device disconnected";
        event.timestamp = std::chrono::system_clock::now();
        emitServerEvent(event);
    }
    
    return result;
}

bool ASCOMClient::setProperty(const std::string& device,
                              const std::string& property,
                              const std::string& /*element*/,
                              const std::string& value) {
    if (!alpacaClient_) {
        return false;
    }

    auto deviceOpt = findDevice(device);
    if (!deviceOpt) {
        return false;
    }

    auto [deviceType, deviceNumber] = *deviceOpt;
    
    // Use PUT request to set property
    auto response = alpacaClient_->put(deviceType, deviceNumber, property,
                                        {{property, value}});
    return response.isSuccess();
}

std::string ASCOMClient::getProperty(const std::string& device,
                                      const std::string& property,
                                      const std::string& /*element*/) const {
    if (!alpacaClient_) {
        return "";
    }

    auto deviceOpt = findDevice(device);
    if (!deviceOpt) {
        return "";
    }

    auto [deviceType, deviceNumber] = *deviceOpt;
    
    // Use GET request to get property
    auto response = alpacaClient_->get(deviceType, deviceNumber, property);
    if (response.isSuccess()) {
        if (response.value.is_string()) {
            return response.value.get<std::string>();
        } else if (response.value.is_number()) {
            return std::to_string(response.value.get<double>());
        } else if (response.value.is_boolean()) {
            return response.value.get<bool>() ? "true" : "false";
        }
    }
    return "";
}

std::string ASCOMClient::getPropertyState(const std::string& /*device*/,
                                           const std::string& /*property*/) const {
    // ASCOM doesn't have property states like INDI
    // Return "Ok" for connected devices
    return "Ok";
}

void ASCOMClient::configureASCOM(const std::string& host, int port) {
    ascomHost_ = host;
    ascomPort_ = port;

    serverConfig_.host = host;
    serverConfig_.port = port;
    
    if (alpacaClient_) {
        alpacaClient_->setServer(host, port);
    }
}

std::optional<ascom::AlpacaServerInfo> ASCOMClient::getAlpacaServerInfo() {
    if (!alpacaClient_) {
        return std::nullopt;
    }
    return alpacaClient_->getServerInfo();
}

std::vector<std::string> ASCOMClient::discoverServers(int timeout) {
    return ascom::AlpacaClient::discoverServers(timeout);
}

std::string ASCOMClient::executeAction(const std::string& deviceName,
                                        const std::string& action,
                                        const std::string& parameters) {
    if (!alpacaClient_) {
        return "";
    }

    auto deviceOpt = findDevice(deviceName);
    if (!deviceOpt) {
        return "";
    }

    auto [deviceType, deviceNumber] = *deviceOpt;
    return alpacaClient_->action(deviceType, deviceNumber, action, parameters);
}

std::vector<std::string> ASCOMClient::getSupportedActions(const std::string& deviceName) const {
    if (!alpacaClient_) {
        return {};
    }

    auto deviceOpt = findDevice(deviceName);
    if (!deviceOpt) {
        return {};
    }

    auto [deviceType, deviceNumber] = *deviceOpt;
    return alpacaClient_->getSupportedActions(deviceType, deviceNumber);
}

DeviceInfo ASCOMClient::convertToDeviceInfo(const ascom::ASCOMDeviceDescription& desc) const {
    DeviceInfo info;
    info.backend = "ASCOM";
    info.id = desc.uniqueId.empty() ? desc.deviceName : desc.uniqueId;
    info.name = desc.deviceName;
    info.displayName = desc.deviceName;
    info.interfaces = deviceTypeToInterface(desc.deviceType);
    info.interfaceString = ascom::deviceTypeToString(desc.deviceType);
    
    if (alpacaClient_) {
        info.connected = alpacaClient_->isDeviceConnected(desc.deviceType, desc.deviceNumber);
        if (info.connected) {
            info.driver = alpacaClient_->getDriverInfo(desc.deviceType, desc.deviceNumber);
            info.driverVersion = alpacaClient_->getDriverVersion(desc.deviceType, desc.deviceNumber);
            info.health = DeviceHealth::Good;
            info.initialized = true;
        }
    }
    
    info.lastUpdate = std::chrono::system_clock::now();
    info.metadata["deviceNumber"] = std::to_string(desc.deviceNumber);
    info.metadata["deviceType"] = ascom::deviceTypeToString(desc.deviceType);
    
    return info;
}

std::optional<std::pair<ascom::ASCOMDeviceType, int>> ASCOMClient::findDevice(
    const std::string& deviceName) const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    for (const auto& desc : deviceCache_) {
        if (desc.deviceName == deviceName) {
            return std::make_pair(desc.deviceType, desc.deviceNumber);
        }
    }
    return std::nullopt;
}

DeviceInterface ASCOMClient::deviceTypeToInterface(ascom::ASCOMDeviceType type) {
    switch (type) {
        case ascom::ASCOMDeviceType::Camera:
            return DeviceInterface::CCD;
        case ascom::ASCOMDeviceType::Telescope:
            return DeviceInterface::Telescope;
        case ascom::ASCOMDeviceType::Focuser:
            return DeviceInterface::Focuser;
        case ascom::ASCOMDeviceType::FilterWheel:
            return DeviceInterface::FilterWheel;
        case ascom::ASCOMDeviceType::Dome:
            return DeviceInterface::Dome;
        case ascom::ASCOMDeviceType::Rotator:
            return DeviceInterface::Rotator;
        case ascom::ASCOMDeviceType::ObservingConditions:
            return DeviceInterface::Weather;
        case ascom::ASCOMDeviceType::SafetyMonitor:
            return DeviceInterface::SafetyMonitor;
        case ascom::ASCOMDeviceType::Switch:
            return DeviceInterface::Switch;
        case ascom::ASCOMDeviceType::Video:
            return DeviceInterface::Video;
        case ascom::ASCOMDeviceType::CoverCalibrator:
            return DeviceInterface::Dustcap | DeviceInterface::Lightbox;
        default:
            return DeviceInterface::General;
    }
}

// Register with client registry
LITHIUM_REGISTER_CLIENT(ASCOMClient, "ascom", "ASCOM Alpaca Device Server",
                        ClientType::Server, "1.0.0")

}  // namespace lithium::client
