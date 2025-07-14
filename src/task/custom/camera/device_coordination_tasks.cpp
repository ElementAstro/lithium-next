#include "device_coordination_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

#define MOCK_DEVICES

namespace lithium::task::task {

// ==================== Mock Device Management System ====================
#ifdef MOCK_DEVICES
class MockDeviceManager {
public:
    struct DeviceInfo {
        std::string name;
        std::string type;
        bool connected = false;
        bool healthy = true;
        double temperature = 20.0;
        json properties;
        std::chrono::time_point<std::chrono::steady_clock> lastUpdate;
    };

    static auto getInstance() -> MockDeviceManager& {
        static MockDeviceManager instance;
        return instance;
    }

    auto scanDevices() -> std::vector<std::string> {
        std::vector<std::string> devices = {
            "Camera_ZWO_ASI294MC", "Telescope_Celestron_CGX",
            "Focuser_ZWO_EAF", "FilterWheel_ZWO_EFW",
            "Guider_ZWO_ASI120MM", "GPS_Device"
        };

        for (const auto& device : devices) {
            if (devices_.find(device) == devices_.end()) {
                DeviceInfo info;
                info.name = device;
                info.type = device.substr(0, device.find('_'));
                info.lastUpdate = std::chrono::steady_clock::now();
                devices_[device] = info;
            }
        }

        spdlog::info("Device scan found {} devices", devices.size());
        return devices;
    }

    auto connectDevice(const std::string& deviceName) -> bool {
        auto it = devices_.find(deviceName);
        if (it == devices_.end()) {
            return false;
        }

        // Simulate connection time
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        it->second.connected = true;
        it->second.lastUpdate = std::chrono::steady_clock::now();
        spdlog::info("Connected to device: {}", deviceName);
        return true;
    }

    auto disconnectDevice(const std::string& deviceName) -> bool {
        auto it = devices_.find(deviceName);
        if (it == devices_.end()) {
            return false;
        }

        it->second.connected = false;
        spdlog::info("Disconnected from device: {}", deviceName);
        return true;
    }

    auto getDeviceHealth(const std::string& deviceName) -> json {
        auto it = devices_.find(deviceName);
        if (it == devices_.end()) {
            return json{{"error", "Device not found"}};
        }

        auto& device = it->second;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - device.lastUpdate).count();

        // Simulate some health issues occasionally
        if (elapsed > 60) {
            device.healthy = false;
        }

        return json{
            {"name", device.name},
            {"type", device.type},
            {"connected", device.connected},
            {"healthy", device.healthy},
            {"temperature", device.temperature},
            {"last_update", elapsed},
            {"properties", device.properties}
        };
    }

    auto getAllDevices() const -> const std::unordered_map<std::string, DeviceInfo>& {
        return devices_;
    }

    auto updateDeviceTemperature(const std::string& deviceName, double temp) -> void {
        auto it = devices_.find(deviceName);
        if (it != devices_.end()) {
            it->second.temperature = temp;
            it->second.lastUpdate = std::chrono::steady_clock::now();
        }
    }

    auto getFilterOffsets() const -> json {
        return json{
            {"Luminance", 0},
            {"Red", -50},
            {"Green", -25},
            {"Blue", -75},
            {"Ha", 100},
            {"OIII", 150},
            {"SII", 125}
        };
    }

    auto setFilterOffset(const std::string& filter, int offset) -> void {
        filterOffsets_[filter] = offset;
        spdlog::info("Set filter offset for {}: {}", filter, offset);
    }

private:
    std::unordered_map<std::string, DeviceInfo> devices_;
    std::unordered_map<std::string, int> filterOffsets_;
};
#endif

// ==================== DeviceScanConnectTask Implementation ====================

auto DeviceScanConnectTask::taskName() -> std::string {
    return "DeviceScanConnect";
}

