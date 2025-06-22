/*
 * asi_hardware_controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI camera hardware accessories controller component

*************************************************/

#ifndef LITHIUM_ASI_CAMERA_HARDWARE_CONTROLLER_HPP
#define LITHIUM_ASI_CAMERA_HARDWARE_CONTROLLER_HPP

#include "../component_base.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

namespace lithium::device::asi::camera {

/**
 * @brief Hardware accessories controller for ASI cameras
 * 
 * This component handles all ASI hardware accessories including
 * EAF focusers and EFW filter wheels with comprehensive control
 * and monitoring capabilities.
 */
class HardwareController : public ComponentBase {
public:
    explicit HardwareController(ASICameraCore* core);
    ~HardwareController() override;

    // ComponentBase interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto getComponentName() const -> std::string override;
    auto onCameraStateChanged(CameraState state) -> void override;

    // EAF (Electronic Auto Focuser) control
    auto hasEAFFocuser() -> bool;
    auto connectEAFFocuser() -> bool;
    auto disconnectEAFFocuser() -> bool;
    auto isEAFFocuserConnected() -> bool;
    auto setEAFFocuserPosition(int position) -> bool;
    auto getEAFFocuserPosition() -> int;
    auto getEAFFocuserMaxPosition() -> int;
    auto isEAFFocuserMoving() -> bool;
    auto stopEAFFocuser() -> bool;
    auto setEAFFocuserStepSize(int stepSize) -> bool;
    auto getEAFFocuserStepSize() -> int;
    auto homeEAFFocuser() -> bool;
    auto calibrateEAFFocuser() -> bool;
    auto getEAFFocuserTemperature() -> double;
    auto enableEAFFocuserBacklashCompensation(bool enable) -> bool;
    auto setEAFFocuserBacklashSteps(int steps) -> bool;
    auto getEAFFocuserFirmware() -> std::string;

    // EFW (Electronic Filter Wheel) control
    auto hasEFWFilterWheel() -> bool;
    auto connectEFWFilterWheel() -> bool;
    auto disconnectEFWFilterWheel() -> bool;
    auto isEFWFilterWheelConnected() -> bool;
    auto setEFWFilterPosition(int position) -> bool;
    auto getEFWFilterPosition() -> int;
    auto getEFWFilterCount() -> int;
    auto isEFWFilterWheelMoving() -> bool;
    auto homeEFWFilterWheel() -> bool;
    auto getEFWFilterWheelFirmware() -> std::string;
    auto setEFWFilterNames(const std::vector<std::string>& names) -> bool;
    auto getEFWFilterNames() -> std::vector<std::string>;
    auto getEFWUnidirectionalMode() -> bool;
    auto setEFWUnidirectionalMode(bool enable) -> bool;
    auto calibrateEFWFilterWheel() -> bool;

    // Hardware coordination
    auto performFocusSequence(const std::vector<int>& positions, 
                             std::function<void(int, bool)> callback = nullptr) -> bool;
    auto performFilterSequence(const std::vector<int>& positions,
                              std::function<void(int, bool)> callback = nullptr) -> bool;
    auto enableHardwareCoordination(bool enable) -> bool;
    auto isHardwareCoordinationEnabled() const -> bool;

    // Movement monitoring
    auto setMovementCallback(std::function<void(const std::string&, bool)> callback) -> void;
    auto enableMovementMonitoring(bool enable) -> bool;
    auto isMovementMonitoringEnabled() const -> bool;

private:
    // EAF (Electronic Auto Focuser) state
    bool hasEAFFocuser_{false};
    int eafFocuserId_{-1};
    bool eafFocuserConnected_{false};
    int eafFocuserPosition_{0};
    int eafFocuserMaxPosition_{0};
    int eafFocuserStepSize_{1};
    bool eafFocuserMoving_{false};
    std::string eafFocuserFirmware_;
    double eafFocuserTemperature_{0.0};
    bool eafBacklashCompensation_{false};
    int eafBacklashSteps_{0};
    
    // EFW (Electronic Filter Wheel) state
    bool hasEFWFilterWheel_{false};
    int efwFilterWheelId_{-1};
    bool efwFilterWheelConnected_{false};
    int efwCurrentPosition_{1};
    int efwFilterCount_{0};
    bool efwFilterWheelMoving_{false};
    std::string efwFirmware_;
    std::vector<std::string> efwFilterNames_;
    bool efwUnidirectionalMode_{false};
    
    // Hardware coordination
    std::atomic_bool hardwareCoordinationEnabled_{false};
    std::atomic_bool movementMonitoringEnabled_{true};
    std::function<void(const std::string&, bool)> movementCallback_;
    mutable std::mutex hardwareMutex_;
    
    // Private helper methods
    auto detectEAFFocuser() -> bool;
    auto detectEFWFilterWheel() -> bool;
    auto initializeEAFFocuser() -> bool;
    auto initializeEFWFilterWheel() -> bool;
    auto shutdownEAFFocuser() -> bool;
    auto shutdownEFWFilterWheel() -> bool;
    auto waitForEAFMovement(int timeoutMs = 30000) -> bool;
    auto waitForEFWMovement(int timeoutMs = 30000) -> bool;
    auto validateEAFPosition(int position) const -> bool;
    auto validateEFWPosition(int position) const -> bool;
    auto notifyMovementChange(const std::string& device, bool moving) -> void;
    auto performSequenceWithCallback(const std::vector<int>& positions,
                                    std::function<bool(int)> mover,
                                    std::function<bool()> checker,
                                    std::function<void(int, bool)> callback) -> bool;
};

} // namespace lithium::device::asi::camera

#endif // LITHIUM_ASI_CAMERA_HARDWARE_CONTROLLER_HPP
