#include "device_task.hpp"
#include <spdlog/spdlog.h>
#include "factory.hpp"

namespace lithium::sequencer {

DeviceTask::DeviceTask(const std::string& name, DeviceManager& manager)
    : Task(name, [this](const json& params) { execute(params); }),
      deviceManager_(&manager) {
    spdlog::info("DeviceTask created with name: {}", name);
    setupDefaults();
}

void DeviceTask::setupDefaults() {
    spdlog::info("Setting up default parameters for DeviceTask");
    // Add parameter definitions
    addParamDefinition("operation", "string", true, nullptr, "Operation type");
    addParamDefinition("deviceName", "string", false, nullptr, "Device name");
    addParamDefinition("deviceType", "string", false, nullptr, "Device type");
    addParamDefinition("timeout", "number", false, 5000, "Timeout (ms)");
    addParamDefinition("retryCount", "number", false, 0, "Retry count");

    // Set task properties
    setLogLevel(2);
    setTimeout(std::chrono::seconds(30));
    spdlog::info("Default parameters set up completed");
}

void DeviceTask::execute(const json& params) {
    spdlog::info("Executing DeviceTask with parameters: {}", params.dump());
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
        spdlog::info("DeviceTask execution completed for operation: {}",
                     operation);

    } catch (const std::exception& e) {
        handleDeviceError(params.value("deviceName", "unknown"), e.what());
        spdlog::error("DeviceTask execution failed: {}", e.what());
        throw;
    }
}

bool DeviceTask::connectDevice(const std::string& name, int timeout) {
    spdlog::info("Connecting device: {} with timeout: {}", name, timeout);
    try {
        if (!deviceManager_) {
            THROW_RUNTIME_ERROR("Device manager not initialized");
        }

        spdlog::debug("Checking if device {} is already connected", name);
        if (deviceManager_->isDeviceConnected(name)) {
            spdlog::info("Device {} is already connected", name);
            return true;
        }

        deviceManager_->connectDeviceByName(name);
        updateDeviceStatus(name,
                           {.isConnected = true,
                            .state = "connected",
                            .lastOperation = std::chrono::system_clock::now()});

        spdlog::info("Device {} connected successfully", name);
        return true;

    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        spdlog::error("Failed to connect device {}: {}", name, e.what());
        return false;
    }
}

std::vector<std::string> DeviceTask::scanDevices(const std::string& type) {
    spdlog::info("Scanning for devices of type: {}", type);
    try {
        auto devices = deviceManager_->scanDevices(type);
        addHistoryEntry("Found " + std::to_string(devices.size()) + " devices");
        spdlog::debug("Scan result: {}", fmt::join(devices, ", "));
        spdlog::info("Scan completed, found {} devices", devices.size());
        return devices;
    } catch (const std::exception& e) {
        spdlog::error("Scan failed: {}", e.what());
        return {};
    }
}

bool DeviceTask::initializeDevice(const std::string& name) {
    spdlog::info("Initializing device: {}", name);
    try {
        if (!deviceManager_->isDeviceConnected(name)) {
            spdlog::error("Device {} not connected", name);
            return false;
        }

        deviceManager_->initializeDevice(name);

        updateDeviceStatus(name,
                           {.isConnected = true,
                            .isInitialized = true,
                            .state = "initialized",
                            .lastOperation = std::chrono::system_clock::now()});
        spdlog::info("Device {} initialized successfully", name);
        return true;
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        spdlog::error("Failed to initialize device {}: {}", name, e.what());
        return false;
    }
}

void DeviceTask::setPriority(const std::string& name, DevicePriority priority) {
    spdlog::debug(
        "Setting priority for device {}: level={}, preempt={}, timeout={}",
        name, priority.level, priority.preempt, priority.timeout);
    std::unique_lock lock(statusMutex_);
    priorities_[name] = priority;
}

void DeviceTask::setConcurrencyLimit(int limit) {
    spdlog::debug("Setting concurrency limit: {}", limit);
    concurrencyLimit_ = std::max(1, limit);
}

void DeviceTask::setRetryStrategy(const std::string& name,
                                  RetryStrategy strategy) {
    spdlog::debug("Setting retry strategy for device {}: {}", name,
                  static_cast<int>(strategy));
    deviceManager_->setDeviceRetryStrategy(name, strategy);
}

DeviceStatus DeviceTask::getDeviceStatus(const std::string& name) const {
    spdlog::debug("Getting status for device: {}", name);
    std::shared_lock lock(statusMutex_);
    if (auto it = deviceStatuses_.find(name); it != deviceStatuses_.end()) {
        return it->second;
    }
    return DeviceStatus{};
}

std::vector<std::string> DeviceTask::getConnectedDevices() const {
    spdlog::debug("Getting list of connected devices");
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
    spdlog::debug("Getting error logs for device: {}", name);
    std::shared_lock lock(statusMutex_);
    if (auto it = deviceStatuses_.find(name); it != deviceStatuses_.end()) {
        return it->second.errors;
    }
    return {};
}

