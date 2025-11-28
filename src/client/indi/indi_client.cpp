/*
 * indi_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: INDI server client implementation

*************************************************/

#include "indi_client.hpp"
#include "iconnector.hpp"

#include "atom/system/software.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client {

INDIClient::INDIClient(std::string name)
    : ServerClient(std::move(name)) {
    spdlog::info("INDIClient created: {}", getName());
}

INDIClient::~INDIClient() {
    if (isServerRunning()) {
        stopServer();
    }
    spdlog::debug("INDIClient destroyed: {}", getName());
}

bool INDIClient::initialize() {
    spdlog::debug("Initializing INDIClient");

    if (!isInstalled()) {
        setError(1, "INDI server not installed");
        return false;
    }

    setState(ClientState::Initialized);
    emitEvent("initialized", "");
    return true;
}

bool INDIClient::destroy() {
    spdlog::debug("Destroying INDIClient");

    if (isServerRunning()) {
        stopServer();
    }

    stopIndiHub();
    connector_.reset();

    setState(ClientState::Uninitialized);
    emitEvent("destroyed", "");
    return true;
}

bool INDIClient::connect(const std::string& target, int /*timeout*/,
                         int /*maxRetry*/) {
    spdlog::debug("Connecting to INDI");
    setState(ClientState::Connecting);

    // Parse target (host:port)
    if (!target.empty()) {
        auto colonPos = target.find(':');
        if (colonPos != std::string::npos) {
            indiHost_ = target.substr(0, colonPos);
            indiPort_ = std::stoi(target.substr(colonPos + 1));
        } else {
            indiHost_ = target;
        }
    }

    // Create connector
    connector_ = std::make_unique<INDIConnector>(
        indiHost_, indiPort_, configPath_, dataPath_, fifoPath_);

    setState(ClientState::Connected);
    emitEvent("connected", indiHost_ + ":" + std::to_string(indiPort_));
    return true;
}

bool INDIClient::disconnect() {
    spdlog::debug("Disconnecting from INDI");
    setState(ClientState::Disconnecting);

    if (isServerRunning()) {
        stopServer();
    }

    connector_.reset();

    setState(ClientState::Disconnected);
    emitEvent("disconnected", "");
    return true;
}

bool INDIClient::isConnected() const {
    return connector_ != nullptr && getState() == ClientState::Connected;
}

std::vector<std::string> INDIClient::scan() {
    return {indiHost_ + ":" + std::to_string(indiPort_)};
}

bool INDIClient::startServer() {
    if (!connector_) {
        setError(10, "Not connected");
        return false;
    }

    if (connector_->startServer()) {
        serverRunning_.store(true);
        emitEvent("server_started", "");
        return true;
    }

    setError(11, "Failed to start INDI server");
    return false;
}

bool INDIClient::stopServer() {
    if (!connector_) {
        return true;
    }

    if (connector_->stopServer()) {
        serverRunning_.store(false);
        emitEvent("server_stopped", "");
        return true;
    }

    return false;
}

bool INDIClient::isServerRunning() const {
    return connector_ && connector_->isRunning();
}

bool INDIClient::isInstalled() const {
    return atom::system::checkSoftwareInstalled("indiserver");
}

bool INDIClient::startDriver(const DriverInfo& driver) {
    if (!connector_) {
        return false;
    }

    auto container = std::make_shared<INDIDeviceContainer>();
    container->label = driver.label;
    container->binary = driver.binary;
    container->skeleton = driver.skeleton;

    return connector_->startDriver(container);
}

bool INDIClient::stopDriver(const std::string& driverName) {
    if (!connector_) {
        return false;
    }

    auto runningDrivers = connector_->getRunningDrivers();
    auto it = runningDrivers.find(driverName);
    if (it != runningDrivers.end()) {
        return connector_->stopDriver(it->second);
    }

    return false;
}

bool INDIClient::startDriver(const std::shared_ptr<INDIDeviceContainer>& container) {
    if (!connector_) {
        return false;
    }
    return connector_->startDriver(container);
}

bool INDIClient::stopDriver(const std::shared_ptr<INDIDeviceContainer>& container) {
    if (!connector_) {
        return false;
    }
    return connector_->stopDriver(container);
}

