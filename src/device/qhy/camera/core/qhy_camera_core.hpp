/*
 * qhy_camera_core.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Core QHY camera functionality with component architecture

*************************************************/

#ifndef LITHIUM_QHY_CAMERA_CORE_HPP
#define LITHIUM_QHY_CAMERA_CORE_HPP

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../../../template/camera.hpp"
#include "../component_base.hpp"
#include "../../qhyccd.h"

namespace lithium::device::qhy::camera {

// Forward declarations
class ComponentBase;

/**
 * @brief Core QHY camera functionality
 *
 * This class provides the foundational QHY camera operations including
 * SDK management, device connection, and component coordination.
 * It serves as the central hub for all camera components.
 */
class QHYCameraCore {
public:
    explicit QHYCameraCore(const std::string& deviceName);
    ~QHYCameraCore();

    // Basic device operations
    auto initialize() -> bool;
    auto destroy() -> bool;
    auto connect(const std::string& deviceName, int timeout = 5000, int maxRetry = 3) -> bool;
    auto disconnect() -> bool;
    auto isConnected() const -> bool;
    auto scan() -> std::vector<std::string>;

    // Device access
    auto getCameraHandle() const -> QHYCamHandle*;
    auto getDeviceName() const -> const std::string&;
    auto getCameraId() const -> const std::string&;

    // Component management
    auto registerComponent(std::shared_ptr<ComponentBase> component) -> void;
    auto unregisterComponent(ComponentBase* component) -> void;

    // State management
    auto updateCameraState(CameraState state) -> void;
    auto getCameraState() const -> CameraState;

    // Current frame access
    auto getCurrentFrame() -> std::shared_ptr<AtomCameraFrame>;
    auto setCurrentFrame(std::shared_ptr<AtomCameraFrame> frame) -> void;

    // QHY SDK utilities
    auto setControlValue(CONTROL_ID controlId, double value) -> bool;
    auto getControlValue(CONTROL_ID controlId, double* value) -> bool;
    auto getControlMinMaxStep(CONTROL_ID controlId, double* min, double* max, double* step) -> bool;
    auto isControlAvailable(CONTROL_ID controlId) -> bool;

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

    // QHY-specific features
    auto enableUSB3Traffic(bool enable) -> bool;
    auto setUSB3Traffic(int traffic) -> bool;
    auto getUSB3Traffic() -> int;
    auto getCameraType() const -> std::string;
    auto hasColorCamera() const -> bool;
    auto hasCooler() const -> bool;
    auto hasFilterWheel() const -> bool;

private:
    // Device information
    std::string deviceName_;
    std::string name_;
    std::string cameraId_;
    QHYCamHandle* cameraHandle_;

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

    // Hardware capabilities
    bool hasColorCamera_{false};
    bool hasCooler_{false};
    bool hasFilterWheel_{false};
    bool hasUSB3_{false};
    std::string cameraType_;
    std::string firmwareVersion_;
    std::string serialNumber_;

    // Private helper methods
    auto initializeQHYSDK() -> bool;
    auto shutdownQHYSDK() -> bool;
    auto findCameraByName(const std::string& name) -> std::string;
    auto loadCameraCapabilities() -> bool;
    auto detectHardwareFeatures() -> bool;
    auto notifyComponents(CameraState state) -> void;
    auto notifyParameterChange(const std::string& name, double value) -> void;
};

} // namespace lithium::device::qhy::camera

#endif // LITHIUM_QHY_CAMERA_CORE_HPP
