#ifndef LITHIUM_INDI_FILTERWHEEL_COMPONENT_BASE_HPP
#define LITHIUM_INDI_FILTERWHEEL_COMPONENT_BASE_HPP

#include <memory>
#include <string>
#include "core/indi_filterwheel_core.hpp"

namespace lithium::device::indi::filterwheel {

/**
 * @brief Base class for all INDI FilterWheel components
 *
 * This follows the ASCOM modular architecture pattern, providing a consistent
 * interface for all filterwheel components. Each component holds a shared reference
 * to the filterwheel core for state management and INDI communication.
 */
template<typename CoreType = INDIFilterWheelCore>
class ComponentBase {
public:
    explicit ComponentBase(std::shared_ptr<CoreType> core)
        : core_(std::move(core)) {}

    virtual ~ComponentBase() = default;

    // Non-copyable, movable
    ComponentBase(const ComponentBase&) = delete;
    ComponentBase& operator=(const ComponentBase&) = delete;
    ComponentBase(ComponentBase&&) = default;
    ComponentBase& operator=(ComponentBase&&) = default;

    /**
     * @brief Initialize the component
     * @return true if initialization was successful, false otherwise
     */
    virtual bool initialize() = 0;

    /**
     * @brief Shutdown and cleanup the component
     */
    virtual void shutdown() = 0;

    /**
     * @brief Get the component's name for logging and identification
     * @return Name of the component
     */
    virtual std::string getComponentName() const = 0;

    /**
     * @brief Validate that the component is ready for operation
     * @return true if component is ready, false otherwise
     */
    virtual bool validateComponentReady() const {
        return core_ && core_->isConnected();
    }

protected:
    /**
     * @brief Get access to the shared core
     * @return Reference to the filterwheel core
     */
    std::shared_ptr<CoreType> getCore() const { return core_; }

private:
    std::shared_ptr<CoreType> core_;
};

// Type alias for convenience
using FilterWheelComponentBase = ComponentBase<INDIFilterWheelCore>;

}  // namespace lithium::device::indi::filterwheel

#endif  // LITHIUM_INDI_FILTERWHEEL_COMPONENT_BASE_HPP