std::unordered_map<std::string, DriverInfo> INDIClient::getRunningDrivers() const {
    std::unordered_map<std::string, DriverInfo> result;

    if (!connector_) {
        return result;
    }

    auto drivers = connector_->getRunningDrivers();
    for (const auto& [name, container] : drivers) {
        DriverInfo info;
        info.name = container->label;
        info.label = container->label;
        info.binary = container->binary;
        info.skeleton = container->skeleton;
        info.running = true;
        result[name] = info;
    }

    return result;
}

std::vector<DriverInfo> INDIClient::getAvailableDrivers() const {
    std::vector<DriverInfo> result;
    for (const auto& driver : availableDrivers_) {
        result.push_back(driver);
    }
    return result;
}

std::vector<DeviceInfo> INDIClient::getDevices() const {
    std::vector<DeviceInfo> result;

    if (!connector_) {
        return result;
    }

    auto devices = connector_->getDevices();
    for (const auto& dev : devices) {
        result.push_back(convertToDeviceInfo(dev));
    }

    return result;
}

DeviceInfo INDIClient::convertToDeviceInfo(
    const std::unordered_map<std::string, std::string>& devMap) const {
    DeviceInfo info;
    info.backend = "INDI";
    
    auto it = devMap.find("device");
    if (it != devMap.end()) {
        info.name = it->second;
        info.id = it->second;  // Use device name as ID for INDI
        info.displayName = it->second;
    }
    
    it = devMap.find("connected");
    if (it != devMap.end()) {
        info.connected = (it->second == "true" || it->second == "On");
    }
    
    it = devMap.find("driver");
    if (it != devMap.end()) {
        info.driver = it->second;
    }
    
    it = devMap.find("version");
    if (it != devMap.end()) {
        info.driverVersion = it->second;
    }
    
    it = devMap.find("interface");
    if (it != devMap.end()) {
        info.interfaceString = it->second;
        info.interfaces = parseInterfaceFlags(it->second);
    }
    
    info.lastUpdate = std::chrono::system_clock::now();
    if (info.connected) {
        info.health = DeviceHealth::Good;
        info.initialized = true;
    }
    
    return info;
}

DeviceInterface INDIClient::parseInterfaceFlags(const std::string& interfaceStr) {
    DeviceInterface flags = DeviceInterface::None;
    
    // Parse INDI interface bitmask or string
    try {
        uint32_t mask = std::stoul(interfaceStr);
        // INDI interface flags mapping
        if (mask & 1) flags = flags | DeviceInterface::General;
        if (mask & 2) flags = flags | DeviceInterface::Telescope;
        if (mask & 4) flags = flags | DeviceInterface::CCD;
        if (mask & 8) flags = flags | DeviceInterface::Guider;
        if (mask & 16) flags = flags | DeviceInterface::Focuser;
        if (mask & 32) flags = flags | DeviceInterface::FilterWheel;
        if (mask & 64) flags = flags | DeviceInterface::Dome;
        if (mask & 128) flags = flags | DeviceInterface::GPS;
        if (mask & 256) flags = flags | DeviceInterface::Weather;
        if (mask & 512) flags = flags | DeviceInterface::AO;
        if (mask & 1024) flags = flags | DeviceInterface::Dustcap;
        if (mask & 2048) flags = flags | DeviceInterface::Lightbox;
        if (mask & 4096) flags = flags | DeviceInterface::Detector;
        if (mask & 8192) flags = flags | DeviceInterface::Rotator;
        if (mask & 16384) flags = flags | DeviceInterface::Spectrograph;
        if (mask & 32768) flags = flags | DeviceInterface::Correlator;
        if (mask & 65536) flags = flags | DeviceInterface::AuxiliaryDevice;
        if (mask & 131072) flags = flags | DeviceInterface::Output;
        if (mask & 262144) flags = flags | DeviceInterface::Input;
    } catch (...) {
        // If not a number, try to parse as string
        if (interfaceStr.find("Telescope") != std::string::npos)
            flags = flags | DeviceInterface::Telescope;
        if (interfaceStr.find("CCD") != std::string::npos)
            flags = flags | DeviceInterface::CCD;
        if (interfaceStr.find("Focuser") != std::string::npos)
            flags = flags | DeviceInterface::Focuser;
        if (interfaceStr.find("FilterWheel") != std::string::npos)
            flags = flags | DeviceInterface::FilterWheel;
        if (interfaceStr.find("Dome") != std::string::npos)
            flags = flags | DeviceInterface::Dome;
    }
    
    return flags;
}

