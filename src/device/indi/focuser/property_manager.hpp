#ifndef LITHIUM_INDI_FOCUSER_PROPERTY_MANAGER_HPP
#define LITHIUM_INDI_FOCUSER_PROPERTY_MANAGER_HPP

#include <libindi/baseclient.h>
#include "types.hpp"

namespace lithium::device::indi::focuser {

/**
 * @brief Manages INDI property watching and updates for the focuser device.
 *
 * This class is responsible for setting up property watchers on the INDI
 * device, handling property updates, and synchronizing the focuser state with
 * the device. It provides modular setup for different property groups
 * (connection, driver info, configuration, focus, temperature, backlash) and
 * interacts with the shared FocuserState.
 */
class PropertyManager : public IFocuserComponent {
public:
    /**
     * @brief Default constructor.
     */
    PropertyManager() = default;
    /**
     * @brief Virtual destructor.
     */
    ~PropertyManager() override = default;

    /**
     * @brief Initialize the property manager with the shared focuser state.
     * @param state Reference to the shared FocuserState structure.
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize(FocuserState& state) override;

    /**
     * @brief Cleanup resources and detach from the focuser state.
     */
    void cleanup() override;

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
     *
     * @param device Reference to the INDI device.
     * @param state Reference to the shared focuser state.
     */
    void setupPropertyWatchers(INDI::BaseDevice& device, FocuserState& state);

private:
    /**
     * @brief Setup property watchers for connection-related properties.
     * @param device Reference to the INDI device.
     * @param state Reference to the shared focuser state.
     */
    void setupConnectionProperties(INDI::BaseDevice& device,
                                   FocuserState& state);
    /**
     * @brief Setup property watchers for driver information properties.
     * @param device Reference to the INDI device.
     * @param state Reference to the shared focuser state.
     */
    void setupDriverInfoProperties(INDI::BaseDevice& device,
                                   FocuserState& state);
    /**
     * @brief Setup property watchers for configuration properties.
     * @param device Reference to the INDI device.
     * @param state Reference to the shared focuser state.
     */
    void setupConfigurationProperties(INDI::BaseDevice& device,
                                      FocuserState& state);
    /**
     * @brief Setup property watchers for focus-related properties.
     * @param device Reference to the INDI device.
     * @param state Reference to the shared focuser state.
     */
    void setupFocusProperties(INDI::BaseDevice& device, FocuserState& state);
    /**
     * @brief Setup property watchers for temperature-related properties.
     * @param device Reference to the INDI device.
     * @param state Reference to the shared focuser state.
     */
    void setupTemperatureProperties(INDI::BaseDevice& device,
                                    FocuserState& state);
    /**
     * @brief Setup property watchers for backlash-related properties.
     * @param device Reference to the INDI device.
     * @param state Reference to the shared focuser state.
     */
    void setupBacklashProperties(INDI::BaseDevice& device, FocuserState& state);

    /**
     * @brief Pointer to the shared focuser state structure.
     */
    FocuserState* state_{nullptr};
};

}  // namespace lithium::device::indi::focuser

#endif  // LITHIUM_INDI_FOCUSER_PROPERTY_MANAGER_HPP
