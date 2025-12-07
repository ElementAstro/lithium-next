/*
 * indigo_weather.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Weather Station Device Implementation

**************************************************/

#include "indigo_weather.hpp"

#include <cmath>
#include <mutex>

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// Safety status conversion
// ============================================================================

auto safetyStatusFromString(std::string_view str) -> SafetyStatus {
    if (str == "SAFE" || str == "Safe") return SafetyStatus::Safe;
    if (str == "UNSAFE" || str == "Unsafe") return SafetyStatus::Unsafe;
    return SafetyStatus::Unknown;
}

// ============================================================================
// Dew Point / Humidity Calculations
// ============================================================================

auto INDIGOWeather::computeRelativeHumidity(double temperature,
                                            double dewPoint) -> double {
    // Magnus formula: RH = 100 * (es(Td) / es(T))
    // where es(T) = 6.1094 * exp((a * T) / (b + T))
    // Constants: a = 17.625, b = 243.04°C

    const double a = 17.625;
    const double b = 243.04;

    double es_t =
        6.1094 * std::exp((a * temperature) / (b + temperature));
    double es_td =
        6.1094 * std::exp((a * dewPoint) / (b + dewPoint));

    double rh = 100.0 * (es_td / es_t);
    return std::clamp(rh, 0.0, 100.0);
}

auto INDIGOWeather::computeDewPoint(double temperature,
                                    double humidity) -> double {
    // Magnus formula inverse
    // Td = (b * ln(RH/100 * es(T)/6.1094)) / (a - ln(RH/100 * es(T)/6.1094))

    const double a = 17.625;
    const double b = 243.04;

    double rh_fraction = humidity / 100.0;
    double es_t =
        6.1094 * std::exp((a * temperature) / (b + temperature));
    double ln_term = std::log(rh_fraction * es_t / 6.1094);

    double dew_point = (b * ln_term) / (a - ln_term);
    return dew_point;
}

// ============================================================================
// INDIGOWeather Implementation
// ============================================================================

class INDIGOWeather::Impl {
public:
    explicit Impl(INDIGOWeather* parent) : parent_(parent) {}

    void onConnected() {
        // Request weather data refresh
        refresh();

        LOG_F(INFO, "INDIGO Weather[{}]: Connected and initialized",
              parent_->getINDIGODeviceName());
    }

    void onDisconnected() {
        LOG_F(INFO, "INDIGO Weather[{}]: Disconnected",
              parent_->getINDIGODeviceName());
    }

    void onPropertyUpdated(const Property& property) {
        const auto& name = property.name;

        if (name == "AUX_WEATHER" || name == "WEATHER_PARAMETERS") {
            updateWeatherParameters(property);
        } else if (name == "AUX_DEW_WARNING") {
            updateDewWarning(property);
        } else if (name == "WEATHER_SAFETY") {
            updateSafetyStatus(property);
        }
    }

    [[nodiscard]] auto getWeatherParameters() const -> WeatherParameters {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_;
    }

