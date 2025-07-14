#include "movement_controller.hpp"

namespace lithium::device::indi::focuser {

MovementController::MovementController(std::shared_ptr<INDIFocuserCore> core)
    : FocuserComponentBase(std::move(core)) {
}

bool MovementController::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    core->getLogger()->info("{}: Initializing movement controller", getComponentName());
    return true;
}

void MovementController::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("{}: Shutting down movement controller", getComponentName());
    }
}

bool MovementController::moveSteps(int steps) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for movement");
        }
        return false;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("REL_FOCUS_POSITION");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find REL_FOCUS_POSITION property");
        return false;
    }

    property[0].value = steps;

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->getLogger()->info("Moving {} steps via INDI", steps);
        updateStatistics(steps);
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

bool MovementController::moveToPosition(int position) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for movement");
        }
        return false;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("ABS_FOCUS_POSITION");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find ABS_FOCUS_POSITION property");
        return false;
    }

    int currentPos = core->getCurrentPosition();
    int steps = position - currentPos;

    property[0].value = position;

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        updateStatistics(steps);
        core->getLogger()->info("Moving to position {} via INDI", position);
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

bool MovementController::moveInward(int steps) {
    auto core = getCore();
    if (!core) {
        return false;
    }

    // Set direction to inward first
    if (!setDirection(FocusDirection::IN)) {
        core->getLogger()->error("Failed to set focuser direction to inward");
        return false;
    }

    return moveSteps(steps);
}

bool MovementController::moveOutward(int steps) {
    auto core = getCore();
    if (!core) {
        return false;
    }

    // Set direction to outward first
    if (!setDirection(FocusDirection::OUT)) {
        core->getLogger()->error("Failed to set focuser direction to outward");
        return false;
    }

    return moveSteps(steps);
}

bool MovementController::moveForDuration(int durationMs) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for timed movement");
        }
        return false;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_TIMER");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find FOCUS_TIMER property");
        return false;
    }

    property[0].value = durationMs;

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->getLogger()->info("Moving for {} ms via INDI", durationMs);
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

bool MovementController::abortMove() {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for abort");
        }
        return false;
    }

    INDI::PropertySwitch property = core->getDevice().getProperty("FOCUS_ABORT_MOTION");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find FOCUS_ABORT_MOTION property");
        return false;
    }

    property[0].setState(ISS_ON);

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->setMoving(false);
        core->getLogger()->info("Aborting focuser movement via INDI");
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

bool MovementController::syncPosition(int position) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for sync");
        }
        return false;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_SYNC");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find FOCUS_SYNC property");
        return false;
    }

    property[0].value = position;

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->setCurrentPosition(position);
        core->getLogger()->info("Syncing position to {} via INDI", position);
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

bool MovementController::setSpeed(double speed) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for speed setting");
        }
        return false;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find FOCUS_SPEED property");
        return false;
    }

    property[0].value = speed;

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->setCurrentSpeed(speed);
        core->getLogger()->info("Setting speed to {} via INDI", speed);
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

std::optional<double> MovementController::getSpeed() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        return std::nullopt;
    }

    return property[0].value;
}

int MovementController::getMaxSpeed() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return 1;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        return 1;
    }

    return static_cast<int>(property[0].max);
}

std::pair<int, int> MovementController::getSpeedRange() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return {1, 1};
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        return {1, 1};
    }

    return {static_cast<int>(property[0].min), static_cast<int>(property[0].max)};
}

bool MovementController::setDirection(FocusDirection direction) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for direction setting");
        }
        return false;
    }

    INDI::PropertySwitch property = core->getDevice().getProperty("FOCUS_MOTION");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find FOCUS_MOTION property");
        return false;
    }

    // Reset all switches
    for (int i = 0; i < property.count(); i++) {
        property[i].setState(ISS_OFF);
    }

    // Set the appropriate direction
    if (direction == FocusDirection::IN) {
        property.findWidgetByName("FOCUS_INWARD")->setState(ISS_ON);
    } else {
        property.findWidgetByName("FOCUS_OUTWARD")->setState(ISS_ON);
    }

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->setDirection(direction);
        core->getLogger()->info("Setting direction to {} via INDI",
                              direction == FocusDirection::IN ? "inward" : "outward");
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

std::optional<FocusDirection> MovementController::getDirection() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return std::nullopt;
    }

    INDI::PropertySwitch property = core->getDevice().getProperty("FOCUS_MOTION");
    if (!property.isValid()) {
        return std::nullopt;
    }

    auto inwardWidget = property.findWidgetByName("FOCUS_INWARD");
    auto outwardWidget = property.findWidgetByName("FOCUS_OUTWARD");

    if (inwardWidget && inwardWidget->getState() == ISS_ON) {
        return FocusDirection::IN;
    } else if (outwardWidget && outwardWidget->getState() == ISS_ON) {
        return FocusDirection::OUT;
    }

    return std::nullopt;
}

