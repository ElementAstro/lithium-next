#ifndef LITHIUM_INDI_CAMERA_COMPONENT_BASE_HPP
#define LITHIUM_INDI_CAMERA_COMPONENT_BASE_HPP

#include <libindi/basedevice.h>
#include <libindi/baseclient.h>
#include <string>

namespace lithium::device::indi::camera {

// Forward declarations
class INDICameraCore;

/**
 * @brief Base interface for all INDI camera components
 * 
 * This interface provides common functionality and access patterns
 * for all camera components. Each component can access the core
 * camera instance and INDI device through this interface.
 */
class ComponentBase {
public:
    explicit ComponentBase(INDICameraCore* core) : core_(core) {}
    virtual ~ComponentBase() = default;

    // Non-copyable, non-movable
    ComponentBase(const ComponentBase&) = delete;
    ComponentBase& operator=(const ComponentBase&) = delete;
    ComponentBase(ComponentBase&&) = delete;
    ComponentBase& operator=(ComponentBase&&) = delete;

    /**
     * @brief Initialize the component
     * @return true if initialization successful
     */
    virtual auto initialize() -> bool = 0;

    /**
     * @brief Cleanup the component
     * @return true if cleanup successful
     */
    virtual auto destroy() -> bool = 0;

    /**
     * @brief Get component name for logging and debugging
     */
    virtual auto getComponentName() const -> std::string = 0;

    /**
     * @brief Handle INDI property updates relevant to this component
     * @param property The INDI property that was updated
     * @return true if the property was handled by this component
     */
    virtual auto handleProperty(INDI::Property property) -> bool { return false; }

protected:
    /**
     * @brief Get access to the core camera instance
     */
    auto getCore() -> INDICameraCore* { return core_; }

    /**
     * @brief Get access to the core camera instance (const)
     */
    auto getCore() const -> const INDICameraCore* { return core_; }

private:
    INDICameraCore* core_;
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_COMPONENT_BASE_HPP