std::optional<DeviceInfo> INDIClient::getDevice(const std::string& name) const {
    auto devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.name == name) {
            return dev;
        }
    }
    return std::nullopt;
}

bool INDIClient::connectDevice(const std::string& deviceName) {
    if (!connector_) {
        setError(20, "Not connected to INDI server");
        return false;
    }
    
    // Set CONNECTION switch to On
    bool result = connector_->setProp(deviceName, "CONNECTION", "CONNECT", "On");
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

bool INDIClient::disconnectDevice(const std::string& deviceName) {
    if (!connector_) {
        return true;
    }
    
    // Set CONNECTION switch to Off
    bool result = connector_->setProp(deviceName, "CONNECTION", "DISCONNECT", "On");
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

bool INDIClient::setProperty(const std::string& device,
                             const std::string& property,
                             const std::string& element,
                             const std::string& value) {
    if (!connector_) {
        return false;
    }
    return connector_->setProp(device, property, element, value);
}

std::string INDIClient::getProperty(const std::string& device,
                                    const std::string& property,
                                    const std::string& element) const {
    if (!connector_) {
        return "";
    }
    return connector_->getProp(device, property, element);
}

std::string INDIClient::getPropertyState(const std::string& device,
                                         const std::string& property) const {
    if (!connector_) {
        return "";
    }
    return connector_->getState(device, property);
}

void INDIClient::configureINDI(const std::string& host, int port,
                               const std::string& configPath,
                               const std::string& dataPath,
                               const std::string& fifoPath) {
    indiHost_ = host;
    indiPort_ = port;
    configPath_ = configPath;
    dataPath_ = dataPath;
    fifoPath_ = fifoPath;

    serverConfig_.host = host;
    serverConfig_.port = port;
    serverConfig_.configPath = configPath;
    serverConfig_.dataPath = dataPath;
    serverConfig_.fifoPath = fifoPath;
}

bool INDIClient::startIndiHub(const std::string& profile, const std::string& mode) {
    if (!isServerRunning()) {
        spdlog::warn("Cannot start IndiHub: INDI server not running");
        return false;
    }

    if (!indihubAgent_) {
        indihubAgent_ = std::make_unique<IndiHubAgent>(
            "", indiHost_, indiPort_);
    }

    try {
        indihubAgent_->start(profile, mode);
        emitEvent("indihub_started", mode);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to start IndiHub: {}", e.what());
        return false;
    }
}

void INDIClient::stopIndiHub() {
    if (indihubAgent_) {
        indihubAgent_->stop();
        emitEvent("indihub_stopped", "");
    }
}

bool INDIClient::isIndiHubRunning() const {
    return indihubAgent_ && indihubAgent_->isRunning();
}

std::string INDIClient::getIndiHubMode() const {
    if (indihubAgent_) {
        return indihubAgent_->getMode();
    }
    return "";
}

int INDIClient::loadDriversFromXML(const std::string& /*path*/) {
    // TODO: Implement XML driver loading
    return 0;
}

void INDIClient::watchDevice(const std::string& deviceName) {
    spdlog::debug("Watching device: {}", deviceName);
    // In a full implementation, this would register for property updates
    // For now, just log the request
}

std::unordered_map<std::string, PropertyValue> INDIClient::getDeviceProperties(
    const std::string& deviceName) const {
    std::unordered_map<std::string, PropertyValue> result;
    
    auto deviceOpt = getDevice(deviceName);
    if (deviceOpt) {
        return deviceOpt->properties;
    }
    
    return result;
}

bool INDIClient::setNumberProperty(const std::string& device,
                                   const std::string& property,
                                   const std::string& element,
                                   double value) {
    return setProperty(device, property, element, std::to_string(value));
}

bool INDIClient::setSwitchProperty(const std::string& device,
                                   const std::string& property,
                                   const std::string& element,
                                   bool value) {
    return setProperty(device, property, element, value ? "On" : "Off");
}

bool INDIClient::setTextProperty(const std::string& device,
                                 const std::string& property,
                                 const std::string& element,
                                 const std::string& value) {
    return setProperty(device, property, element, value);
}

// Register with client registry
LITHIUM_REGISTER_CLIENT(INDIClient, "indi", "INDI Device Server",
                        ClientType::Server, "1.0.0", "indiserver")

}  // namespace lithium::client
