/*
 * ascom_observingconditions.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Observing Conditions Device Implementation

**************************************************/

#include "ascom_observingconditions.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client::ascom {

ASCOMObservingConditions::ASCOMObservingConditions(std::string name, int deviceNumber)
    : ASCOMDeviceBase(std::move(name), ASCOMDeviceType::ObservingConditions, deviceNumber) {
    spdlog::debug("ASCOMObservingConditions created: {}", name_);
}

ASCOMObservingConditions::~ASCOMObservingConditions() {
    spdlog::debug("ASCOMObservingConditions destroyed: {}", name_);
}

auto ASCOMObservingConditions::connect(int timeout) -> bool {
    if (!ASCOMDeviceBase::connect(timeout)) {
        return false;
    }
    spdlog::info("ObservingConditions {} connected", name_);
    return true;
}

// ==================== Weather Data ====================

auto ASCOMObservingConditions::getWeatherData() -> WeatherData {
    std::lock_guard<std::mutex> lock(weatherMutex_);

    weatherData_.cloudCover = getCloudCover().value_or(0.0);
    weatherData_.dewPoint = getDewPoint().value_or(0.0);
    weatherData_.humidity = getHumidity().value_or(0.0);
    weatherData_.pressure = getPressure().value_or(0.0);
    weatherData_.rainRate = getRainRate().value_or(0.0);
    weatherData_.skyBrightness = getSkyBrightness().value_or(0.0);
    weatherData_.skyQuality = getSkyQuality().value_or(0.0);
    weatherData_.skyTemperature = getSkyTemperature().value_or(0.0);
    weatherData_.starFWHM = getStarFWHM().value_or(0.0);
    weatherData_.temperature = getTemperature().value_or(0.0);
    weatherData_.windDirection = getWindDirection().value_or(0.0);
    weatherData_.windGust = getWindGust().value_or(0.0);
    weatherData_.windSpeed = getWindSpeed().value_or(0.0);

    return weatherData_;
}

auto ASCOMObservingConditions::getCloudCover() -> std::optional<double> {
    return getDoubleProperty("cloudcover");
}

auto ASCOMObservingConditions::getDewPoint() -> std::optional<double> {
    return getDoubleProperty("dewpoint");
}

auto ASCOMObservingConditions::getHumidity() -> std::optional<double> {
    return getDoubleProperty("humidity");
}

auto ASCOMObservingConditions::getPressure() -> std::optional<double> {
    return getDoubleProperty("pressure");
}

auto ASCOMObservingConditions::getRainRate() -> std::optional<double> {
    return getDoubleProperty("rainrate");
}

auto ASCOMObservingConditions::getSkyBrightness() -> std::optional<double> {
    return getDoubleProperty("skybrightness");
}

auto ASCOMObservingConditions::getSkyQuality() -> std::optional<double> {
    return getDoubleProperty("skyquality");
}

auto ASCOMObservingConditions::getSkyTemperature() -> std::optional<double> {
    return getDoubleProperty("skytemperature");
}

auto ASCOMObservingConditions::getStarFWHM() -> std::optional<double> {
    return getDoubleProperty("starfwhm");
}

auto ASCOMObservingConditions::getTemperature() -> std::optional<double> {
    return getDoubleProperty("temperature");
}

auto ASCOMObservingConditions::getWindDirection() -> std::optional<double> {
    return getDoubleProperty("winddirection");
}

auto ASCOMObservingConditions::getWindGust() -> std::optional<double> {
    return getDoubleProperty("windgust");
}

auto ASCOMObservingConditions::getWindSpeed() -> std::optional<double> {
    return getDoubleProperty("windspeed");
}

// ==================== Sensor Info ====================

auto ASCOMObservingConditions::getSensorDescription(const std::string& sensor)
    -> std::optional<std::string> {
    auto response = getProperty("sensordescription",
                                {{"SensorName", sensor}});
    if (response.isSuccess() && response.value.is_string()) {
        return response.value.get<std::string>();
    }
    return std::nullopt;
}

auto ASCOMObservingConditions::getTimeSinceLastUpdate(const std::string& sensor)
    -> std::optional<double> {
    auto response = getProperty("timesincelastupdate",
                                {{"SensorName", sensor}});
    if (response.isSuccess() && response.value.is_number()) {
        return response.value.get<double>();
    }
    return std::nullopt;
}

// ==================== Refresh ====================

auto ASCOMObservingConditions::refresh() -> bool {
    if (!isConnected()) return false;
    return setProperty("refresh", {}).isSuccess();
}

auto ASCOMObservingConditions::getAveragePeriod() -> double {
    return getDoubleProperty("averageperiod").value_or(0.0);
}

auto ASCOMObservingConditions::setAveragePeriod(double hours) -> bool {
    return setDoubleProperty("averageperiod", hours);
}

// ==================== Status ====================

auto ASCOMObservingConditions::getStatus() const -> json {
    json status = ASCOMDeviceBase::getStatus();

    std::lock_guard<std::mutex> lock(weatherMutex_);
    status["weather"] = weatherData_.toJson();

    return status;
}

}  // namespace lithium::client::ascom