void DeviceScanConnectTask::execute(const json& params) {
    try {
        validateScanParameters(params);

        bool scanOnly = params.value("scan_only", false);
        bool autoConnect = params.value("auto_connect", true);
        std::vector<std::string> deviceTypes;

        if (params.contains("device_types")) {
            deviceTypes = params["device_types"].get<std::vector<std::string>>();
        } else {
            deviceTypes = {"Camera", "Telescope", "Focuser", "FilterWheel", "Guider"};
        }

        spdlog::info("Device scan starting for types: {}", json(deviceTypes).dump());

#ifdef MOCK_DEVICES
        auto& deviceManager = MockDeviceManager::getInstance();

        // Scan for devices
        auto foundDevices = deviceManager.scanDevices();
        spdlog::info("Found {} devices during scan", foundDevices.size());

        if (!scanOnly && autoConnect) {
            int connectedCount = 0;
            for (const auto& device : foundDevices) {
                // Check if device type is in requested types
                bool shouldConnect = false;
                for (const auto& type : deviceTypes) {
                    if (device.find(type) != std::string::npos) {
                        shouldConnect = true;
                        break;
                    }
                }

                if (shouldConnect) {
                    if (deviceManager.connectDevice(device)) {
                        connectedCount++;
                    }
                }
            }

            spdlog::info("Connected to {}/{} devices", connectedCount, foundDevices.size());
        }
#endif

        LOG_F(INFO, "Device scan and connect completed successfully");

    } catch (const std::exception& e) {
        handleConnectionError(*this, e);
        throw;
    }
}

auto DeviceScanConnectTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<DeviceScanConnectTask>("DeviceScanConnect",
        [](const json& params) {
            DeviceScanConnectTask taskInstance("DeviceScanConnect", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void DeviceScanConnectTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "scan_only",
        .type = "boolean",
        .required = false,
        .defaultValue = false,
        .description = "Only scan devices, don't connect"
    });

    task.addParameter({
        .name = "auto_connect",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Automatically connect to found devices"
    });

    task.addParameter({
        .name = "device_types",
        .type = "array",
        .required = false,
        .defaultValue = json::array({"Camera", "Telescope", "Focuser", "FilterWheel"}),
        .description = "Types of devices to scan for"
    });
}

void DeviceScanConnectTask::validateScanParameters(const json& params) {
    if (params.contains("device_types")) {
        if (!params["device_types"].is_array()) {
            throw atom::error::InvalidArgument("device_types must be an array");
        }

        std::vector<std::string> validTypes = {"Camera", "Telescope", "Focuser", "FilterWheel", "Guider", "GPS"};
        for (const auto& type : params["device_types"]) {
            if (std::find(validTypes.begin(), validTypes.end(), type.get<std::string>()) == validTypes.end()) {
                throw atom::error::InvalidArgument("Invalid device type: " + type.get<std::string>());
            }
        }
    }
}

void DeviceScanConnectTask::handleConnectionError(Task& task, const std::exception& e) {
    task.setErrorType(TaskErrorType::DeviceError);
    spdlog::error("Device scan/connect error: {}", e.what());
}

// ==================== DeviceHealthMonitorTask Implementation ====================

auto DeviceHealthMonitorTask::taskName() -> std::string {
    return "DeviceHealthMonitor";
}

void DeviceHealthMonitorTask::execute(const json& params) {
    try {
        validateHealthParameters(params);

        int duration = params.value("duration", 60);
        int interval = params.value("interval", 10);
        bool alertOnFailure = params.value("alert_on_failure", true);

        spdlog::info("Starting device health monitoring for {} seconds", duration);

#ifdef MOCK_DEVICES
        auto& deviceManager = MockDeviceManager::getInstance();

        auto startTime = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count() < duration) {

            json healthReport = json::object();

            for (const auto& [deviceName, deviceInfo] : deviceManager.getAllDevices()) {
                auto health = deviceManager.getDeviceHealth(deviceName);
                healthReport[deviceName] = health;

                if (alertOnFailure && (!health["connected"].get<bool>() || !health["healthy"].get<bool>())) {
                    spdlog::warn("Device health alert: {} is not healthy", deviceName);
                }
            }

            spdlog::debug("Health check completed: {}", healthReport.dump(2));

            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
#endif

        LOG_F(INFO, "Device health monitoring completed");

    } catch (const std::exception& e) {
        spdlog::error("DeviceHealthMonitorTask failed: {}", e.what());
        throw;
    }
}

