#ifndef LITHIUM_INDI_FOCUSER_PROPERTY_MANAGER_HPP
#define LITHIUM_INDI_FOCUSER_PROPERTY_MANAGER_HPP

#include <libindi/baseclient.h>
#include "component_base.hpp"

namespace lithium::device::indi::focuser {

/**
 * @brief Manages INDI property watching and updates for the focuser device.
 *
 * This class is responsible for setting up property watchers on the INDI
 * device, handling property updates, and synchronizing the focuser state with
 * the device. It provides modular setup for different property groups
 * (connection, driver info, configuration, focus, temperature, backlash) and
 * interacts with the shared INDIFocuserCore.
 */
class PropertyManager : public FocuserComponentBase {
public:
    /**
     * @brief Constructor with shared core.
     * @param core Shared pointer to the INDIFocuserCore
     */
    explicit PropertyManager(std::shared_ptr<INDIFocuserCore> core);

    /**
     * @brief Virtual destructor.
     */
    ~PropertyManager() override = default;

    /**
     * @brief Initialize the property manager.
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
    std::string getComponentName() const override { return "PropertyManager"; }

    /**
     * @brief Setup property watchers for the device.
     *
     * This method sets up all relevant property watchers on the INDI device,
     * ensuring that the focuser state is kept in sync with device property
     * changes.
     */
    void setupPropertyWatchers();

    /**
     * @brief Handle property updates from the INDI device.
     * @param property The property that was updated
     */
    void handlePropertyUpdate(const INDI::Property& property);

private:
    /**
     * @brief Setup property watchers for connection-related properties.
     */
    void setupConnectionProperties();

    /**
     * @brief Setup property watchers for driver information properties.
     */
    void setupDriverInfoProperties();

    /**
     * @brief Setup property watchers for configuration properties.
     */
    void setupConfigurationProperties();

    /**
     * @brief Setup property watchers for focus-related properties.
     */
    void setupFocusProperties();

    /**
     * @brief Setup property watchers for temperature-related properties.
     */
    void setupTemperatureProperties();

    /**
     * @brief Setup property watchers for backlash-related properties.
     */
    void setupBacklashProperties();

    /**
     * @brief Handle switch property updates.
     * @param property The switch property that was updated
     */
    void handleSwitchPropertyUpdate(const INDI::PropertySwitch& property);

    /**
     * @brief Handle number property updates.
     * @param property The number property that was updated
     */
    void handleNumberPropertyUpdate(const INDI::PropertyNumber& property);

    /**
     * @brief Handle text property updates.
     * @param property The text property that was updated
     */
    void handleTextPropertyUpdate(const INDI::PropertyText& property);
};

}  // namespace lithium::device::indi::focuser

#endif  // LITHIUM_INDI_FOCUSER_PROPERTY_MANAGER_HPP
