/*
 * manager_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: INDI Manager Client implementation

**************************************************/

#include "manager_client.hpp"
#include "manager_connector.hpp"

#include "atom/log/spdlog_logger.hpp"
#include "atom/system/software.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client::indi_manager {

ManagerClient::ManagerClient(std::string name) : ServerClient(std::move(name)) {
    LOG_INFO("ManagerClient created: {}", getName());
}

ManagerClient::~ManagerClient() {
    if (connector_) {
        connector_->stopServer();
    }
    stopIndiHub();
}

// ==================== Lifecycle ====================

bool ManagerClient::initialize() {
    LOG_INFO("Initializing ManagerClient");

    if (!atom::system::checkSoftwareInstalled("indiserver")) {
        setError(1, "INDI server not installed");
        return false;
    }

    setState(ClientState::Initialized);
    return true;
}

bool ManagerClient::destroy() {
    LOG_INFO("Destroying ManagerClient");

    if (connector_) {
        connector_->stopServer();
        connector_.reset();
    }

    stopIndiHub();
    setState(ClientState::Uninitialized);
    return true;
}

bool ManagerClient::connect(const std::string& target, int timeout,
                            int maxRetry) {
    LOG_INFO("Connecting to INDI server: {}", target);

    // Parse target (host:port format)
    std::string host = indiHost_;
    int port = indiPort_;

    auto colonPos = target.find(':');
    if (colonPos != std::string::npos) {
        host = target.substr(0, colonPos);
        port = std::stoi(target.substr(colonPos + 1));
    }

    // Create connector if not exists
    if (!connector_) {
        connector_ = std::make_unique<ManagerConnector>(host, port, configPath_,
                                                        dataPath_, fifoPath_);
    }

    // Start server if not running
    for (int retry = 0; retry < maxRetry; retry++) {
        if (connector_->startServer()) {
            setState(ClientState::Connected);
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout / maxRetry));
    }

    setError(2, "Failed to connect to INDI server");
    return false;
}

bool ManagerClient::disconnect() {
    LOG_INFO("Disconnecting from INDI server");

    if (connector_) {
        connector_->stopServer();
    }

    setState(ClientState::Disconnected);
    return true;
}

bool ManagerClient::isConnected() const {
    return connector_ && connector_->isRunning();
}

std::vector<std::string> ManagerClient::scan() {
    // Return default INDI server location
    return {"localhost:7624"};
}

// ==================== Server Control ====================

bool ManagerClient::startServer() {
    if (!connector_) {
        connector_ = std::make_unique<ManagerConnector>(
            indiHost_, indiPort_, configPath_, dataPath_, fifoPath_);
    }
    return connector_->startServer();
}

bool ManagerClient::stopServer() {
    if (!connector_) {
        return true;
    }
    return connector_->stopServer();
}

bool ManagerClient::isServerRunning() const {
    return connector_ && connector_->isRunning();
}

bool ManagerClient::isInstalled() const {
    return atom::system::checkSoftwareInstalled("indiserver");
}

// ==================== Driver Management ====================

bool ManagerClient::startDriver(const DriverInfo& driver) {
    if (!connector_) {
        setError(10, "Not connected to INDI server");
        return false;
    }

    auto container = std::make_shared<DeviceContainer>(
        driver.name, driver.label, driver.version, driver.binary,
        driver.manufacturer, driver.skeleton, false);

    return connector_->startDriver(container);
}

bool ManagerClient::stopDriver(const std::string& driverName) {
    if (!connector_) {
        return true;
    }

    return connector_->stopDriverByName(driverName);
}

std::unordered_map<std::string, DriverInfo> ManagerClient::getRunningDrivers()
    const {
    std::unordered_map<std::string, DriverInfo> result;

    if (!connector_) {
        return result;
    }

    auto drivers = const_cast<ManagerConnector*>(connector_.get())->getRunningDrivers();
    for (const auto& [name, container] : drivers) {
        DriverInfo info;
        info.name = container->name;
        info.label = container->label;
        info.version = container->version;
        info.binary = container->binary;
        info.manufacturer = container->family;
        info.skeleton = container->skeleton;
        info.backend = "INDI";
        info.running = true;
        result[name] = info;
    }

    return result;
}

std::vector<DriverInfo> ManagerClient::getAvailableDrivers() const {
    std::vector<DriverInfo> result;
    result.reserve(availableDrivers_.size());

    for (const auto& driver : availableDrivers_) {
        result.push_back(driver);
    }

    return result;
}

bool ManagerClient::startDriver(const std::shared_ptr<DeviceContainer>& container) {
    if (!connector_) {
        return false;
    }
    return connector_->startDriver(container);
}

bool ManagerClient::stopDriver(const std::shared_ptr<DeviceContainer>& container) {
    if (!connector_) {
        return true;
    }
    return connector_->stopDriver(container);
}

// ==================== Device Management ====================

std::vector<DeviceInfo> ManagerClient::getDevices() const {
    std::vector<DeviceInfo> result;

    if (!connector_) {
        return result;
    }

    auto devices = const_cast<ManagerConnector*>(connector_.get())->getDevices();
    for (const auto& devMap : devices) {
        result.push_back(convertToDeviceInfo(devMap));
    }

    return result;
}