auto DeviceHealthMonitorTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<DeviceHealthMonitorTask>("DeviceHealthMonitor",
        [](const json& params) {
            DeviceHealthMonitorTask taskInstance("DeviceHealthMonitor", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void DeviceHealthMonitorTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "duration",
        .type = "integer",
        .required = false,
        .defaultValue = 60,
        .description = "Monitoring duration in seconds"
    });

    task.addParameter({
        .name = "interval",
        .type = "integer",
        .required = false,
        .defaultValue = 10,
        .description = "Check interval in seconds"
    });

    task.addParameter({
        .name = "alert_on_failure",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Generate alerts for device failures"
    });
}

void DeviceHealthMonitorTask::validateHealthParameters(const json& params) {
    if (params.contains("duration")) {
        int duration = params["duration"];
        if (duration < 10 || duration > 86400) {
            throw atom::error::InvalidArgument("Duration must be between 10 and 86400 seconds");
        }
    }

    if (params.contains("interval")) {
        int interval = params["interval"];
        if (interval < 1 || interval > 3600) {
            throw atom::error::InvalidArgument("Interval must be between 1 and 3600 seconds");
        }
    }
}

// ==================== AutoFilterSequenceTask Implementation ====================

auto AutoFilterSequenceTask::taskName() -> std::string {
    return "AutoFilterSequence";
}

void AutoFilterSequenceTask::execute(const json& params) {
    try {
        validateFilterSequenceParameters(params);

        std::vector<json> filterSequence = params["filter_sequence"];
        bool autoFocus = params.value("auto_focus_per_filter", true);
        int repetitions = params.value("repetitions", 1);

        spdlog::info("Starting auto filter sequence with {} filters, {} repetitions",
                    filterSequence.size(), repetitions);

        for (int rep = 0; rep < repetitions; ++rep) {
            spdlog::info("Filter sequence repetition {}/{}", rep + 1, repetitions);

            for (size_t i = 0; i < filterSequence.size(); ++i) {
                const auto& filterConfig = filterSequence[i];

                std::string filterName = filterConfig["filter"];
                int exposureCount = filterConfig["count"];
                double exposureTime = filterConfig["exposure"];

                spdlog::info("Filter {}: {} x {:.1f}s exposures",
                           filterName, exposureCount, exposureTime);

                // Change filter (mock implementation)
                spdlog::info("Changing to filter: {}", filterName);
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));

                // Auto-focus if enabled
                if (autoFocus) {
                    spdlog::info("Performing autofocus for filter: {}", filterName);
                    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
                }

                // Take exposures
                for (int exp = 0; exp < exposureCount; ++exp) {
                    spdlog::info("Taking exposure {}/{} with filter {}",
                               exp + 1, exposureCount, filterName);
                    std::this_thread::sleep_for(std::chrono::milliseconds(
                        static_cast<int>(exposureTime * 100))); // Simulate exposure
                }
            }
        }

        LOG_F(INFO, "Auto filter sequence completed successfully");

    } catch (const std::exception& e) {
        spdlog::error("AutoFilterSequenceTask failed: {}", e.what());
        throw;
    }
}

auto AutoFilterSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<AutoFilterSequenceTask>("AutoFilterSequence",
        [](const json& params) {
            AutoFilterSequenceTask taskInstance("AutoFilterSequence", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void AutoFilterSequenceTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "filter_sequence",
        .type = "array",
        .required = true,
        .defaultValue = json::array(),
        .description = "Array of filter configurations"
    });

    task.addParameter({
        .name = "auto_focus_per_filter",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Perform autofocus when changing filters"
    });

    task.addParameter({
        .name = "repetitions",
        .type = "integer",
        .required = false,
        .defaultValue = 1,
        .description = "Number of times to repeat the sequence"
    });
}

void AutoFilterSequenceTask::validateFilterSequenceParameters(const json& params) {
    if (!params.contains("filter_sequence")) {
        throw atom::error::InvalidArgument("Missing required parameter: filter_sequence");
    }

    auto sequence = params["filter_sequence"];
    if (!sequence.is_array() || sequence.empty()) {
        throw atom::error::InvalidArgument("filter_sequence must be a non-empty array");
    }

    for (const auto& filterConfig : sequence) {
        if (!filterConfig.contains("filter") || !filterConfig.contains("count") ||
            !filterConfig.contains("exposure")) {
            throw atom::error::InvalidArgument("Each filter config must have filter, count, and exposure");
        }
    }
}

