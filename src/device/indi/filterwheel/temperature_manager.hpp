#ifndef LITHIUM_INDI_FILTERWHEEL_TEMPERATURE_MANAGER_HPP
#define LITHIUM_INDI_FILTERWHEEL_TEMPERATURE_MANAGER_HPP

#include "component_base.hpp"
#include <optional>

namespace lithium::device::indi::filterwheel {

/**
 * @brief Manages temperature monitoring for INDI filter wheels.
 *
 * This component handles temperature sensor readings and monitoring for
 * filter wheels that support temperature sensors. Not all filter wheels
 * have temperature sensors, so this component gracefully handles devices
 * without temperature capabilities.
 */
class TemperatureManager : public FilterWheelComponentBase {
public:
    /**
     * @brief Constructor with shared core.
     * @param core Shared pointer to the INDIFilterWheelCore
     */
    explicit TemperatureManager(std::shared_ptr<INDIFilterWheelCore> core);

    /**
     * @brief Virtual destructor.
     */
    ~TemperatureManager() override = default;

    /**
     * @brief Initialize the temperature manager.
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize() override;

    /**
     * @brief Cleanup resources and shutdown the component.
     */
    void shutdown() override;

    /**
     * @brief Get the component's name for logging and identification.
     * @return Name of the component.
     */
    std::string getComponentName() const override { return "TemperatureManager"; }

    /**
     * @brief Check if the filter wheel has a temperature sensor.
     * @return true if temperature sensor is available, false otherwise.
     */
    bool hasTemperatureSensor() const;

    /**
     * @brief Get current temperature reading.
     * @return Temperature in degrees Celsius if available, nullopt otherwise.
     */
    std::optional<double> getTemperature() const;

    /**
     * @brief Set up temperature property monitoring.
     */
    void setupTemperatureWatchers();

    /**
     * @brief Handle temperature property updates.
     * @param property The INDI temperature property.
     */
    void handleTemperatureProperty(const INDI::PropertyNumber& property);

private:
    bool initialized_{false};
    bool hasSensor_ = false;
    std::optional<double> currentTemperature_;

    // Temperature monitoring
    void checkTemperatureCapability();
};

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_TEMPERATURE_MANAGER_HPP
