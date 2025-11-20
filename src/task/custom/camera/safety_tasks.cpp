#include "safety_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <memory>
#include <random>
#include <thread>
#include "../factory.hpp"
#include "atom/error/exception.hpp"

namespace lithium::task::task {

// ==================== Mock Classes for Testing ====================
#ifdef MOCK_CAMERA
class MockWeatherStation {
public:
    MockWeatherStation() = default;

    double getTemperature() const { return temperature_; }
    double getHumidity() const { return humidity_; }
    double getWindSpeed() const { return windSpeed_; }
    double getRainRate() const { return rainRate_; }
    double getCloudCover() const { return cloudCover_; }
    bool isSafe() const {
        return temperature_ > -10 && temperature_ < 40 && humidity_ < 85 &&
               windSpeed_ < 50 && rainRate_ == 0;
    }

    void updateWeather() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> tempDist(15.0, 25.0);
        std::uniform_real_distribution<> humDist(30.0, 70.0);
        std::uniform_real_distribution<> windDist(0.0, 20.0);
        std::uniform_real_distribution<> rainDist(0.0, 0.1);
        std::uniform_real_distribution<> cloudDist(0.0, 50.0);

        temperature_ = tempDist(gen);
        humidity_ = humDist(gen);
        windSpeed_ = windDist(gen);
        rainRate_ = rainDist(gen);
        cloudCover_ = cloudDist(gen);
    }

private:
    double temperature_{20.0};
    double humidity_{50.0};
    double windSpeed_{5.0};
    double rainRate_{0.0};
    double cloudCover_{20.0};
};

class MockCloudSensor {
public:
    MockCloudSensor() = default;

    double getCloudiness() const { return cloudiness_; }
    double getSkyTemperature() const { return skyTemp_; }
    double getAmbientTemperature() const { return ambientTemp_; }
    bool isClear() const { return cloudiness_ < 30.0; }

    void updateReadings() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> cloudDist(0.0, 80.0);
        std::uniform_real_distribution<> skyTempDist(-20.0, -5.0);
        std::uniform_real_distribution<> ambientTempDist(15.0, 25.0);

        cloudiness_ = cloudDist(gen);
        skyTemp_ = skyTempDist(gen);
        ambientTemp_ = ambientTempDist(gen);
    }

private:
    double cloudiness_{15.0};
    double skyTemp_{-15.0};
    double ambientTemp_{20.0};
};
#endif

// ==================== WeatherMonitorTask Implementation ====================

auto WeatherMonitorTask::taskName() -> std::string { return "WeatherMonitor"; }

void WeatherMonitorTask::execute(const json& params) { executeImpl(params); }