// ==================== FocusFilterOptimizationTask Implementation ====================

auto FocusFilterOptimizationTask::taskName() -> std::string {
    return "FocusFilterOptimization";
}

void FocusFilterOptimizationTask::execute(const json& params) {
    try {
        validateFocusFilterParameters(params);

        std::vector<std::string> filters = params["filters"];
        double exposureTime = params.value("exposure_time", 3.0);
        bool saveOffsets = params.value("save_offsets", true);

        spdlog::info("Optimizing focus offsets for {} filters", filters.size());

#ifdef MOCK_DEVICES
        auto& deviceManager = MockDeviceManager::getInstance();

        // Start with luminance as reference
        int referencePosition = 25000;
        json focusOffsets;

        for (const auto& filter : filters) {
            spdlog::info("Measuring focus offset for filter: {}", filter);

            // Change to filter
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            // Perform autofocus
            spdlog::info("Performing autofocus with filter: {}", filter);
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));

            // Simulate focus position measurement
            int focusPosition = referencePosition;
            if (filter == "Red") focusPosition -= 50;
            else if (filter == "Green") focusPosition -= 25;
            else if (filter == "Blue") focusPosition -= 75;
            else if (filter == "Ha") focusPosition += 100;
            else if (filter == "OIII") focusPosition += 150;
            else if (filter == "SII") focusPosition += 125;

            int offset = focusPosition - referencePosition;
            focusOffsets[filter] = offset;

            if (saveOffsets) {
                deviceManager.setFilterOffset(filter, offset);
            }

            spdlog::info("Filter {} focus offset: {}", filter, offset);
        }

        spdlog::info("Focus filter optimization completed: {}", focusOffsets.dump(2));
#endif

        LOG_F(INFO, "Focus filter optimization completed");

    } catch (const std::exception& e) {
        spdlog::error("FocusFilterOptimizationTask failed: {}", e.what());
        throw;
    }
}

auto FocusFilterOptimizationTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<FocusFilterOptimizationTask>("FocusFilterOptimization",
        [](const json& params) {
            FocusFilterOptimizationTask taskInstance("FocusFilterOptimization", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void FocusFilterOptimizationTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "filters",
        .type = "array",
        .required = true,
        .defaultValue = json::array({"Luminance", "Red", "Green", "Blue"}),
        .description = "List of filters to optimize"
    });

    task.addParameter({
        .name = "exposure_time",
        .type = "number",
        .required = false,
        .defaultValue = 3.0,
        .description = "Exposure time for focus measurements"
    });

    task.addParameter({
        .name = "save_offsets",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Save measured offsets to device configuration"
    });
}

void FocusFilterOptimizationTask::validateFocusFilterParameters(const json& params) {
    if (!params.contains("filters")) {
        throw atom::error::InvalidArgument("Missing required parameter: filters");
    }

    auto filters = params["filters"];
    if (!filters.is_array() || filters.empty()) {
        throw atom::error::InvalidArgument("filters must be a non-empty array");
    }
}

// ==================== IntelligentAutoFocusTask Implementation ====================

auto IntelligentAutoFocusTask::taskName() -> std::string {
    return "IntelligentAutoFocus";
}

