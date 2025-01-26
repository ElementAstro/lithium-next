#include "device_task.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::sequencer {

DeviceTask::DeviceTask(const std::string& name, DeviceManager& manager)
    : Task(name, [this](const json& params) { execute(params); }),
      deviceManager_(&manager) {
    LOG_F(INFO, "DeviceTask created with name: {}", name);
    setupDefaults();
}

void DeviceTask::setupDefaults() {
    LOG_F(INFO, "Setting up default parameters for DeviceTask");
    // 添加参数定义
    addParamDefinition("operation", "string", true, nullptr, "操作类型");
    addParamDefinition("deviceName", "string", false, nullptr, "设备名称");
    addParamDefinition("deviceType", "string", false, nullptr, "设备类型");
    addParamDefinition("timeout", "number", false, 5000, "超时时间(ms)");
    addParamDefinition("retryCount", "number", false, 0, "重试次数");

    // 设置任务属性
    // setPriority(8);
    setLogLevel(2);
    setTimeout(std::chrono::seconds(30));
    LOG_F(INFO, "Default parameters set up completed");
}

void DeviceTask::execute(const json& params) {
    LOG_F(INFO, "Executing DeviceTask with parameters: {}", params.dump());
    try {
        validateParameters(params);

        std::string operation = params["operation"].get<std::string>();
        std::string deviceName = params.value("deviceName", "");

        if (operation == "connect") {
            connectDevice(deviceName, params.value("timeout", 5000));
        } else if (operation == "scan") {
            std::string deviceType = params["deviceType"].get<std::string>();
            scanDevices(deviceType);
        } else if (operation == "initialize") {
            initializeDevice(deviceName);
        }

        addHistoryEntry("Completed " + operation +
                        " for device: " + deviceName);
        LOG_F(INFO, "DeviceTask execution completed for operation: {}",
              operation);

    } catch (const std::exception& e) {
        handleDeviceError(params.value("deviceName", "unknown"), e.what());
        LOG_F(ERROR, "DeviceTask execution failed: {}", e.what());
        throw;
    }
}

bool DeviceTask::connectDevice(const std::string& name, int timeout) {
    LOG_F(INFO, "Connecting device: {} with timeout: {}", name, timeout);
    try {
        if (!deviceManager_) {
            THROW_RUNTIME_ERROR("Device manager not initialized");
        }

        LOG_F(DEBUG, "Checking if device {} is already connected", name);
        if (deviceManager_->isDeviceConnected(name)) {
            LOG_F(INFO, "Device {} is already connected", name);
            return true;
        }

        deviceManager_->connectDeviceByName(name);
        updateDeviceStatus(name,
                           {.isConnected = true,
                            .state = "connected",
                            .lastOperation = std::chrono::system_clock::now()});

        LOG_F(INFO, "Device {} connected successfully", name);
        return true;

    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        LOG_F(ERROR, "Failed to connect device {}: {}", name, e.what());
        return false;
    }
}

std::vector<std::string> DeviceTask::scanDevices(const std::string& type) {
    LOG_F(INFO, "Scanning for devices of type: {}", type);
    try {
        auto devices = deviceManager_->scanDevices(type);
        addHistoryEntry("Found " + std::to_string(devices.size()) + " devices");
        LOG_F(DEBUG, "Scan result: {}", devices);
        LOG_F(INFO, "Scan completed, found {} devices", devices.size());
        return devices;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Scan failed: {}", e.what());
        return {};
    }
}

bool DeviceTask::initializeDevice(const std::string& name) {
    LOG_F(INFO, "Initializing device: {}", name);
    try {
        if (!deviceManager_->isDeviceConnected(name)) {
            LOG_F(ERROR, "Device {} not connected", name);
            return false;
        }

        deviceManager_->initializeDevice(name);

        updateDeviceStatus(name,
                           {.isConnected = true,
                            .isInitialized = true,
                            .state = "initialized",
                            .lastOperation = std::chrono::system_clock::now()});
        LOG_F(INFO, "Device {} initialized successfully", name);
        return true;
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        LOG_F(ERROR, "Failed to initialize device {}: {}", name, e.what());
        return false;
    }
}

void DeviceTask::setPriority(const std::string& name, DevicePriority priority) {
    LOG_F(DEBUG, "Setting priority for device {}: {}", name, priority);
    std::unique_lock lock(statusMutex_);
    priorities_[name] = priority;
}

void DeviceTask::setConcurrencyLimit(int limit) {
    LOG_F(DEBUG, "Setting concurrency limit: {}", limit);
    concurrencyLimit_ = std::max(1, limit);
}

void DeviceTask::setRetryStrategy(const std::string& name,
                                  RetryStrategy strategy) {
    LOG_F(DEBUG, "Setting retry strategy for device {}: {}", name, strategy);
    deviceManager_->setDeviceRetryStrategy(name, strategy);
}

