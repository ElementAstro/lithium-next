/*
 * filterwheel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Complete INDI FilterWheel implementation using modular components

*************************************************/

#include "filterwheel.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

INDIFilterwheel::INDIFilterwheel(std::string name)
    : INDIFilterwheelBase(name),
      INDIFilterwheelControl(name),
      INDIFilterwheelFilterManager(name),
      INDIFilterwheelStatistics(name),
      INDIFilterwheelConfiguration(name) {

    initializeComponents();
}

auto INDIFilterwheel::setPosition(int position) -> bool {
    // Record the move for statistics before attempting the move
    // Note: We record here to ensure stats are updated even if move fails

    // Call the control implementation to actually move the filter wheel
    bool success = INDIFilterwheelControl::setPosition(position);

    if (success) {
        // Only record successful moves for statistics
        recordMove();

        // Notify move complete callback
        if (move_complete_callback_) {
            move_complete_callback_(true, "Filter wheel moved successfully");
        }

        logger_->info("Filter wheel successfully moved to position {}", position);
    } else {
        // Notify move complete callback with error
        if (move_complete_callback_) {
            move_complete_callback_(false, "Failed to move filter wheel");
        }

        logger_->error("Failed to move filter wheel to position {}", position);
    }

    return success;
}

void INDIFilterwheel::initializeComponents() {
    logger_->info("Initializing modular filterwheel components for: {}", name_);

    // Initialize all components
    INDIFilterwheelBase::initialize();

    logger_->debug("All filterwheel components initialized successfully");
}

// Factory function for creating filterwheel instances
std::shared_ptr<INDIFilterwheel> createINDIFilterwheel(const std::string& name) {
    return std::make_shared<INDIFilterwheel>(name);
}
