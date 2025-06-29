/*
 * control.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FilterWheel control operations

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_FILTERWHEEL_CONTROL_HPP
#define LITHIUM_DEVICE_INDI_FILTERWHEEL_CONTROL_HPP

#include "base.hpp"

class INDIFilterwheelControl : public virtual INDIFilterwheelBase {
public:
    explicit INDIFilterwheelControl(std::string name);
    ~INDIFilterwheelControl() override = default;

    // Position control
    auto getPositionDetails() -> std::optional<std::tuple<double, double, double>>;
    auto getPosition() -> std::optional<int> override;
    auto setPosition(int position) -> bool override;

    // Movement control
    auto isMoving() const -> bool override;
    auto abortMotion() -> bool override;
    auto homeFilterWheel() -> bool override;
    auto calibrateFilterWheel() -> bool override;

    // Validation
    auto getFilterCount() -> int override;
    auto isValidPosition(int position) -> bool override;

protected:
    void updateMovementState(bool isMoving);
    auto waitForMovementComplete(int timeoutMs = 10000) -> bool;
};

#endif // LITHIUM_DEVICE_INDI_FILTERWHEEL_CONTROL_HPP
