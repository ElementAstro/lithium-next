/*
 * indigo_weather.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Weather Station Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_WEATHER_HPP
#define LITHIUM_CLIENT_INDIGO_WEATHER_HPP

#include "indigo_device_base.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lithium::client::indigo {

/**
 * @brief Weather safety status
 */
enum class SafetyStatus : uint8_t {
    Safe,      ///< Safe for observation
    Unsafe,    ///< Not safe for observation
    Unknown    ///< Status unknown or not available
};

/**
 * @brief Weather parameters structure
 */
struct WeatherParameters {
    std::optional<double> temperature;      // Celsius
    std::optional<double> humidity;         // Percentage (0-100)
    std::optional<double> pressure;         // mbar/hPa
    std::optional<double> windSpeed;        // m/s
    std::optional<double> windGust;         // m/s
    std::optional<double> windDirection;    // degrees (0-360)
    std::optional<double> dewPoint;         // Celsius
    std::optional<double> cloudCover;       // Percentage (0-100)
    std::optional<double> rainRate;         // mm/h
    std::optional<double> skyBrightness;    // Magnitude/arcsec^2
    std::optional<double> starFWHM;         // Arcseconds
    std::optional<double> visibility;       // km
    std::optional<double> elevation;        // meters (above sea level)
};

/**
 * @brief Dew warning status
 */
struct DewWarningStatus {
    bool warning{false};        // Dew warning active
    double dewPoint{0.0};       // Current dew point
    double airTemperature{0.0}; // Current air temperature
    double dewDelta{0.0};       // Difference: airTemperature - dewPoint
};

/**
 * @brief Weather data refresh control
 */
struct WeatherRefreshControl {
    bool enabled{true};
    std::chrono::seconds interval{30};  // Refresh interval in seconds
    std::chrono::system_clock::time_point lastUpdate;
};

/**
 * @brief INDIGO Weather Station Device
 *
 * Provides weather monitoring functionality for astronomical observatories:
 * - Temperature, humidity, and pressure monitoring
 * - Wind speed and direction measurements
 * - Dew warning system
 * - Cloud cover and rain detection
 * - Safety status for observation
 * - Automatic refresh control
 *
 * INDIGO weather properties:
 * - AUX_WEATHER (number vector) - Various weather parameters
 * - AUX_DEW_WARNING (light) - Dew warning status indicator
 * - WEATHER_PARAMETERS (number vector) - Extended weather parameters
 * - WEATHER_SAFETY (switch) - Safety status (SAFE/UNSAFE)
 */
class INDIGOWeather : public INDIGODeviceBase {
public:
    /**
     * @brief Constructor
     * @param deviceName INDIGO device name
     */
    explicit INDIGOWeather(const std::string& deviceName);

    /**
     * @brief Destructor
     */
    ~INDIGOWeather() override;

    // ==================== Weather Data Retrieval ====================

    /**
     * @brief Get all weather parameters
     */
    [[nodiscard]] auto getWeatherParameters() const -> WeatherParameters;

    /**
     * @brief Get current temperature in Celsius
     */
    [[nodiscard]] auto getTemperature() const -> std::optional<double>;

    /**
     * @brief Get current humidity percentage (0-100)
     */
    [[nodiscard]] auto getHumidity() const -> std::optional<double>;

    /**
     * @brief Get current pressure in mbar/hPa
     */
    [[nodiscard]] auto getPressure() const -> std::optional<double>;

    /**
     * @brief Get current wind speed in m/s
     */
    [[nodiscard]] auto getWindSpeed() const -> std::optional<double>;

    /**
     * @brief Get wind gust speed in m/s
     */
    [[nodiscard]] auto getWindGust() const -> std::optional<double>;

    /**
     * @brief Get wind direction in degrees (0-360)
     */
    [[nodiscard]] auto getWindDirection() const -> std::optional<double>;

    /**
     * @brief Get dew point in Celsius
     */
    [[nodiscard]] auto getDewPoint() const -> std::optional<double>;

    /**
     * @brief Get cloud cover percentage (0-100)
     */
    [[nodiscard]] auto getCloudCover() const -> std::optional<double>;

    /**
     * @brief Get rain rate in mm/h
     */
    [[nodiscard]] auto getRainRate() const -> std::optional<double>;

    /**
     * @brief Get sky brightness in magnitude/arcsec^2
     */
    [[nodiscard]] auto getSkyBrightness() const -> std::optional<double>;

    /**
     * @brief Get average star FWHM in arcseconds
     */
    [[nodiscard]] auto getStarFWHM() const -> std::optional<double>;

    /**
     * @brief Get visibility in kilometers
     */
    [[nodiscard]] auto getVisibility() const -> std::optional<double>;

    /**
     * @brief Get site elevation in meters above sea level
     */
    [[nodiscard]] auto getElevation() const -> std::optional<double>;

