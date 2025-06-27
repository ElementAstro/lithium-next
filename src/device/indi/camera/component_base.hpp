/*
 * component_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Base interface for INDI Camera Components

This interface provides common functionality and access patterns
for all camera components, following the ASCOM modular architecture pattern.

*************************************************/

#ifndef LITHIUM_INDI_CAMERA_COMPONENT_BASE_HPP
#define LITHIUM_INDI_CAMERA_COMPONENT_BASE_HPP

#include <libindi/basedevice.h>
#include <libindi/baseclient.h>
#include <string>
#include <memory>

namespace lithium::device::indi::camera {

// Forward declarations
class INDICameraCore;

/**
 * @brief Base interface for all INDI camera components
 * 
 * This interface provides common functionality and access patterns
 * for all camera components, similar to ASCOM's component architecture.
 * Each component can access the core camera instance and INDI device 
 * through this interface.
 */
class ComponentBase {
public:
    explicit ComponentBase(std::shared_ptr<INDICameraCore> core) : core_(core) {}
    virtual ~ComponentBase() = default;

    // Non-copyable, non-movable (following ASCOM pattern)
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

    /**
     * @brief Check if component is ready for operation
     * @return true if component is ready
     */
    virtual auto isReady() const -> bool { return true; }

protected:
    /**
     * @brief Get access to the core camera instance
     */
    auto getCore() -> std::shared_ptr<INDICameraCore> { return core_; }

    /**
     * @brief Get access to the core camera instance (const)
     */
    auto getCore() const -> std::shared_ptr<const INDICameraCore> { return core_; }

private:
    std::shared_ptr<INDICameraCore> core_;
};

} // namespace lithium::device::indi::camera

#endif // LITHIUM_INDI_CAMERA_COMPONENT_BASE_HPP
