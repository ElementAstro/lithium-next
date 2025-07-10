/*
 * weather_monitor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Weather Monitoring Component Implementation

*************************************************/

#include "weather_monitor.hpp"

#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>

namespace lithium::ascom::dome::components {

WeatherMonitor::WeatherMonitor() {
    spdlog::info("Initializing Weather Monitor");
    current_weather_.timestamp = std::chrono::system_clock::now();
}

WeatherMonitor::~WeatherMonitor() {
    spdlog::info("Destroying Weather Monitor");
    stopMonitoring();
}

auto WeatherMonitor::startMonitoring() -> bool {
    if (is_monitoring_.load()) {
        return true;
    }

    spdlog::info("Starting weather monitoring");
    
    is_monitoring_.store(true);
    stop_monitoring_.store(false);
    
    monitoring_thread_ = std::make_unique<std::thread>(&WeatherMonitor::monitoringLoop, this);
    
    return true;
}

auto WeatherMonitor::stopMonitoring() -> bool {
    if (!is_monitoring_.load()) {
        return true;
    }

    spdlog::info("Stopping weather monitoring");
    
    stop_monitoring_.store(true);
    is_monitoring_.store(false);
    
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    monitoring_thread_.reset();
    
    return true;
}

auto WeatherMonitor::isMonitoring() -> bool {
    return is_monitoring_.load();
}

auto WeatherMonitor::getCurrentWeather() -> WeatherData {
    return current_weather_;
}

auto WeatherMonitor::getWeatherHistory(int hours) -> std::vector<WeatherData> {
    std::vector<WeatherData> filtered_history;
    auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(hours);
    
    for (const auto& data : weather_history_) {
        if (data.timestamp >= cutoff_time) {
            filtered_history.push_back(data);
        }
    }
    
    return filtered_history;
}

auto WeatherMonitor::isSafeToOperate() -> bool {
    if (!safety_enabled_.load()) {
        return true;
    }
    
    return is_safe_.load();
}

auto WeatherMonitor::getWeatherStatus() -> std::string {
    if (!safety_enabled_.load()) {
        return "Weather safety disabled";
    }
    
    if (is_safe_.load()) {
        return "Weather conditions safe for operation";
    } else {
        return "Weather conditions unsafe - dome operations restricted";
    }
}

auto WeatherMonitor::setWeatherThresholds(const WeatherThresholds& thresholds) -> bool {
    thresholds_ = thresholds;
    spdlog::info("Updated weather safety thresholds");
    return true;
}

auto WeatherMonitor::getWeatherThresholds() -> WeatherThresholds {
    return thresholds_;
}

auto WeatherMonitor::enableWeatherSafety(bool enable) -> bool {
    safety_enabled_.store(enable);
    spdlog::info("{} weather safety monitoring", enable ? "Enabled" : "Disabled");
    return true;
}

auto WeatherMonitor::isWeatherSafetyEnabled() -> bool {
    return safety_enabled_.load();
}

auto WeatherMonitor::setWeatherCallback(std::function<void(const WeatherData&)> callback) -> void {
    weather_callback_ = callback;
}

auto WeatherMonitor::setSafetyCallback(std::function<void(bool, const std::string&)> callback) -> void {
    safety_callback_ = callback;
}

auto WeatherMonitor::addWeatherSource(const std::string& source_url) -> bool {
    weather_sources_.push_back(source_url);
    spdlog::info("Added weather source: {}", source_url);
    return true;
}

auto WeatherMonitor::removeWeatherSource(const std::string& source_url) -> bool {
    auto it = std::find(weather_sources_.begin(), weather_sources_.end(), source_url);
    if (it != weather_sources_.end()) {
        weather_sources_.erase(it);
        spdlog::info("Removed weather source: {}", source_url);
        return true;
    }
    return false;
}

auto WeatherMonitor::updateFromExternalSource() -> bool {
    auto weather_data = fetchExternalWeatherData();
    if (weather_data) {
        current_weather_ = *weather_data;
        return true;
    }
    return false;
}

auto WeatherMonitor::monitoringLoop() -> void {
    while (!stop_monitoring_.load()) {
        // Update weather data from external sources
        updateFromExternalSource();
        
        // Check safety conditions
        bool safe = checkWeatherSafety(current_weather_);
        bool previous_safe = is_safe_.load();
        is_safe_.store(safe);
        
        // Trigger callbacks
        if (weather_callback_) {
            weather_callback_(current_weather_);
        }
        
        if (safety_callback_ && safe != previous_safe) {
            safety_callback_(safe, safe ? "Weather conditions improved" : "Weather conditions deteriorated");
        }
        
        // Add to history (limit to last 24 hours)
        weather_history_.push_back(current_weather_);
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::hours(24);
        weather_history_.erase(
            std::remove_if(weather_history_.begin(), weather_history_.end(),
                          [cutoff_time](const WeatherData& data) {
                              return data.timestamp < cutoff_time;
                          }),
            weather_history_.end());
        
        std::this_thread::sleep_for(std::chrono::minutes(1)); // Update every minute
    }
}

auto WeatherMonitor::checkWeatherSafety(const WeatherData& data) -> bool {
    if (!safety_enabled_.load()) {
        return true;
    }
    
    // Check wind speed
    if (data.wind_speed > thresholds_.max_wind_speed) {
        spdlog::warn("Wind speed too high: {:.1f} m/s (max: {:.1f})", 
                     data.wind_speed, thresholds_.max_wind_speed);
        return false;
    }
    
    // Check rain rate
    if (data.rain_rate > thresholds_.max_rain_rate) {
        spdlog::warn("Rain rate too high: {:.1f} mm/h (max: {:.1f})", 
                     data.rain_rate, thresholds_.max_rain_rate);
        return false;
    }
    
    // Check temperature range
    if (data.temperature < thresholds_.min_temperature || 
        data.temperature > thresholds_.max_temperature) {
        spdlog::warn("Temperature out of range: {:.1f}°C (range: {:.1f} to {:.1f})", 
                     data.temperature, thresholds_.min_temperature, thresholds_.max_temperature);
        return false;
    }
    
    // Check humidity
    if (data.humidity > thresholds_.max_humidity) {
        spdlog::warn("Humidity too high: {:.1f}% (max: {:.1f})", 
                     data.humidity, thresholds_.max_humidity);
        return false;
    }
    
    return true;
}

auto WeatherMonitor::fetchExternalWeatherData() -> std::optional<WeatherData> {
    // TODO: Implement actual weather data fetching from external sources
    // This is a placeholder implementation
    
    WeatherData data;
    data.timestamp = std::chrono::system_clock::now();
    data.temperature = 20.0;
    data.humidity = 60.0;
    data.pressure = 1013.25;
    data.wind_speed = 5.0;
    data.wind_direction = 180.0;
    data.rain_rate = 0.0;
    data.condition = WeatherCondition::CLEAR;
    
    return data;
}

auto WeatherMonitor::parseWeatherData(const std::string& json_data) -> std::optional<WeatherData> {
    // TODO: Implement JSON parsing for weather data
    return std::nullopt;
}

auto WeatherMonitor::getConditionString(WeatherCondition condition) -> std::string {
    switch (condition) {
        case WeatherCondition::CLEAR: return "Clear";
        case WeatherCondition::CLOUDY: return "Cloudy";
        case WeatherCondition::OVERCAST: return "Overcast";
        case WeatherCondition::RAIN: return "Rain";
        case WeatherCondition::SNOW: return "Snow";
        case WeatherCondition::WIND: return "Windy";
        case WeatherCondition::UNKNOWN: return "Unknown";
        default: return "Invalid";
    }
}

} // namespace lithium::ascom::dome::components
