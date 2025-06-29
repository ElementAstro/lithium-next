/*
 * dome_weather.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Weather - Weather Monitoring Component

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_DOME_WEATHER_HPP
#define LITHIUM_DEVICE_INDI_DOME_WEATHER_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <functional>
#include <mutex>
#include <optional>
#include <string>

// Forward declarations
class INDIDomeClient;

/**
 * @brief Weather condition data structure
 *
 * Holds current weather parameters and safety state.
 */
struct WeatherCondition {
    bool safe{true};           ///< True if weather is safe for operation
    double temperature{20.0};  ///< Temperature in Celsius
    double humidity{50.0};     ///< Relative humidity in percent
    double windSpeed{0.0};     ///< Wind speed in m/s
    bool rainDetected{false};  ///< True if rain is detected
};

/**
 * @brief Weather safety limits structure
 *
 * Defines operational weather limits for dome safety automation.
 */
struct WeatherLimits {
    double maxWindSpeed{15.0};     ///< Maximum safe wind speed (m/s)
    double minTemperature{-10.0};  ///< Minimum safe temperature (°C)
    double maxTemperature{50.0};   ///< Maximum safe temperature (°C)
    double maxHumidity{85.0};      ///< Maximum safe humidity (%)
    bool rainProtection{true};     ///< True to enable rain protection
};

/**
 * @brief Dome weather monitoring component
 *
 * Handles weather monitoring, safety checks, and weather-based automation for
 * INDI domes. Provides callback registration, device synchronization, and
 * safety automation.
 */
class DomeWeatherManager {
public:
    /**
     * @brief Construct a DomeWeatherManager for a given INDI dome client.
     * @param client Pointer to the associated INDIDomeClient.
     */
    explicit DomeWeatherManager(INDIDomeClient* client);
    ~DomeWeatherManager() = default;

    /**
     * @brief Enable or disable weather monitoring.
     * @param enable True to enable, false to disable.
     * @return True if the operation succeeded.
     */
    auto enableWeatherMonitoring(bool enable) -> bool;

    /**
     * @brief Check if weather monitoring is enabled.
     * @return True if enabled, false otherwise.
     */
    auto isWeatherMonitoringEnabled() -> bool;

    /**
     * @brief Check if current weather is safe for dome operation.
     * @return True if safe, false otherwise.
     */
    auto isWeatherSafe() -> bool;

    /**
     * @brief Get the current weather condition (if available).
     * @return Optional WeatherCondition struct.
     */
    auto getWeatherCondition() -> std::optional<WeatherCondition>;

    /**
     * @brief Set operational weather safety limits.
     * @param limits WeatherLimits struct.
     * @return True if set successfully.
     */
    auto setWeatherLimits(const WeatherLimits& limits) -> bool;

    /**
     * @brief Get the current weather safety limits.
     * @return WeatherLimits struct.
     */
    auto getWeatherLimits() -> WeatherLimits;

    /**
     * @brief Enable or disable auto-close on unsafe weather.
     * @param enable True to enable, false to disable.
     * @return True if set successfully.
     */
    auto enableAutoCloseOnUnsafeWeather(bool enable) -> bool;

    /**
     * @brief Check if auto-close on unsafe weather is enabled.
     * @return True if enabled, false otherwise.
     */
    auto isAutoCloseEnabled() -> bool;

    /**
     * @brief Handle an INDI property update related to weather.
     * @param property The INDI property to process.
     */
    void handleWeatherProperty(const INDI::Property& property);

    /**
     * @brief Synchronize internal state with the device's current properties.
     */
    void synchronizeWithDevice();

    /**
     * @brief Check current weather status and update safety state.
     */
    void checkWeatherStatus();

    /**
     * @brief Perform safety checks and automation (e.g., auto-close dome).
     */
    void performSafetyChecks();

    /**
     * @brief Register a callback for weather safety events.
     * @param callback Function to call on weather safety changes.
     */
    using WeatherCallback =
        std::function<void(bool safe, const std::string& details)>;
    void setWeatherCallback(WeatherCallback callback);

private:
    INDIDomeClient* client_;            ///< Associated INDI dome client
    mutable std::mutex weather_mutex_;  ///< Mutex for thread-safe state access

    bool weather_monitoring_enabled_{
        false};                      ///< Weather monitoring enabled flag
    bool weather_safe_{true};        ///< Current weather safety state
    bool auto_close_enabled_{true};  ///< Auto-close on unsafe weather flag
    WeatherLimits weather_limits_;   ///< Current weather safety limits

    WeatherCallback weather_callback_;  ///< Registered weather event callback

    /**
     * @brief Notify the registered callback of a weather safety event.
     * @param safe True if weather is safe, false otherwise.
     * @param details Details about the weather event.
     */
    void notifyWeatherEvent(bool safe, const std::string& details);

    /**
     * @brief Check if wind speed is within safe limits.
     * @param windSpeed Wind speed in m/s.
     * @return True if safe, false otherwise.
     */
    auto checkWindSpeed(double windSpeed) -> bool;

    /**
     * @brief Check if temperature is within safe limits.
     * @param temperature Temperature in Celsius.
     * @return True if safe, false otherwise.
     */
    auto checkTemperature(double temperature) -> bool;

    /**
     * @brief Check if humidity is within safe limits.
     * @param humidity Relative humidity in percent.
     * @return True if safe, false otherwise.
     */
    auto checkHumidity(double humidity) -> bool;

    /**
     * @brief Check if rain detection is within safe limits.
     * @param rainDetected True if rain is detected.
     * @return True if safe, false otherwise.
     */
    auto checkRain(bool rainDetected) -> bool;

    /**
     * @brief Get the INDI property for weather data (number type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    auto getWeatherProperty() -> INDI::PropertyViewNumber*;

    /**
     * @brief Get the INDI property for rain detection (switch type).
     * @return Pointer to the property view, or nullptr if not found.
     */
    auto getRainProperty() -> INDI::PropertyViewSwitch*;
};

#endif  // LITHIUM_DEVICE_INDI_DOME_WEATHER_HPP
