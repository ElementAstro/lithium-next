/*
 * iconnector.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: INDI Server Connector - Enhanced implementation

**************************************************/

#include "iconnector.hpp"
#include "container.hpp"

#include "atom/error/exception.hpp"
#include "atom/io/io.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/system/command.hpp"
#include "atom/system/software.hpp"

#include <sstream>

using namespace lithium::client::indi;

INDIConnector::INDIConnector(const std::string &hst, int prt,
                             const std::string &cfg, const std::string &dta,
                             const std::string &fif)
    : host_(hst),
      port_(prt),
      config_path_(cfg),
      data_path_(dta),
      fifo_path_(fif) {
    LOG_INFO( "Initializing INDI Connector - Host: {}, Port: {}", host_,
          port_);

    if (port_ <= 0 || port_ > 65535) {
        THROW_RUNTIME_ERROR("Invalid port number");
    }

    validatePaths();
    initializeComponents();
}

INDIConnector::INDIConnector(const INDIServerConfig& config)
    : host_(config.host),
      port_(config.port),
      config_path_(config.configDir),
      data_path_(config.dataDir),
      fifo_path_(config.fifoPath) {
    LOG_INFO( "Initializing INDI Connector with config - Host: {}, Port: {}", 
          host_, port_);

    validatePaths();
    
    // Create server manager with provided config
    serverManager_ = std::make_unique<INDIServerManager>(config);
    
    // Create FIFO channel
    FifoChannelConfig fifoConfig;
    fifoConfig.fifoPath = config.fifoPath;
    fifoChannel_ = std::make_unique<FifoChannel>(fifoConfig);
}

INDIConnector::~INDIConnector() {
    if (isRunning()) {
        stopServer();
    }
}

void INDIConnector::validatePaths() {
    if (!config_path_.empty() && !atom::io::isFolderExists(config_path_)) {
        LOG_WARN( "Config directory does not exist: {}", config_path_);
        if (!atom::io::createDirectory(config_path_)) {
            LOG_ERROR( "Failed to create config directory");
        }
    }

    if (!data_path_.empty() && !atom::io::isFolderExists(data_path_)) {
        LOG_WARN( "Data directory does not exist: {}", data_path_);
    }
}

void INDIConnector::initializeComponents() {
    // Create server configuration
    INDIServerConfig serverConfig;
    serverConfig.host = host_;
    serverConfig.port = port_;
    serverConfig.fifoPath = fifo_path_;
    serverConfig.configDir = config_path_;
    serverConfig.dataDir = data_path_;
    serverConfig.startMode = ServerStartMode::Verbose;
    serverConfig.enableFifo = true;
    serverConfig.enableLogging = true;
    
    // Create server manager
    serverManager_ = std::make_unique<INDIServerManager>(serverConfig);
    
    // Create FIFO channel
    FifoChannelConfig fifoConfig;
    fifoConfig.fifoPath = fifo_path_;
    fifoConfig.retryCount = 3;
    fifoConfig.retryDelayMs = 100;
    fifoConfig.queueCommands = true;
    fifoChannel_ = std::make_unique<FifoChannel>(fifoConfig);
}

// ==================== Server Lifecycle ====================

auto INDIConnector::startServer() -> bool {
    LOG_INFO( "Starting INDI server on port {}", port_);

    if (!serverManager_) {
        LOG_ERROR( "Server manager not initialized");
        return false;
    }

    if (serverManager_->isRunning()) {
        LOG_INFO( "INDI server already running");
        return true;
    }

    bool result = serverManager_->start();
    
    if (result) {
        LOG_INFO( "INDI server started successfully");
        // Update FIFO path in case it changed
        if (fifoChannel_) {
            fifoChannel_->setFifoPath(serverManager_->getFifoPath());
        }
    } else {
        LOG_ERROR( "Failed to start INDI server: {}", 
              serverManager_->getLastError());
    }

    return result;
}

auto INDIConnector::stopServer() -> bool {
    LOG_INFO( "Stopping INDI server");

    if (!serverManager_) {
        return true;
    }

    // Stop all drivers first
    {
        std::lock_guard lock(driverMutex_);
        for (const auto& [label, driver] : running_drivers_) {
            if (fifoChannel_) {
                fifoChannel_->stopDriver(driver->binary);
            }
        }
        running_drivers_.clear();
    }

    // Close FIFO channel
    if (fifoChannel_) {
        fifoChannel_->close();
    }

    // Stop server
    bool result = serverManager_->stop();
    
    if (result) {
        LOG_INFO( "INDI server stopped successfully");
    } else {
        LOG_ERROR( "Failed to stop INDI server gracefully, forcing");
        result = serverManager_->stop(true);
    }

    return result;
}

auto INDIConnector::restartServer() -> bool {
    LOG_INFO( "Restarting INDI server");
    
    if (!serverManager_) {
        return false;
    }
    
    return serverManager_->restart();
}

auto INDIConnector::isRunning() -> bool {
    return serverManager_ && serverManager_->isRunning();
}

auto INDIConnector::isInstalled() -> bool {
    return INDIServerManager::isInstalled("indiserver");
}

