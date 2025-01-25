/*
 * focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomFocuser Simulator and Basic Definition

*************************************************/

#pragma once

#include <optional>
#include "device.hpp"

enum class BAUD_RATE { B9600, B19200, B38400, B57600, B115200, B230400, NONE };
enum class FocusMode { ALL, ABSOLUTE, RELATIVE, NONE };
enum class FocusDirection { IN, OUT, NONE };

class AtomFocuser : public AtomDriver {
public:
    explicit AtomFocuser(std::string name) : AtomDriver(name) {}

    // 获取和设置调焦器速度
    virtual auto getSpeed() -> std::optional<double> = 0;
    virtual auto setSpeed(double speed) -> bool = 0;

    // 获取和设置调焦器移动方向
    virtual auto getDirection() -> std::optional<FocusDirection> = 0;
    virtual auto setDirection(FocusDirection direction) -> bool = 0;

    // 获取和设置调焦器最大限制
    virtual auto getMaxLimit() -> std::optional<int> = 0;
    virtual auto setMaxLimit(int maxLimit) -> bool = 0;

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

    // 获取调焦器温度
    virtual auto getExternalTemperature() -> std::optional<double> = 0;
    virtual auto getChipTemperature() -> std::optional<double> = 0;
};
