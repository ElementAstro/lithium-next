/*
 * asi_camera_core.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Core ASI camera functionality with component architecture

*************************************************/

#ifndef LITHIUM_ASI_CAMERA_CORE_HPP
#define LITHIUM_ASI_CAMERA_CORE_HPP

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../../../template/camera.hpp"
#include "../component_base.hpp"
#include "../../ASICamera2.h"

namespace lithium::device::asi::camera {

// Forward declarations
class ComponentBase;

/**
 * @brief Core ASI camera functionality
 * 
 * This class provides the foundational ASI camera operations including
 * SDK management, device connection, and component coordination.
 * It serves as the central hub for all camera components.
 */
class ASICameraCore {
public:
    explicit ASICameraCore(const std::string& deviceName);
    ~ASICameraCore();

    // Basic device operations
    auto initialize() -> bool;
    auto destroy() -> bool;
    auto connect(const std::string& deviceName, int timeout = 5000, int maxRetry = 3) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;
    auto scan() -> std::vector<std::string>;

    // Device access
    auto getCameraId() const -> int;
    auto getDeviceName() const -> const std::string&;
    auto getCameraInfo() const -> const ASI_CAMERA_INFO*;

    // Component management
    auto registerComponent(std::shared_ptr<ComponentBase> component) -> void;
    auto unregisterComponent(ComponentBase* component) -> void;

    // State management
    auto updateCameraState(CameraState state) -> void;
    auto getCameraState() const -> CameraState;

    // Current frame access
    auto getCurrentFrame() -> std::shared_ptr<AtomCameraFrame>;
    auto setCurrentFrame(std::shared_ptr<AtomCameraFrame> frame) -> void;

    // ASI SDK utilities
    auto setControlValue(ASI_CONTROL_TYPE controlType, long value, ASI_BOOL isAuto = ASI_FALSE) -> bool;
    auto getControlValue(ASI_CONTROL_TYPE controlType, long* value, ASI_BOOL* isAuto = nullptr) -> bool;
    auto getControlCaps(ASI_CONTROL_TYPE controlType, ASI_CONTROL_CAPS* caps) -> bool;

    // Parameter management
    auto setParameter(const std::string& name, double value) -> void;
    auto getParameter(const std::string& name) -> double;
    auto hasParameter(const std::string& name) const -> bool;

    // Callback management
    auto setStateChangeCallback(std::function<void(CameraState)> callback) -> void;
    auto setParameterChangeCallback(std::function<void(const std::string&, double)> callback) -> void;

    // Hardware access
    auto getSDKVersion() const -> std::string;
    auto getFirmwareVersion() const -> std::string;
    auto getCameraModel() const -> std::string;
    auto getSerialNumber() const -> std::string;

private:
    // Device information
    std::string deviceName_;
    std::string name_;
    int cameraId_;
    ASI_CAMERA_INFO* cameraInfo_;
    
    // Connection state
    std::atomic_bool isConnected_{false};
    std::atomic_bool isInitialized_{false};
    CameraState currentState_{CameraState::IDLE};
    
    // Component management
    std::vector<std::shared_ptr<ComponentBase>> components_;
    mutable std::mutex componentsMutex_;
    
    // Parameter storage
    std::map<std::string, double> parameters_;
    mutable std::mutex parametersMutex_;
    
    // Current frame
    std::shared_ptr<AtomCameraFrame> currentFrame_;
    mutable std::mutex frameMutex_;
    
    // Callbacks
    std::function<void(CameraState)> stateChangeCallback_;
    std::function<void(const std::string&, double)> parameterChangeCallback_;
    mutable std::mutex callbacksMutex_;
    
    // Private helper methods
    auto initializeASISDK() -> bool;
    auto shutdownASISDK() -> bool;
    auto findCameraByName(const std::string& name) -> int;
    auto loadCameraInfo(int cameraId) -> bool;
    auto notifyComponents(CameraState state) -> void;
    auto notifyParameterChange(const std::string& name, double value) -> void;
};

} // namespace lithium::device::asi::camera

#endif // LITHIUM_ASI_CAMERA_CORE_HPP