auto INDIConnector::getServerState() const -> ServerState {
    if (!serverManager_) {
        return ServerState::Stopped;
    }
    return serverManager_->getState();
}

auto INDIConnector::getServerUptime() const -> std::optional<std::chrono::seconds> {
    if (!serverManager_) {
        return std::nullopt;
    }
    return serverManager_->getUptime();
}

auto INDIConnector::getLastError() const -> std::string {
    if (!serverManager_) {
        return "Server manager not initialized";
    }
    return serverManager_->getLastError();
}

// ==================== Configuration ====================

auto INDIConnector::setServerConfig(const INDIServerConfig& config) -> bool {
    if (!serverManager_) {
        return false;
    }
    
    bool result = serverManager_->setConfig(config);
    if (result) {
        // Update local state
        host_ = config.host;
        port_ = config.port;
        fifo_path_ = config.fifoPath;
        config_path_ = config.configDir;
        data_path_ = config.dataDir;
        
        // Update FIFO channel
        if (fifoChannel_) {
            fifoChannel_->setFifoPath(config.fifoPath);
        }
    }
    return result;
}

auto INDIConnector::getServerConfig() const -> const INDIServerConfig& {
    static INDIServerConfig emptyConfig;
    if (!serverManager_) {
        return emptyConfig;
    }
    return serverManager_->getConfig();
}

void INDIConnector::setFifoConfig(const FifoChannelConfig& config) {
    if (fifoChannel_) {
        fifoChannel_->setConfig(config);
    }
}

// ==================== Driver Management ====================

auto INDIConnector::startDriver(
    const std::shared_ptr<INDIDeviceContainer> &driver) -> bool {
    if (!driver) {
        LOG_ERROR( "Invalid driver pointer");
        return false;
    }

    if (!isRunning()) {
        LOG_ERROR( "Cannot start driver: server not running");
        return false;
    }

    LOG_INFO( "Starting INDI driver: {} ({})", driver->label, driver->binary);

    if (!fifoChannel_) {
        LOG_ERROR( "FIFO channel not initialized");
        return false;
    }

    // Send start command via FIFO
    FifoResult result = fifoChannel_->startDriver(driver->binary, driver->skeleton);
    
    if (result.success) {
        std::lock_guard lock(driverMutex_);
        running_drivers_.emplace(driver->label, driver);
        notifyDriverEvent(driver->label, true);
        LOG_INFO( "Driver {} started successfully", driver->label);
        return true;
    }

    LOG_ERROR( "Failed to start driver {}: {}", driver->label, result.errorMessage);
    return false;
}

auto INDIConnector::stopDriver(
    const std::shared_ptr<INDIDeviceContainer> &driver) -> bool {
    if (!driver) {
        LOG_ERROR( "Invalid driver pointer");
        return false;
    }

    LOG_INFO( "Stopping INDI driver: {}", driver->label);

    if (!fifoChannel_) {
        LOG_ERROR( "FIFO channel not initialized");
        return false;
    }

    // Send stop command via FIFO
    FifoResult result = fifoChannel_->stopDriver(driver->binary);
    
    {
        std::lock_guard lock(driverMutex_);
        running_drivers_.erase(driver->label);
    }
    
    if (result.success) {
        notifyDriverEvent(driver->label, false);
        LOG_INFO( "Driver {} stopped successfully", driver->label);
        return true;
    }

    LOG_WARN( "Stop command sent but may have failed: {}", result.errorMessage);
    return true;  // Driver removed from tracking regardless
}

auto INDIConnector::restartDriver(
    const std::shared_ptr<INDIDeviceContainer> &driver) -> bool {
    if (!driver) {
        return false;
    }

    LOG_INFO( "Restarting INDI driver: {}", driver->label);

    if (!fifoChannel_) {
        return false;
    }

    FifoResult result = fifoChannel_->restartDriver(driver->binary, driver->skeleton);
    return result.success;
}

auto INDIConnector::startDriverByName(const std::string& binary, 
                                      const std::string& skeleton) -> bool {
    if (!isRunning()) {
        LOG_ERROR( "Cannot start driver: server not running");
        return false;
    }

    if (!fifoChannel_) {
        return false;
    }

    LOG_INFO( "Starting driver by name: {}", binary);
    FifoResult result = fifoChannel_->startDriver(binary, skeleton);
    
    if (result.success) {
        // Create a container for tracking
        auto container = std::make_shared<INDIDeviceContainer>();
        container->binary = binary;
        container->label = binary;
        container->skeleton = skeleton;
        
        std::lock_guard lock(driverMutex_);
        running_drivers_.emplace(binary, container);
        notifyDriverEvent(binary, true);
    }
    
    return result.success;
}

auto INDIConnector::stopDriverByName(const std::string& binary) -> bool {
    if (!fifoChannel_) {
        return false;
    }

    LOG_INFO( "Stopping driver by name: {}", binary);
    FifoResult result = fifoChannel_->stopDriver(binary);
    
    {
        std::lock_guard lock(driverMutex_);
        running_drivers_.erase(binary);
    }
    
    notifyDriverEvent(binary, false);
    return result.success;
}

