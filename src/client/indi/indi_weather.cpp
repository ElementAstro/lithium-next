/*
 * indi_weather.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Weather Station Device Implementation

**************************************************/

#include "indi_weather.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDIWeather::INDIWeather(std::string name) : INDIDeviceBase(std::move(name)) {
    LOG_DEBUG("INDIWeather created: {}", name_);
}

INDIWeather::~INDIWeather() { LOG_DEBUG("INDIWeather destroyed: {}", name_); }

// ==================== Connection ====================

auto INDIWeather::connect(const std::string& deviceName, int timeout,
                          int maxRetry) -> bool {
    if (!INDIDeviceBase::connect(deviceName, timeout, maxRetry)) {
        return false;
    }

    setupPropertyWatchers();
    LOG_INFO("Weather station {} connected", deviceName);
    return true;
}

auto INDIWeather::disconnect() -> bool { return INDIDeviceBase::disconnect(); }

// ==================== Weather Data ====================

auto INDIWeather::getWeatherData() const -> WeatherData {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_;
}

auto INDIWeather::getTemperature() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.temperature;
}

auto INDIWeather::getHumidity() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.humidity;
}

auto INDIWeather::getPressure() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.pressure;
}

auto INDIWeather::getWindSpeed() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.windSpeed;
}

auto INDIWeather::getWindDirection() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.windDirection;
}

auto INDIWeather::getDewPoint() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.dewPoint;
}

auto INDIWeather::getSkyQuality() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.skyQuality;
}

auto INDIWeather::isRaining() const -> bool {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.isRaining;
}

auto INDIWeather::isSafe() const -> bool {
    std::lock_guard<std::mutex> lock(weatherMutex_);
    return weatherData_.isSafe;
}

// ==================== Parameters ====================

auto INDIWeather::getParameter(const std::string& name) const
    -> std::optional<WeatherParameter> {
    std::lock_guard<std::mutex> lock(parametersMutex_);
    for (const auto& param : parameters_) {
        if (param.name == name) {
            return param;
        }
    }
    return std::nullopt;
}

auto INDIWeather::getParameters() const -> std::vector<WeatherParameter> {
    std::lock_guard<std::mutex> lock(parametersMutex_);
    return parameters_;
}

// ==================== Location ====================

auto INDIWeather::getLocation() const -> LocationInfo {
    std::lock_guard<std::mutex> lock(locationMutex_);
    return location_;
}

auto INDIWeather::setLocation(double lat, double lon, double elev) -> bool {
    if (!isConnected()) {
        return false;
    }

    bool success = true;
    success &= setNumberProperty("GEOGRAPHIC_COORD", "LAT", lat);
    success &= setNumberProperty("GEOGRAPHIC_COORD", "LONG", lon);
    success &= setNumberProperty("GEOGRAPHIC_COORD", "ELEV", elev);

    if (success) {
        std::lock_guard<std::mutex> lock(locationMutex_);
        location_.latitude = lat;
        location_.longitude = lon;
        location_.elevation = elev;
    }

    return success;
}

// ==================== Refresh ====================

auto INDIWeather::refresh() -> bool {
    if (!isConnected()) {
        return false;
    }

    weatherState_.store(WeatherState::Updating);

    if (!setSwitchProperty("WEATHER_REFRESH", "REFRESH", true)) {
        LOG_ERROR("Failed to refresh weather data");
        weatherState_.store(WeatherState::Error);
        return false;
    }

    return true;
}

auto INDIWeather::setRefreshPeriod(int seconds) -> bool {
    if (!isConnected()) {
        return false;
    }

    if (!setNumberProperty("WEATHER_UPDATE", "PERIOD", static_cast<double>(seconds))) {
        LOG_ERROR("Failed to set refresh period");
        return false;
    }

    refreshPeriod_.store(seconds);
    return true;
}

auto INDIWeather::getRefreshPeriod() const -> int { return refreshPeriod_.load(); }

// ==================== Status ====================

auto INDIWeather::getWeatherState() const -> WeatherState {
    return weatherState_.load();
}

auto INDIWeather::getStatus() const -> json {
    json status = INDIDeviceBase::getStatus();

    status["weatherState"] = static_cast<int>(weatherState_.load());
    status["weather"] = getWeatherData().toJson();
    status["location"] = getLocation().toJson();
    status["refreshPeriod"] = refreshPeriod_.load();

    json paramsJson = json::array();
    auto params = getParameters();
    for (const auto& param : params) {
        paramsJson.push_back(param.toJson());
    }
    status["parameters"] = paramsJson;

    return status;
}

