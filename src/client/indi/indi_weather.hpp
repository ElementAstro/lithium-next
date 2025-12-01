/*
 * indi_weather.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Weather Station Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_WEATHER_HPP
#define LITHIUM_CLIENT_INDI_WEATHER_HPP

#include "indi_device_base.hpp"

namespace lithium::client::indi {

/**
 * @brief Weather state enumeration
 */
enum class WeatherState : uint8_t { Idle, Updating, Alert, Error, Unknown };

/**
 * @brief Weather parameter status
 */
enum class ParameterStatus : uint8_t { Ok, Warning, Alert, Unknown };

/**
 * @brief Weather parameter information
 */
struct WeatherParameter {
    std::string name;
    std::string label;
    double value{0.0};
    double min{0.0};
    double max{100.0};
    double warningMin{0.0};
    double warningMax{100.0};
    double alertMin{0.0};
    double alertMax{100.0};
    ParameterStatus status{ParameterStatus::Unknown};

    [[nodiscard]] auto toJson() const -> json {
        return {{"name", name},
                {"label", label},
                {"value", value},
                {"min", min},
                {"max", max},
                {"warningMin", warningMin},
                {"warningMax", warningMax},
                {"alertMin", alertMin},
                {"alertMax", alertMax},
                {"status", static_cast<int>(status)}};
    }
};

/**
 * @brief Weather data container
 */
struct WeatherData {
    double temperature{0.0};      // Temperature in Celsius
    double humidity{0.0};         // Relative humidity %
    double pressure{0.0};         // Atmospheric pressure hPa
    double windSpeed{0.0};        // Wind speed m/s
    double windGust{0.0};         // Wind gust m/s
    double windDirection{0.0};    // Wind direction degrees
    double dewPoint{0.0};         // Dew point Celsius
    double cloudCover{0.0};       // Cloud cover %
    double skyQuality{0.0};       // Sky quality mag/arcsec^2
    double skyBrightness{0.0};    // Sky brightness
    double rainRate{0.0};         // Rain rate mm/h
    bool isRaining{false};
    bool isSafe{true};

    [[nodiscard]] auto toJson() const -> json {
        return {{"temperature", temperature},
                {"humidity", humidity},
                {"pressure", pressure},
                {"windSpeed", windSpeed},
                {"windGust", windGust},
                {"windDirection", windDirection},
                {"dewPoint", dewPoint},
                {"cloudCover", cloudCover},
                {"skyQuality", skyQuality},
                {"skyBrightness", skyBrightness},
                {"rainRate", rainRate},
                {"isRaining", isRaining},
                {"isSafe", isSafe}};
    }
};

/**
 * @brief Location information
 */
struct LocationInfo {
    double latitude{0.0};
    double longitude{0.0};
    double elevation{0.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"latitude", latitude},
                {"longitude", longitude},
                {"elevation", elevation}};
    }
};

/**
 * @brief INDI Weather Station Device
 *
 * Provides weather station functionality including:
 * - Temperature, humidity, pressure
 * - Wind speed and direction
 * - Sky quality
 * - Safety status
 */
class INDIWeather : public INDIDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new INDI Weather
     * @param name Device name
     */
    explicit INDIWeather(std::string name);

    /**
     * @brief Destructor
     */
    ~INDIWeather() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Weather";
    }

    // ==================== Connection ====================

    auto connect(const std::string& deviceName, int timeout = DEFAULT_TIMEOUT_MS,
                 int maxRetry = 3) -> bool override;

    auto disconnect() -> bool override;

    // ==================== Weather Data ====================

    /**
     * @brief Get current weather data
     * @return Weather data
     */
    [[nodiscard]] auto getWeatherData() const -> WeatherData;

    /**
     * @brief Get temperature
     * @return Temperature in Celsius
     */
    [[nodiscard]] auto getTemperature() const -> std::optional<double>;

    /**
     * @brief Get humidity
     * @return Relative humidity %
     */
    [[nodiscard]] auto getHumidity() const -> std::optional<double>;

    /**
     * @brief Get pressure
     * @return Atmospheric pressure hPa
     */
    [[nodiscard]] auto getPressure() const -> std::optional<double>;

    /**
     * @brief Get wind speed
     * @return Wind speed m/s
     */
    [[nodiscard]] auto getWindSpeed() const -> std::optional<double>;

    /**
     * @brief Get wind direction
     * @return Wind direction degrees
     */
    [[nodiscard]] auto getWindDirection() const -> std::optional<double>;

    /**
     * @brief Get dew point
     * @return Dew point Celsius
     */
    [[nodiscard]] auto getDewPoint() const -> std::optional<double>;

    /**
     * @brief Get sky quality
     * @return Sky quality mag/arcsec^2
     */
    [[nodiscard]] auto getSkyQuality() const -> std::optional<double>;

    /**
     * @brief Check if it's raining
     * @return true if raining
     */
    [[nodiscard]] auto isRaining() const -> bool;

    /**
     * @brief Check if conditions are safe
     * @return true if safe
     */
    [[nodiscard]] auto isSafe() const -> bool;

    // ==================== Parameters ====================

    /**
     * @brief Get weather parameter by name
     * @param name Parameter name
     * @return Parameter, or nullopt if not found
     */
    [[nodiscard]] auto getParameter(const std::string& name) const
        -> std::optional<WeatherParameter>;

    /**
     * @brief Get all weather parameters
     * @return Vector of parameters
     */
    [[nodiscard]] auto getParameters() const -> std::vector<WeatherParameter>;

    // ==================== Location ====================

    /**
     * @brief Get location information
     * @return Location info
     */
    [[nodiscard]] auto getLocation() const -> LocationInfo;

    /**
     * @brief Set location
     * @param lat Latitude
     * @param lon Longitude
     * @param elev Elevation
     * @return true if set successfully
     */
    auto setLocation(double lat, double lon, double elev) -> bool;

    // ==================== Refresh ====================

    /**
     * @brief Refresh weather data
     * @return true if refresh started
     */
    auto refresh() -> bool;

    /**
     * @brief Set refresh period
     * @param seconds Refresh period in seconds
     * @return true if set successfully
     */
    auto setRefreshPeriod(int seconds) -> bool;

    /**
     * @brief Get refresh period
     * @return Refresh period in seconds
     */
    [[nodiscard]] auto getRefreshPeriod() const -> int;

    // ==================== Status ====================

    /**
     * @brief Get weather state
     * @return Weather state
     */
    [[nodiscard]] auto getWeatherState() const -> WeatherState;

    /**
     * @brief Get weather status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

protected:
    // ==================== Property Handlers ====================

    void onPropertyDefined(const INDIProperty& property) override;
    void onPropertyUpdated(const INDIProperty& property) override;

private:
    // ==================== Internal Methods ====================

    void handleWeatherProperty(const INDIProperty& property);
    void handleLocationProperty(const INDIProperty& property);
    void handleParameterProperty(const INDIProperty& property);

    void setupPropertyWatchers();
    void updateSafetyStatus();

    // ==================== Member Variables ====================

    // Weather state
    std::atomic<WeatherState> weatherState_{WeatherState::Idle};

    // Weather data
    WeatherData weatherData_;
    mutable std::mutex weatherMutex_;

    // Parameters
    std::vector<WeatherParameter> parameters_;
    mutable std::mutex parametersMutex_;

    // Location
    LocationInfo location_;
    mutable std::mutex locationMutex_;

    // Refresh
    std::atomic<int> refreshPeriod_{60};
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_WEATHER_HPP
