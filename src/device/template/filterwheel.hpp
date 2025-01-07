/*
 * focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: AtomFilterWheel Simulator and Basic Definition

*************************************************/

#pragma once

#include "device.hpp"

#include <optional>

class AtomFilterWheel : public AtomDriver {
public:
    explicit AtomFilterWheel(std::string name) : AtomDriver(name) {}

    virtual auto getPosition()
        -> std::optional<std::tuple<double, double, double>> = 0;
    virtual auto setPosition(int position) -> bool = 0;
    virtual auto getSlotName() -> std::optional<std::string> = 0;
    virtual auto setSlotName(std::string_view name) -> bool = 0;
};
