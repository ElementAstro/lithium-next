/*
 * control.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: FilterWheel control operations implementation

*************************************************/

#include "control.hpp"

#include <chrono>
#include <thread>

#include "atom/utils/qtimer.hpp"

INDIFilterwheelControl::INDIFilterwheelControl(std::string name)
    : INDIFilterwheelBase(name) {
}

auto INDIFilterwheelControl::getPositionDetails() -> std::optional<std::tuple<double, double, double>> {
    INDI::PropertyNumber property = device_.getProperty("FILTER_SLOT");
    if (!property.isValid()) {
        logger_->error("Unable to find FILTER_SLOT property");
        return std::nullopt;
    }
    return std::make_tuple(property[0].getValue(), property[0].getMin(), property[0].getMax());
}

auto INDIFilterwheelControl::getPosition() -> std::optional<int> {
    INDI::PropertyNumber property = device_.getProperty("FILTER_SLOT");
    if (!property.isValid()) {
        logger_->error("Unable to find FILTER_SLOT property");
        return std::nullopt;
    }
    return static_cast<int>(property[0].getValue());
}

auto INDIFilterwheelControl::setPosition(int position) -> bool {
    if (!isValidPosition(position)) {
        logger_->error("Invalid position: {}", position);
        return false;
    }

    INDI::PropertyNumber property = device_.getProperty("FILTER_SLOT");
    if (!property.isValid()) {
        logger_->error("Unable to find FILTER_SLOT property");
        return false;
    }

    logger_->info("Setting filter position to: {}", position);
    updateFilterWheelState(FilterWheelState::MOVING);

    property[0].value = position;
    sendNewProperty(property);

    // Wait for movement to complete
    if (!waitForMovementComplete()) {
        logger_->error("Timeout waiting for filter wheel to reach position {}", position);
        updateFilterWheelState(FilterWheelState::ERROR);
        return false;
    }

    // Update statistics
    total_moves_++;
    last_move_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    updateFilterWheelState(FilterWheelState::IDLE);
    logger_->info("Filter wheel successfully moved to position {}", position);

    // Notify callback if set
    if (position_callback_) {
        std::string filterName = position < static_cast<int>(slotNames_.size()) ?
                                slotNames_[position] : "Unknown";
        position_callback_(position, filterName);
    }

    return true;
}

auto INDIFilterwheelControl::isMoving() const -> bool {
    return filterwheel_state_ == FilterWheelState::MOVING;
}

auto INDIFilterwheelControl::abortMotion() -> bool {
    INDI::PropertySwitch property = device_.getProperty("FILTER_ABORT_MOTION");
    if (!property.isValid()) {
        logger_->warn("FILTER_ABORT_MOTION property not available");
        return false;
    }

    logger_->info("Aborting filter wheel motion");
    property[0].s = ISS_ON;
    sendNewProperty(property);

    updateFilterWheelState(FilterWheelState::IDLE);
    logger_->info("Filter wheel motion aborted");
    return true;
}

auto INDIFilterwheelControl::homeFilterWheel() -> bool {
    INDI::PropertySwitch property = device_.getProperty("FILTER_HOME");
    if (!property.isValid()) {
        logger_->warn("FILTER_HOME property not available");
        return false;
    }

    logger_->info("Homing filter wheel...");
    updateFilterWheelState(FilterWheelState::MOVING);

    property[0].s = ISS_ON;
    sendNewProperty(property);

    if (!waitForMovementComplete()) {
        logger_->error("Timeout waiting for filter wheel homing");
        updateFilterWheelState(FilterWheelState::ERROR);
        return false;
    }

    updateFilterWheelState(FilterWheelState::IDLE);
    logger_->info("Filter wheel homing completed");
    return true;
}

auto INDIFilterwheelControl::calibrateFilterWheel() -> bool {
    INDI::PropertySwitch property = device_.getProperty("FILTER_CALIBRATE");
    if (!property.isValid()) {
        logger_->warn("FILTER_CALIBRATE property not available");
        return false;
    }

    logger_->info("Calibrating filter wheel...");
    updateFilterWheelState(FilterWheelState::MOVING);

    property[0].s = ISS_ON;
    sendNewProperty(property);

    if (!waitForMovementComplete()) {
        logger_->error("Timeout waiting for filter wheel calibration");
        updateFilterWheelState(FilterWheelState::ERROR);
        return false;
    }

    updateFilterWheelState(FilterWheelState::IDLE);
    logger_->info("Filter wheel calibration completed");
    return true;
}

auto INDIFilterwheelControl::getFilterCount() -> int {
    return static_cast<int>(slotNames_.size());
}

auto INDIFilterwheelControl::isValidPosition(int position) -> bool {
    return position >= minSlot_ && position <= maxSlot_;
}

void INDIFilterwheelControl::updateMovementState(bool isMoving) {
    if (isMoving) {
        updateFilterWheelState(FilterWheelState::MOVING);
    } else {
        updateFilterWheelState(FilterWheelState::IDLE);
    }
}

auto INDIFilterwheelControl::waitForMovementComplete(int timeoutMs) -> bool {
    atom::utils::ElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        INDI::PropertyNumber property = device_.getProperty("FILTER_SLOT");
        if (property.isValid() && property.getState() == IPS_OK) {
            return true;
        }
    }

    return false;
}
