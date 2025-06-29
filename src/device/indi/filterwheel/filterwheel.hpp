/*
 * filterwheel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Complete INDI FilterWheel implementation using modular components

*************************************************/

#ifndef LITHIUM_DEVICE_INDI_FILTERWHEEL_FILTERWHEEL_HPP
#define LITHIUM_DEVICE_INDI_FILTERWHEEL_FILTERWHEEL_HPP

#include "base.hpp"
#include "control.hpp"
#include "filter_manager.hpp"
#include "statistics.hpp"
#include "configuration.hpp"

class INDIFilterwheel : public INDIFilterwheelControl,
                       public INDIFilterwheelFilterManager,
                       public INDIFilterwheelStatistics,
                       public INDIFilterwheelConfiguration {
public:
    explicit INDIFilterwheel(std::string name);
    ~INDIFilterwheel() override = default;

    // Override the base setPosition to include statistics recording
    auto setPosition(int position) -> bool override;

private:
    // Ensure proper initialization order
    void initializeComponents();
};

#endif // LITHIUM_DEVICE_INDI_FILTERWHEEL_FILTERWHEEL_HPP
