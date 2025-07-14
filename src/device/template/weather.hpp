/*
 * weather.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomWeatherStation device following INDI architecture

*************************************************/

#pragma once

#include "device.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

enum class WeatherState {
    OK,
    WARNING,
    ALERT,
    ERROR,
    UNKNOWN
};

enum class WeatherCondition {
    CLEAR,
    CLOUDY,
    OVERCAST,
    RAIN,
    SNOW,
    FOG,
    STORM,
    UNKNOWN
};

// Weather parameters structure
struct WeatherParameters {
    // Temperature
    std::optional<double> temperature;      // Celsius
    std::optional<double> humidity;         // Percentage 0-100
    std::optional<double> pressure;         // hPa
    std::optional<double> dewPoint;         // Celsius

    // Wind
    std::optional<double> windSpeed;        // m/s
    std::optional<double> windDirection;    // degrees
    std::optional<double> windGust;         // m/s

    // Precipitation
    std::optional<double> rainRate;         // mm/hr
    std::optional<double> cloudCover;       // Percentage 0-100
    std::optional<double> skyTemperature;   // Celsius

    // Light and sky quality
    std::optional<double> skyBrightness;    // mag/arcsec²
    std::optional<double> seeing;           // arcseconds
    std::optional<double> transparency;     // Percentage 0-100

    // Additional sensors
    std::optional<double> uvIndex;
    std::optional<double> solarRadiation;   // W/m²
    std::optional<double> lightLevel;       // lux

    std::chrono::system_clock::time_point timestamp;
} ATOM_ALIGNAS(128);

// Weather limits for safety
struct WeatherLimits {
    // Temperature limits
    std::optional<double> minTemperature{-20.0};
    std::optional<double> maxTemperature{50.0};

    // Humidity limits
    std::optional<double> maxHumidity{95.0};

    // Wind limits
    std::optional<double> maxWindSpeed{15.0};      // m/s
    std::optional<double> maxWindGust{20.0};       // m/s

    // Precipitation limits
    std::optional<double> maxRainRate{0.1};        // mm/hr

    // Cloud cover limits
    std::optional<double> maxCloudCover{80.0};     // Percentage

    // Sky temperature limits
    std::optional<double> minSkyTemperature{-40.0}; // Celsius

    // Seeing limits
    std::optional<double> maxSeeing{5.0};          // arcseconds
    std::optional<double> minTransparency{30.0};   // Percentage
} ATOM_ALIGNAS(128);

// Weather capabilities
struct WeatherCapabilities {
    bool hasTemperature{false};
    bool hasHumidity{false};
    bool hasPressure{false};
    bool hasDewPoint{false};
    bool hasWind{false};
    bool hasRain{false};
    bool hasCloudSensor{false};
    bool hasSkyTemperature{false};
    bool hasSkyQuality{false};
    bool hasUV{false};
    bool hasSolarRadiation{false};
    bool hasLightSensor{false};
    bool canCalibrateAll{false};
} ATOM_ALIGNAS(16);

class AtomWeatherStation : public AtomDriver {
public:
    explicit AtomWeatherStation(std::string name) : AtomDriver(std::move(name)) {
        setType("Weather");
        weather_parameters_.timestamp = std::chrono::system_clock::now();
    }

    ~AtomWeatherStation() override = default;

    // Capabilities
    const WeatherCapabilities& getWeatherCapabilities() const { return weather_capabilities_; }
    void setWeatherCapabilities(const WeatherCapabilities& caps) { weather_capabilities_ = caps; }

    // Limits
    const WeatherLimits& getWeatherLimits() const { return weather_limits_; }
    void setWeatherLimits(const WeatherLimits& limits) { weather_limits_ = limits; }

    // State
    WeatherState getWeatherState() const { return weather_state_; }
    WeatherCondition getWeatherCondition() const { return weather_condition_; }

    // Main weather data access
    virtual auto getWeatherParameters() -> WeatherParameters = 0;
    virtual auto updateWeatherData() -> bool = 0;
    virtual auto getLastUpdateTime() -> std::chrono::system_clock::time_point = 0;

    // Individual parameter access
    virtual auto getTemperature() -> std::optional<double> = 0;
    virtual auto getHumidity() -> std::optional<double> = 0;
    virtual auto getPressure() -> std::optional<double> = 0;
    virtual auto getDewPoint() -> std::optional<double> = 0;
    virtual auto getWindSpeed() -> std::optional<double> = 0;
    virtual auto getWindDirection() -> std::optional<double> = 0;
    virtual auto getWindGust() -> std::optional<double> = 0;
    virtual auto getRainRate() -> std::optional<double> = 0;
    virtual auto getCloudCover() -> std::optional<double> = 0;
    virtual auto getSkyTemperature() -> std::optional<double> = 0;
    virtual auto getSkyBrightness() -> std::optional<double> = 0;
    virtual auto getSeeing() -> std::optional<double> = 0;
    virtual auto getTransparency() -> std::optional<double> = 0;