    [[nodiscard]] auto getTemperature() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.temperature;
    }

    [[nodiscard]] auto getHumidity() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.humidity;
    }

    [[nodiscard]] auto getPressure() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.pressure;
    }

    [[nodiscard]] auto getWindSpeed() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.windSpeed;
    }

    [[nodiscard]] auto getWindGust() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.windGust;
    }

    [[nodiscard]] auto getWindDirection() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.windDirection;
    }

    [[nodiscard]] auto getDewPoint() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.dewPoint;
    }

    [[nodiscard]] auto getCloudCover() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.cloudCover;
    }

    [[nodiscard]] auto getRainRate() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.rainRate;
    }

    [[nodiscard]] auto getSkyBrightness() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.skyBrightness;
    }

    [[nodiscard]] auto getStarFWHM() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.starFWHM;
    }

    [[nodiscard]] auto getVisibility() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.visibility;
    }

    [[nodiscard]] auto getElevation() const -> std::optional<double> {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.elevation;
    }

    [[nodiscard]] auto getDewWarningStatus() const -> DewWarningStatus {
        std::lock_guard<std::mutex> lock(dewMutex_);
        return dewWarningStatus_;
    }

    [[nodiscard]] auto isDewWarning() const -> bool {
        std::lock_guard<std::mutex> lock(dewMutex_);
        return dewWarningStatus_.warning;
    }

    void setDewWarningCallback(DewWarningCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        dewWarningCallback_ = std::move(callback);
    }

    [[nodiscard]] auto getSafetyStatus() const -> SafetyStatus {
        std::lock_guard<std::mutex> lock(safetyMutex_);
        return safetyStatus_;
    }

    [[nodiscard]] auto isSafe() const -> bool {
        std::lock_guard<std::mutex> lock(safetyMutex_);
        return safetyStatus_ == SafetyStatus::Safe;
    }

    [[nodiscard]] auto isUnsafe() const -> bool {
        std::lock_guard<std::mutex> lock(safetyMutex_);
        return safetyStatus_ == SafetyStatus::Unsafe;
    }

    void setSafetyStatusCallback(SafetyStatusCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        safetyStatusCallback_ = std::move(callback);
    }

    auto setAutoRefreshEnabled(bool enabled) -> DeviceResult<bool> {
        std::lock_guard<std::mutex> lock(refreshMutex_);
        refreshControl_.enabled = enabled;

        LOG_F(INFO, "INDIGO Weather[{}]: Auto-refresh {}",
              parent_->getINDIGODeviceName(),
              enabled ? "enabled" : "disabled");

        return true;
    }

    [[nodiscard]] auto isAutoRefreshEnabled() const -> bool {
        std::lock_guard<std::mutex> lock(refreshMutex_);
        return refreshControl_.enabled;
    }

    auto setRefreshInterval(std::chrono::seconds interval)
        -> DeviceResult<bool> {
        if (interval.count() < 1) {
            return DeviceResult<bool>::error("Interval must be at least 1 second");
        }

        std::lock_guard<std::mutex> lock(refreshMutex_);
        refreshControl_.interval = interval;

        LOG_F(INFO, "INDIGO Weather[{}]: Refresh interval set to {}s",
              parent_->getINDIGODeviceName(), interval.count());

        return true;
    }

    [[nodiscard]] auto getRefreshInterval() const -> std::chrono::seconds {
        std::lock_guard<std::mutex> lock(refreshMutex_);
        return refreshControl_.interval;
    }

    auto refresh() -> DeviceResult<bool> {
        // Request property refresh from INDIGO
        auto result = parent_->setNumberProperty("AUX_WEATHER_REFRESH",
                                                 "REFRESH", 1.0);

        if (!result.has_value()) {
            // If specific refresh property doesn't exist, try to request all
            // properties
            LOG_F(WARNING, "INDIGO Weather[{}]: AUX_WEATHER_REFRESH not found",
                  parent_->getINDIGODeviceName());
        }

        std::lock_guard<std::mutex> lock(refreshMutex_);
        refreshControl_.lastUpdate = std::chrono::system_clock::now();

        return true;
    }

    [[nodiscard]] auto getRefreshControl() const -> WeatherRefreshControl {
        std::lock_guard<std::mutex> lock(refreshMutex_);
        return refreshControl_;
    }

    [[nodiscard]] auto getLastUpdateTime() const
        -> std::chrono::system_clock::time_point {
        std::lock_guard<std::mutex> lock(refreshMutex_);
        return refreshControl_.lastUpdate;
    }

    // ==================== Data Availability Checks ====================

    [[nodiscard]] auto hasTemperature() const -> bool {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.temperature.has_value();
    }

    [[nodiscard]] auto hasHumidity() const -> bool {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.humidity.has_value();
    }

    [[nodiscard]] auto hasPressure() const -> bool {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.pressure.has_value();
    }

    [[nodiscard]] auto hasWindData() const -> bool {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.windSpeed.has_value() ||
               weatherParams_.windDirection.has_value();
    }

    [[nodiscard]] auto hasDewPoint() const -> bool {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.dewPoint.has_value();
    }

    [[nodiscard]] auto hasCloudCover() const -> bool {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.cloudCover.has_value();
    }

    [[nodiscard]] auto hasRainDetection() const -> bool {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.rainRate.has_value();
    }

    [[nodiscard]] auto hasSkyMonitoring() const -> bool {
        std::lock_guard<std::mutex> lock(weatherMutex_);
        return weatherParams_.skyBrightness.has_value() ||
               weatherParams_.starFWHM.has_value();
    }

    // ==================== Utility ====================

    [[nodiscard]] auto getCapabilities() const -> json {
        json caps = json::object();

        caps["hasTemperature"] = hasTemperature();
        caps["hasHumidity"] = hasHumidity();
        caps["hasPressure"] = hasPressure();
        caps["hasWindData"] = hasWindData();
        caps["hasDewPoint"] = hasDewPoint();
        caps["hasCloudCover"] = hasCloudCover();
        caps["hasRainDetection"] = hasRainDetection();
        caps["hasSkyMonitoring"] = hasSkyMonitoring();

        return caps;
    }

    [[nodiscard]] auto getStatus() const -> json {
        json status = json::object();

        {
            std::lock_guard<std::mutex> lock(weatherMutex_);

            if (weatherParams_.temperature) {
                status["temperature"] = {
                    {"value", weatherParams_.temperature.value()},
                    {"unit", "Celsius"}
                };
            }

            if (weatherParams_.humidity) {
                status["humidity"] = {
                    {"value", weatherParams_.humidity.value()},
                    {"unit", "%"}
                };
            }

            if (weatherParams_.pressure) {
                status["pressure"] = {
                    {"value", weatherParams_.pressure.value()},
                    {"unit", "mbar"}
                };
            }

            if (weatherParams_.windSpeed) {
                status["windSpeed"] = {
                    {"value", weatherParams_.windSpeed.value()},
                    {"unit", "m/s"}
                };
            }

            if (weatherParams_.windGust) {
                status["windGust"] = {
                    {"value", weatherParams_.windGust.value()},
                    {"unit", "m/s"}
                };
            }

            if (weatherParams_.windDirection) {
                status["windDirection"] = {
                    {"value", weatherParams_.windDirection.value()},
                    {"unit", "degrees"}
                };
            }

            if (weatherParams_.dewPoint) {
                status["dewPoint"] = {
                    {"value", weatherParams_.dewPoint.value()},
                    {"unit", "Celsius"}
                };
            }

            if (weatherParams_.cloudCover) {
                status["cloudCover"] = {
                    {"value", weatherParams_.cloudCover.value()},
                    {"unit", "%"}
                };
            }

            if (weatherParams_.rainRate) {
                status["rainRate"] = {
                    {"value", weatherParams_.rainRate.value()},
                    {"unit", "mm/h"}
                };
            }

            if (weatherParams_.skyBrightness) {
                status["skyBrightness"] = {
                    {"value", weatherParams_.skyBrightness.value()},
                    {"unit", "mag/arcsec^2"}
                };
            }

            if (weatherParams_.starFWHM) {
                status["starFWHM"] = {
                    {"value", weatherParams_.starFWHM.value()},
                    {"unit", "arcsec"}
                };
            }

            if (weatherParams_.visibility) {
                status["visibility"] = {
                    {"value", weatherParams_.visibility.value()},
                    {"unit", "km"}
                };
            }

            if (weatherParams_.elevation) {
                status["elevation"] = {
                    {"value", weatherParams_.elevation.value()},
                    {"unit", "meters"}
                };
            }
        }

        {
            std::lock_guard<std::mutex> lock(dewMutex_);
            status["dewWarning"] = {
                {"active", dewWarningStatus_.warning},
                {"dewPoint", dewWarningStatus_.dewPoint},
                {"airTemperature", dewWarningStatus_.airTemperature},
                {"dewDelta", dewWarningStatus_.dewDelta}
            };
        }

        {
            std::lock_guard<std::mutex> lock(safetyMutex_);
            status["safetyStatus"] =
                std::string(safetyStatusToString(safetyStatus_));
        }

        {
            std::lock_guard<std::mutex> lock(refreshMutex_);
            status["autoRefreshEnabled"] = refreshControl_.enabled;
            status["refreshIntervalSeconds"] = refreshControl_.interval.count();
        }

        return status;
    }