void IntelligentAutoFocusTask::execute(const json& params) {
    try {
        validateIntelligentFocusParameters(params);

        bool useTemperatureCompensation = params.value("temperature_compensation", true);
        bool useFilterOffsets = params.value("filter_offsets", true);
        std::string currentFilter = params.value("current_filter", "Luminance");
        double exposureTime = params.value("exposure_time", 3.0);

        spdlog::info("Intelligent autofocus with temp compensation: {}, filter offsets: {}",
                    useTemperatureCompensation, useFilterOffsets);

#ifdef MOCK_DEVICES
        auto& deviceManager = MockDeviceManager::getInstance();

        // Get current temperature
        double currentTemp = 15.0;  // Simulate current temperature
        double lastFocusTemp = 20.0;  // Last focus temperature

        int basePosition = 25000;
        int targetPosition = basePosition;

        // Apply temperature compensation
        if (useTemperatureCompensation) {
            double tempDelta = currentTemp - lastFocusTemp;
            int tempOffset = static_cast<int>(tempDelta * -10);  // -10 steps per degree
            targetPosition += tempOffset;
            spdlog::info("Temperature compensation: {} steps for {:.1f}°C change",
                        tempOffset, tempDelta);
        }

        // Apply filter offset
        if (useFilterOffsets) {
            auto offsets = deviceManager.getFilterOffsets();
            if (offsets.contains(currentFilter)) {
                int filterOffset = offsets[currentFilter];
                targetPosition += filterOffset;
                spdlog::info("Filter offset for {}: {} steps", currentFilter, filterOffset);
            }
        }

        spdlog::info("Moving focuser to intelligent position: {}", targetPosition);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        // Perform fine autofocus
        spdlog::info("Performing fine autofocus adjustment");
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));

        spdlog::info("Intelligent autofocus completed at position: {}", targetPosition);
#endif

        LOG_F(INFO, "Intelligent autofocus completed");

    } catch (const std::exception& e) {
        spdlog::error("IntelligentAutoFocusTask failed: {}", e.what());
        throw;
    }
}

auto IntelligentAutoFocusTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<IntelligentAutoFocusTask>("IntelligentAutoFocus",
        [](const json& params) {
            IntelligentAutoFocusTask taskInstance("IntelligentAutoFocus", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void IntelligentAutoFocusTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "temperature_compensation",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Use temperature compensation"
    });

    task.addParameter({
        .name = "filter_offsets",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Use filter-specific focus offsets"
    });

    task.addParameter({
        .name = "current_filter",
        .type = "string",
        .required = false,
        .defaultValue = "Luminance",
        .description = "Currently installed filter"
    });

    task.addParameter({
        .name = "exposure_time",
        .type = "number",
        .required = false,
        .defaultValue = 3.0,
        .description = "Exposure time for focus measurement"
    });
}

void IntelligentAutoFocusTask::validateIntelligentFocusParameters(const json& params) {
    if (params.contains("exposure_time")) {
        double exposure = params["exposure_time"];
        if (exposure < 0.1 || exposure > 60.0) {
            throw atom::error::InvalidArgument("Exposure time must be between 0.1 and 60 seconds");
        }
    }
}

// ==================== CoordinatedShutdownTask Implementation ====================

auto CoordinatedShutdownTask::taskName() -> std::string {
    return "CoordinatedShutdown";
}

