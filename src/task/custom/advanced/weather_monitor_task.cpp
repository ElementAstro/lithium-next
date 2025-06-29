#include "weather_monitor_task.hpp"
#include <chrono>
#include <memory>
#include <thread>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto WeatherMonitorTask::taskName() -> std::string { return "WeatherMonitor"; }

void WeatherMonitorTask::execute(const json& params) { executeImpl(params); }

void WeatherMonitorTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing WeatherMonitor task '{}' with params: {}",
          getName(), params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        double monitorInterval = params.value("monitor_interval_minutes", 5.0);
        double cloudCoverLimit = params.value("cloud_cover_limit", 30.0);
        double windSpeedLimit = params.value("wind_speed_limit", 25.0);
        double humidityLimit = params.value("humidity_limit", 85.0);
        double temperatureMin = params.value("temperature_min", -20.0);
        double temperatureMax = params.value("temperature_max", 35.0);
        double dewPointLimit = params.value("dew_point_limit", 2.0);
        bool rainDetection = params.value("rain_detection", true);
        bool emailAlerts = params.value("email_alerts", true);
        double monitorDuration = params.value("monitor_duration_hours", 24.0);

        json weatherLimits = {
            {"cloud_cover_limit", cloudCoverLimit},
            {"wind_speed_limit", windSpeedLimit},
            {"humidity_limit", humidityLimit},
            {"temperature_min", temperatureMin},
            {"temperature_max", temperatureMax},
            {"dew_point_limit", dewPointLimit},
            {"rain_detection", rainDetection}
        };

        LOG_F(INFO, "Starting weather monitoring for {:.1f} hours with {:.1f} minute intervals",
              monitorDuration, monitorInterval);

        auto monitorEnd = std::chrono::steady_clock::now() +
                         std::chrono::hours(static_cast<int>(monitorDuration));

        bool lastWeatherState = true; // true = safe, false = unsafe

        while (std::chrono::steady_clock::now() < monitorEnd) {
            json currentWeather = getCurrentWeatherData();
            bool weatherSafe = evaluateWeatherConditions(currentWeather, weatherLimits);

            LOG_F(INFO, "Weather check - Safe: {}, Clouds: {:.1f}%, Wind: {:.1f}km/h, "
                       "Humidity: {:.1f}%, Temp: {:.1f}°C",
                  weatherSafe ? "YES" : "NO",
                  currentWeather.value("cloud_cover", 0.0),
                  currentWeather.value("wind_speed", 0.0),
                  currentWeather.value("humidity", 0.0),
                  currentWeather.value("temperature", 0.0));

            // Handle weather state changes
            if (weatherSafe && !lastWeatherState) {
                LOG_F(INFO, "Weather conditions improved - resuming operations");
                handleSafeWeather();
                if (emailAlerts) {
                    sendWeatherAlert("Weather conditions have improved. Operations resumed.");
                }
            } else if (!weatherSafe && lastWeatherState) {
                LOG_F(WARNING, "Weather conditions deteriorated - securing equipment");
                handleUnsafeWeather();
                if (emailAlerts) {
                    sendWeatherAlert("Unsafe weather detected. Equipment secured.");
                }
            }

            lastWeatherState = weatherSafe;

            // Sleep until next monitoring interval
            std::this_thread::sleep_for(
                std::chrono::minutes(static_cast<int>(monitorInterval)));
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::hours>(
            endTime - startTime);
        LOG_F(INFO, "WeatherMonitor task '{}' completed after {} hours",
              getName(), duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(ERROR, "WeatherMonitor task '{}' failed after {} minutes: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

json WeatherMonitorTask::getCurrentWeatherData() {
    // In real implementation, this would connect to weather APIs or local weather station
    // For now, simulate weather data

    json weather = {
        {"cloud_cover", 15.0 + (rand() % 40)},        // 15-55%
        {"wind_speed", 5.0 + (rand() % 20)},          // 5-25 km/h
        {"humidity", 45.0 + (rand() % 40)},           // 45-85%
        {"temperature", 10.0 + (rand() % 20)},        // 10-30°C
        {"dew_point", 5.0 + (rand() % 15)},           // 5-20°C
        {"pressure", 1010.0 + (rand() % 30)},         // 1010-1040 hPa
        {"rain_detected", (rand() % 10) == 0},        // 10% chance
        {"timestamp", std::time(nullptr)}
    };

    return weather;
}

bool WeatherMonitorTask::evaluateWeatherConditions(const json& weather, const json& limits) {
    // Check cloud cover
    if (weather["cloud_cover"].get<double>() > limits["cloud_cover_limit"].get<double>()) {
        return false;
    }

    // Check wind speed
    if (weather["wind_speed"].get<double>() > limits["wind_speed_limit"].get<double>()) {
        return false;
    }

    // Check humidity
    if (weather["humidity"].get<double>() > limits["humidity_limit"].get<double>()) {
        return false;
    }

    // Check temperature range
    double temp = weather["temperature"].get<double>();
    if (temp < limits["temperature_min"].get<double>() ||
        temp > limits["temperature_max"].get<double>()) {
        return false;
    }

    // Check dew point proximity
    double dewPoint = weather["dew_point"].get<double>();
    if ((temp - dewPoint) < limits["dew_point_limit"].get<double>()) {
        return false;
    }

    // Check rain detection
    if (limits["rain_detection"].get<bool>() && weather["rain_detected"].get<bool>()) {
        return false;
    }

    return true;
}

void WeatherMonitorTask::handleUnsafeWeather() {
    LOG_F(WARNING, "Implementing weather safety protocols");

    // In real implementation, this would:
    // 1. Stop current imaging sequences
    // 2. Close observatory roof/dome
    // 3. Park telescope to safe position
    // 4. Cover equipment
    // 5. Shut down sensitive electronics

    // Simulate safety actions
    std::this_thread::sleep_for(std::chrono::seconds(5));
    LOG_F(INFO, "Equipment secured due to unsafe weather");
}

void WeatherMonitorTask::handleSafeWeather() {
    LOG_F(INFO, "Weather conditions safe - resuming operations");

    // In real implementation, this would:
    // 1. Open observatory roof/dome
    // 2. Unpark telescope
    // 3. Resume suspended sequences
    // 4. Restart equipment cooling

    // Simulate resumption actions
    std::this_thread::sleep_for(std::chrono::seconds(3));
    LOG_F(INFO, "Operations resumed after weather improvement");
}

void WeatherMonitorTask::sendWeatherAlert(const std::string& message) {
    LOG_F(INFO, "Weather Alert: {}", message);

    // In real implementation, this would send email/SMS notifications
    // For now, just log the alert
}

void WeatherMonitorTask::validateWeatherMonitorParameters(const json& params) {
    if (params.contains("monitor_interval_minutes")) {
        double interval = params["monitor_interval_minutes"].get<double>();
        if (interval < 0.5 || interval > 60.0) {
            THROW_INVALID_ARGUMENT("Monitor interval must be between 0.5 and 60 minutes");
        }
    }

    if (params.contains("monitor_duration_hours")) {
        double duration = params["monitor_duration_hours"].get<double>();
        if (duration <= 0 || duration > 168.0) {
            THROW_INVALID_ARGUMENT("Monitor duration must be between 0 and 168 hours (1 week)");
        }
    }
}

auto WeatherMonitorTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<WeatherMonitorTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced WeatherMonitor task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(10);
    task->setTimeout(std::chrono::seconds(604800));  // 1 week timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void WeatherMonitorTask::defineParameters(Task& task) {
    task.addParamDefinition("monitor_interval_minutes", "double", false, 5.0,
                            "Interval between weather checks in minutes");
    task.addParamDefinition("cloud_cover_limit", "double", false, 30.0,
                            "Maximum acceptable cloud cover percentage");
    task.addParamDefinition("wind_speed_limit", "double", false, 25.0,
                            "Maximum acceptable wind speed in km/h");
    task.addParamDefinition("humidity_limit", "double", false, 85.0,
                            "Maximum acceptable humidity percentage");
    task.addParamDefinition("temperature_min", "double", false, -20.0,
                            "Minimum acceptable temperature in Celsius");
    task.addParamDefinition("temperature_max", "double", false, 35.0,
                            "Maximum acceptable temperature in Celsius");
    task.addParamDefinition("dew_point_limit", "double", false, 2.0,
                            "Minimum temperature-dew point difference");
    task.addParamDefinition("rain_detection", "bool", false, true,
                            "Enable rain detection safety");
    task.addParamDefinition("email_alerts", "bool", false, true,
                            "Send email alerts on weather changes");
    task.addParamDefinition("monitor_duration_hours", "double", false, 24.0,
                            "Duration to monitor weather in hours");
}

}  // namespace lithium::task::task
