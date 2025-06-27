#ifndef LITHIUM_INDI_FOCUSER_TEMPERATURE_MANAGER_HPP
#define LITHIUM_INDI_FOCUSER_TEMPERATURE_MANAGER_HPP

#include "component_base.hpp"

namespace lithium::device::indi::focuser {

/**
 * @brief Manages temperature monitoring and compensation for the focuser
 * device.
 *
 * This class provides interfaces for reading temperature sensors,
 * enabling/disabling temperature compensation, and applying compensation logic
 * to maintain focus accuracy as temperature changes. It interacts with the
 * shared INDIFocuserCore and is designed to be used as a component in the focuser
 * control system.
 */
class TemperatureManager : public FocuserComponentBase {
public:
    /**
     * @brief Constructor with shared core.
     * @param core Shared pointer to the INDIFocuserCore
     */
    explicit TemperatureManager(std::shared_ptr<INDIFocuserCore> core);
    
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
     * @brief Shutdown and cleanup the component.
     */
    void shutdown() override;

    /**
     * @brief Get the component's name for logging and identification.
     * @return Name of the component.
     */
    std::string getComponentName() const override {
        return "TemperatureManager";
    }

    // Temperature monitoring

    /**
     * @brief Get the current external temperature from the focuser sensor, if
     * available.
     * @return Optional value containing the temperature in Celsius, or
     * std::nullopt if unavailable.
     */
    std::optional<double> getExternalTemperature() const;

    /**
     * @brief Get the current chip temperature from the focuser, if available.
     * @return Optional value containing the chip temperature in Celsius, or
     * std::nullopt if unavailable.
     */
    std::optional<double> getChipTemperature() const;

    /**
     * @brief Check if the focuser has a temperature sensor.
     * @return true if a temperature sensor is present, false otherwise.
     */
    bool hasTemperatureSensor() const;

    // Temperature compensation

    /**
     * @brief Get the current temperature compensation settings.
     * @return The TemperatureCompensation structure with current settings.
     */
    TemperatureCompensation getTemperatureCompensation() const;

    /**
     * @brief Set new temperature compensation parameters.
     * @param comp The new TemperatureCompensation settings to apply.
     * @return true if the settings were applied successfully, false otherwise.
     */
    bool setTemperatureCompensation(const TemperatureCompensation& comp);

    /**
     * @brief Enable or disable temperature compensation.
     * @param enable true to enable, false to disable.
     * @return true if the operation succeeded, false otherwise.
     */
    bool enableTemperatureCompensation(bool enable);

    /**
     * @brief Check if temperature compensation is currently enabled.
     * @return true if enabled, false otherwise.
     */
    bool isTemperatureCompensationEnabled() const;

    // Temperature-based auto adjustment

    /**
     * @brief Check and apply temperature compensation if needed based on the
     * latest readings.
     *
     * This method should be called periodically to ensure focus is maintained
     * as temperature changes.
     */
    void checkTemperatureCompensation();

    /**
     * @brief Calculate the number of compensation steps required for a given
     * temperature change.
     * @param temperatureDelta The change in temperature (Celsius) since the
     * last compensation.
     * @return The number of steps to move the focuser to compensate for the
     * temperature change.
     */
    double calculateCompensationSteps(double temperatureDelta) const;

private:
    /**
     * @brief Last temperature value used for compensation (Celsius).
     */
    double lastCompensationTemperature_{20.0};

    /**
     * @brief Apply the calculated temperature compensation to the focuser.
     * @param temperatureDelta The change in temperature (Celsius) since the
     * last compensation.
     */
    void applyTemperatureCompensation(double temperatureDelta);
};

}  // namespace lithium::device::indi::focuser

#endif  // LITHIUM_INDI_FOCUSER_TEMPERATURE_MANAGER_HPP