    // Safety checks
    virtual auto isSafeToObserve() -> bool = 0;
    virtual auto getWarningConditions() -> std::vector<std::string> = 0;
    virtual auto getAlertConditions() -> std::vector<std::string> = 0;
    virtual auto checkWeatherLimits() -> WeatherState = 0;

    // Historical data
    virtual auto getHistoricalData(std::chrono::minutes duration) -> std::vector<WeatherParameters> = 0;
    virtual auto getAverageParameters(std::chrono::minutes duration) -> WeatherParameters = 0;
    virtual auto getMinMaxParameters(std::chrono::minutes duration) -> std::pair<WeatherParameters, WeatherParameters> = 0;

    // Calibration
    virtual auto calibrateTemperature(double reference) -> bool = 0;
    virtual auto calibrateHumidity(double reference) -> bool = 0;
    virtual auto calibratePressure(double reference) -> bool = 0;
    virtual auto calibrateAll() -> bool = 0;
    virtual auto resetCalibration() -> bool = 0;

    // Data logging
    virtual auto enableDataLogging(bool enable) -> bool = 0;
    virtual auto isDataLoggingEnabled() -> bool = 0;
    virtual auto getLogFilePath() -> std::string = 0;
    virtual auto setLogFilePath(const std::string& path) -> bool = 0;
    virtual auto exportData(const std::string& filename, std::chrono::hours duration) -> bool = 0;

    // Monitoring and alerts
    virtual auto setUpdateInterval(std::chrono::seconds interval) -> bool = 0;
    virtual auto getUpdateInterval() -> std::chrono::seconds = 0;
    virtual auto enableAlerts(bool enable) -> bool = 0;
    virtual auto areAlertsEnabled() -> bool = 0;

    // Weather condition analysis
    virtual auto analyzeWeatherTrend() -> std::string = 0;
    virtual auto predictWeatherCondition(std::chrono::minutes ahead) -> WeatherCondition = 0;
    virtual auto getRecommendations() -> std::vector<std::string> = 0;

    // Sensor management
    virtual auto getSensorList() -> std::vector<std::string> = 0;
    virtual auto getSensorStatus(const std::string& sensor) -> bool = 0;
    virtual auto calibrateSensor(const std::string& sensor) -> bool = 0;
    virtual auto resetSensor(const std::string& sensor) -> bool = 0;

    // Event callbacks
    using WeatherCallback = std::function<void(const WeatherParameters&)>;
    using StateCallback = std::function<void(WeatherState state, const std::string& message)>;
    using AlertCallback = std::function<void(const std::string& alert)>;

    virtual void setWeatherCallback(WeatherCallback callback) { weather_callback_ = std::move(callback); }
    virtual void setStateCallback(StateCallback callback) { state_callback_ = std::move(callback); }
    virtual void setAlertCallback(AlertCallback callback) { alert_callback_ = std::move(callback); }

    // Utility methods
    virtual auto temperatureToString(double temp, bool celsius = true) -> std::string;
    virtual auto windDirectionToString(double degrees) -> std::string;
    virtual auto weatherStateToString(WeatherState state) -> std::string;
    virtual auto weatherConditionToString(WeatherCondition condition) -> std::string;

protected:
    WeatherState weather_state_{WeatherState::UNKNOWN};
    WeatherCondition weather_condition_{WeatherCondition::UNKNOWN};
    WeatherCapabilities weather_capabilities_;
    WeatherLimits weather_limits_;
    WeatherParameters weather_parameters_;

    // Configuration
    std::chrono::seconds update_interval_{30};
    bool data_logging_enabled_{false};
    bool alerts_enabled_{true};
    std::string log_file_path_;

    // Historical data storage
    std::vector<WeatherParameters> historical_data_;
    static constexpr size_t MAX_HISTORICAL_RECORDS = 2880; // 24 hours at 30s intervals

    // Callbacks
    WeatherCallback weather_callback_;
    StateCallback state_callback_;
    AlertCallback alert_callback_;

    // Utility methods
    virtual void updateWeatherState(WeatherState state) { weather_state_ = state; }
    virtual void updateWeatherCondition(WeatherCondition condition) { weather_condition_ = condition; }
    virtual void notifyWeatherUpdate(const WeatherParameters& params);
    virtual void notifyStateChange(WeatherState state, const std::string& message = "");
    virtual void notifyAlert(const std::string& alert);
    virtual void addHistoricalRecord(const WeatherParameters& params);
    virtual void cleanupHistoricalData();
};

// Inline utility implementations
inline auto AtomWeatherStation::temperatureToString(double temp, bool celsius) -> std::string {
    if (celsius) {
        return std::to_string(temp) + "°C";
    } else {
        return std::to_string(temp * 9.0 / 5.0 + 32.0) + "°F";
    }
}

inline auto AtomWeatherStation::windDirectionToString(double degrees) -> std::string {
    const std::array<std::string, 16> directions = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    int index = static_cast<int>((degrees + 11.25) / 22.5) % 16;
    return directions[index];
}