auto INDIConnector::isDriverRunning(const std::string& driverLabel) const -> bool {
    std::lock_guard lock(driverMutex_);
    return running_drivers_.find(driverLabel) != running_drivers_.end();
}

// ==================== Property Access ====================

auto INDIConnector::setProp(const std::string &dev, const std::string &prop,
                            const std::string &element,
                            const std::string &value) -> bool {
    std::stringstream sss;
    sss << "indi_setprop " << dev << "." << prop << "." << element << "="
        << value;
    std::string cmd = sss.str();
    DLOG_INFO( "Cmd: {}", cmd);
    try {
        if (atom::system::executeCommand(cmd, false) != "") {
            LOG_ERROR( "Failed to execute command: {}", cmd);
            return false;
        }
    } catch (const atom::error::RuntimeError &e) {
        LOG_ERROR( "Failed to execute command: {} with {}", cmd, e.what());
        return false;
    }
    DLOG_INFO( "Set property: {}.{} to {}", dev, prop, value);
    return true;
}

auto INDIConnector::getProp(const std::string &dev, const std::string &prop,
                            const std::string &element) -> std::string {
    std::stringstream sss;
    sss << "indi_getprop " << dev << "." << prop << "." << element;
    std::string cmd = sss.str();
    try {
        std::string output = atom::system::executeCommand(cmd, false);
        size_t equalsPos = output.find('=');
        if (equalsPos != std::string::npos && equalsPos + 1 < output.length()) {
            return output.substr(equalsPos + 1,
                                 output.length() - equalsPos - 2);
        }
    } catch (const atom::error::RuntimeError &e) {
        LOG_ERROR( "Failed to execute command: {} with {}", cmd, e.what());
    }
    return "";
}

auto INDIConnector::getState(const std::string &dev,
                             const std::string &prop) -> std::string {
    return getProp(dev, prop, "_STATE");
}

// ==================== Device/Driver Queries ====================

#if ENABLE_FASTHASH
emhash8::HashMap<std::string, std::shared_ptr<INDIDeviceContainer>>
INDIConnector::getRunningDrivers()
#else
auto INDIConnector::getRunningDrivers()
    -> std::unordered_map<std::string, std::shared_ptr<INDIDeviceContainer>>
#endif
{
    std::lock_guard lock(driverMutex_);
    return running_drivers_;
}

#if ENABLE_FASTHASH
std::vector<emhash8::HashMap<std::string, std::string>>
INDIConnector::getDevices()
#else
auto INDIConnector::getDevices()
    -> std::vector<std::unordered_map<std::string, std::string>>
#endif
{
#if ENABLE_FASTHASH
    std::vector<emhash8::HashMap<std::string, std::string>> devices;
#else
    std::vector<std::unordered_map<std::string, std::string>> devices;
#endif
    std::string cmd = "indi_getprop *.CONNECTION.CONNECT";

    try {
        auto result = atom::system::executeCommand(cmd, false);
        std::vector<std::string> lines = {"", ""};
        for (char token : result) {
            if (token == '\n') {
                std::unordered_map<std::string, std::string> device;
                std::stringstream sss(lines[0]);
                std::string item;
                while (getline(sss, item, '.')) {
                    device["device"] = item;
                }
                device["connected"] = (lines[1] == "On") ? "true" : "false";
                devices.push_back(device);
                lines = {"", ""};
            } else if (token == '=') {
                lines[1] = lines[1].substr(0, lines[1].length() - 1);
            } else if (token == ' ') {
                continue;
            } else {
                lines[(lines[0] == "") ? 0 : 1] += token;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR( "Failed to get devices: {}", e.what());
    }
    return devices;
}

auto INDIConnector::getRunningDriverCount() const -> size_t {
    std::lock_guard lock(driverMutex_);
    return running_drivers_.size();
}

// ==================== Events ====================

void INDIConnector::setServerEventCallback(ServerEventCallback callback) {
    if (serverManager_) {
        serverManager_->setEventCallback(std::move(callback));
    }
}

void INDIConnector::setDriverEventCallback(DriverEventCallback callback) {
    driverEventCallback_ = std::move(callback);
}

void INDIConnector::notifyDriverEvent(const std::string& driver, bool started) {
    if (driverEventCallback_) {
        driverEventCallback_(driver, started);
    }
}

// ==================== FIFO Channel Access ====================

auto INDIConnector::getFifoChannel() -> FifoChannel* {
    return fifoChannel_.get();
}

auto INDIConnector::sendFifoCommand(const std::string& command) -> bool {
    if (!fifoChannel_) {
        LOG_ERROR( "FIFO channel not initialized");
        return false;
    }
    
    FifoResult result = fifoChannel_->sendRaw(command);
    return result.success;
}

// ==================== Statistics ====================

auto INDIConnector::getTotalFifoCommands() const -> uint64_t {
    if (!fifoChannel_) {
        return 0;
    }
    return fifoChannel_->getTotalCommandsSent();
}

auto INDIConnector::getTotalFifoErrors() const -> uint64_t {
    if (!fifoChannel_) {
        return 0;
    }
    return fifoChannel_->getTotalErrors();
}
