/*
 * focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced AtomFocuser following INDI architecture

*************************************************/

#pragma once

#include <array>
#include <functional>
#include <optional>
#include "device.hpp"

enum class BAUD_RATE { 
    B9600, 
    B19200, 
    B38400, 
    B57600, 
    B115200, 
    B230400, 
    NONE 
};

enum class FocusMode { 
    ALL, 
    ABSOLUTE, 
    RELATIVE, 
    NONE 
};

enum class FocusDirection { 
    IN, 
    OUT, 
    NONE 
};

enum class FocuserState {
    IDLE,
    MOVING,
    ERROR
};

// Focuser capabilities
struct FocuserCapabilities {
    bool canAbsoluteMove{true};
    bool canRelativeMove{true};
    bool canAbort{true};
    bool canReverse{false};
    bool canSync{false};
    bool hasTemperature{false};
    bool hasBacklash{false};
    bool hasSpeedControl{false};
    int maxPosition{65535};
    int minPosition{0};
} ATOM_ALIGNAS(16);

// Temperature compensation
struct TemperatureCompensation {
    bool enabled{false};
    double coefficient{0.0};  // steps per degree C
    double temperature{0.0};
    double compensationOffset{0.0};
} ATOM_ALIGNAS(32);

class AtomFocuser : public AtomDriver {
public:
    explicit AtomFocuser(std::string name) : AtomDriver(std::move(name)) {
        setType("Focuser");
    }
    
    ~AtomFocuser() override = default;

    // Capabilities
    const FocuserCapabilities& getFocuserCapabilities() const { return focuser_capabilities_; }
    void setFocuserCapabilities(const FocuserCapabilities& caps) { focuser_capabilities_ = caps; }

    // State
    FocuserState getFocuserState() const { return focuser_state_; }
    virtual bool isMoving() const = 0;

    // 获取和设置调焦器速度
    virtual auto getSpeed() -> std::optional<double> = 0;
    virtual auto setSpeed(double speed) -> bool = 0;
    virtual auto getMaxSpeed() -> int = 0;
    virtual auto getSpeedRange() -> std::pair<int, int> = 0;

    // 获取和设置调焦器移动方向
    virtual auto getDirection() -> std::optional<FocusDirection> = 0;
    virtual auto setDirection(FocusDirection direction) -> bool = 0;

    // 获取和设置调焦器最大限制
    virtual auto getMaxLimit() -> std::optional<int> = 0;
    virtual auto setMaxLimit(int maxLimit) -> bool = 0;
    virtual auto getMinLimit() -> std::optional<int> = 0;
    virtual auto setMinLimit(int minLimit) -> bool = 0;

    // 获取和设置调焦器反转状态
    virtual auto isReversed() -> std::optional<bool> = 0;
    virtual auto setReversed(bool reversed) -> bool = 0;

    // 调焦器移动控制
    virtual auto moveSteps(int steps) -> bool = 0;
    virtual auto moveToPosition(int position) -> bool = 0;
    virtual auto getPosition() -> std::optional<int> = 0;
    virtual auto moveForDuration(int durationMs) -> bool = 0;
    virtual auto abortMove() -> bool = 0;
    virtual auto syncPosition(int position) -> bool = 0;

    // 相对移动
    virtual auto moveInward(int steps) -> bool = 0;
    virtual auto moveOutward(int steps) -> bool = 0;

    // 背隙补偿
    virtual auto getBacklash() -> int = 0;
    virtual auto setBacklash(int backlash) -> bool = 0;
    virtual auto enableBacklashCompensation(bool enable) -> bool = 0;
    virtual auto isBacklashCompensationEnabled() -> bool = 0;

    // 获取调焦器温度
    virtual auto getExternalTemperature() -> std::optional<double> = 0;
    virtual auto getChipTemperature() -> std::optional<double> = 0;
    virtual auto hasTemperatureSensor() -> bool = 0;

    // 温度补偿
    virtual auto getTemperatureCompensation() -> TemperatureCompensation = 0;
    virtual auto setTemperatureCompensation(const TemperatureCompensation& comp) -> bool = 0;
    virtual auto enableTemperatureCompensation(bool enable) -> bool = 0;

    // 自动对焦支持
    virtual auto startAutoFocus() -> bool = 0;
    virtual auto stopAutoFocus() -> bool = 0;
    virtual auto isAutoFocusing() -> bool = 0;
    virtual auto getAutoFocusProgress() -> double = 0;

    // 预设位置
    virtual auto savePreset(int slot, int position) -> bool = 0;
    virtual auto loadPreset(int slot) -> bool = 0;
    virtual auto getPreset(int slot) -> std::optional<int> = 0;
    virtual auto deletePreset(int slot) -> bool = 0;

    // 统计信息
    virtual auto getTotalSteps() -> uint64_t = 0;
    virtual auto resetTotalSteps() -> bool = 0;
    virtual auto getLastMoveSteps() -> int = 0;
    virtual auto getLastMoveDuration() -> int = 0;

    // Event callbacks
    using PositionCallback = std::function<void(int position)>;
    using TemperatureCallback = std::function<void(double temperature)>;
    using MoveCompleteCallback = std::function<void(bool success, const std::string& message)>;

    virtual void setPositionCallback(PositionCallback callback) { position_callback_ = std::move(callback); }
    virtual void setTemperatureCallback(TemperatureCallback callback) { temperature_callback_ = std::move(callback); }
    virtual void setMoveCompleteCallback(MoveCompleteCallback callback) { move_complete_callback_ = std::move(callback); }

protected:
    FocuserState focuser_state_{FocuserState::IDLE};
    FocuserCapabilities focuser_capabilities_;
    TemperatureCompensation temperature_compensation_;
    
    // Current state
    int current_position_{0};
    int target_position_{0};
    double current_speed_{50.0};
    bool is_reversed_{false};
    int backlash_steps_{0};
    
    // Statistics
    uint64_t total_steps_{0};
    int last_move_steps_{0};
    int last_move_duration_{0};
    
    // Presets
    std::array<std::optional<int>, 10> presets_;
    
    // Callbacks
    PositionCallback position_callback_;
    TemperatureCallback temperature_callback_;
    MoveCompleteCallback move_complete_callback_;
    
    // Utility methods
    virtual void updateFocuserState(FocuserState state) { focuser_state_ = state; }
    virtual void notifyPositionChange(int position);
    virtual void notifyTemperatureChange(double temperature);
    virtual void notifyMoveComplete(bool success, const std::string& message = "");
};
