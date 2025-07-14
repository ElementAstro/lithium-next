/*
 * weather_monitor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Weather Monitoring Component

*************************************************/

#pragma once

#include <string>
#include <atomic>
#include <functional>
#include <chrono>
#include <vector>
#include <optional>
#include <thread>
#include <memory>

namespace lithium::ascom::dome::components {

/**
 * @brief Weather conditions enumeration
 */
enum class WeatherCondition {
    CLEAR,
    CLOUDY,
    OVERCAST,
    RAIN,
    SNOW,
    WIND,
    UNKNOWN
};

/**
 * @brief Weather data structure
 */
struct WeatherData {
    double temperature{0.0};        // Celsius
    double humidity{0.0};           // Percentage
    double pressure{0.0};           // hPa
    double wind_speed{0.0};         // m/s
    double wind_direction{0.0};     // degrees
    double rain_rate{0.0};          // mm/hour
    WeatherCondition condition{WeatherCondition::UNKNOWN};
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Weather safety thresholds
 */
struct WeatherThresholds {
    double max_wind_speed{15.0};    // m/s
    double max_rain_rate{0.1};      // mm/hour
    double min_temperature{-20.0};  // Celsius
    double max_temperature{50.0};   // Celsius
    double max_humidity{95.0};      // Percentage
};

/**
 * @brief Weather Monitoring Component for ASCOM Dome
 */
class WeatherMonitor {
public:
    explicit WeatherMonitor();
    virtual ~WeatherMonitor();

    // === Weather Monitoring ===
    auto startMonitoring() -> bool;
    auto stopMonitoring() -> bool;
    auto isMonitoring() -> bool;

    // === Weather Data ===
    auto getCurrentWeather() -> WeatherData;
    auto getWeatherHistory(int hours) -> std::vector<WeatherData>;
    auto isSafeToOperate() -> bool;
    auto getWeatherStatus() -> std::string;

    // === Safety Configuration ===
    auto setWeatherThresholds(const WeatherThresholds& thresholds) -> bool;
    auto getWeatherThresholds() -> WeatherThresholds;
    auto enableWeatherSafety(bool enable) -> bool;
    auto isWeatherSafetyEnabled() -> bool;

    // === Callbacks ===
    auto setWeatherCallback(std::function<void(const WeatherData&)> callback) -> void;
    auto setSafetyCallback(std::function<void(bool, const std::string&)> callback) -> void;

    // === External Weather Sources ===
    auto addWeatherSource(const std::string& source_url) -> bool;
    auto removeWeatherSource(const std::string& source_url) -> bool;
    auto updateFromExternalSource() -> bool;

private:
    std::atomic<bool> is_monitoring_{false};
    std::atomic<bool> safety_enabled_{true};
    std::atomic<bool> is_safe_{true};

    WeatherData current_weather_;
    WeatherThresholds thresholds_;
    std::vector<WeatherData> weather_history_;
    std::vector<std::string> weather_sources_;

    std::function<void(const WeatherData&)> weather_callback_;
    std::function<void(bool, const std::string&)> safety_callback_;

    std::unique_ptr<std::thread> monitoring_thread_;
    std::atomic<bool> stop_monitoring_{false};

    auto monitoringLoop() -> void;
    auto checkWeatherSafety(const WeatherData& data) -> bool;
    auto fetchExternalWeatherData() -> std::optional<WeatherData>;
    auto parseWeatherData(const std::string& json_data) -> std::optional<WeatherData>;
    auto getConditionString(WeatherCondition condition) -> std::string;
};

} // namespace lithium::ascom::dome::components
