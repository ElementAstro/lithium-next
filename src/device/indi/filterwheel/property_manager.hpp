#ifndef LITHIUM_INDI_FILTERWHEEL_PROPERTY_MANAGER_HPP
#define LITHIUM_INDI_FILTERWHEEL_PROPERTY_MANAGER_HPP

#include "component_base.hpp"
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

namespace lithium::device::indi::filterwheel {

/**
 * @brief Manages INDI property watching and synchronization for FilterWheel
 *
 * This component handles all INDI property interactions, including watching for
 * property updates and maintaining synchronization between INDI properties
 * and the internal state.
 */
class PropertyManager : public FilterWheelComponentBase {
public:
    explicit PropertyManager(std::shared_ptr<INDIFilterWheelCore> core);
    ~PropertyManager() override = default;

    bool initialize() override;
    void shutdown() override;
    std::string getComponentName() const override { return "PropertyManager"; }

    /**
     * @brief Set up property watchers for all relevant INDI properties
     */
    void setupPropertyWatchers();

    /**
     * @brief Update internal state from INDI property values
     */
    void syncFromProperties();

private:
    // Property handlers
    void handleConnectionProperty(const INDI::PropertySwitch& property);
    void handleDriverInfoProperty(const INDI::PropertyText& property);
    void handleDebugProperty(const INDI::PropertySwitch& property);
    void handlePollingProperty(const INDI::PropertyNumber& property);
    void handleFilterSlotProperty(const INDI::PropertyNumber& property);
    void handleFilterNameProperty(const INDI::PropertyText& property);
    void handleFilterWheelProperty(const INDI::PropertySwitch& property);

    bool initialized_{false};
};

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_PROPERTY_MANAGER_HPP