// ==================== Property Handlers ====================

void INDIWeather::onPropertyDefined(const INDIProperty& property) {
    INDIDeviceBase::onPropertyDefined(property);

    if (property.name == "WEATHER_STATUS" ||
        property.name.find("WEATHER_") == 0) {
        handleWeatherProperty(property);
    } else if (property.name == "GEOGRAPHIC_COORD") {
        handleLocationProperty(property);
    } else if (property.name.find("WEATHER_PARAMETERS") == 0) {
        handleParameterProperty(property);
    }
}

void INDIWeather::onPropertyUpdated(const INDIProperty& property) {
    INDIDeviceBase::onPropertyUpdated(property);

    if (property.name == "WEATHER_STATUS" ||
        property.name.find("WEATHER_") == 0) {
        handleWeatherProperty(property);

        if (property.state == PropertyState::Ok) {
            weatherState_.store(WeatherState::Idle);
        } else if (property.state == PropertyState::Alert) {
            weatherState_.store(WeatherState::Alert);
        }
    } else if (property.name == "GEOGRAPHIC_COORD") {
        handleLocationProperty(property);
    }
}

// ==================== Internal Methods ====================

void INDIWeather::handleWeatherProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(weatherMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "WEATHER_TEMPERATURE") {
            weatherData_.temperature = elem.value;
        } else if (elem.name == "WEATHER_HUMIDITY") {
            weatherData_.humidity = elem.value;
        } else if (elem.name == "WEATHER_PRESSURE") {
            weatherData_.pressure = elem.value;
        } else if (elem.name == "WEATHER_WIND_SPEED") {
            weatherData_.windSpeed = elem.value;
        } else if (elem.name == "WEATHER_WIND_GUST") {
            weatherData_.windGust = elem.value;
        } else if (elem.name == "WEATHER_WIND_DIRECTION") {
            weatherData_.windDirection = elem.value;
        } else if (elem.name == "WEATHER_DEWPOINT") {
            weatherData_.dewPoint = elem.value;
        } else if (elem.name == "WEATHER_CLOUD_COVER") {
            weatherData_.cloudCover = elem.value;
        } else if (elem.name == "WEATHER_SKY_QUALITY") {
            weatherData_.skyQuality = elem.value;
        } else if (elem.name == "WEATHER_SKY_BRIGHTNESS") {
            weatherData_.skyBrightness = elem.value;
        } else if (elem.name == "WEATHER_RAIN_RATE") {
            weatherData_.rainRate = elem.value;
        }
    }

    // Check rain status
    for (const auto& elem : property.lights) {
        if (elem.name == "WEATHER_RAIN") {
            weatherData_.isRaining = (elem.state == PropertyState::Alert);
        }
    }

    updateSafetyStatus();
}

void INDIWeather::handleLocationProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(locationMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "LAT") {
            location_.latitude = elem.value;
        } else if (elem.name == "LONG") {
            location_.longitude = elem.value;
        } else if (elem.name == "ELEV") {
            location_.elevation = elem.value;
        }
    }
}

void INDIWeather::handleParameterProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(parametersMutex_);

    for (const auto& elem : property.numbers) {
        WeatherParameter param;
        param.name = elem.name;
        param.label = elem.label;
        param.value = elem.value;
        param.min = elem.min;
        param.max = elem.max;

        // Check if parameter already exists
        bool found = false;
        for (auto& existing : parameters_) {
            if (existing.name == param.name) {
                existing = param;
                found = true;
                break;
            }
        }
        if (!found) {
            parameters_.push_back(param);
        }
    }
}

void INDIWeather::setupPropertyWatchers() {
    // Watch weather status
    watchProperty("WEATHER_STATUS", [this](const INDIProperty& prop) {
        handleWeatherProperty(prop);
    });

    // Watch location
    watchProperty("GEOGRAPHIC_COORD", [this](const INDIProperty& prop) {
        handleLocationProperty(prop);
    });
}

void INDIWeather::updateSafetyStatus() {
    // Simple safety check - can be extended
    weatherData_.isSafe = !weatherData_.isRaining &&
                          weatherData_.windSpeed < 50.0 &&
                          weatherData_.humidity < 95.0;
}

}  // namespace lithium::client::indi
