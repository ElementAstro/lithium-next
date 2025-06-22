/*
 * qhy_component_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Base component interface for QHY camera system

*************************************************/

#ifndef LITHIUM_QHY_CAMERA_COMPONENT_BASE_HPP
#define LITHIUM_QHY_CAMERA_COMPONENT_BASE_HPP

#include <string>
#include "../../template/camera.hpp"

namespace lithium::device::qhy::camera {

// Forward declarations
class QHYCameraCore;

/**
 * @brief Base interface for all QHY camera components
 * 
 * This interface provides common functionality and access patterns
 * for all camera components. Each component can access the core
 * camera instance and QHY SDK through this interface.
 */
class ComponentBase {
public:
    explicit ComponentBase(QHYCameraCore* core) : core_(core) {}
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
     * @brief Handle camera state changes relevant to this component
     * @param state The new camera state
     */
    virtual auto onCameraStateChanged(CameraState state) -> void {}

    /**
     * @brief Handle camera parameter updates
     * @param param Parameter name
     * @param value Parameter value
     */
    virtual auto onParameterChanged(const std::string& param, double value) -> void {}

protected:
    /**
     * @brief Get access to the core camera instance
     */
    auto getCore() -> QHYCameraCore* { return core_; }
    auto getCore() const -> const QHYCameraCore* { return core_; }

private:
    QHYCameraCore* core_;
};

} // namespace lithium::device::qhy::camera

#endif // LITHIUM_QHY_CAMERA_COMPONENT_BASE_HPP