std::optional<int> MovementController::getPosition() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("ABS_FOCUS_POSITION");
    if (!property.isValid()) {
        return std::nullopt;
    }

    return static_cast<int>(property[0].value);
}

bool MovementController::isMoving() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return false;
    }

    INDI::PropertySwitch property = core->getDevice().getProperty("FOCUS_MOTION");
    if (!property.isValid()) {
        return false;
    }

    // Check if any motion switch is active
    for (int i = 0; i < property.count(); i++) {
        if (property[i].getState() == ISS_ON) {
            return true;
        }
    }
    return false;
}

bool MovementController::setMaxLimit(int maxLimit) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for limit setting");
        }
        return false;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_MAX");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find FOCUS_MAX property");
        return false;
    }

    property[0].value = maxLimit;

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->setMaxPosition(maxLimit);
        core->getLogger()->info("Setting max limit to {} via INDI", maxLimit);
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

std::optional<int> MovementController::getMaxLimit() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_MAX");
    if (!property.isValid()) {
        return std::nullopt;
    }

    return static_cast<int>(property[0].value);
}

bool MovementController::setMinLimit(int minLimit) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for limit setting");
        }
        return false;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_MIN");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find FOCUS_MIN property");
        return false;
    }

    property[0].value = minLimit;

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->setMinPosition(minLimit);
        core->getLogger()->info("Setting min limit to {} via INDI", minLimit);
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

std::optional<int> MovementController::getMinLimit() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return std::nullopt;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty("FOCUS_MIN");
    if (!property.isValid()) {
        return std::nullopt;
    }

    return static_cast<int>(property[0].value);
}

bool MovementController::setReversed(bool reversed) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        if (core) {
            core->getLogger()->error("Device not available for reverse setting");
        }
        return false;
    }

    INDI::PropertySwitch property = core->getDevice().getProperty("FOCUS_REVERSE_MOTION");
    if (!property.isValid()) {
        core->getLogger()->error("Unable to find FOCUS_REVERSE_MOTION property");
        return false;
    }

    // Reset all switches
    for (int i = 0; i < property.count(); i++) {
        property[i].setState(ISS_OFF);
    }

    if (reversed) {
        property[0].setState(ISS_ON);  // Enable reverse
    } else {
        property[1].setState(ISS_ON);  // Disable reverse
    }

    // Real INDI client interaction
    if (core->getClient()) {
        core->getClient()->sendNewProperty(property);
        core->setReversed(reversed);
        core->getLogger()->info("Setting reverse motion to {} via INDI", reversed);
        return true;
    } else {
        core->getLogger()->error("INDI client not available");
        return false;
    }
}

std::optional<bool> MovementController::isReversed() const {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return std::nullopt;
    }

    INDI::PropertySwitch property = core->getDevice().getProperty("FOCUS_REVERSE_MOTION");
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
    auto core = getCore();
    if (!core) {
        return;
    }

    // Update core position tracking
    int currentPos = core->getCurrentPosition();
    core->setCurrentPosition(currentPos + steps);

    // Record the move for statistics
    auto now = std::chrono::steady_clock::now();
    if (lastMoveStart_.time_since_epoch().count() > 0) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - lastMoveStart_).count();
        core->getLogger()->debug("Move duration: {} ms", duration);
    }
    lastMoveStart_ = now;
}

bool MovementController::sendPropertyUpdate(const std::string& propertyName, double value) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid() || !core->getClient()) {
        return false;
    }

    INDI::PropertyNumber property = core->getDevice().getProperty(propertyName.c_str());
    if (!property.isValid()) {
        return false;
    }

    property[0].value = value;
    core->getClient()->sendNewProperty(property);
    return true;
}

bool MovementController::sendPropertyUpdate(const std::string& propertyName, const std::vector<bool>& states) {
    auto core = getCore();
    if (!core || !core->getDevice().isValid() || !core->getClient()) {
        return false;
    }

    INDI::PropertySwitch property = core->getDevice().getProperty(propertyName.c_str());
    if (!property.isValid() || property.count() < static_cast<int>(states.size())) {
        return false;
    }

    for (size_t i = 0; i < states.size() && i < static_cast<size_t>(property.count()); i++) {
        property[i].setState(states[i] ? ISS_ON : ISS_OFF);
    }

    core->getClient()->sendNewProperty(property);
    return true;
}

} // namespace lithium::device::indi::focuser
