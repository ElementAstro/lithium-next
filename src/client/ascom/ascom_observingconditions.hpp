/*
 * ascom_observingconditions.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Observing Conditions (Weather) Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_OBSERVINGCONDITIONS_HPP
#define LITHIUM_CLIENT_ASCOM_OBSERVINGCONDITIONS_HPP

#include "ascom_device_base.hpp"

namespace lithium::client::ascom {

/**
 * @brief Weather data container
 */
struct WeatherData {
    double cloudCover{0.0};       // %
    double dewPoint{0.0};         // Celsius
    double humidity{0.0};         // %
    double pressure{0.0};         // hPa
    double rainRate{0.0};         // mm/hr
    double skyBrightness{0.0};    // lux
    double skyQuality{0.0};       // mag/arcsec^2
    double skyTemperature{0.0};   // Celsius
    double starFWHM{0.0};         // arcsec
    double temperature{0.0};      // Celsius
    double windDirection{0.0};    // degrees
    double windGust{0.0};         // m/s
    double windSpeed{0.0};        // m/s

    [[nodiscard]] auto toJson() const -> json {
        return {{"cloudCover", cloudCover},
                {"dewPoint", dewPoint},
                {"humidity", humidity},
                {"pressure", pressure},
                {"rainRate", rainRate},
                {"skyBrightness", skyBrightness},
                {"skyQuality", skyQuality},
                {"skyTemperature", skyTemperature},
                {"starFWHM", starFWHM},
                {"temperature", temperature},
                {"windDirection", windDirection},
                {"windGust", windGust},
                {"windSpeed", windSpeed}};
    }
};

/**
 * @brief Sensor description
 */
struct SensorDescription {
    std::string name;
    std::string description;
    double timeSinceLastUpdate{0.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"name", name},
                {"description", description},
                {"timeSinceLastUpdate", timeSinceLastUpdate}};
    }
};

/**
 * @brief ASCOM Observing Conditions Device
 *
 * Provides weather/environmental data including:
 * - Temperature, humidity, pressure
 * - Wind speed and direction
 * - Cloud cover, sky quality
 * - Dew point, rain rate
 */
class ASCOMObservingConditions : public ASCOMDeviceBase {
public:
    explicit ASCOMObservingConditions(std::string name, int deviceNumber = 0);
    ~ASCOMObservingConditions() override;

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "ObservingConditions";
    }

    auto connect(int timeout = DEFAULT_TIMEOUT_MS) -> bool override;

    // ==================== Weather Data ====================

    [[nodiscard]] auto getWeatherData() -> WeatherData;

    [[nodiscard]] auto getCloudCover() -> std::optional<double>;
    [[nodiscard]] auto getDewPoint() -> std::optional<double>;
    [[nodiscard]] auto getHumidity() -> std::optional<double>;
    [[nodiscard]] auto getPressure() -> std::optional<double>;
    [[nodiscard]] auto getRainRate() -> std::optional<double>;
    [[nodiscard]] auto getSkyBrightness() -> std::optional<double>;
    [[nodiscard]] auto getSkyQuality() -> std::optional<double>;
    [[nodiscard]] auto getSkyTemperature() -> std::optional<double>;
    [[nodiscard]] auto getStarFWHM() -> std::optional<double>;
    [[nodiscard]] auto getTemperature() -> std::optional<double>;
    [[nodiscard]] auto getWindDirection() -> std::optional<double>;
    [[nodiscard]] auto getWindGust() -> std::optional<double>;
    [[nodiscard]] auto getWindSpeed() -> std::optional<double>;

    // ==================== Sensor Info ====================

    [[nodiscard]] auto getSensorDescription(const std::string& sensor)
        -> std::optional<std::string>;
    [[nodiscard]] auto getTimeSinceLastUpdate(const std::string& sensor)
        -> std::optional<double>;

    // ==================== Refresh ====================

    auto refresh() -> bool;
    [[nodiscard]] auto getAveragePeriod() -> double;
    auto setAveragePeriod(double hours) -> bool;

    // ==================== Status ====================

    [[nodiscard]] auto getStatus() const -> json override;

private:
    WeatherData weatherData_;
    mutable std::mutex weatherMutex_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_OBSERVINGCONDITIONS_HPP
