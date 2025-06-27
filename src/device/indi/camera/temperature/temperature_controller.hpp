#ifndef LITHIUM_INDI_CAMERA_TEMPERATURE_CONTROLLER_HPP
#define LITHIUM_INDI_CAMERA_TEMPERATURE_CONTROLLER_HPP

#include "../component_base.hpp"
#include "../../../template/camera.hpp"

#include <atomic>
#include <optional>

namespace lithium::device::indi::camera {

/**
 * @brief Temperature control component for INDI cameras
 * 
 * This component handles camera cooling operations, temperature
 * monitoring, and thermal management. Uses the global TemperatureInfo
 * struct from the camera template for consistency.
 */
class TemperatureController : public ComponentBase {
public:
    explicit TemperatureController(std::shared_ptr<INDICameraCore> core);
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