DeviceStatus DeviceTask::getDeviceStatus(const std::string& name) const {
    LOG_F(DEBUG, "Getting status for device: {}", name);
    std::shared_lock lock(statusMutex_);
    if (auto it = deviceStatuses_.find(name); it != deviceStatuses_.end()) {
        return it->second;
    }
    return DeviceStatus{};
}

std::vector<std::string> DeviceTask::getConnectedDevices() const {
    LOG_F(DEBUG, "Getting list of connected devices");
    std::vector<std::string> devices;
    std::shared_lock lock(statusMutex_);
    for (const auto& [name, status] : deviceStatuses_) {
        if (status.isConnected) {
            devices.push_back(name);
        }
    }
    return devices;
}

std::vector<std::string> DeviceTask::getErrorLogs(
    const std::string& name) const {
    LOG_F(DEBUG, "Getting error logs for device: {}", name);
    std::shared_lock lock(statusMutex_);
    if (auto it = deviceStatuses_.find(name); it != deviceStatuses_.end()) {
        return it->second.errors;
    }
    return {};
}

void DeviceTask::disconnectDevice(const std::string& name) {
    LOG_F(INFO, "Disconnecting device: {}", name);
    try {
        // deviceManager_->disconnectDevice(name);
        cleanupDevice(name);
        LOG_F(INFO, "Device {} disconnected successfully", name);
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        LOG_F(ERROR, "Failed to disconnect device {}: {}", name, e.what());
    }
}

void DeviceTask::resetDevice(const std::string& name) {
    LOG_F(INFO, "Resetting device: {}", name);
    try {
        deviceManager_->resetDevice(name);
        updateDeviceStatus(name,
                           {.isConnected = true,
                            .isInitialized = false,
                            .state = "reset",
                            .lastOperation = std::chrono::system_clock::now()});
        LOG_F(INFO, "Device {} reset successfully", name);
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        LOG_F(ERROR, "Failed to reset device {}: {}", name, e.what());
    }
}

void DeviceTask::abortOperation(const std::string& name) {
    LOG_F(WARNING, "Aborting operations for device: {}", name);
    shouldStop_ = true;
    deviceManager_->abortDeviceOperation(name);
}

void DeviceTask::validateParameters(const json& params) {
    LOG_F(DEBUG, "Validating parameters: {}", params.dump());
    if (!params.contains("operation")) {
        THROW_RUNTIME_ERROR("Missing required parameter: operation");
    }
}

void DeviceTask::monitorDevice(const std::string& deviceName) {
    LOG_F(DEBUG, "Monitoring device: {}", deviceName);
    if (!checkDeviceHealth(deviceName)) {
        handleDeviceError(deviceName, "Device health check failed");
        LOG_F(ERROR, "Device health check failed for: {}", deviceName);
    }
}

bool DeviceTask::checkDeviceHealth(const std::string& name) {
    LOG_F(DEBUG, "Checking health for device: {}", name);
    try {
        float health = deviceManager_->getDeviceHealth(name);
        updateDeviceStatus(name,
                           {.isConnected = true,
                            .health = health,
                            .state = health > 0.5 ? "healthy" : "unhealthy",
                            .lastOperation = std::chrono::system_clock::now()});
        LOG_F(INFO, "Device {} health check result: {}", name, health);
        return health > 0.5;
    } catch (...) {
        LOG_F(ERROR, "Failed to check health for device: {}", name);
        return false;
    }
}

void DeviceTask::cleanupDevice(const std::string& name) {
    LOG_F(DEBUG, "Cleaning up device: {}", name);
    std::unique_lock lock(statusMutex_);
    deviceStatuses_.erase(name);
    priorities_.erase(name);
}

std::string DeviceTask::validateDeviceOperation(DeviceOperation op,
                                                const std::string& name) {
    LOG_F(DEBUG, "Validating device operation: {} for device: {}", op, name);
    if (!deviceManager_->isDeviceValid(name)) {
        return "Invalid device name";
    }

    switch (op) {
        case DeviceOperation::Initialize:
            if (!deviceManager_->isDeviceConnected(name)) {
                return "Device not connected";
            }
            break;
        case DeviceOperation::Configure:
            if (!getDeviceStatus(name).isInitialized) {
                return "Device not initialized";
            }
            break;
        default:
            break;
    }

    return "";
}

void DeviceTask::updateDeviceStatus(const std::string& name,
                                    const DeviceStatus& status) {
    LOG_F(DEBUG, "Updating status for device: {}", name);
    std::unique_lock lock(statusMutex_);
    deviceStatuses_[name] = status;
}

void DeviceTask::handleDeviceError(const std::string& deviceName,
                                   const std::string& error) {
    LOG_F(ERROR, "Device error({}): {}", deviceName, error);

    std::unique_lock lock(statusMutex_);
    if (auto it = deviceStatuses_.find(deviceName);
        it != deviceStatuses_.end()) {
        it->second.errors.push_back(error);
    }
}

}  // namespace lithium::sequencer
