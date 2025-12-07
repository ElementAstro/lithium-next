/**
 * @file observatory_tasks.cpp
 * @brief Implementation of observatory tasks
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "observatory_tasks.hpp"
#include <random>
#include <thread>

namespace lithium::task::observatory {

// ============================================================================
// WeatherMonitorTask Implementation
// ============================================================================

void WeatherMonitorTask::setupParameters() {
    addParamDefinition("check_interval", "integer", false, 60,
                       "Check interval in seconds");
    addParamDefinition("duration", "integer", false, 0,
                       "Monitor duration (0=continuous)");
    addParamDefinition("wind_threshold", "number", false, 40.0,
                       "Wind speed threshold (km/h)");
    addParamDefinition("humidity_threshold", "number", false, 85.0,
                       "Humidity threshold (%)");
    addParamDefinition("rain_threshold", "boolean", false, true,
                       "Stop on any rain detection");
}

void WeatherMonitorTask::executeImpl(const json& params) {
    int checkInterval = params.value("check_interval", 60);
    int duration = params.value("duration", 0);
    double windThreshold = params.value("wind_threshold", 40.0);
    double humidityThreshold = params.value("humidity_threshold", 85.0);

    logProgress("Starting weather monitoring");
    logProgress("Thresholds - Wind: " + std::to_string(windThreshold) +
                " km/h, Humidity: " + std::to_string(humidityThreshold) + "%");

    int elapsed = 0;
    while (shouldContinue() && (duration == 0 || elapsed < duration)) {
        SafetyStatus status = checkWeather();

        logProgress("Weather check: " +
                    std::string(status.isSafe ? "SAFE" : "UNSAFE"));
        logProgress("Temp: " + std::to_string(status.temperature) + "°C, " +
                    "Humidity: " + std::to_string(status.humidity) + "%, " +
                    "Wind: " + std::to_string(status.windSpeed) + " km/h");

        if (!status.isSafe) {
            logProgress("ALERT: Unsafe conditions detected - " + status.reason);
            // In real implementation, would trigger safety shutdown
        }

        if (duration == 0) {
            // Continuous mode - just do one check for simulation
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::seconds(std::min(checkInterval, 1)));
        elapsed += checkInterval;
    }

    logProgress("Weather monitoring complete", 1.0);
}

SafetyStatus WeatherMonitorTask::checkWeather() {
    // Simulate weather reading
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> tempDist(10.0, 25.0);
    std::uniform_real_distribution<> humidityDist(40.0, 80.0);
    std::uniform_real_distribution<> windDist(0.0, 30.0);

    SafetyStatus status;
    status.temperature = tempDist(gen);
    status.humidity = humidityDist(gen);
    status.windSpeed = windDist(gen);
    status.cloudCover = 20.0;
    status.weather = WeatherCondition::Clear;
    status.isSafe = true;
    status.reason = "All conditions nominal";

    return status;
}

// ============================================================================
// CloudDetectionTask Implementation
// ============================================================================

void CloudDetectionTask::setupParameters() {
    addParamDefinition("threshold", "number", false, 50.0,
                       "Cloud cover threshold (%)");
    addParamDefinition("exposure", "number", false, 1.0,
                       "Sky quality meter exposure");
}

void CloudDetectionTask::executeImpl(const json& params) {
    double threshold = params.value("threshold", 50.0);

    logProgress("Checking cloud cover");

    double cloudCover = measureCloudCover();

    logProgress("Cloud cover: " + std::to_string(cloudCover) + "%");

    if (cloudCover > threshold) {
        logProgress("WARNING: Cloud cover exceeds threshold");
    } else {
        logProgress("Cloud cover within acceptable range");
    }

    logProgress("Cloud detection complete", 1.0);
}

double CloudDetectionTask::measureCloudCover() {
    // Simulate cloud measurement
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dist(0.0, 40.0);

    return dist(gen);
}

// ============================================================================
// SafetyShutdownTask Implementation
// ============================================================================

void SafetyShutdownTask::setupParameters() {
    addParamDefinition("reason", "string", false, "Manual shutdown",
                       "Shutdown reason");
    addParamDefinition("park_mount", "boolean", false, true, "Park mount");
    addParamDefinition("close_dome", "boolean", false, true, "Close dome/roof");
    addParamDefinition("warm_camera", "boolean", false, true, "Warm up camera");
    addParamDefinition("emergency", "boolean", false, false,
                       "Emergency shutdown (faster)");
}

void SafetyShutdownTask::executeImpl(const json& params) {
    std::string reason = params.value("reason", "Manual shutdown");
    bool parkMount = params.value("park_mount", true);
    bool closeDome = params.value("close_dome", true);
    bool warmCamera = params.value("warm_camera", true);
    bool emergency = params.value("emergency", false);

    logProgress("INITIATING SAFETY SHUTDOWN: " + reason);

    // Step 1: Stop imaging
    logProgress("Stopping imaging...", 0.1);
    stopImaging();

    // Step 2: Stop guiding
    logProgress("Stopping autoguiding...", 0.2);
    stopGuiding();

    // Step 3: Park mount
    if (parkMount) {
        logProgress("Parking mount...", 0.4);
        this->parkMount();
    }

    // Step 4: Close dome
    if (closeDome) {
        logProgress("Closing dome...", 0.6);
        this->closeDome();
    }

    // Step 5: Warm camera (if not emergency)
    if (warmCamera && !emergency) {
        logProgress("Warming camera...", 0.8);
        this->warmCamera();
    }

    logProgress("Safety shutdown complete", 1.0);
}

void SafetyShutdownTask::stopImaging() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

void SafetyShutdownTask::stopGuiding() {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

void SafetyShutdownTask::parkMount() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void SafetyShutdownTask::closeDome() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

void SafetyShutdownTask::warmCamera() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

// ============================================================================
// ObservatoryStartupTask Implementation
// ============================================================================

void ObservatoryStartupTask::setupParameters() {
    addParamDefinition("unpark_mount", "boolean", false, true, "Unpark mount");
    addParamDefinition("open_dome", "boolean", false, true, "Open dome/roof");
    addParamDefinition("cool_camera", "boolean", false, true, "Cool camera");
    addParamDefinition("target_temp", "number", false, -10.0,
                       "Camera target temperature");
    addParamDefinition("safety_check", "boolean", false, true,
                       "Perform safety check first");
}

void ObservatoryStartupTask::executeImpl(const json& params) {
    bool unparkMount = params.value("unpark_mount", true);
    bool openDome = params.value("open_dome", true);
    bool coolCamera = params.value("cool_camera", true);
    double targetTemp = params.value("target_temp", -10.0);
    bool safetyCheck = params.value("safety_check", true);

    logProgress("Starting observatory startup sequence");

    // Safety check first
    if (safetyCheck) {
        logProgress("Performing safety check...", 0.1);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        logProgress("Safety check passed");
    }

    // Open dome
    if (openDome) {
        logProgress("Opening dome...", 0.3);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    // Unpark mount
    if (unparkMount) {
        logProgress("Unparking mount...", 0.5);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Cool camera
    if (coolCamera) {
        logProgress("Cooling camera to " + std::to_string(targetTemp) + "°C...",
                    0.7);
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    logProgress("Observatory startup complete", 1.0);
}

// ============================================================================
// DomeControlTask Implementation
// ============================================================================

void DomeControlTask::setupParameters() {
    addParamDefinition("action", "string", true, nullptr,
                       "Action (open/close/goto/slave)");
    addParamDefinition("azimuth", "number", false, nullptr,
                       "Target azimuth for goto");
    addParamDefinition("slave_enable", "boolean", false, nullptr,
                       "Enable/disable slaving");
}

void DomeControlTask::executeImpl(const json& params) {
    auto actionResult = ParamValidator::required(params, "action");
    if (!actionResult) {
        throw std::invalid_argument("Action is required");
    }

    std::string action = params["action"].get<std::string>();

    if (action == "open") {
        logProgress("Opening dome shutter");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        logProgress("Dome shutter open", 1.0);
    } else if (action == "close") {
        logProgress("Closing dome shutter");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        logProgress("Dome shutter closed", 1.0);
    } else if (action == "goto") {
        if (!params.contains("azimuth")) {
            throw std::invalid_argument("Azimuth required for goto action");
        }
        double azimuth = params["azimuth"].get<double>();
        logProgress("Rotating dome to azimuth " + std::to_string(azimuth) +
                    "°");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        logProgress("Dome at target azimuth", 1.0);
    } else if (action == "slave") {
        bool enable = params.value("slave_enable", true);
        logProgress(enable ? "Enabling dome slaving"
                           : "Disabling dome slaving");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        logProgress(
            "Dome slaving " + std::string(enable ? "enabled" : "disabled"),
            1.0);
    } else {
        throw std::invalid_argument("Unknown action: " + action);
    }
}

// ============================================================================
// FlatPanelTask Implementation
// ============================================================================

void FlatPanelTask::setupParameters() {
    addParamDefinition("action", "string", true, nullptr,
                       "Action (on/off/brightness)");
    addParamDefinition("brightness", "integer", false, 128,
                       "Brightness level (0-255)");
}

void FlatPanelTask::executeImpl(const json& params) {
    auto actionResult = ParamValidator::required(params, "action");
    if (!actionResult) {
        throw std::invalid_argument("Action is required");
    }

    std::string action = params["action"].get<std::string>();

    if (action == "on") {
        int brightness = params.value("brightness", 128);
        logProgress("Turning on flat panel at brightness " +
                    std::to_string(brightness));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        logProgress("Flat panel on", 1.0);
    } else if (action == "off") {
        logProgress("Turning off flat panel");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        logProgress("Flat panel off", 1.0);
    } else if (action == "brightness") {
        int brightness = params.value("brightness", 128);
        logProgress("Setting flat panel brightness to " +
                    std::to_string(brightness));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        logProgress("Brightness set", 1.0);
    } else {
        throw std::invalid_argument("Unknown action: " + action);
    }
}

// ============================================================================
// SafetyCheckTask Implementation
// ============================================================================

void SafetyCheckTask::setupParameters() {
    addParamDefinition("check_weather", "boolean", false, true,
                       "Check weather");
    addParamDefinition("check_devices", "boolean", false, true,
                       "Check device status");
    addParamDefinition("check_power", "boolean", false, true,
                       "Check power status");
}

void SafetyCheckTask::executeImpl(const json& params) {
    bool checkWeather = params.value("check_weather", true);
    bool checkDevices = params.value("check_devices", true);
    bool checkPower = params.value("check_power", true);

    logProgress("Performing safety check");

    SafetyStatus status = performCheck();

    if (checkWeather) {
        logProgress(
            "Weather: " + std::string(status.weather == WeatherCondition::Clear
                                          ? "Clear"
                                          : "Check conditions"),
            0.3);
    }

    if (checkDevices) {
        logProgress("Devices: Connected and responding", 0.6);
    }

    if (checkPower) {
        logProgress("Power: Normal", 0.9);
    }

    if (status.isSafe) {
        logProgress("Safety check PASSED - All systems nominal", 1.0);
    } else {
        logProgress("Safety check FAILED - " + status.reason);
        throw std::runtime_error("Safety check failed: " + status.reason);
    }
}

SafetyStatus SafetyCheckTask::performCheck() {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    SafetyStatus status;
    status.isSafe = true;
    status.weather = WeatherCondition::Clear;
    status.temperature = 15.0;
    status.humidity = 60.0;
    status.windSpeed = 10.0;
    status.cloudCover = 10.0;
    status.reason = "All systems nominal";

    return status;
}

}  // namespace lithium::task::observatory
