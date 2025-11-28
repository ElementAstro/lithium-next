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
        DeviceInfo info;
        auto it = dev.find("device");
        if (it != dev.end()) {
            info.name = it->second;
        }
        it = dev.find("connected");
        if (it != dev.end()) {
            info.connected = (it->second == "true");
        }
        result.push_back(info);
    }

    return result;
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

// Register with client registry
LITHIUM_REGISTER_CLIENT(INDIClient, "indi", "INDI Device Server",
                        ClientType::Server, "1.0.0", "indiserver")

}  // namespace lithium::client