DeviceInfo ManagerClient::convertToDeviceInfo(
    const std::unordered_map<std::string, std::string>& devMap) const {
    DeviceInfo info;

    auto it = devMap.find("device");
    if (it != devMap.end()) {
        info.name = it->second;
        info.id = it->second;
    }

    it = devMap.find("driver");
    if (it != devMap.end()) {
        info.driver = it->second;
    }

    it = devMap.find("interface");
    if (it != devMap.end()) {
        info.interfaces = parseInterfaceFlags(it->second);
        info.interfaceString = it->second;
    }

    info.backend = "INDI";
    return info;
}

DeviceInterface ManagerClient::parseInterfaceFlags(
    const std::string& interfaceStr) {
    DeviceInterface flags = DeviceInterface::General;

    if (interfaceStr.find("Telescope") != std::string::npos)
        flags = flags | DeviceInterface::Telescope;
    if (interfaceStr.find("CCD") != std::string::npos)
        flags = flags | DeviceInterface::CCD;
    if (interfaceStr.find("Guider") != std::string::npos)
        flags = flags | DeviceInterface::Guider;
    if (interfaceStr.find("Focuser") != std::string::npos)
        flags = flags | DeviceInterface::Focuser;
    if (interfaceStr.find("FilterWheel") != std::string::npos)
        flags = flags | DeviceInterface::FilterWheel;
    if (interfaceStr.find("Dome") != std::string::npos)
        flags = flags | DeviceInterface::Dome;

    return flags;
}

std::optional<DeviceInfo> ManagerClient::getDevice(const std::string& name) const {
    auto devices = getDevices();
    for (const auto& dev : devices) {
        if (dev.name == name) {
            return dev;
        }
    }
    return std::nullopt;
}

bool ManagerClient::connectDevice(const std::string& deviceName) {
    if (!connector_) {
        setError(20, "Not connected to INDI server");
        return false;
    }

    bool result =
        connector_->setProp(deviceName, "CONNECTION", "CONNECT", "On");
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

bool ManagerClient::disconnectDevice(const std::string& deviceName) {
    if (!connector_) {
        return true;
    }

    bool result =
        connector_->setProp(deviceName, "CONNECTION", "DISCONNECT", "On");
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

// ==================== Property Access ====================

bool ManagerClient::setProperty(const std::string& device,
                                const std::string& property,
                                const std::string& element,
                                const std::string& value) {
    if (!connector_) {
        return false;
    }
    return connector_->setProp(device, property, element, value);
}

std::string ManagerClient::getProperty(const std::string& device,
                                        const std::string& property,
                                        const std::string& element) const {
    if (!connector_) {
        return "";
    }
    return const_cast<ManagerConnector*>(connector_.get())
        ->getProp(device, property, element);
}

std::string ManagerClient::getPropertyState(const std::string& device,
                                            const std::string& property) const {
    if (!connector_) {
        return "";
    }
    return const_cast<ManagerConnector*>(connector_.get())
        ->getState(device, property);
}

// ==================== INDI-Specific ====================

void ManagerClient::configureINDI(const std::string& host, int port,
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

ConnectorInterface* ManagerClient::getConnector() const {
    return connector_.get();
}

bool ManagerClient::startIndiHub(const std::string& profile,
                                 const std::string& mode) {
    if (!isServerRunning()) {
        spdlog::warn("Cannot start IndiHub: INDI server not running");
        return false;
    }

    if (!indihubAgent_) {
        indihubAgent_ =
            std::make_unique<IndiHubAgent>("", indiHost_, indiPort_);
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

void ManagerClient::stopIndiHub() {
    if (indihubAgent_) {
        indihubAgent_->stop();
        emitEvent("indihub_stopped", "");
    }
}

bool ManagerClient::isIndiHubRunning() const {
    return indihubAgent_ && indihubAgent_->isRunning();
}

std::string ManagerClient::getIndiHubMode() const {
    if (indihubAgent_) {
        return indihubAgent_->getMode();
    }
    return "";
}

int ManagerClient::loadDriversFromXML(const std::string& /*path*/) {
    // TODO: Implement XML driver loading
    return 0;
}

void ManagerClient::watchDevice(const std::string& deviceName) {
    spdlog::debug("Watching device: {}", deviceName);
}

std::unordered_map<std::string, PropertyValue> ManagerClient::getDeviceProperties(
    const std::string& deviceName) const {
    std::unordered_map<std::string, PropertyValue> result;

    auto deviceOpt = getDevice(deviceName);
    if (deviceOpt) {
        return deviceOpt->properties;
    }

    return result;
}

bool ManagerClient::setNumberProperty(const std::string& device,
                                      const std::string& property,
                                      const std::string& element, double value) {
    return setProperty(device, property, element, std::to_string(value));
}

bool ManagerClient::setSwitchProperty(const std::string& device,
                                      const std::string& property,
                                      const std::string& element, bool value) {
    return setProperty(device, property, element, value ? "On" : "Off");
}

bool ManagerClient::setTextProperty(const std::string& device,
                                    const std::string& property,
                                    const std::string& element,
                                    const std::string& value) {
    return setProperty(device, property, element, value);
}

// Register with client registry
LITHIUM_REGISTER_CLIENT(ManagerClient, "indi-manager", "INDI Device Server Manager",
                        ClientType::Server, "1.0.0", "indiserver")

}  // namespace lithium::client::indi_manager
