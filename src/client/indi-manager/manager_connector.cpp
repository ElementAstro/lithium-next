/*
 * manager_connector.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Manager Connector - Enhanced implementation

**************************************************/

#include "manager_connector.hpp"

#include "atom/error/exception.hpp"
#include "atom/io/io.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/system/command.hpp"
#include "atom/system/software.hpp"

#include <sstream>

namespace lithium::client::indi_manager {

ManagerConnector::ManagerConnector(const std::string& host, int port,
                                   const std::string& configPath,
                                   const std::string& dataPath,
                                   const std::string& fifoPath)
    : host_(host),
      port_(port),
      configPath_(configPath),
      dataPath_(dataPath),
      fifoPath_(fifoPath) {
    LOG_INFO("Initializing INDI Manager Connector - Host: {}, Port: {}", host_,
             port_);

    if (port_ <= 0 || port_ > 65535) {
        THROW_RUNTIME_ERROR("Invalid port number");
    }

    validatePaths();
    initializeComponents();
}

ManagerConnector::ManagerConnector(const ServerConfig& config)
    : host_(config.host),
      port_(config.port),
      configPath_(config.configDir),
      dataPath_(config.dataDir),
      fifoPath_(config.fifoPath) {
    LOG_INFO("Initializing INDI Manager Connector with config - Host: {}, Port: {}",
             host_, port_);

    validatePaths();

    serverManager_ = std::make_unique<ServerManager>(config);

    FifoChannelConfig fifoConfig;
    fifoConfig.fifoPath = config.fifoPath;
    fifoChannel_ = std::make_unique<FifoChannel>(fifoConfig);
}

ManagerConnector::~ManagerConnector() {
    if (isRunning()) {
        stopServer();
    }
}

void ManagerConnector::validatePaths() {
    if (!configPath_.empty() && !atom::io::isFolderExists(configPath_)) {
        LOG_WARN("Config directory does not exist: {}", configPath_);
        if (!atom::io::createDirectory(configPath_)) {
            LOG_ERROR("Failed to create config directory");
        }
    }

    if (!dataPath_.empty() && !atom::io::isFolderExists(dataPath_)) {
        LOG_WARN("Data directory does not exist: {}", dataPath_);
    }
}

void ManagerConnector::initializeComponents() {
    ServerConfig serverConfig;
    serverConfig.host = host_;
    serverConfig.port = port_;
    serverConfig.fifoPath = fifoPath_;
    serverConfig.configDir = configPath_;
    serverConfig.dataDir = dataPath_;
    serverConfig.startMode = ServerStartMode::Verbose;
    serverConfig.enableFifo = true;
    serverConfig.enableLogging = true;

    serverManager_ = std::make_unique<ServerManager>(serverConfig);

    FifoChannelConfig fifoConfig;
    fifoConfig.fifoPath = fifoPath_;
    fifoConfig.retryCount = 3;
    fifoConfig.retryDelayMs = 100;
    fifoConfig.queueCommands = true;
    fifoChannel_ = std::make_unique<FifoChannel>(fifoConfig);
}

// ==================== Server Lifecycle ====================

auto ManagerConnector::startServer() -> bool {
    LOG_INFO("Starting INDI server on port {}", port_);

    if (!serverManager_) {
        LOG_ERROR("Server manager not initialized");
        return false;
    }

    if (serverManager_->isRunning()) {
        LOG_INFO("INDI server already running");
        return true;
    }

    bool result = serverManager_->start();

    if (result) {
        LOG_INFO("INDI server started successfully");
        if (fifoChannel_) {
            fifoChannel_->setFifoPath(serverManager_->getFifoPath());
        }
    } else {
        LOG_ERROR("Failed to start INDI server: {}",
                  serverManager_->getLastError());
    }

    return result;
}

auto ManagerConnector::stopServer() -> bool {
    LOG_INFO("Stopping INDI server");

    if (!serverManager_) {
        return true;
    }

    {
        std::lock_guard lock(driverMutex_);
        for (const auto& [label, driver] : runningDrivers_) {
            if (fifoChannel_) {
                fifoChannel_->stopDriver(driver->binary);
            }
        }
        runningDrivers_.clear();
    }

    if (fifoChannel_) {
        fifoChannel_->close();
    }

    bool result = serverManager_->stop();

    if (result) {
        LOG_INFO("INDI server stopped successfully");
    } else {
        LOG_ERROR("Failed to stop INDI server gracefully, forcing");
        result = serverManager_->stop(true);
    }

    return result;
}

auto ManagerConnector::restartServer() -> bool {
    LOG_INFO("Restarting INDI server");

    if (!serverManager_) {
        return false;
    }

    return serverManager_->restart();
}

auto ManagerConnector::isRunning() -> bool {
    return serverManager_ && serverManager_->isRunning();
}

auto ManagerConnector::isInstalled() -> bool {
    return ServerManager::isInstalled("indiserver");
}

auto ManagerConnector::getServerState() const -> ServerState {
    if (!serverManager_) {
        return ServerState::Stopped;
    }
    return serverManager_->getState();
}

auto ManagerConnector::getServerUptime() const
    -> std::optional<std::chrono::seconds> {
    if (!serverManager_) {
        return std::nullopt;
    }
    return serverManager_->getUptime();
}

auto ManagerConnector::getLastError() const -> std::string {
    if (!serverManager_) {
        return "Server manager not initialized";
    }
    return serverManager_->getLastError();
}

// ==================== Configuration ====================

auto ManagerConnector::setServerConfig(const ServerConfig& config) -> bool {
    if (!serverManager_) {
        return false;
    }

    bool result = serverManager_->setConfig(config);
    if (result) {
        host_ = config.host;
        port_ = config.port;
        fifoPath_ = config.fifoPath;
        configPath_ = config.configDir;
        dataPath_ = config.dataDir;

        if (fifoChannel_) {
            fifoChannel_->setFifoPath(config.fifoPath);
        }
    }
    return result;
}

auto ManagerConnector::getServerConfig() const -> const ServerConfig& {
    static ServerConfig emptyConfig;
    if (!serverManager_) {
        return emptyConfig;
    }
    return serverManager_->getConfig();
}

void ManagerConnector::setFifoConfig(const FifoChannelConfig& config) {
    if (fifoChannel_) {
        fifoChannel_->setConfig(config);
    }
}

// ==================== Driver Management ====================

auto ManagerConnector::startDriver(const std::shared_ptr<DeviceContainer>& driver)
    -> bool {
    if (!driver) {
        LOG_ERROR("Invalid driver pointer");
        return false;
    }

    if (!isRunning()) {
        LOG_ERROR("Cannot start driver: server not running");
        return false;
    }

    LOG_INFO("Starting INDI driver: {} ({})", driver->label, driver->binary);

    if (!fifoChannel_) {
        LOG_ERROR("FIFO channel not initialized");
        return false;
    }

    FifoResult result =
        fifoChannel_->startDriver(driver->binary, driver->skeleton);

    if (result.success) {
        std::lock_guard lock(driverMutex_);
        runningDrivers_.emplace(driver->label, driver);
        notifyDriverEvent(driver->label, true);
        LOG_INFO("Driver {} started successfully", driver->label);
        return true;
    }

    LOG_ERROR("Failed to start driver {}: {}", driver->label,
              result.errorMessage);
    return false;
}

auto ManagerConnector::stopDriver(const std::shared_ptr<DeviceContainer>& driver)
    -> bool {
    if (!driver) {
        LOG_ERROR("Invalid driver pointer");
        return false;
    }

    LOG_INFO("Stopping INDI driver: {}", driver->label);

    if (!fifoChannel_) {
        LOG_ERROR("FIFO channel not initialized");
        return false;
    }

    FifoResult result = fifoChannel_->stopDriver(driver->binary);

    {
        std::lock_guard lock(driverMutex_);
        runningDrivers_.erase(driver->label);
    }

    if (result.success) {
        notifyDriverEvent(driver->label, false);
        LOG_INFO("Driver {} stopped successfully", driver->label);
        return true;
    }

    LOG_WARN("Stop command sent but may have failed: {}", result.errorMessage);
    return true;
}

auto ManagerConnector::restartDriver(
    const std::shared_ptr<DeviceContainer>& driver) -> bool {
    if (!driver) {
        return false;
    }

    LOG_INFO("Restarting INDI driver: {}", driver->label);

    if (!fifoChannel_) {
        return false;
    }

    FifoResult result =
        fifoChannel_->restartDriver(driver->binary, driver->skeleton);
    return result.success;
}

auto ManagerConnector::startDriverByName(const std::string& binary,
                                         const std::string& skeleton) -> bool {
    if (!isRunning()) {
        LOG_ERROR("Cannot start driver: server not running");
        return false;
    }

    if (!fifoChannel_) {
        return false;
    }

    LOG_INFO("Starting driver by name: {}", binary);
    FifoResult result = fifoChannel_->startDriver(binary, skeleton);

    if (result.success) {
        auto container = std::make_shared<DeviceContainer>();
        container->binary = binary;
        container->label = binary;
        container->skeleton = skeleton;

        std::lock_guard lock(driverMutex_);
        runningDrivers_.emplace(binary, container);
        notifyDriverEvent(binary, true);
    }

    return result.success;
}

auto ManagerConnector::stopDriverByName(const std::string& binary) -> bool {
    if (!fifoChannel_) {
        return false;
    }

    LOG_INFO("Stopping driver by name: {}", binary);
    FifoResult result = fifoChannel_->stopDriver(binary);

    {
        std::lock_guard lock(driverMutex_);
        runningDrivers_.erase(binary);
    }

    if (result.success) {
        notifyDriverEvent(binary, false);
    }

    return result.success;
}

auto ManagerConnector::isDriverRunning(const std::string& driverLabel) const
    -> bool {
    std::lock_guard lock(driverMutex_);
    return runningDrivers_.find(driverLabel) != runningDrivers_.end();
}

// ==================== Property Access ====================

auto ManagerConnector::setProp(const std::string& dev, const std::string& prop,
                               const std::string& element,
                               const std::string& value) -> bool {
    std::string cmd = "indi_setprop \"" + dev + "." + prop + "." + element +
                      "=" + value + "\"";
    try {
        atom::system::executeCommand(cmd, false);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to set property: {}", e.what());
        return false;
    }
}

auto ManagerConnector::getProp(const std::string& dev, const std::string& prop,
                               const std::string& element) -> std::string {
    std::string cmd =
        "indi_getprop \"" + dev + "." + prop + "." + element + "\"";
    try {
        return atom::system::executeCommand(cmd, false);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get property: {}", e.what());
        return "";
    }
}

auto ManagerConnector::getState(const std::string& dev, const std::string& prop)
    -> std::string {
    std::string cmd = "indi_getprop -1 \"" + dev + "." + prop + "._STATE\"";
    try {
        return atom::system::executeCommand(cmd, false);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get state: {}", e.what());
        return "";
    }
}

// ==================== Device/Driver Queries ====================

auto ManagerConnector::getRunningDrivers()
    -> std::unordered_map<std::string, std::shared_ptr<DeviceContainer>> {
    std::lock_guard lock(driverMutex_);
    return runningDrivers_;
}

auto ManagerConnector::getDevices()
    -> std::vector<std::unordered_map<std::string, std::string>> {
    std::vector<std::unordered_map<std::string, std::string>> devices;

    try {
        std::string output =
            atom::system::executeCommand("indi_getprop *.CONNECTION.*", false);

        // Parse output to extract device information
        std::istringstream iss(output);
        std::string line;
        std::unordered_map<std::string, std::string> currentDevice;

        while (std::getline(iss, line)) {
            if (line.empty())
                continue;

            auto dotPos = line.find('.');
            if (dotPos != std::string::npos) {
                std::string deviceName = line.substr(0, dotPos);
                if (currentDevice.empty() ||
                    currentDevice["device"] != deviceName) {
                    if (!currentDevice.empty()) {
                        devices.push_back(currentDevice);
                    }
                    currentDevice.clear();
                    currentDevice["device"] = deviceName;
                }
            }
        }

        if (!currentDevice.empty()) {
            devices.push_back(currentDevice);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get devices: {}", e.what());
    }

    return devices;
}

auto ManagerConnector::getRunningDriverCount() const -> size_t {
    std::lock_guard lock(driverMutex_);
    return runningDrivers_.size();
}

// ==================== Events ====================

void ManagerConnector::setServerEventCallback(ServerEventCallback callback) {
    if (serverManager_) {
        serverManager_->setEventCallback(std::move(callback));
    }
}

void ManagerConnector::setDriverEventCallback(DriverEventCallback callback) {
    driverEventCallback_ = std::move(callback);
}

// ==================== FIFO Channel Access ====================

auto ManagerConnector::getFifoChannel() -> FifoChannel* {
    return fifoChannel_.get();
}

auto ManagerConnector::sendFifoCommand(const std::string& command) -> bool {
    if (!fifoChannel_) {
        return false;
    }
    return fifoChannel_->sendRaw(command).success;
}

// ==================== Statistics ====================

auto ManagerConnector::getTotalFifoCommands() const -> uint64_t {
    if (!fifoChannel_) {
        return 0;
    }
    return fifoChannel_->getTotalCommandsSent();
}

auto ManagerConnector::getTotalFifoErrors() const -> uint64_t {
    if (!fifoChannel_) {
        return 0;
    }
    return fifoChannel_->getTotalErrors();
}

void ManagerConnector::notifyDriverEvent(const std::string& driver,
                                         bool started) {
    if (driverEventCallback_) {
        driverEventCallback_(driver, started);
    }
}

}  // namespace lithium::client::indi_manager
