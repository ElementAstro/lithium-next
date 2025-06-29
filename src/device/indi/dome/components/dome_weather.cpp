/*
 * dome_weather.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Weather - Weather Monitoring Implementation

*************************************************/

#include "dome_weather.hpp"
#include "../dome_client.hpp"

#include <spdlog/spdlog.h>
#include <cmath>
#include <string_view>

DomeWeatherManager::DomeWeatherManager(INDIDomeClient* client)
    : client_(client) {
    // Initialize default weather limits
    weather_limits_.maxWindSpeed = 15.0;     // m/s
    weather_limits_.minTemperature = -10.0;  // °C
    weather_limits_.maxTemperature = 50.0;   // °C
    weather_limits_.maxHumidity = 85.0;      // %
    weather_limits_.rainProtection = true;
}

// Weather monitoring
auto DomeWeatherManager::enableWeatherMonitoring(bool enable) -> bool {
    std::lock_guard<std::mutex> lock(weather_mutex_);

    if (!client_->isConnected()) {
        spdlog::error("[DomeWeatherManager] Device not connected");
        return false;
    }

    // Try to find weather monitoring property
    auto weatherProp = client_->getBaseDevice().getProperty("WEATHER_OVERRIDE");
    if (weatherProp.isValid() && weatherProp.getType() == INDI_SWITCH) {
        auto weatherSwitch = weatherProp.getSwitch();
        if (weatherSwitch) {
            weatherSwitch->reset();

            if (enable) {
                auto enableWidget =
                    weatherSwitch->findWidgetByName("WEATHER_OVERRIDE_DISABLE");
                if (enableWidget) {
                    enableWidget->setState(ISS_ON);
                }
            } else {
                auto disableWidget =
                    weatherSwitch->findWidgetByName("WEATHER_OVERRIDE_ENABLE");
                if (disableWidget) {
                    disableWidget->setState(ISS_ON);
                }
            }

            client_->sendNewProperty(weatherSwitch);
        }
    }

    weather_monitoring_enabled_ = enable;
    spdlog::info("[DomeWeatherManager] {} weather monitoring",
                 enable ? "Enabled" : "Disabled");

    if (enable) {
        // Perform initial weather check
        checkWeatherStatus();
    }

    return true;
}

auto DomeWeatherManager::isWeatherMonitoringEnabled() -> bool {
    std::lock_guard<std::mutex> lock(weather_mutex_);
    return weather_monitoring_enabled_;
}

auto DomeWeatherManager::isWeatherSafe() -> bool {
    std::lock_guard<std::mutex> lock(weather_mutex_);
    return weather_safe_;
}

auto DomeWeatherManager::getWeatherCondition()
    -> std::optional<WeatherCondition> {
    std::scoped_lock lock(weather_mutex_);
    if (!client_->isConnected() || !weather_monitoring_enabled_) {
        return std::nullopt;
    }
    WeatherCondition condition;
    condition.safe = weather_safe_;
    if (auto* weatherProp = getWeatherProperty(); weatherProp) {
        for (int i = 0, n = weatherProp->count(); i < n; ++i) {
            auto* widget = weatherProp->at(i);
            std::string_view name = widget->getName();
            double value = widget->getValue();
            if (name == "WEATHER_TEMPERATURE" || name == "TEMPERATURE") {
                condition.temperature = value;
            } else if (name == "WEATHER_HUMIDITY" || name == "HUMIDITY") {
                condition.humidity = value;
            } else if (name == "WEATHER_WIND_SPEED" || name == "WIND_SPEED") {
                condition.windSpeed = value;
            }
        }
    }
    if (auto* rainProp = getRainProperty(); rainProp) {
        if (auto* rainWidget = rainProp->findWidgetByName("RAIN_DETECTED");
            rainWidget) {
            condition.rainDetected = (rainWidget->getState() == ISS_ON);
        }
    }
    return condition;
}