void DeviceTask::disconnectDevice(const std::string& name) {
    spdlog::info("Disconnecting device: {}", name);
    try {
        // deviceManager_->disconnectDevice(name);
        cleanupDevice(name);
        spdlog::info("Device {} disconnected successfully", name);
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        spdlog::error("Failed to disconnect device {}: {}", name, e.what());
    }
}

void DeviceTask::resetDevice(const std::string& name) {
    spdlog::info("Resetting device: {}", name);
    try {
        deviceManager_->resetDevice(name);
        updateDeviceStatus(name,
                           {.isConnected = true,
                            .isInitialized = false,
                            .state = "reset",
                            .lastOperation = std::chrono::system_clock::now()});
        spdlog::info("Device {} reset successfully", name);
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        spdlog::error("Failed to reset device {}: {}", name, e.what());
    }
}

void DeviceTask::abortOperation(const std::string& name) {
    spdlog::warn("Aborting operations for device: {}", name);
    shouldStop_ = true;
    deviceManager_->abortDeviceOperation(name);
}

void DeviceTask::validateParameters(const json& params) {
    spdlog::debug("Validating parameters: {}", params.dump());
    if (!params.contains("operation")) {
        THROW_RUNTIME_ERROR("Missing required parameter: operation");
    }
}

void DeviceTask::monitorDevice(const std::string& deviceName) {
    spdlog::debug("Monitoring device: {}", deviceName);
    if (!checkDeviceHealth(deviceName)) {
        handleDeviceError(deviceName, "Device health check failed");
        spdlog::error("Device health check failed for: {}", deviceName);
    }
}

bool DeviceTask::checkDeviceHealth(const std::string& name) {
    spdlog::debug("Checking health for device: {}", name);
    try {
        float health = deviceManager_->getDeviceHealth(name);
        updateDeviceStatus(name,
                           {.isConnected = true,
                            .health = health,
                            .state = health > 0.5 ? "healthy" : "unhealthy",
                            .lastOperation = std::chrono::system_clock::now()});
        spdlog::info("Device {} health check result: {}", name, health);
        return health > 0.5;
    } catch (...) {
        spdlog::error("Failed to check health for device: {}", name);
        return false;
    }
}

void DeviceTask::cleanupDevice(const std::string& name) {
    spdlog::debug("Cleaning up device: {}", name);
    std::unique_lock lock(statusMutex_);
    deviceStatuses_.erase(name);
    priorities_.erase(name);
}

std::string DeviceTask::validateDeviceOperation(DeviceOperation op,
                                                const std::string& name) {
    spdlog::debug("Validating device operation: {} for device: {}",
                  static_cast<int>(op), name);
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
    spdlog::debug("Updating status for device: {}", name);
    std::unique_lock lock(statusMutex_);
    deviceStatuses_[name] = status;
}

void DeviceTask::handleDeviceError(const std::string& deviceName,
                                   const std::string& error) {
    spdlog::error("Device error ({}): {}", deviceName, error);

    std::unique_lock lock(statusMutex_);
    if (auto it = deviceStatuses_.find(deviceName);
        it != deviceStatuses_.end()) {
        it->second.errors.push_back(error);
    }
}

// Register DeviceTask with factory
namespace {
static auto device_task_registrar = TaskRegistrar<DeviceTask>(
    "device_task",
    TaskInfo{.name = "device_task",
             .description = "Manage and control astronomical devices",
             .category = "hardware",
             .requiredParameters = {"operation"},
             .parameterSchema =
                 json{{"operation",
                       {{"type", "string"},
                        {"description", "Device operation to perform"},
                        {"enum", json::array({"connect", "scan", "initialize",
                                              "configure", "test"})}}},
                      {"deviceName",
                       {{"type", "string"},
                        {"description", "Name of the device to operate on"}}},
                      {"deviceType",
                       {{"type", "string"},
                        {"description",
                         "Type of device (camera, mount, filterwheel, etc.)"}}},
                      {"timeout",
                       {{"type", "number"},
                        {"description", "Operation timeout in milliseconds"},
                        {"default", 5000}}},
                      {"retryCount",
                       {{"type", "number"},
                        {"description", "Number of retry attempts"},
                        {"default", 0}}},
                      {"port",
                       {{"type", "string"},
                        {"description", "Device connection port"}}},
                      {"config",
                       {{"type", "object"},
                        {"description",
                         "Device-specific configuration parameters"}}}},
             .version = "1.0.0",
             .dependencies = {},
             .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<DeviceTask> {
        // Get device manager instance from global pointer or create one
        // For now, we'll create a simple instance
        static DeviceManager deviceManager;
        return std::make_unique<DeviceTask>(name, deviceManager);
    });
}  // namespace

}  // namespace lithium::sequencer