private:
    void updateWeatherParameters(const Property& property) {
        std::lock_guard<std::mutex> lock(weatherMutex_);

        // Parse INDIGO weather properties
        for (const auto& elem : property.numberElements) {
            if (elem.name == "TEMPERATURE" || elem.name == "TEMP") {
                weatherParams_.temperature = elem.value;
            } else if (elem.name == "HUMIDITY") {
                weatherParams_.humidity = elem.value;
            } else if (elem.name == "PRESSURE") {
                weatherParams_.pressure = elem.value;
            } else if (elem.name == "WIND_SPEED" ||
                       elem.name == "WIND_SPD") {
                weatherParams_.windSpeed = elem.value;
            } else if (elem.name == "WIND_GUST" ||
                       elem.name == "GUST_SPEED") {
                weatherParams_.windGust = elem.value;
            } else if (elem.name == "WIND_DIRECTION" ||
                       elem.name == "WIND_DIR") {
                weatherParams_.windDirection = elem.value;
            } else if (elem.name == "DEW_POINT" ||
                       elem.name == "DEWPOINT") {
                weatherParams_.dewPoint = elem.value;
            } else if (elem.name == "CLOUD_COVER") {
                weatherParams_.cloudCover = elem.value;
            } else if (elem.name == "RAIN" || elem.name == "RAIN_RATE") {
                weatherParams_.rainRate = elem.value;
            } else if (elem.name == "SKY_BRIGHTNESS" ||
                       elem.name == "SKY_BRIGHT") {
                weatherParams_.skyBrightness = elem.value;
            } else if (elem.name == "STAR_FWHM" || elem.name == "FWHM") {
                weatherParams_.starFWHM = elem.value;
            } else if (elem.name == "VISIBILITY") {
                weatherParams_.visibility = elem.value;
            } else if (elem.name == "ELEVATION") {
                weatherParams_.elevation = elem.value;
            }
        }

        std::lock_guard<std::mutex> refreshLock(refreshMutex_);
        refreshControl_.lastUpdate = std::chrono::system_clock::now();
    }

    void updateDewWarning(const Property& property) {
        bool newWarning = false;

        // Check light elements for dew warning
        for (const auto& elem : property.lightElements) {
            if (elem.name == "WARNING") {
                newWarning =
                    (elem.state == PropertyState::Alert);
            }
        }

        std::lock_guard<std::mutex> lock(dewMutex_);

        // Calculate dew delta if we have temperature and dew point
        if (weatherParams_.temperature &&
            weatherParams_.dewPoint) {
            dewWarningStatus_.airTemperature =
                weatherParams_.temperature.value();
            dewWarningStatus_.dewPoint =
                weatherParams_.dewPoint.value();
            dewWarningStatus_.dewDelta =
                dewWarningStatus_.airTemperature -
                dewWarningStatus_.dewPoint;
        }

        bool previousWarning = dewWarningStatus_.warning;
        dewWarningStatus_.warning = newWarning;

        if (previousWarning != newWarning) {
            auto callback = [this, newWarning]() {
                std::lock_guard<std::mutex> cbLock(callbackMutex_);
                if (dewWarningCallback_) {
                    auto status = getDewWarningStatus();
                    dewWarningCallback_(newWarning, status);
                }
            };

            // Execute callback in background or on caller thread
            // For now, just call it directly with lock
            std::lock_guard<std::mutex> cbLock(callbackMutex_);
            if (dewWarningCallback_) {
                dewWarningCallback_(newWarning, dewWarningStatus_);
            }
        }
    }

    void updateSafetyStatus(const Property& property) {
        SafetyStatus newStatus = SafetyStatus::Unknown;

        // Check switch elements for safety status
        for (const auto& elem : property.switchElements) {
            if (elem.name == "SAFE" && elem.value) {
                newStatus = SafetyStatus::Safe;
                break;
            } else if (elem.name == "UNSAFE" && elem.value) {
                newStatus = SafetyStatus::Unsafe;
                break;
            }
        }

        std::lock_guard<std::mutex> lock(safetyMutex_);
        SafetyStatus previousStatus = safetyStatus_;
        safetyStatus_ = newStatus;

        if (previousStatus != newStatus) {
            std::lock_guard<std::mutex> cbLock(callbackMutex_);
            if (safetyStatusCallback_) {
                safetyStatusCallback_(newStatus);
            }
        }
    }

    INDIGOWeather* parent_;

    // Weather data
    mutable std::mutex weatherMutex_;
    WeatherParameters weatherParams_;

    // Dew warning
    mutable std::mutex dewMutex_;
    DewWarningStatus dewWarningStatus_;

    // Safety status
    mutable std::mutex safetyMutex_;
    SafetyStatus safetyStatus_{SafetyStatus::Unknown};

    // Refresh control
    mutable std::mutex refreshMutex_;
    WeatherRefreshControl refreshControl_;

    // Callbacks
    mutable std::mutex callbackMutex_;
    DewWarningCallback dewWarningCallback_;
    SafetyStatusCallback safetyStatusCallback_;
};