// Weather limits
auto DomeWeatherManager::setWeatherLimits(const WeatherLimits& limits) -> bool {
    std::lock_guard<std::mutex> lock(weather_mutex_);

    // Validate limits
    if (limits.maxWindSpeed < 0 || limits.maxWindSpeed > 100) {
        spdlog::error("[DomeWeatherManager] Invalid wind speed limit: {}",
                      limits.maxWindSpeed);
        return false;
    }

    if (limits.minTemperature >= limits.maxTemperature) {
        spdlog::error(
            "[DomeWeatherManager] Invalid temperature range: {} to {}",
            limits.minTemperature, limits.maxTemperature);
        return false;
    }

    if (limits.maxHumidity < 0 || limits.maxHumidity > 100) {
        spdlog::error("[DomeWeatherManager] Invalid humidity limit: {}",
                      limits.maxHumidity);
        return false;
    }

    weather_limits_ = limits;

    spdlog::info(
        "[DomeWeatherManager] Weather limits updated: Wind={:.1f}m/s, "
        "Temp={:.1f}-{:.1f}°C, Humidity={:.1f}%, Rain={}",
        limits.maxWindSpeed, limits.minTemperature, limits.maxTemperature,
        limits.maxHumidity, limits.rainProtection ? "protected" : "ignored");

    // Recheck weather status with new limits
    if (weather_monitoring_enabled_) {
        checkWeatherStatus();
    }

    return true;
}

auto DomeWeatherManager::getWeatherLimits() -> WeatherLimits {
    std::lock_guard<std::mutex> lock(weather_mutex_);
    return weather_limits_;
}

// Weather automation
auto DomeWeatherManager::enableAutoCloseOnUnsafeWeather(bool enable) -> bool {
    std::lock_guard<std::mutex> lock(weather_mutex_);

    auto_close_enabled_ = enable;
    spdlog::info("[DomeWeatherManager] {} auto-close on unsafe weather",
                 enable ? "Enabled" : "Disabled");
    return true;
}

auto DomeWeatherManager::isAutoCloseEnabled() -> bool {
    std::lock_guard<std::mutex> lock(weather_mutex_);
    return auto_close_enabled_;
}

// INDI property handling
void DomeWeatherManager::handleWeatherProperty(const INDI::Property& property) {
    if (!property.isValid()) {
        return;
    }
    std::string_view propertyName = property.getName();
    if (propertyName.find("WEATHER") != std::string_view::npos ||
        propertyName == "TEMPERATURE" || propertyName == "HUMIDITY" ||
        propertyName == "WIND_SPEED" || propertyName == "RAIN") {
        spdlog::debug("[DomeWeatherManager] Weather property updated: {}",
                      propertyName);
        if (weather_monitoring_enabled_) {
            checkWeatherStatus();
        }
    }
}

void DomeWeatherManager::synchronizeWithDevice() {
    if (!client_->isConnected()) {
        return;
    }

    // Check current weather monitoring state
    auto weatherProp = client_->getBaseDevice().getProperty("WEATHER_OVERRIDE");
    if (weatherProp.isValid()) {
        handleWeatherProperty(weatherProp);
    }

    // Update weather status
    if (weather_monitoring_enabled_) {
        checkWeatherStatus();
    }

    spdlog::debug("[DomeWeatherManager] Synchronized with device");
}

// Weather safety checks
void DomeWeatherManager::checkWeatherStatus() {
    if (!weather_monitoring_enabled_) {
        return;
    }

    auto condition = getWeatherCondition();
    if (!condition) {
        spdlog::warn("[DomeWeatherManager] Unable to get weather condition");
        return;
    }

    bool previouslySafe = weather_safe_;
    bool currentlySafe = true;
    std::string details;

    // Check all weather parameters
    if (!checkWindSpeed(condition->windSpeed)) {
        currentlySafe = false;
        details += "High wind speed (" + std::to_string(condition->windSpeed) +
                   "m/s); ";
    }

    if (!checkTemperature(condition->temperature)) {
        currentlySafe = false;
        details += "Temperature out of range (" +
                   std::to_string(condition->temperature) + "°C); ";
    }

    if (!checkHumidity(condition->humidity)) {
        currentlySafe = false;
        details +=
            "High humidity (" + std::to_string(condition->humidity) + "%); ";
    }

    if (!checkRain(condition->rainDetected)) {
        currentlySafe = false;
        details += "Rain detected; ";
    }

    // Update weather safety state
    {
        std::lock_guard<std::mutex> lock(weather_mutex_);
        weather_safe_ = currentlySafe;
    }

    // Notify if weather state changed
    if (previouslySafe != currentlySafe) {
        if (currentlySafe) {
            spdlog::info(
                "[DomeWeatherManager] Weather is now safe for operations");
            notifyWeatherEvent(true, "Weather conditions improved");
        } else {
            spdlog::warn("[DomeWeatherManager] Weather is now unsafe: {}",
                         details);
            notifyWeatherEvent(false, details);

            // Auto-close dome if enabled
            if (auto_close_enabled_) {
                performSafetyChecks();
            }
        }
    }
}

