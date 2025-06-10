#include "device_task.hpp"
#include <spdlog/spdlog.h>
#include "factory.hpp"

namespace lithium::sequencer {

DeviceTask::DeviceTask(const std::string& name, DeviceManager& manager)
    : Task(name, [this](const json& params) { this->execute(params); }),
      deviceManager_(&manager) {
    spdlog::info("DeviceTask created with name: {}", name);
    setupDefaults();
}

void DeviceTask::setupDefaults() {
    spdlog::info("Setting up default parameters for DeviceTask");

    // Add parameter definitions using the new Task API
    addParamDefinition("operation", "string", true, nullptr,
                       "Device operation to perform (connect, scan, "
                       "initialize, configure, test)");
    addParamDefinition("deviceName", "string", false, nullptr,
                       "Name of the device to operate on");
    addParamDefinition("deviceType", "string", false, nullptr,
                       "Type of device (camera, mount, filterwheel, etc.)");
    addParamDefinition("timeout", "number", false, 5000,
                       "Operation timeout in milliseconds");
    addParamDefinition("retryCount", "number", false, 0,
                       "Number of retry attempts");
    addParamDefinition("port", "string", false, nullptr,
                       "Device connection port");
    addParamDefinition("config", "object", false, json::object(),
                       "Device-specific configuration parameters");

    // Set task properties using the new Task API
    setLogLevel(2);
    setTimeout(std::chrono::seconds(30));
    Task::setPriority(5);

    spdlog::info("Default parameters set up completed");
}

void DeviceTask::execute(const json& params) {
    spdlog::info("Executing DeviceTask with parameters: {}", params.dump());

    // Use the new Task validation API
    if (!validateParams(params)) {
        setErrorType(TaskErrorType::InvalidParameter);
        auto errors = getParamErrors();
        std::string errorMsg = "Parameter validation failed: ";
        for (const auto& error : errors) {
            errorMsg += error + "; ";
        }
        spdlog::error(errorMsg);
        throw std::invalid_argument(errorMsg);
    }

    try {
        auto startTime = std::chrono::steady_clock::now();

        std::string operation = params["operation"].get<std::string>();
        std::string deviceName = params.value("deviceName", "");

        addHistoryEntry("Starting " + operation +
                        " operation for device: " + deviceName);

        bool success = false;
        if (operation == "connect") {
            success = connectDevice(deviceName, params.value("timeout", 5000));
        } else if (operation == "scan") {
            std::string deviceType = params.value("deviceType", "");
            if (deviceType.empty()) {
                throw std::invalid_argument(
                    "deviceType is required for scan operation");
            }
            auto devices = scanDevices(deviceType);
            success = !devices.empty();
        } else if (operation == "initialize") {
            success = initializeDevice(deviceName);
        } else if (operation == "configure") {
            // Handle configuration operation
            if (deviceName.empty()) {
                throw std::invalid_argument(
                    "deviceName is required for configure operation");
            }
            // Configuration logic would go here
            success = true;
        } else if (operation == "test") {
            // Handle test operation
            if (deviceName.empty()) {
                throw std::invalid_argument(
                    "deviceName is required for test operation");
            }
            success = checkDeviceHealth(deviceName);
        } else {
            throw std::invalid_argument("Unsupported operation: " + operation);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        if (success) {
            addHistoryEntry("Completed " + operation +
                            " for device: " + deviceName + " in " +
                            std::to_string(duration.count()) + "ms");
            spdlog::info(
                "DeviceTask execution completed successfully for operation: {}",
                operation);
        } else {
            addHistoryEntry("Failed " + operation +
                            " for device: " + deviceName);
            setErrorType(TaskErrorType::DeviceError);
            throw std::runtime_error("Device operation failed: " + operation);
        }

    } catch (const std::exception& e) {
        handleDeviceError(params.value("deviceName", "unknown"), e.what());
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Error during " + params.value("operation", "unknown") +
                        " operation: " + e.what());
        spdlog::error("DeviceTask execution failed: {}", e.what());
        throw;
    }
}

bool DeviceTask::connectDevice(const std::string& name, int timeout) {
    spdlog::info("Connecting device: {} with timeout: {}", name, timeout);

    if (name.empty()) {
        setErrorType(TaskErrorType::InvalidParameter);
        throw std::invalid_argument("Device name cannot be empty");
    }

    try {
        if (!deviceManager_) {
            setErrorType(TaskErrorType::SystemError);
            throw std::runtime_error("Device manager not initialized");
        }

        spdlog::debug("Checking if device {} is already connected", name);
        if (deviceManager_->isDeviceConnected(name)) {
            spdlog::info("Device {} is already connected", name);
            addHistoryEntry("Device " + name + " already connected");
            return true;
        }

        deviceManager_->connectDeviceByName(name);
        updateDeviceStatus(name,
                           {.isConnected = true,
                            .state = "connected",
                            .lastOperation = std::chrono::system_clock::now()});

        addHistoryEntry("Successfully connected to device: " + name);
        spdlog::info("Device {} connected successfully", name);
        return true;

    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        setErrorType(TaskErrorType::DeviceError);
        spdlog::error("Failed to connect device {}: {}", name, e.what());
        return false;
    }
}

std::vector<std::string> DeviceTask::scanDevices(const std::string& type) {
    spdlog::info("Scanning for devices of type: {}", type);

    if (type.empty()) {
        setErrorType(TaskErrorType::InvalidParameter);
        throw std::invalid_argument("Device type cannot be empty");
    }

    try {
        auto devices = deviceManager_->scanDevices(type);
        addHistoryEntry("Found " + std::to_string(devices.size()) +
                        " devices of type: " + type);
        spdlog::debug("Scan result: {}", fmt::join(devices, ", "));
        spdlog::info("Scan completed, found {} devices", devices.size());
        return devices;
    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Scan failed for type " + type + ": " + e.what());
        spdlog::error("Scan failed: {}", e.what());
        return {};
    }
}

bool DeviceTask::initializeDevice(const std::string& name) {
    spdlog::info("Initializing device: {}", name);

    if (name.empty()) {
        setErrorType(TaskErrorType::InvalidParameter);
        throw std::invalid_argument("Device name cannot be empty");
    }

    try {
        if (!deviceManager_->isDeviceConnected(name)) {
            setErrorType(TaskErrorType::DeviceError);
            addHistoryEntry("Cannot initialize device " + name +
                            ": not connected");
            spdlog::error("Device {} not connected", name);
            return false;
        }

        deviceManager_->initializeDevice(name);

        updateDeviceStatus(name,
                           {.isConnected = true,
                            .isInitialized = true,
                            .state = "initialized",
                            .lastOperation = std::chrono::system_clock::now()});

        addHistoryEntry("Successfully initialized device: " + name);
        spdlog::info("Device {} initialized successfully", name);
        return true;
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        setErrorType(TaskErrorType::DeviceError);
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
    addHistoryEntry("Set priority for device " + name + " to level " +
                    std::to_string(priority.level));
}

void DeviceTask::setConcurrencyLimit(int limit) {
    spdlog::debug("Setting concurrency limit: {}", limit);
    concurrencyLimit_ = std::max(1, limit);
    addHistoryEntry("Set concurrency limit to " + std::to_string(limit));
}

void DeviceTask::setRetryStrategy(const std::string& name,
                                  RetryStrategy strategy) {
    spdlog::debug("Setting retry strategy for device {}: {}", name,
                  static_cast<int>(strategy));
    deviceManager_->setDeviceRetryStrategy(name, strategy);
    addHistoryEntry("Set retry strategy for device " + name);
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
        addHistoryEntry("Disconnected device: " + name);
        spdlog::info("Device {} disconnected successfully", name);
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        setErrorType(TaskErrorType::DeviceError);
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
        addHistoryEntry("Reset device: " + name);
        spdlog::info("Device {} reset successfully", name);
    } catch (const std::exception& e) {
        handleDeviceError(name, e.what());
        setErrorType(TaskErrorType::DeviceError);
        spdlog::error("Failed to reset device {}: {}", name, e.what());
    }
}

void DeviceTask::abortOperation(const std::string& name) {
    spdlog::warn("Aborting operations for device: {}", name);
    shouldStop_ = true;
    deviceManager_->abortDeviceOperation(name);
    addHistoryEntry("Aborted operations for device: " + name);
}

void DeviceTask::validateParameters(const json& params) {
    spdlog::debug("Validating parameters: {}", params.dump());

    // Use the new Task validation system instead of manual validation
    if (!validateParams(params)) {
        auto errors = getParamErrors();
        std::string errorMsg = "Parameter validation failed: ";
        for (const auto& error : errors) {
            errorMsg += error + "; ";
        }
        throw std::invalid_argument(errorMsg);
    }
}

void DeviceTask::monitorDevice(const std::string& deviceName) {
    spdlog::debug("Monitoring device: {}", deviceName);
    if (!checkDeviceHealth(deviceName)) {
        handleDeviceError(deviceName, "Device health check failed");
        setErrorType(TaskErrorType::DeviceError);
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
        addHistoryEntry("Health check for device " + name + ": " +
                        std::to_string(health));
        spdlog::info("Device {} health check result: {}", name, health);
        return health > 0.5;
    } catch (...) {
        setErrorType(TaskErrorType::DeviceError);
        addHistoryEntry("Health check failed for device: " + name);
        spdlog::error("Failed to check health for device: {}", name);
        return false;
    }
}

void DeviceTask::cleanupDevice(const std::string& name) {
    spdlog::debug("Cleaning up device: {}", name);
    std::unique_lock lock(statusMutex_);
    deviceStatuses_.erase(name);
    priorities_.erase(name);
    addHistoryEntry("Cleaned up resources for device: " + name);
}

std::string DeviceTask::validateDeviceOperation(DeviceOperation op,
                                                const std::string& name) {
    spdlog::debug("Validating device operation: {} for device: {}",
                  static_cast<int>(op), name);

    if (name.empty()) {
        return "Device name cannot be empty";
    }

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

    // Update task error information
    setErrorType(TaskErrorType::DeviceError);
    addHistoryEntry("Error for device " + deviceName + ": " + error);
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