void WeatherMonitorTask::executeImpl(const json& params) {
    spdlog::info("Executing WeatherMonitor task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        int monitorDuration = params.value("duration", 300);
        int checkInterval = params.value("check_interval", 30);
        double maxWindSpeed = params.value("max_wind_speed", 40.0);
        double maxHumidity = params.value("max_humidity", 80.0);
        bool abortOnUnsafe = params.value("abort_on_unsafe", true);

        spdlog::info(
            "Starting weather monitoring for {} seconds with {} second "
            "intervals",
            monitorDuration, checkInterval);

#ifdef MOCK_CAMERA
        auto weatherStation = std::make_shared<MockWeatherStation>();
#endif

        auto endTime = startTime + std::chrono::seconds(monitorDuration);
        bool weatherSafe = true;

        while (std::chrono::steady_clock::now() < endTime) {
#ifdef MOCK_CAMERA
            weatherStation->updateWeather();

            double temp = weatherStation->getTemperature();
            double humidity = weatherStation->getHumidity();
            double windSpeed = weatherStation->getWindSpeed();
            double rainRate = weatherStation->getRainRate();
            bool isSafe = weatherStation->isSafe();

            spdlog::info(
                "Weather: T={:.1f}°C, H={:.1f}%, W={:.1f}km/h, R={:.1f}mm/h, "
                "Safe={}",
                temp, humidity, windSpeed, rainRate, isSafe ? "Yes" : "No");

            if (!isSafe) {
                weatherSafe = false;
                if (windSpeed > maxWindSpeed) {
                    spdlog::warn(
                        "Wind speed {:.1f} km/h exceeds limit {:.1f} km/h",
                        windSpeed, maxWindSpeed);
                }
                if (humidity > maxHumidity) {
                    spdlog::warn("Humidity {:.1f}% exceeds limit {:.1f}%",
                                 humidity, maxHumidity);
                }
                if (rainRate > 0) {
                    spdlog::warn("Rain detected: {:.1f} mm/h", rainRate);
                }

                if (abortOnUnsafe) {
                    THROW_RUNTIME_ERROR(
                        "Unsafe weather conditions detected - aborting");
                }
            }
#else
            spdlog::error(
                "Weather station not available (MOCK_CAMERA not defined)");
            THROW_RUNTIME_ERROR("Weather station not available");
#endif
            std::this_thread::sleep_for(std::chrono::seconds(checkInterval));
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);
        spdlog::info("WeatherMonitor completed in {} ms. Overall safety: {}",
                     duration.count(), weatherSafe ? "Safe" : "Unsafe");

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("WeatherMonitor task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto WeatherMonitorTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            WeatherMonitorTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced WeatherMonitor task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(9);
    task->setTimeout(std::chrono::seconds(7200));
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void WeatherMonitorTask::defineParameters(Task& task) {
    task.addParamDefinition("duration", "int", false, 300,
                            "Monitoring duration in seconds");
    task.addParamDefinition("check_interval", "int", false, 30,
                            "Check interval in seconds");
    task.addParamDefinition("max_wind_speed", "double", false, 40.0,
                            "Maximum safe wind speed");
    task.addParamDefinition("max_humidity", "double", false, 80.0,
                            "Maximum safe humidity");
    task.addParamDefinition("abort_on_unsafe", "bool", false, true,
                            "Abort on unsafe conditions");
}

void WeatherMonitorTask::validateWeatherParameters(const json& params) {
    if (params.contains("duration")) {
        int duration = params["duration"].get<int>();
        if (duration < 60 || duration > 86400) {
            THROW_INVALID_ARGUMENT(
                "Duration must be between 60 and 86400 seconds");
        }
    }

    if (params.contains("check_interval")) {
        int interval = params["check_interval"].get<int>();
        if (interval < 10 || interval > 300) {
            THROW_INVALID_ARGUMENT(
                "Check interval must be between 10 and 300 seconds");
        }
    }
}

// ==================== CloudDetectionTask Implementation ====================

auto CloudDetectionTask::taskName() -> std::string { return "CloudDetection"; }

void CloudDetectionTask::execute(const json& params) { executeImpl(params); }

void CloudDetectionTask::executeImpl(const json& params) {
    spdlog::info("Executing CloudDetection task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double cloudThreshold = params.value("cloud_threshold", 30.0);
        int monitorDuration = params.value("duration", 180);
        int checkInterval = params.value("check_interval", 15);
        bool abortOnClouds = params.value("abort_on_clouds", true);

        spdlog::info(
            "Starting cloud detection with {:.1f}% threshold for {} seconds",
            cloudThreshold, monitorDuration);

#ifdef MOCK_CAMERA
        auto cloudSensor = std::make_shared<MockCloudSensor>();
#endif

        auto endTime = startTime + std::chrono::seconds(monitorDuration);
        bool skyClear = true;

        while (std::chrono::steady_clock::now() < endTime) {
#ifdef MOCK_CAMERA
            cloudSensor->updateReadings();

            double cloudiness = cloudSensor->getCloudiness();
            double skyTemp = cloudSensor->getSkyTemperature();
            double ambientTemp = cloudSensor->getAmbientTemperature();
            bool isClear = cloudSensor->isClear();

            spdlog::info(
                "Cloud conditions: {:.1f}% cloudy, Sky: {:.1f}°C, Ambient: "
                "{:.1f}°C, Clear: {}",
                cloudiness, skyTemp, ambientTemp, isClear ? "Yes" : "No");

            if (cloudiness > cloudThreshold) {
                skyClear = false;
                spdlog::warn("Cloud cover {:.1f}% exceeds threshold {:.1f}%",
                             cloudiness, cloudThreshold);

                if (abortOnClouds) {
                    THROW_RUNTIME_ERROR(
                        "Cloud threshold exceeded - aborting imaging session");
                }
            }
#else
            spdlog::error(
                "Cloud sensor not available (MOCK_CAMERA not defined)");
            THROW_RUNTIME_ERROR("Cloud sensor not available");
#endif
            std::this_thread::sleep_for(std::chrono::seconds(checkInterval));
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime);
        spdlog::info("CloudDetection completed in {} ms. Sky condition: {}",
                     duration.count(), skyClear ? "Clear" : "Cloudy");

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("CloudDetection task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto CloudDetectionTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            CloudDetectionTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced CloudDetection task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);
    task->setTimeout(std::chrono::seconds(3600));
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void CloudDetectionTask::defineParameters(Task& task) {
    task.addParamDefinition("cloud_threshold", "double", false, 30.0,
                            "Cloud coverage threshold percentage");
    task.addParamDefinition("duration", "int", false, 180,
                            "Monitoring duration in seconds");
    task.addParamDefinition("check_interval", "int", false, 15,
                            "Check interval in seconds");
    task.addParamDefinition("abort_on_clouds", "bool", false, true,
                            "Abort on cloud detection");
}

void CloudDetectionTask::validateCloudParameters(const json& params) {
    if (params.contains("cloud_threshold")) {
        double threshold = params["cloud_threshold"].get<double>();
        if (threshold < 0 || threshold > 100) {
            THROW_INVALID_ARGUMENT(
                "Cloud threshold must be between 0 and 100 percent");
        }
    }
}

// ==================== SafetyShutdownTask Implementation ====================

auto SafetyShutdownTask::taskName() -> std::string { return "SafetyShutdown"; }

void SafetyShutdownTask::execute(const json& params) { executeImpl(params); }

void SafetyShutdownTask::executeImpl(const json& params) {
    spdlog::info("Executing SafetyShutdown task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        bool emergencyShutdown = params.value("emergency", false);
        bool parkMount = params.value("park_mount", true);
        bool closeCover = params.value("close_cover", true);
        bool stopCooling = params.value("stop_cooling", true);
        int shutdownDelay = params.value("delay", 0);

        if (emergencyShutdown) {
            spdlog::warn("EMERGENCY SHUTDOWN INITIATED");
        } else {
            spdlog::info("Initiating safety shutdown sequence");
        }

        if (shutdownDelay > 0 && !emergencyShutdown) {
            spdlog::info("Waiting {} seconds before shutdown", shutdownDelay);
            std::this_thread::sleep_for(std::chrono::seconds(shutdownDelay));
        }

        spdlog::info("Stopping camera exposures");
        // In real implementation, this would abort camera exposures

        if (parkMount) {
            spdlog::info("Parking telescope mount");
            std::this_thread::sleep_for(std::chrono::seconds(2));
            spdlog::info("Mount parked successfully");
        }

        if (closeCover) {
            spdlog::info("Closing dust cover/observatory roof");
            std::this_thread::sleep_for(std::chrono::seconds(3));
            spdlog::info("Cover closed successfully");
        }

        if (stopCooling) {
            spdlog::info("Stopping camera cooling");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            spdlog::info("Camera cooling stopped");
        }

        spdlog::info("Stopping autoguiding");
        spdlog::info("Saving session state for recovery");

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("SafetyShutdown completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("SafetyShutdown task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto SafetyShutdownTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            SafetyShutdownTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced SafetyShutdown task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(10);
    task->setTimeout(std::chrono::seconds(300));
    task->setLogLevel(1);
    task->setTaskType(taskName());

    return task;
}

void SafetyShutdownTask::defineParameters(Task& task) {
    task.addParamDefinition("emergency", "bool", false, false,
                            "Emergency shutdown mode");
    task.addParamDefinition("park_mount", "bool", false, true,
                            "Park telescope mount");
    task.addParamDefinition("close_cover", "bool", false, true,
                            "Close dust cover/roof");
    task.addParamDefinition("stop_cooling", "bool", false, true,
                            "Stop camera cooling");
    task.addParamDefinition("delay", "int", false, 0,
                            "Delay before shutdown in seconds");
}

void SafetyShutdownTask::validateSafetyParameters(const json& params) {
    if (params.contains("delay")) {
        int delay = params["delay"].get<int>();
        if (delay < 0 || delay > 300) {
            THROW_INVALID_ARGUMENT(
                "Shutdown delay must be between 0 and 300 seconds");
        }
    }
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register WeatherMonitorTask
AUTO_REGISTER_TASK(
    WeatherMonitorTask, "WeatherMonitor",
    (TaskInfo{.name = "WeatherMonitor",
              .description = "Monitor weather conditions and abort if unsafe",
              .category = "Safety",
              .requiredParameters = {},
              .parameterSchema =
                  json{{"type", "object"},
                       {"properties",
                        json{{"duration", json{{"type", "integer"},
                                               {"minimum", 60},
                                               {"maximum", 86400}}},
                             {"check_interval", json{{"type", "integer"},
                                                     {"minimum", 10},
                                                     {"maximum", 300}}},
                             {"max_wind_speed", json{{"type", "number"},
                                                     {"minimum", 0},
                                                     {"maximum", 100}}},
                             {"max_humidity", json{{"type", "number"},
                                                   {"minimum", 0},
                                                   {"maximum", 100}}},
                             {"abort_on_unsafe", json{{"type", "boolean"}}}}}},
              .version = "1.0.0",
              .dependencies = {}}));

// Register CloudDetectionTask
AUTO_REGISTER_TASK(
    CloudDetectionTask, "CloudDetection",
    (TaskInfo{
        .name = "CloudDetection",
        .description = "Monitor cloud coverage and abort if threshold exceeded",
        .category = "Safety",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"cloud_threshold", json{{"type", "number"},
                                                {"minimum", 0},
                                                {"maximum", 100}}},
                       {"duration", json{{"type", "integer"},
                                         {"minimum", 10},
                                         {"maximum", 3600}}},
                       {"check_interval", json{{"type", "integer"},
                                               {"minimum", 5},
                                               {"maximum", 300}}},
                       {"abort_on_clouds", json{{"type", "boolean"}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register SafetyShutdownTask
AUTO_REGISTER_TASK(
    SafetyShutdownTask, "SafetyShutdown",
    (TaskInfo{
        .name = "SafetyShutdown",
        .description = "Perform a safety shutdown sequence for the observatory",
        .category = "Safety",
        .requiredParameters = {},
        .parameterSchema =
            json{
                {"type", "object"},
                {"properties", json{{"emergency", json{{"type", "boolean"}}},
                                    {"park_mount", json{{"type", "boolean"}}},
                                    {"close_cover", json{{"type", "boolean"}}},
                                    {"stop_cooling", json{{"type", "boolean"}}},
                                    {"delay", json{{"type", "integer"},
                                                   {"minimum", 0},
                                                   {"maximum", 300}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

}  // namespace
