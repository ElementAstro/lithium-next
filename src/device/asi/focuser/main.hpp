/*
 * asi_focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Electronic Auto Focuser (EAF) dedicated module

*************************************************/

#pragma once

#include "device/template/focuser.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Forward declaration
namespace lithium::device::asi::focuser::controller {
class ASIFocuserControllerV2;
using ASIFocuserController = ASIFocuserControllerV2;
}

namespace lithium::device::asi::focuser {

/**
 * @brief Dedicated ASI Electronic Auto Focuser (EAF) controller
 *
 * This class provides complete control over ASI EAF focusers,
 * including position control, temperature monitoring, backlash
 * compensation, and automated focusing sequences.
 */
class ASIFocuser : public AtomFocuser {
public:
    explicit ASIFocuser(const std::string& name = "ASI Focuser");
    ~ASIFocuser() override;

    // Non-copyable and non-movable
    ASIFocuser(const ASIFocuser&) = delete;
    ASIFocuser& operator=(const ASIFocuser&) = delete;
    ASIFocuser(ASIFocuser&&) = delete;
    ASIFocuser& operator=(ASIFocuser&&) = delete;

    // Basic device interface (from AtomDriver)
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName = "", int timeout = 30000,
                 int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto isConnected() const -> bool override;
    auto scan() -> std::vector<std::string> override;

    // AtomFocuser interface implementation
    auto isMoving() const -> bool override;

    // Speed control
    auto getSpeed() -> std::optional<double> override;
    auto setSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> int override;
    auto getSpeedRange() -> std::pair<int, int> override;

    // Direction control
    auto getDirection() -> std::optional<FocusDirection> override;
    auto setDirection(FocusDirection direction) -> bool override;

    // Limits
    auto getMaxLimit() -> std::optional<int> override;
    auto setMaxLimit(int maxLimit) -> bool override;
    auto getMinLimit() -> std::optional<int> override;
    auto setMinLimit(int minLimit) -> bool override;

    // Reverse control
    auto isReversed() -> std::optional<bool> override;
    auto setReversed(bool reversed) -> bool override;

    // Movement control
    auto moveSteps(int steps) -> bool override;
    auto moveToPosition(int position) -> bool override;
    auto getPosition() -> std::optional<int> override;
    auto moveForDuration(int durationMs) -> bool override;
    auto abortMove() -> bool override;
    auto syncPosition(int position) -> bool override;

    // Relative movement
    auto moveInward(int steps) -> bool override;
    auto moveOutward(int steps) -> bool override;

    // Backlash compensation
    auto getBacklash() -> int override;
    auto setBacklash(int backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Temperature monitoring
    auto getExternalTemperature() -> std::optional<double> override;
    auto getChipTemperature() -> std::optional<double> override;
    auto hasTemperatureSensor() -> bool override;

    // Temperature compensation
    auto getTemperatureCompensation() -> TemperatureCompensation override;
    auto setTemperatureCompensation(const TemperatureCompensation& comp)
        -> bool override;
    auto enableTemperatureCompensation(bool enable) -> bool override;

    // Auto focus
    auto startAutoFocus() -> bool override;
    auto stopAutoFocus() -> bool override;
    auto isAutoFocusing() -> bool override;
    auto getAutoFocusProgress() -> double override;

    // Presets
    auto savePreset(int slot, int position) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<int> override;
    auto deletePreset(int slot) -> bool override;

    // Statistics
    auto getTotalSteps() -> uint64_t override;
    auto resetTotalSteps() -> bool override;
    auto getLastMoveSteps() -> int override;
    auto getLastMoveDuration() -> int override;

    // Callbacks
    void setPositionCallback(PositionCallback callback) override;
    void setTemperatureCallback(TemperatureCallback callback) override;
    void setMoveCompleteCallback(MoveCompleteCallback callback) override;

    // ASI-specific extended functionality
    auto setPosition(int position) -> bool;  // Legacy compatibility
    auto getMaxPosition() const -> int;
    auto stopMovement() -> bool;
    auto setStepSize(int stepSize) -> bool;
    auto getStepSize() const -> int;
    auto homeToZero() -> bool;
    auto setHomePosition() -> bool;
    auto calibrateFocuser() -> bool;
    auto findOptimalPosition(int startPos, int endPos, int stepSize)
        -> std::optional<int>;

    // Advanced features
    auto setTemperatureCoefficient(double coefficient) -> bool;
    auto getTemperatureCoefficient() const -> double;
    auto setMovementDirection(bool reverse) -> bool;
    auto isDirectionReversed() const -> bool;
    auto enableBeep(bool enable) -> bool;
    auto isBeepEnabled() const -> bool;

    // Focusing sequences
    auto performFocusSequence(const std::vector<int>& positions,
                              std::function<double(int)> qualityMeasure =
                                  nullptr) -> std::optional<int>;
    auto performCoarseFineAutofocus(int coarseStepSize, int fineStepSize,
                                    int searchRange) -> std::optional<int>;
    auto performVCurveFocus(int startPos, int endPos, int stepCount)
        -> std::optional<int>;

    // Configuration management
    auto saveConfiguration(const std::string& filename) -> bool;
    auto loadConfiguration(const std::string& filename) -> bool;
    auto resetToDefaults() -> bool;

    // Hardware information
    auto getFirmwareVersion() const -> std::string;
    auto getSerialNumber() const -> std::string;
    auto getModelName() const -> std::string;
    auto getMaxStepSize() const -> int;
    auto setDeviceAlias(const std::string& alias) -> bool;
    auto getSDKVersion() -> std::string;
    
    // Enhanced hardware control
    auto resetFocuserPosition(int position = 0) -> bool;
    auto setMaxStepPosition(int maxStep) -> bool;
    auto getMaxStepPosition() -> int;
    auto getStepRange() -> int;

    // Status and diagnostics
    auto getLastError() const -> std::string;
    auto getMovementCount() const -> uint32_t;
    auto getOperationHistory() const -> std::vector<std::string>;
    auto performSelfTest() -> bool;

    // High resolution mode
    auto enableHighResolutionMode(bool enable) -> bool;
    auto isHighResolutionMode() const -> bool;
    auto getResolution() const -> double;  // microns per step
    auto calibrateResolution() -> bool;

private:
    std::unique_ptr<controller::ASIFocuserController> controller_;
};

/**
 * @brief Factory function to create ASI Focuser instances
 */
std::unique_ptr<ASIFocuser> createASIFocuser(
    const std::string& name = "ASI EAF");

}  // namespace lithium::device::asi::focuser
