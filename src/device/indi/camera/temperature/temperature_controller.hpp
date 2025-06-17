#ifndef LITHIUM_INDI_CAMERA_TEMPERATURE_CONTROLLER_HPP
#define LITHIUM_INDI_CAMERA_TEMPERATURE_CONTROLLER_HPP

#include "../component_base.hpp"
#include "../../../template/camera.hpp"

#include <atomic>
#include <optional>

namespace lithium::device::indi::camera {
// 温度信息结构体定义，修复 hasCooler 未定义问题
struct TemperatureInfo {
    double current = 0.0;
    double target = 0.0;
    double power = 0.0;
    bool hasCooler = false;
};

/**
 * @brief Temperature control component for INDI cameras
 * 
 * This component handles camera cooling operations, temperature
 * monitoring, and thermal management.
 */
class TemperatureController : public ComponentBase {
public:
    explicit TemperatureController(INDICameraCore* core);
    ~TemperatureController() override = default;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto handleProperty(INDI::Property property) -> bool override;

    // Temperature control
    auto startCooling(double targetTemp) -> bool;
    auto stopCooling() -> bool;
    auto isCoolerOn() const -> bool;
    auto setTemperature(double temperature) -> bool;
    auto getTemperature() const -> std::optional<double>;
    auto getTemperatureInfo() const -> TemperatureInfo;
    auto getCoolingPower() const -> std::optional<double>;
    auto hasCooler() const -> bool;

private:
    // Temperature state
    std::atomic_bool isCooling_{false};
    std::atomic<double> currentTemperature_{0.0};
    std::atomic<double> targetTemperature_{0.0};
    std::atomic<double> coolingPower_{0.0};
    
    // Temperature info structure
    TemperatureInfo temperatureInfo_;
    
    // Property handlers
    void handleTemperatureProperty(INDI::Property property);
    void handleCoolerProperty(INDI::Property property);
    void handleCoolerPowerProperty(INDI::Property property);
    
    // Helper methods
    void updateTemperatureInfo();
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_TEMPERATURE_CONTROLLER_HPP
