/*
 * asi_temperature_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI camera temperature controller component

*************************************************/

#ifndef LITHIUM_ASI_CAMERA_TEMPERATURE_CONTROLLER_HPP
#define LITHIUM_ASI_CAMERA_TEMPERATURE_CONTROLLER_HPP

#include "../component_base.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <optional>

namespace lithium::device::asi::camera {

/**
 * @brief Temperature control component for ASI cameras
 * 
 * This component handles all temperature-related operations including
 * cooling control, temperature monitoring, and thermal management
 * using the ASI SDK.
 */
class TemperatureController : public ComponentBase {
public:
    explicit TemperatureController(ASICameraCore* core);
    ~TemperatureController() override;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto onCameraStateChanged(CameraState state) -> void override;

    // Temperature control
    auto startCooling(double targetTemp) -> bool;
    auto stopCooling() -> bool;
    auto isCoolerOn() const -> bool;
    auto getTemperature() const -> std::optional<double>;
    auto getTargetTemperature() const -> double;
    auto getCoolingPower() const -> double;

    // Temperature monitoring
    auto enableTemperatureMonitoring(bool enable) -> bool;
    auto isTemperatureMonitoringEnabled() const -> bool;
    auto getTemperatureHistory() const -> std::vector<std::pair<std::chrono::system_clock::time_point, double>>;
    auto clearTemperatureHistory() -> void;

    // Fan control (for cameras with fans)
    auto hasFan() const -> bool;
    auto enableFan(bool enable) -> bool;
    auto isFanEnabled() const -> bool;
    auto setFanSpeed(int speed) -> bool;
    auto getFanSpeed() const -> int;

    // Anti-dew heater (for cameras with heaters)
    auto hasAntiDewHeater() const -> bool;
    auto enableAntiDewHeater(bool enable) -> bool;
    auto isAntiDewHeaterEnabled() const -> bool;
    auto setAntiDewHeaterPower(int power) -> bool;
    auto getAntiDewHeaterPower() const -> int;

    // Temperature statistics
    auto getMinTemperature() const -> double;
    auto getMaxTemperature() const -> double;
    auto getAverageTemperature() const -> double;
    auto getTemperatureStability() const -> double;
    auto resetTemperatureStatistics() -> void;

private:
    // Temperature state
    std::atomic_bool coolerEnabled_{false};
    std::atomic_bool fanEnabled_{false};
    std::atomic_bool antiDewHeaterEnabled_{false};
    std::atomic_bool temperatureMonitoringEnabled_{true};
    
    double targetTemperature_{-10.0};
    double currentTemperature_{25.0};
    double coolingPower_{0.0};
    int fanSpeed_{0};
    int antiDewHeaterPower_{0};
    
    // Temperature monitoring
    std::thread temperatureThread_;
    mutable std::mutex temperatureMutex_;
    std::vector<std::pair<std::chrono::system_clock::time_point, double>> temperatureHistory_;
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
    
    // Temperature statistics
    double minTemperature_{100.0};
    double maxTemperature_{-100.0};
    double temperatureSum_{0.0};
    uint32_t temperatureCount_{0};
    
    // Hardware capabilities
    bool hasCooler_{false};
    bool hasFan_{false};
    bool hasAntiDewHeater_{false};
    
    // Private helper methods
    auto temperatureThreadFunction() -> void;
    auto updateTemperatureReading() -> bool;
    auto updateCoolingControl() -> bool;
    auto updateFanControl() -> bool;
    auto updateAntiDewHeater() -> bool;
    auto addTemperatureToHistory(double temperature) -> void;
    auto updateTemperatureStatistics(double temperature) -> void;
    auto detectHardwareCapabilities() -> void;
    auto isValidTemperature(double temperature) const -> bool;
    auto calculateTemperatureStability() const -> double;
};

} // namespace lithium::device::asi::camera

#endif // LITHIUM_ASI_CAMERA_TEMPERATURE_CONTROLLER_HPP
