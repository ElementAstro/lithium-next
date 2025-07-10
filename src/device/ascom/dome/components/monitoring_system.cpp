/*
 * monitoring_system.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Monitoring System Implementation

*************************************************/

#include "monitoring_system.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>

namespace lithium::ascom::dome::components {

MonitoringSystem::MonitoringSystem(std::shared_ptr<HardwareInterface> hardware)
    : hardware_interface_(std::move(hardware)) {
    spdlog::debug("MonitoringSystem initialized");
    start_time_ = std::chrono::steady_clock::now();
    last_health_check_ = start_time_;
}

MonitoringSystem::~MonitoringSystem() {
    stopMonitoring();
}

auto MonitoringSystem::startMonitoring() -> bool {
    if (is_monitoring_) {
        spdlog::warn("Monitoring already started");
        return true;
    }
    
    if (!hardware_interface_) {
        spdlog::error("Hardware interface not available");
        return false;
    }
    
    spdlog::info("Starting dome monitoring system");
    
    is_monitoring_ = true;
    monitoring_thread_ = std::make_unique<std::thread>([this]() {
        monitoringLoop();
    });
    
    return true;
}

auto MonitoringSystem::stopMonitoring() -> bool {
    if (!is_monitoring_) {
        return true;
    }
    
    spdlog::info("Stopping dome monitoring system");
    
    is_monitoring_ = false;
    
    if (monitoring_thread_ && monitoring_thread_->joinable()) {
        monitoring_thread_->join();
    }
    
    return true;
}

auto MonitoringSystem::isMonitoring() -> bool {
    return is_monitoring_;
}

auto MonitoringSystem::setMonitoringInterval(std::chrono::milliseconds interval) {
    monitoring_interval_ = interval;
}

auto MonitoringSystem::getLatestData() -> MonitoringData {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return latest_data_;
}

auto MonitoringSystem::getHistoricalData(int count) -> std::vector<MonitoringData> {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (count <= 0 || historical_data_.empty()) {
        return {};
    }
    
    int start_idx = std::max(0, static_cast<int>(historical_data_.size()) - count);
    return std::vector<MonitoringData>(
        historical_data_.begin() + start_idx,
        historical_data_.end()
    );
}

auto MonitoringSystem::getDataSince(std::chrono::steady_clock::time_point since) -> std::vector<MonitoringData> {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    std::vector<MonitoringData> result;
    for (const auto& data : historical_data_) {
        if (data.timestamp >= since) {
            result.push_back(data);
        }
    }
    
    return result;
}

auto MonitoringSystem::setTemperatureThreshold(double min_temp, double max_temp) {
    min_temperature_ = min_temp;
    max_temperature_ = max_temp;
    spdlog::info("Temperature threshold set: {:.1f}°C to {:.1f}°C", min_temp, max_temp);
}

auto MonitoringSystem::setHumidityThreshold(double min_humidity, double max_humidity) {
    min_humidity_ = min_humidity;
    max_humidity_ = max_humidity;
    spdlog::info("Humidity threshold set: {:.1f}% to {:.1f}%", min_humidity, max_humidity);
}

auto MonitoringSystem::setPowerThreshold(double min_voltage, double max_voltage) {
    min_voltage_ = min_voltage;
    max_voltage_ = max_voltage;
    spdlog::info("Power threshold set: {:.1f}V to {:.1f}V", min_voltage, max_voltage);
}

auto MonitoringSystem::setCurrentThreshold(double max_current) {
    max_current_ = max_current;
    spdlog::info("Current threshold set: {:.1f}A", max_current);
}

auto MonitoringSystem::performHealthCheck() -> bool {
    spdlog::debug("Performing system health check");
    
    last_health_check_ = std::chrono::steady_clock::now();
    
    bool motor_ok = checkMotorHealth();
    bool shutter_ok = checkShutterHealth();
    bool power_ok = checkPowerHealth();
    bool temp_ok = checkTemperatureHealth();
    
    bool overall_health = motor_ok && shutter_ok && power_ok && temp_ok;
    
    if (!overall_health) {
        notifyAlert("health_check", "System health check failed");
    }
    
    return overall_health;
}

auto MonitoringSystem::getSystemHealth() -> std::unordered_map<std::string, bool> {
    return {
        {"motor", checkMotorHealth()},
        {"shutter", checkShutterHealth()},
        {"power", checkPowerHealth()},
        {"temperature", checkTemperatureHealth()}
    };
}

auto MonitoringSystem::getLastHealthCheck() -> std::chrono::steady_clock::time_point {
    return last_health_check_;
}

void MonitoringSystem::setMonitoringCallback(MonitoringCallback callback) {
    monitoring_callback_ = std::move(callback);
}

void MonitoringSystem::setAlertCallback(AlertCallback callback) {
    alert_callback_ = std::move(callback);
}

auto MonitoringSystem::getAverageTemperature(std::chrono::minutes duration) -> double {
    auto since = std::chrono::steady_clock::now() - duration;
    auto data = getDataSince(since);
    
    if (data.empty()) {
        return 0.0;
    }
    
    double sum = std::accumulate(data.begin(), data.end(), 0.0,
        [](double acc, const MonitoringData& d) {
            return acc + d.temperature;
        });
    
    return sum / data.size();
}

auto MonitoringSystem::getAverageHumidity(std::chrono::minutes duration) -> double {
    auto since = std::chrono::steady_clock::now() - duration;
    auto data = getDataSince(since);
    
    if (data.empty()) {
        return 0.0;
    }
    
    double sum = std::accumulate(data.begin(), data.end(), 0.0,
        [](double acc, const MonitoringData& d) {
            return acc + d.humidity;
        });
    
    return sum / data.size();
}

auto MonitoringSystem::getAveragePower(std::chrono::minutes duration) -> double {
    auto since = std::chrono::steady_clock::now() - duration;
    auto data = getDataSince(since);
    
    if (data.empty()) {
        return 0.0;
    }
    
    double sum = std::accumulate(data.begin(), data.end(), 0.0,
        [](double acc, const MonitoringData& d) {
            return acc + d.power_voltage;
        });
    
    return sum / data.size();
}

auto MonitoringSystem::getUptime() -> std::chrono::seconds {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
}

void MonitoringSystem::monitoringLoop() {
    spdlog::debug("Starting monitoring loop");
    
    while (is_monitoring_) {
        try {
            auto data = collectData();
            
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                latest_data_ = data;
                addToHistory(data);
            }
            
            checkThresholds(data);
            
            if (monitoring_callback_) {
                monitoring_callback_(data);
            }
            
            // Perform periodic health check (every 5 minutes)
            auto now = std::chrono::steady_clock::now();
            if (now - last_health_check_ > std::chrono::minutes(5)) {
                performHealthCheck();
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Monitoring loop error: {}", e.what());
            notifyAlert("monitoring_error", e.what());
        }
        
        std::this_thread::sleep_for(monitoring_interval_);
    }
    
    spdlog::debug("Monitoring loop stopped");
}

auto MonitoringSystem::collectData() -> MonitoringData {
    MonitoringData data;
    data.timestamp = std::chrono::steady_clock::now();
    
    // In a real implementation, this would collect actual sensor data
    // For now, we'll use placeholder values
    data.temperature = 25.0;  // Celsius
    data.humidity = 50.0;     // Percentage
    data.power_voltage = 12.0; // Volts
    data.power_current = 2.0;  // Amperes
    data.motor_status = true;
    data.shutter_status = true;
    
    return data;
}

void MonitoringSystem::checkThresholds(const MonitoringData& data) {
    // Temperature check
    if (data.temperature < min_temperature_ || data.temperature > max_temperature_) {
        notifyAlert("temperature", 
            "Temperature out of range: " + std::to_string(data.temperature) + "°C");
    }
    
    // Humidity check
    if (data.humidity < min_humidity_ || data.humidity > max_humidity_) {
        notifyAlert("humidity",
            "Humidity out of range: " + std::to_string(data.humidity) + "%");
    }
    
    // Power check
    if (data.power_voltage < min_voltage_ || data.power_voltage > max_voltage_) {
        notifyAlert("power",
            "Voltage out of range: " + std::to_string(data.power_voltage) + "V");
    }
    
    // Current check
    if (data.power_current > max_current_) {
        notifyAlert("current",
            "Current too high: " + std::to_string(data.power_current) + "A");
    }
}

void MonitoringSystem::addToHistory(const MonitoringData& data) {
    historical_data_.push_back(data);
    
    // Keep only the last MAX_HISTORICAL_DATA entries
    if (historical_data_.size() > MAX_HISTORICAL_DATA) {
        historical_data_.erase(historical_data_.begin());
    }
}

void MonitoringSystem::notifyAlert(const std::string& alert_type, const std::string& message) {
    spdlog::warn("Alert [{}]: {}", alert_type, message);
    if (alert_callback_) {
        alert_callback_(alert_type, message);
    }
}

auto MonitoringSystem::checkMotorHealth() -> bool {
    // Check motor status and current draw
    auto data = getLatestData();
    return data.motor_status && data.power_current < max_current_;
}

auto MonitoringSystem::checkShutterHealth() -> bool {
    // Check shutter status
    auto data = getLatestData();
    return data.shutter_status;
}

auto MonitoringSystem::checkPowerHealth() -> bool {
    // Check power supply voltage
    auto data = getLatestData();
    return data.power_voltage >= min_voltage_ && data.power_voltage <= max_voltage_;
}

auto MonitoringSystem::checkTemperatureHealth() -> bool {
    // Check temperature is within safe range
    auto data = getLatestData();
    return data.temperature >= min_temperature_ && data.temperature <= max_temperature_;
}

} // namespace lithium::ascom::dome::components