void CoordinatedShutdownTask::execute(const json& params) {
    try {
        bool parkTelescope = params.value("park_telescope", true);
        bool stopCooling = params.value("stop_cooling", true);
        bool disconnectDevices = params.value("disconnect_devices", true);

        spdlog::info("Starting coordinated shutdown sequence");

        // 1. Stop any ongoing exposures
        spdlog::info("Stopping ongoing exposures...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // 2. Stop guiding
        spdlog::info("Stopping autoguiding...");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // 3. Park telescope
        if (parkTelescope) {
            spdlog::info("Parking telescope...");
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        }

        // 4. Stop camera cooling
        if (stopCooling) {
            spdlog::info("Disabling camera cooling...");
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }

        // 5. Disconnect devices
        if (disconnectDevices) {
#ifdef MOCK_DEVICES
            auto& deviceManager = MockDeviceManager::getInstance();
            for (const auto& [deviceName, deviceInfo] : deviceManager.getAllDevices()) {
                if (deviceInfo.connected) {
                    deviceManager.disconnectDevice(deviceName);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }
#endif
        }

        spdlog::info("Coordinated shutdown completed successfully");
        LOG_F(INFO, "Coordinated shutdown completed");

    } catch (const std::exception& e) {
        spdlog::error("CoordinatedShutdownTask failed: {}", e.what());
        throw;
    }
}

auto CoordinatedShutdownTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<CoordinatedShutdownTask>("CoordinatedShutdown",
        [](const json& params) {
            CoordinatedShutdownTask taskInstance("CoordinatedShutdown", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void CoordinatedShutdownTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "park_telescope",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Park telescope during shutdown"
    });

    task.addParameter({
        .name = "stop_cooling",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Stop camera cooling during shutdown"
    });

    task.addParameter({
        .name = "disconnect_devices",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Disconnect all devices during shutdown"
    });
}

// ==================== EnvironmentMonitorTask Implementation ====================

auto EnvironmentMonitorTask::taskName() -> std::string {
    return "EnvironmentMonitor";
}

void EnvironmentMonitorTask::execute(const json& params) {
    try {
        validateEnvironmentParameters(params);

        int duration = params.value("duration", 300);
        int interval = params.value("interval", 30);
        double maxWindSpeed = params.value("max_wind_speed", 10.0);
        double maxHumidity = params.value("max_humidity", 85.0);

        spdlog::info("Starting environment monitoring for {} seconds", duration);

        auto startTime = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count() < duration) {

            // Simulate environmental readings
            double temperature = 15.0 + (rand() % 10 - 5);
            double humidity = 50.0 + (rand() % 30);
            double windSpeed = 3.0 + (rand() % 8);
            double pressure = 1013.25 + (rand() % 20 - 10);

            json envData = {
                {"temperature", temperature},
                {"humidity", humidity},
                {"wind_speed", windSpeed},
                {"pressure", pressure},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()}
            };

            spdlog::info("Environment: T={:.1f}°C, H={:.1f}%, W={:.1f}m/s, P={:.1f}hPa",
                        temperature, humidity, windSpeed, pressure);

            // Check alert conditions
            if (windSpeed > maxWindSpeed) {
                spdlog::warn("Wind speed alert: {:.1f} m/s exceeds limit {:.1f} m/s",
                           windSpeed, maxWindSpeed);
            }

            if (humidity > maxHumidity) {
                spdlog::warn("Humidity alert: {:.1f}% exceeds limit {:.1f}%",
                           humidity, maxHumidity);
            }

            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }

        LOG_F(INFO, "Environment monitoring completed");

    } catch (const std::exception& e) {
        spdlog::error("EnvironmentMonitorTask failed: {}", e.what());
        throw;
    }
}

auto EnvironmentMonitorTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<EnvironmentMonitorTask>("EnvironmentMonitor",
        [](const json& params) {
            EnvironmentMonitorTask taskInstance("EnvironmentMonitor", nullptr);
            taskInstance.execute(params);
        });

    defineParameters(*task);
    return task;
}

void EnvironmentMonitorTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "duration",
        .type = "integer",
        .required = false,
        .defaultValue = 300,
        .description = "Monitoring duration in seconds"
    });

    task.addParameter({
        .name = "interval",
        .type = "integer",
        .required = false,
        .defaultValue = 30,
        .description = "Check interval in seconds"
    });

    task.addParameter({
        .name = "max_wind_speed",
        .type = "number",
        .required = false,
        .defaultValue = 10.0,
        .description = "Maximum safe wind speed (m/s)"
    });

    task.addParameter({
        .name = "max_humidity",
        .type = "number",
        .required = false,
        .defaultValue = 85.0,
        .description = "Maximum safe humidity (%)"
    });
}