// ============================================================================
// INDIGOWeather Interface
// ============================================================================

INDIGOWeather::INDIGOWeather(const std::string& deviceName)
    : INDIGODeviceBase(deviceName, "Weather Station"),
      weatherImpl_(std::make_unique<Impl>(this)) {}

INDIGOWeather::~INDIGOWeather() = default;

auto INDIGOWeather::getWeatherParameters() const -> WeatherParameters {
    return weatherImpl_->getWeatherParameters();
}

auto INDIGOWeather::getTemperature() const -> std::optional<double> {
    return weatherImpl_->getTemperature();
}

auto INDIGOWeather::getHumidity() const -> std::optional<double> {
    return weatherImpl_->getHumidity();
}

auto INDIGOWeather::getPressure() const -> std::optional<double> {
    return weatherImpl_->getPressure();
}

auto INDIGOWeather::getWindSpeed() const -> std::optional<double> {
    return weatherImpl_->getWindSpeed();
}

auto INDIGOWeather::getWindGust() const -> std::optional<double> {
    return weatherImpl_->getWindGust();
}

auto INDIGOWeather::getWindDirection() const -> std::optional<double> {
    return weatherImpl_->getWindDirection();
}

auto INDIGOWeather::getDewPoint() const -> std::optional<double> {
    return weatherImpl_->getDewPoint();
}