    // ==================== Dew Warning ====================

    /**
     * @brief Get dew warning status
     */
    [[nodiscard]] auto getDewWarningStatus() const -> DewWarningStatus;

    /**
     * @brief Check if dew warning is active
     */
    [[nodiscard]] auto isDewWarning() const -> bool;

    /**
     * @brief Get dew warning callback type
     */
    using DewWarningCallback = std::function<void(bool warning, const DewWarningStatus&)>;

    /**
     * @brief Register callback for dew warning changes
     */
    void setDewWarningCallback(DewWarningCallback callback);

    // ==================== Safety Status ====================

    /**
     * @brief Get current safety status
     */
    [[nodiscard]] auto getSafetyStatus() const -> SafetyStatus;

    /**
     * @brief Check if conditions are safe for observation
     */
    [[nodiscard]] auto isSafe() const -> bool;

    /**
     * @brief Check if conditions are unsafe for observation
     */
    [[nodiscard]] auto isUnsafe() const -> bool;

    /**
     * @brief Get safety status callback type
     */
    using SafetyStatusCallback = std::function<void(SafetyStatus status)>;

    /**
     * @brief Register callback for safety status changes
     */
    void setSafetyStatusCallback(SafetyStatusCallback callback);

    // ==================== Refresh Control ====================

    /**
     * @brief Enable automatic weather data refresh
     * @param enabled Enable or disable auto-refresh
     */
    auto setAutoRefreshEnabled(bool enabled) -> DeviceResult<bool>;

    /**
     * @brief Check if auto-refresh is enabled
     */
    [[nodiscard]] auto isAutoRefreshEnabled() const -> bool;

    /**
     * @brief Set auto-refresh interval
     * @param interval Refresh interval in seconds
     */
    auto setRefreshInterval(std::chrono::seconds interval) -> DeviceResult<bool>;

    /**
     * @brief Get auto-refresh interval
     */
    [[nodiscard]] auto getRefreshInterval() const -> std::chrono::seconds;

    /**
     * @brief Manually refresh all weather data
     */
    auto refresh() -> DeviceResult<bool>;

    /**
     * @brief Get weather refresh control settings
     */
    [[nodiscard]] auto getRefreshControl() const -> WeatherRefreshControl;

    /**
     * @brief Get time of last weather data update
     */
    [[nodiscard]] auto getLastUpdateTime() const -> std::chrono::system_clock::time_point;

    // ==================== Weather Data Availability ====================

    /**
     * @brief Check if temperature data is available
     */
    [[nodiscard]] auto hasTemperature() const -> bool;

    /**
     * @brief Check if humidity data is available
     */
    [[nodiscard]] auto hasHumidity() const -> bool;

    /**
     * @brief Check if pressure data is available
     */
    [[nodiscard]] auto hasPressure() const -> bool;

    /**
     * @brief Check if wind data is available
     */
    [[nodiscard]] auto hasWindData() const -> bool;

    /**
     * @brief Check if dew point data is available
     */
    [[nodiscard]] auto hasDewPoint() const -> bool;

    /**
     * @brief Check if cloud cover data is available
     */
    [[nodiscard]] auto hasCloudCover() const -> bool;

    /**
     * @brief Check if rain detection is available
     */
    [[nodiscard]] auto hasRainDetection() const -> bool;

    /**
     * @brief Check if sky monitoring is available
     */
    [[nodiscard]] auto hasSkyMonitoring() const -> bool;

    // ==================== Utility ====================

    /**
     * @brief Get weather capabilities as JSON
     */
    [[nodiscard]] auto getCapabilities() const -> json;

    /**
     * @brief Get current weather status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json;

    /**
     * @brief Compute relative humidity from temperature and dew point
     */
    [[nodiscard]] static auto computeRelativeHumidity(
        double temperature, double dewPoint) -> double;

    /**
     * @brief Compute dew point from temperature and humidity
     */
    [[nodiscard]] static auto computeDewPoint(double temperature,
                                              double humidity) -> double;

protected:
    void onConnected() override;
    void onDisconnected() override;
    void onPropertyUpdated(const Property& property) override;

private:
    class Impl;
    std::unique_ptr<Impl> weatherImpl_;
};

// ============================================================================
// Safety status conversion
// ============================================================================

[[nodiscard]] constexpr auto safetyStatusToString(SafetyStatus status) noexcept
    -> std::string_view {
    switch (status) {
        case SafetyStatus::Safe: return "Safe";
        case SafetyStatus::Unsafe: return "Unsafe";
        case SafetyStatus::Unknown: return "Unknown";
        default: return "Unknown";
    }
}

[[nodiscard]] auto safetyStatusFromString(std::string_view str)
    -> SafetyStatus;

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_WEATHER_HPP