void DomeWeatherManager::performSafetyChecks() {
    if (!weather_safe_ && auto_close_enabled_) {
        spdlog::warn(
            "[DomeWeatherManager] Unsafe weather detected, initiating safety "
            "procedures");

        // Close shutter if weather is unsafe
        auto shutterManager = client_->getShutterManager();
        if (shutterManager &&
            shutterManager->getShutterState() != ShutterState::CLOSED) {
            spdlog::info(
                "[DomeWeatherManager] Closing shutter due to unsafe weather");
            [[maybe_unused]] bool _ = shutterManager->closeShutter();
        }
        // Stop dome motion if active
        auto motionManager = client_->getMotionManager();
        if (motionManager && motionManager->isMoving()) {
            spdlog::info(
                "[DomeWeatherManager] Stopping dome motion due to unsafe "
                "weather");
            [[maybe_unused]] bool _ = motionManager->stopRotation();
        }
    }
}

// Weather callback registration
void DomeWeatherManager::setWeatherCallback(WeatherCallback callback) {
    std::lock_guard<std::mutex> lock(weather_mutex_);
    weather_callback_ = std::move(callback);
}

// Internal methods
void DomeWeatherManager::notifyWeatherEvent(bool safe,
                                            const std::string& details) {
    if (weather_callback_) {
        try {
            weather_callback_(safe, details);
        } catch (const std::exception& ex) {
            spdlog::error("[DomeWeatherManager] Weather callback error: {}",
                          ex.what());
        }
    }
}

auto DomeWeatherManager::checkWindSpeed(double windSpeed) -> bool {
    return windSpeed <= weather_limits_.maxWindSpeed;
}

auto DomeWeatherManager::checkTemperature(double temperature) -> bool {
    return temperature >= weather_limits_.minTemperature &&
           temperature <= weather_limits_.maxTemperature;
}

auto DomeWeatherManager::checkHumidity(double humidity) -> bool {
    return humidity <= weather_limits_.maxHumidity;
}

auto DomeWeatherManager::checkRain(bool rainDetected) -> bool {
    if (!weather_limits_.rainProtection) {
        return true;  // Rain protection disabled
    }
    return !rainDetected;
}

// INDI property helpers
auto DomeWeatherManager::getWeatherProperty() -> INDI::PropertyViewNumber* {
    if (!client_->isConnected()) {
        return nullptr;
    }

    // Try common weather property names
    std::vector<std::string> propertyNames = {
        "WEATHER_PARAMETERS", "WEATHER_DATA", "WEATHER", "ENVIRONMENT_DATA"};

    for (const auto& propName : propertyNames) {
        auto property = client_->getBaseDevice().getProperty(propName.c_str());
        if (property.isValid() && property.getType() == INDI_NUMBER) {
            return property.getNumber();
        }
    }

    return nullptr;
}

auto DomeWeatherManager::getRainProperty() -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected()) {
        return nullptr;
    }

    // Try common rain detection property names
    std::vector<std::string> propertyNames = {"RAIN_SENSOR", "RAIN_DETECTION",
                                              "RAIN_STATUS", "WEATHER_RAIN"};

    for (const auto& propName : propertyNames) {
        auto property = client_->getBaseDevice().getProperty(propName.c_str());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }

    return nullptr;
}