void EnvironmentMonitorTask::validateEnvironmentParameters(const json& params) {
    if (params.contains("duration")) {
        int duration = params["duration"];
        if (duration < 60 || duration > 86400) {
            throw atom::error::InvalidArgument("Duration must be between 60 and 86400 seconds");
        }
    }

    if (params.contains("max_wind_speed")) {
        double windSpeed = params["max_wind_speed"];
        if (windSpeed < 0.0 || windSpeed > 50.0) {
            throw atom::error::InvalidArgument("Max wind speed must be between 0 and 50 m/s");
        }
    }
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register DeviceScanConnectTask
AUTO_REGISTER_TASK(
    DeviceScanConnectTask, "DeviceScanConnect",
    (TaskInfo{
        .name = "DeviceScanConnect",
        .description = "Scans for and connects to available astrophotography devices",
        .category = "Device",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"scan_only", json{{"type", "boolean"}}},
                       {"auto_connect", json{{"type", "boolean"}}},
                       {"device_types", json{{"type", "array"},
                                             {"items", json{{"type", "string"}}}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register DeviceHealthMonitorTask
AUTO_REGISTER_TASK(
    DeviceHealthMonitorTask, "DeviceHealthMonitor",
    (TaskInfo{
        .name = "DeviceHealthMonitor",
        .description = "Monitors health status of connected devices",
        .category = "Device",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"duration", json{{"type", "integer"},
                                         {"minimum", 10},
                                         {"maximum", 86400}}},
                       {"interval", json{{"type", "integer"},
                                         {"minimum", 1},
                                         {"maximum", 3600}}},
                       {"alert_on_failure", json{{"type", "boolean"}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register AutoFilterSequenceTask
AUTO_REGISTER_TASK(
    AutoFilterSequenceTask, "AutoFilterSequence",
    (TaskInfo{
        .name = "AutoFilterSequence",
        .description = "Automated multi-filter imaging sequence with filter wheel control",
        .category = "Sequence",
        .requiredParameters = {"filter_sequence"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"filter_sequence", json{{"type", "array"}}},
                       {"auto_focus_per_filter", json{{"type", "boolean"}}},
                       {"repetitions", json{{"type", "integer"},
                                            {"minimum", 1},
                                            {"maximum", 100}}}}}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure", "AutoFocus"}}));

// Register FocusFilterOptimizationTask
AUTO_REGISTER_TASK(
    FocusFilterOptimizationTask, "FocusFilterOptimization",
    (TaskInfo{
        .name = "FocusFilterOptimization",
        .description = "Measures and optimizes focus offsets for different filters",
        .category = "Focus",
        .requiredParameters = {"filters"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"filters", json{{"type", "array"}}},
                       {"exposure_time", json{{"type", "number"},
                                              {"minimum", 0.1},
                                              {"maximum", 60}}},
                       {"save_offsets", json{{"type", "boolean"}}}}}},
        .version = "1.0.0",
        .dependencies = {"AutoFocus"}}));

// Register IntelligentAutoFocusTask
AUTO_REGISTER_TASK(
    IntelligentAutoFocusTask, "IntelligentAutoFocus",
    (TaskInfo{
        .name = "IntelligentAutoFocus",
        .description = "Advanced autofocus with temperature compensation and filter offsets",
        .category = "Focus",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"temperature_compensation", json{{"type", "boolean"}}},
                       {"filter_offsets", json{{"type", "boolean"}}},
                       {"current_filter", json{{"type", "string"}}},
                       {"exposure_time", json{{"type", "number"},
                                              {"minimum", 0.1},
                                              {"maximum", 60}}}}}},
        .version = "1.0.0",
        .dependencies = {"AutoFocus"}}));

// Register CoordinatedShutdownTask
AUTO_REGISTER_TASK(
    CoordinatedShutdownTask, "CoordinatedShutdown",
    (TaskInfo{
        .name = "CoordinatedShutdown",
        .description = "Safely shuts down all devices in proper sequence",
        .category = "System",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"park_telescope", json{{"type", "boolean"}}},
                       {"stop_cooling", json{{"type", "boolean"}}},
                       {"disconnect_devices", json{{"type", "boolean"}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register EnvironmentMonitorTask
AUTO_REGISTER_TASK(
    EnvironmentMonitorTask, "EnvironmentMonitor",
    (TaskInfo{
        .name = "EnvironmentMonitor",
        .description = "Monitors environmental conditions and generates alerts",
        .category = "Safety",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"duration", json{{"type", "integer"},
                                         {"minimum", 60},
                                         {"maximum", 86400}}},
                       {"interval", json{{"type", "integer"},
                                         {"minimum", 10},
                                         {"maximum", 3600}}},
                       {"max_wind_speed", json{{"type", "number"},
                                               {"minimum", 0},
                                               {"maximum", 50}}},
                       {"max_humidity", json{{"type", "number"},
                                             {"minimum", 0},
                                             {"maximum", 100}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

}  // namespace