auto INDIGOWeather::getCloudCover() const -> std::optional<double> {
    return weatherImpl_->getCloudCover();
}

auto INDIGOWeather::getRainRate() const -> std::optional<double> {
    return weatherImpl_->getRainRate();
}

auto INDIGOWeather::getSkyBrightness() const -> std::optional<double> {
    return weatherImpl_->getSkyBrightness();
}

auto INDIGOWeather::getStarFWHM() const -> std::optional<double> {
    return weatherImpl_->getStarFWHM();
}

auto INDIGOWeather::getVisibility() const -> std::optional<double> {
    return weatherImpl_->getVisibility();
}

auto INDIGOWeather::getElevation() const -> std::optional<double> {
    return weatherImpl_->getElevation();
}

auto INDIGOWeather::getDewWarningStatus() const -> DewWarningStatus {
    return weatherImpl_->getDewWarningStatus();
}

auto INDIGOWeather::isDewWarning() const -> bool {
    return weatherImpl_->isDewWarning();
}

void INDIGOWeather::setDewWarningCallback(DewWarningCallback callback) {
    weatherImpl_->setDewWarningCallback(std::move(callback));
}

auto INDIGOWeather::getSafetyStatus() const -> SafetyStatus {
    return weatherImpl_->getSafetyStatus();
}

auto INDIGOWeather::isSafe() const -> bool {
    return weatherImpl_->isSafe();
}

auto INDIGOWeather::isUnsafe() const -> bool {
    return weatherImpl_->isUnsafe();
}

void INDIGOWeather::setSafetyStatusCallback(SafetyStatusCallback callback) {
    weatherImpl_->setSafetyStatusCallback(std::move(callback));
}

auto INDIGOWeather::setAutoRefreshEnabled(bool enabled) -> DeviceResult<bool> {
    return weatherImpl_->setAutoRefreshEnabled(enabled);
}

auto INDIGOWeather::isAutoRefreshEnabled() const -> bool {
    return weatherImpl_->isAutoRefreshEnabled();
}

auto INDIGOWeather::setRefreshInterval(std::chrono::seconds interval)
    -> DeviceResult<bool> {
    return weatherImpl_->setRefreshInterval(interval);
}

auto INDIGOWeather::getRefreshInterval() const -> std::chrono::seconds {
    return weatherImpl_->getRefreshInterval();
}

auto INDIGOWeather::refresh() -> DeviceResult<bool> {
    return weatherImpl_->refresh();
}

auto INDIGOWeather::getRefreshControl() const -> WeatherRefreshControl {
    return weatherImpl_->getRefreshControl();
}

auto INDIGOWeather::getLastUpdateTime() const
    -> std::chrono::system_clock::time_point {
    return weatherImpl_->getLastUpdateTime();
}

auto INDIGOWeather::hasTemperature() const -> bool {
    return weatherImpl_->hasTemperature();
}

auto INDIGOWeather::hasHumidity() const -> bool {
    return weatherImpl_->hasHumidity();
}

auto INDIGOWeather::hasPressure() const -> bool {
    return weatherImpl_->hasPressure();
}

auto INDIGOWeather::hasWindData() const -> bool {
    return weatherImpl_->hasWindData();
}

auto INDIGOWeather::hasDewPoint() const -> bool {
    return weatherImpl_->hasDewPoint();
}

auto INDIGOWeather::hasCloudCover() const -> bool {
    return weatherImpl_->hasCloudCover();
}

auto INDIGOWeather::hasRainDetection() const -> bool {
    return weatherImpl_->hasRainDetection();
}

auto INDIGOWeather::hasSkyMonitoring() const -> bool {
    return weatherImpl_->hasSkyMonitoring();
}

auto INDIGOWeather::getCapabilities() const -> json {
    return weatherImpl_->getCapabilities();
}

auto INDIGOWeather::getStatus() const -> json {
    return weatherImpl_->getStatus();
}

void INDIGOWeather::onConnected() {
    weatherImpl_->onConnected();
}

void INDIGOWeather::onDisconnected() {
    weatherImpl_->onDisconnected();
}

void INDIGOWeather::onPropertyUpdated(const Property& property) {
    weatherImpl_->onPropertyUpdated(property);
}

}  // namespace lithium::client::indigo
