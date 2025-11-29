/**
 * @file safety_tasks.cpp
 * @brief Implementation of safety tasks
 */

#include "safety_tasks.hpp"
#include <thread>

namespace lithium::task::camera {

// WeatherMonitorTask
void WeatherMonitorTask::setupParameters() {
    addParamDefinition("check_interval", "number", false, 60.0,
                       "Check interval (seconds)");
    addParamDefinition("wind_limit", "number", false, 30.0,
                       "Wind speed limit (km/h)");
    addParamDefinition("humidity_limit", "number", false, 85.0,
                       "Humidity limit (%)");
    addParamDefinition("rain_threshold", "number", false, 0.1,
                       "Rain threshold");
    addParamDefinition("cloud_limit", "number", false, 50.0,
                       "Cloud cover limit (%)");
    addParamDefinition("action_on_unsafe", "string", false, "park",
                       "Action on unsafe (park/close/alert)");
}

void WeatherMonitorTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
}

void WeatherMonitorTask::executeImpl(const json& params) {
    double windLimit = params.value("wind_limit", 30.0);
    double humidityLimit = params.value("humidity_limit", 85.0);

    logProgress("Checking weather conditions");

    // Simulated weather readings
    double wind = 12.5;
    double humidity = 65.0;
    double cloudCover = 20.0;
    bool rain = false;

    logProgress("Wind: " + std::to_string(wind) +
                " km/h (limit: " + std::to_string(windLimit) + ")");
    logProgress("Humidity: " + std::to_string(humidity) +
                "% (limit: " + std::to_string(humidityLimit) + ")");
    logProgress("Cloud cover: " + std::to_string(cloudCover) + "%");
    logProgress("Rain: " + std::string(rain ? "Yes" : "No"));

    bool safe = wind < windLimit && humidity < humidityLimit && !rain;
    logProgress("Weather status: " + std::string(safe ? "SAFE" : "UNSAFE"));

    logProgress("Weather check complete", 1.0);
}

// CloudDetectionTask
void CloudDetectionTask::setupParameters() {
    addParamDefinition("exposure", "number", false, 10.0, "All-sky exposure");
    addParamDefinition("threshold", "number", false, 50.0,
                       "Cloud threshold (%)");
    addParamDefinition("analysis_method", "string", false, "brightness",
                       "Analysis method");
}

void CloudDetectionTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
}

void CloudDetectionTask::executeImpl(const json& params) {
    double threshold = params.value("threshold", 50.0);

    logProgress("Taking all-sky image");
    std::this_thread::sleep_for(std::chrono::seconds(1));

    logProgress("Analyzing cloud cover");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Simulated result
    double cloudCover = 25.0;
    bool clear = cloudCover < threshold;

    logProgress("Cloud cover: " + std::to_string(cloudCover) + "%");
    logProgress("Sky status: " + std::string(clear ? "CLEAR" : "CLOUDY"));

    logProgress("Cloud detection complete", 1.0);
}

// SafetyShutdownTask
void SafetyShutdownTask::setupParameters() {
    addParamDefinition("park_mount", "boolean", false, true, "Park mount");
    addParamDefinition("close_roof", "boolean", false, true, "Close roof/dome");
    addParamDefinition("warm_camera", "boolean", false, true, "Warm up camera");
    addParamDefinition("disconnect_devices", "boolean", false, false,
                       "Disconnect devices");
    addParamDefinition("emergency", "boolean", false, false,
                       "Emergency shutdown");
}

void SafetyShutdownTask::validateParams(const json& params) {
    CameraTaskBase::validateParams(params);
}

void SafetyShutdownTask::executeImpl(const json& params) {
    bool parkMount = params.value("park_mount", true);
    bool closeRoof = params.value("close_roof", true);
    bool warmCamera = params.value("warm_camera", true);
    bool emergency = params.value("emergency", false);

    if (emergency) {
        logProgress("EMERGENCY SHUTDOWN INITIATED");
    } else {
        logProgress("Starting safety shutdown sequence");
    }

    if (parkMount) {
        logProgress("Parking mount");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (closeRoof) {
        logProgress("Closing roof/dome");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (warmCamera) {
        logProgress("Warming up camera");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    logProgress("Safety shutdown complete", 1.0);
}

}  // namespace lithium::task::camera
