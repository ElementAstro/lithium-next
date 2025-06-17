#include "movement_controller.hpp"

namespace lithium::device::indi::focuser {

bool MovementController::initialize(FocuserState& state) {
    state_ = &state;
    state_->logger_->info("{}: Initializing movement controller", getComponentName());
    return true;
}

void MovementController::cleanup() {
    if (state_) {
        state_->logger_->info("{}: Cleaning up movement controller", getComponentName());
    }
    state_ = nullptr;
    client_ = nullptr;
}

bool MovementController::moveSteps(int steps) {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for movement");
        }
        return false;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("REL_FOCUS_POSITION");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find REL_FOCUS_POSITION property");
        return false;
    }

    property[0].value = steps;
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    updateStatistics(steps);
    state_->logger_->info("Moving {} steps", steps);
    return true;
}

bool MovementController::moveToPosition(int position) {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for movement");
        }
        return false;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("ABS_FOCUS_POSITION");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find ABS_FOCUS_POSITION property");
        return false;
    }

    int currentPos = state_->currentPosition_.load();
    int steps = position - currentPos;
    
    property[0].value = position;
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    state_->targetPosition_ = position;
    updateStatistics(steps);
    state_->logger_->info("Moving to position {}", position);
    return true;
}

bool MovementController::moveInward(int steps) {
    if (!setDirection(FocusDirection::IN)) {
        return false;
    }
    return moveSteps(steps);
}

bool MovementController::moveOutward(int steps) {
    if (!setDirection(FocusDirection::OUT)) {
        return false;
    }
    return moveSteps(steps);
}

bool MovementController::moveForDuration(int durationMs) {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for timed movement");
        }
        return false;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("FOCUS_TIMER");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find FOCUS_TIMER property");
        return false;
    }

    property[0].value = durationMs;
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    state_->logger_->info("Moving for {} ms", durationMs);
    return true;
}

bool MovementController::abortMove() {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for abort");
        }
        return false;
    }

    INDI::PropertySwitch property = state_->device_.getProperty("FOCUS_ABORT_MOTION");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find FOCUS_ABORT_MOTION property");
        return false;
    }

    property[0].setState(ISS_ON);
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    state_->isFocuserMoving_ = false;
    state_->logger_->info("Aborting focuser movement");
    return true;
}

bool MovementController::syncPosition(int position) {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for sync");
        }
        return false;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("FOCUS_SYNC");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find FOCUS_SYNC property");
        return false;
    }

    property[0].value = position;
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    state_->currentPosition_ = position;
    state_->logger_->info("Syncing position to {}", position);
    return true;
}

bool MovementController::setSpeed(double speed) {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for speed setting");
        }
        return false;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find FOCUS_SPEED property");
        return false;
    }

    property[0].value = speed;
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    state_->currentFocusSpeed_ = speed;
    state_->logger_->info("Setting focuser speed to {}", speed);
    return true;
}

std::optional<double> MovementController::getSpeed() const {
    if (!state_ || !state_->device_.isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        return std::nullopt;
    }
    
    return property[0].getValue();
}

int MovementController::getMaxSpeed() const {
    // Most INDI focusers don't have a specific max speed property
    // Return a reasonable default
    return 100;
}

std::pair<int, int> MovementController::getSpeedRange() const {
    // Standard INDI focuser speed range
    return {1, 100};
}

bool MovementController::setDirection(FocusDirection direction) {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for direction setting");
        }
        return false;
    }

    INDI::PropertySwitch property = state_->device_.getProperty("FOCUS_MOTION");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find FOCUS_MOTION property");
        return false;
    }

    if (FocusDirection::IN == direction) {
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
    } else {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
    }
    
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    state_->focusDirection_ = direction;
    state_->logger_->info("Setting focuser direction to {}", 
                         direction == FocusDirection::IN ? "IN" : "OUT");
    return true;
}

std::optional<FocusDirection> MovementController::getDirection() const {
    if (!state_ || !state_->device_.isValid()) {
        return std::nullopt;
    }

    INDI::PropertySwitch property = state_->device_.getProperty("FOCUS_MOTION");
    if (!property.isValid()) {
        return std::nullopt;
    }
    
    if (property[0].getState() == ISS_ON) {
        return FocusDirection::IN;
    }
    return FocusDirection::OUT;
}

std::optional<int> MovementController::getPosition() const {
    if (!state_ || !state_->device_.isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("ABS_FOCUS_POSITION");
    if (!property.isValid()) {
        return std::nullopt;
    }
    
    return property[0].getValue();
}

bool MovementController::isMoving() const {
    if (!state_) {
        return false;
    }
    return state_->isFocuserMoving_.load();
}

bool MovementController::setMaxLimit(int maxLimit) {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for max limit setting");
        }
        return false;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("FOCUS_MAX");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find FOCUS_MAX property");
        return false;
    }

    property[0].value = maxLimit;
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    state_->maxPosition_ = maxLimit;
    state_->logger_->info("Setting max position limit to {}", maxLimit);
    return true;
}

std::optional<int> MovementController::getMaxLimit() const {
    if (!state_ || !state_->device_.isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = state_->device_.getProperty("FOCUS_MAX");
    if (!property.isValid()) {
        return std::nullopt;
    }
    
    return property[0].getValue();
}

bool MovementController::setMinLimit(int minLimit) {
    if (!state_) {
        return false;
    }

    state_->minPosition_ = minLimit;
    state_->logger_->info("Setting min position limit to {}", minLimit);
    return true;
}

std::optional<int> MovementController::getMinLimit() const {
    if (!state_) {
        return std::nullopt;
    }
    return state_->minPosition_;
}

bool MovementController::setReversed(bool reversed) {
    if (!state_ || !state_->device_.isValid()) {
        if (state_) {
            state_->logger_->error("Device not available for reverse setting");
        }
        return false;
    }

    INDI::PropertySwitch property = state_->device_.getProperty("FOCUS_REVERSE_MOTION");
    if (!property.isValid()) {
        state_->logger_->error("Unable to find FOCUS_REVERSE_MOTION property");
        return false;
    }

    if (reversed) {
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
    } else {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
    }
    
    if (client_) {
        client_->sendNewProperty(property);
    }
    
    state_->isReverse_ = reversed;
    state_->logger_->info("Setting focuser reverse to {}", reversed ? "ON" : "OFF");
    return true;
}

std::optional<bool> MovementController::isReversed() const {
    if (!state_ || !state_->device_.isValid()) {
        return std::nullopt;
    }

    INDI::PropertySwitch property = state_->device_.getProperty("FOCUS_REVERSE_MOTION");
    if (!property.isValid()) {
        return std::nullopt;
    }
    
    if (property[0].getState() == ISS_ON) {
        return true;
    }
    if (property[1].getState() == ISS_ON) {
        return false;
    }
    return std::nullopt;
}

void MovementController::updateStatistics(int steps) {
    if (!state_) {
        return;
    }

    state_->lastMoveSteps_ = steps;
    state_->totalSteps_ += std::abs(steps);
    
    auto now = std::chrono::steady_clock::now();
    if (lastMoveStart_.time_since_epoch().count() > 0) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastMoveStart_).count();
        state_->lastMoveDuration_ = static_cast<int>(duration);
    }
    lastMoveStart_ = now;
}

} // namespace lithium::device::indi::focuser
