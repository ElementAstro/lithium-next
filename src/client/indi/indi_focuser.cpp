/*
 * indi_focuser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Focuser Device Implementation

**************************************************/

#include "indi_focuser.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::client::indi {

// ==================== Constructor/Destructor ====================

INDIFocuser::INDIFocuser(std::string name) : INDIDeviceBase(std::move(name)) {
    LOG_DEBUG("INDIFocuser created: {}", name_);
}

INDIFocuser::~INDIFocuser() {
    if (isMoving()) {
        abortMove();
    }
    LOG_DEBUG("INDIFocuser destroyed: {}", name_);
}

// ==================== Connection ====================

auto INDIFocuser::connect(const std::string& deviceName, int timeout,
                          int maxRetry) -> bool {
    if (!INDIDeviceBase::connect(deviceName, timeout, maxRetry)) {
        return false;
    }

    setupPropertyWatchers();
    LOG_INFO("Focuser {} connected", deviceName);
    return true;
}

auto INDIFocuser::disconnect() -> bool {
    if (isMoving()) {
        abortMove();
    }
    return INDIDeviceBase::disconnect();
}

// ==================== Position Control ====================

auto INDIFocuser::moveToPosition(int position) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot move: focuser not connected");
        return false;
    }

    if (isMoving()) {
        LOG_WARN("Focuser already moving");
        return false;
    }

    LOG_INFO("Moving focuser to position: {}", position);

    focuserState_.store(FocuserState::Moving);
    isMoving_.store(true);

    if (!setNumberProperty("ABS_FOCUS_POSITION", "FOCUS_ABSOLUTE_POSITION",
                           static_cast<double>(position))) {
        LOG_ERROR("Failed to set absolute position");
        focuserState_.store(FocuserState::Error);
        isMoving_.store(false);
        return false;
    }

    return true;
}

auto INDIFocuser::moveSteps(int steps) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot move: focuser not connected");
        return false;
    }

    if (isMoving()) {
        LOG_WARN("Focuser already moving");
        return false;
    }

    LOG_INFO("Moving focuser {} steps", steps);

    // Set direction based on sign
    if (steps > 0) {
        setDirection(FocusDirection::Out);
    } else if (steps < 0) {
        setDirection(FocusDirection::In);
        steps = -steps;  // Make positive for the property
    }

    focuserState_.store(FocuserState::Moving);
    isMoving_.store(true);

    if (!setNumberProperty("REL_FOCUS_POSITION", "FOCUS_RELATIVE_POSITION",
                           static_cast<double>(steps))) {
        LOG_ERROR("Failed to set relative position");
        focuserState_.store(FocuserState::Error);
        isMoving_.store(false);
        return false;
    }

    return true;
}

auto INDIFocuser::moveForDuration(int durationMs) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot move: focuser not connected");
        return false;
    }

    if (isMoving()) {
        LOG_WARN("Focuser already moving");
        return false;
    }

    LOG_INFO("Moving focuser for {} ms", durationMs);

    focuserState_.store(FocuserState::Moving);
    isMoving_.store(true);

    if (!setNumberProperty("FOCUS_TIMER", "FOCUS_TIMER_VALUE",
                           static_cast<double>(durationMs))) {
        LOG_ERROR("Failed to set timer");
        focuserState_.store(FocuserState::Error);
        isMoving_.store(false);
        return false;
    }

    return true;
}

auto INDIFocuser::abortMove() -> bool {
    if (!isMoving()) {
        return true;
    }

    LOG_INFO("Aborting focuser move");

    if (!setSwitchProperty("FOCUS_ABORT_MOTION", "ABORT", true)) {
        LOG_ERROR("Failed to abort move");
        return false;
    }

    focuserState_.store(FocuserState::Idle);
    isMoving_.store(false);
    moveCondition_.notify_all();

    return true;
}

auto INDIFocuser::syncPosition(int position) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot sync: focuser not connected");
        return false;
    }

    LOG_INFO("Syncing focuser position to: {}", position);

    if (!setNumberProperty("FOCUS_SYNC", "FOCUS_SYNC_VALUE",
                           static_cast<double>(position))) {
        LOG_ERROR("Failed to sync position");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        positionInfo_.absolute = position;
    }

    return true;
}

auto INDIFocuser::getPosition() const -> std::optional<int> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_.absolute;
}

auto INDIFocuser::getPositionInfo() const -> FocuserPosition {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_;
}

auto INDIFocuser::isMoving() const -> bool { return isMoving_.load(); }

auto INDIFocuser::waitForMove(std::chrono::milliseconds timeout) -> bool {
    if (!isMoving()) {
        return true;
    }

    std::unique_lock<std::mutex> lock(positionMutex_);
    return moveCondition_.wait_for(lock, timeout, [this] {
        return !isMoving_.load();
    });
}

// ==================== Speed Control ====================

auto INDIFocuser::setSpeed(double speed) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set speed: focuser not connected");
        return false;
    }

    LOG_DEBUG("Setting focuser speed to: {}", speed);

    if (!setNumberProperty("FOCUS_SPEED", "FOCUS_SPEED_VALUE", speed)) {
        LOG_ERROR("Failed to set speed");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(speedMutex_);
        speedInfo_.current = speed;
    }

    return true;
}

auto INDIFocuser::getSpeed() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(speedMutex_);
    return speedInfo_.current;
}

auto INDIFocuser::getSpeedInfo() const -> FocuserSpeed {
    std::lock_guard<std::mutex> lock(speedMutex_);
    return speedInfo_;
}

// ==================== Direction Control ====================

auto INDIFocuser::setDirection(FocusDirection direction) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName;
    switch (direction) {
        case FocusDirection::In:
            elemName = "FOCUS_INWARD";
            break;
        case FocusDirection::Out:
            elemName = "FOCUS_OUTWARD";
            break;
        default:
            return false;
    }

    if (!setSwitchProperty("FOCUS_MOTION", elemName, true)) {
        LOG_ERROR("Failed to set direction");
        return false;
    }

    direction_.store(direction);
    return true;
}

auto INDIFocuser::getDirection() const -> FocusDirection {
    return direction_.load();
}

auto INDIFocuser::setReversed(bool reversed) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName = reversed ? "ENABLED" : "DISABLED";

    if (!setSwitchProperty("FOCUS_REVERSE_MOTION", elemName, true)) {
        LOG_ERROR("Failed to set reverse motion");
        return false;
    }

    isReversed_.store(reversed);
    return true;
}

auto INDIFocuser::isReversed() const -> std::optional<bool> {
    return isReversed_.load();
}

// ==================== Limits ====================

auto INDIFocuser::setMaxLimit(int maxLimit) -> bool {
    if (!isConnected()) {
        LOG_ERROR("Cannot set max limit: focuser not connected");
        return false;
    }

    LOG_DEBUG("Setting max limit to: {}", maxLimit);

    if (!setNumberProperty("FOCUS_MAX", "FOCUS_MAX_VALUE",
                           static_cast<double>(maxLimit))) {
        LOG_ERROR("Failed to set max limit");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(positionMutex_);
        positionInfo_.maxPosition = maxLimit;
    }

    return true;
}

auto INDIFocuser::getMaxLimit() const -> std::optional<int> {
    std::lock_guard<std::mutex> lock(positionMutex_);
    return positionInfo_.maxPosition;
}

// ==================== Temperature ====================

auto INDIFocuser::getExternalTemperature() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    if (!temperatureInfo_.hasExternal) {
        return std::nullopt;
    }
    return temperatureInfo_.external;
}

auto INDIFocuser::getChipTemperature() const -> std::optional<double> {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    if (!temperatureInfo_.hasChip) {
        return std::nullopt;
    }
    return temperatureInfo_.chip;
}

auto INDIFocuser::getTemperatureInfo() const -> FocuserTemperature {
    std::lock_guard<std::mutex> lock(temperatureMutex_);
    return temperatureInfo_;
}

// ==================== Backlash ====================

auto INDIFocuser::setBacklashEnabled(bool enabled) -> bool {
    if (!isConnected()) {
        return false;
    }

    std::string elemName = enabled ? "ENABLED" : "DISABLED";

    if (!setSwitchProperty("FOCUS_BACKLASH_TOGGLE", elemName, true)) {
        LOG_ERROR("Failed to set backlash toggle");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(backlashMutex_);
        backlashInfo_.enabled = enabled;
    }

    return true;
}

auto INDIFocuser::setBacklashSteps(int steps) -> bool {
    if (!isConnected()) {
        return false;
    }

    if (!setNumberProperty("FOCUS_BACKLASH_STEPS", "FOCUS_BACKLASH_VALUE",
                           static_cast<double>(steps))) {
        LOG_ERROR("Failed to set backlash steps");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(backlashMutex_);
        backlashInfo_.steps = steps;
    }

    return true;
}

auto INDIFocuser::getBacklashInfo() const -> BacklashInfo {
    std::lock_guard<std::mutex> lock(backlashMutex_);
    return backlashInfo_;
}

// ==================== Mode ====================

auto INDIFocuser::getFocusMode() const -> FocusMode {
    return focusMode_.load();
}

// ==================== Status ====================

auto INDIFocuser::getFocuserState() const -> FocuserState {
    return focuserState_.load();
}

auto INDIFocuser::getStatus() const -> json {
    json status = INDIDeviceBase::getStatus();

    status["focuserState"] = static_cast<int>(focuserState_.load());
    status["isMoving"] = isMoving();
    status["direction"] = static_cast<int>(direction_.load());
    status["isReversed"] = isReversed_.load();
    status["focusMode"] = static_cast<int>(focusMode_.load());

    status["position"] = getPositionInfo().toJson();
    status["speed"] = getSpeedInfo().toJson();
    status["temperature"] = getTemperatureInfo().toJson();
    status["backlash"] = getBacklashInfo().toJson();

    return status;
}

// ==================== Property Handlers ====================

void INDIFocuser::onPropertyDefined(const INDIProperty& property) {
    INDIDeviceBase::onPropertyDefined(property);

    if (property.name == "ABS_FOCUS_POSITION") {
        handlePositionProperty(property);
    } else if (property.name == "FOCUS_SPEED") {
        handleSpeedProperty(property);
    } else if (property.name == "FOCUS_MOTION") {
        handleDirectionProperty(property);
    } else if (property.name == "FOCUS_TEMPERATURE") {
        handleTemperatureProperty(property);
    } else if (property.name == "FOCUS_MAX") {
        handleMaxLimitProperty(property);
    } else if (property.name == "FOCUS_BACKLASH_STEPS") {
        handleBacklashProperty(property);
    }
}

void INDIFocuser::onPropertyUpdated(const INDIProperty& property) {
    INDIDeviceBase::onPropertyUpdated(property);

    if (property.name == "ABS_FOCUS_POSITION" ||
        property.name == "REL_FOCUS_POSITION") {
        handlePositionProperty(property);

        // Check if move completed
        if (property.state == PropertyState::Ok && isMoving()) {
            focuserState_.store(FocuserState::Idle);
            isMoving_.store(false);
            moveCondition_.notify_all();
        } else if (property.state == PropertyState::Alert) {
            focuserState_.store(FocuserState::Error);
            isMoving_.store(false);
            moveCondition_.notify_all();
        }
    } else if (property.name == "FOCUS_SPEED") {
        handleSpeedProperty(property);
    } else if (property.name == "FOCUS_MOTION") {
        handleDirectionProperty(property);
    } else if (property.name == "FOCUS_TEMPERATURE" ||
               property.name == "CHIP_TEMPERATURE") {
        handleTemperatureProperty(property);
    } else if (property.name == "FOCUS_ABORT_MOTION") {
        handleAbortProperty(property);
    } else if (property.name == "FOCUS_BACKLASH_TOGGLE" ||
               property.name == "FOCUS_BACKLASH_STEPS") {
        handleBacklashProperty(property);
    }
}

// ==================== Internal Methods ====================

void INDIFocuser::handlePositionProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "FOCUS_ABSOLUTE_POSITION") {
            positionInfo_.absolute = static_cast<int>(elem.value);
            positionInfo_.maxPosition = static_cast<int>(elem.max);
            positionInfo_.minPosition = static_cast<int>(elem.min);
        } else if (elem.name == "FOCUS_RELATIVE_POSITION") {
            positionInfo_.relative = static_cast<int>(elem.value);
        }
    }
}

void INDIFocuser::handleSpeedProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(speedMutex_);

    for (const auto& elem : property.numbers) {
        if (elem.name == "FOCUS_SPEED_VALUE") {
            speedInfo_.current = elem.value;
            speedInfo_.min = elem.min;
            speedInfo_.max = elem.max;
        }
    }
}

void INDIFocuser::handleDirectionProperty(const INDIProperty& property) {
    for (const auto& elem : property.switches) {
        if (elem.on) {
            if (elem.name == "FOCUS_INWARD") {
                direction_.store(FocusDirection::In);
            } else if (elem.name == "FOCUS_OUTWARD") {
                direction_.store(FocusDirection::Out);
            }
        }
    }
}

void INDIFocuser::handleTemperatureProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(temperatureMutex_);

    if (property.name == "FOCUS_TEMPERATURE") {
        temperatureInfo_.hasExternal = true;
        if (auto temp = property.getNumber("FOCUS_TEMPERATURE_VALUE")) {
            temperatureInfo_.external = *temp;
        }
    } else if (property.name == "CHIP_TEMPERATURE") {
        temperatureInfo_.hasChip = true;
        if (auto temp = property.getNumber("CHIP_TEMPERATURE_VALUE")) {
            temperatureInfo_.chip = *temp;
        }
    }
}

void INDIFocuser::handleBacklashProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(backlashMutex_);

    if (property.name == "FOCUS_BACKLASH_TOGGLE") {
        if (auto enabled = property.getSwitch("ENABLED")) {
            backlashInfo_.enabled = *enabled;
        }
    } else if (property.name == "FOCUS_BACKLASH_STEPS") {
        if (auto steps = property.getNumber("FOCUS_BACKLASH_VALUE")) {
            backlashInfo_.steps = static_cast<int>(*steps);
        }
    }
}

void INDIFocuser::handleMaxLimitProperty(const INDIProperty& property) {
    std::lock_guard<std::mutex> lock(positionMutex_);

    if (auto maxLimit = property.getNumber("FOCUS_MAX_VALUE")) {
        positionInfo_.maxPosition = static_cast<int>(*maxLimit);
    }
}

void INDIFocuser::handleAbortProperty(const INDIProperty& property) {
    if (property.state == PropertyState::Ok) {
        focuserState_.store(FocuserState::Idle);
        isMoving_.store(false);
        moveCondition_.notify_all();
    }
}

void INDIFocuser::setupPropertyWatchers() {
    // Watch position property
    watchProperty("ABS_FOCUS_POSITION", [this](const INDIProperty& prop) {
        handlePositionProperty(prop);
    });

    // Watch temperature property
    watchProperty("FOCUS_TEMPERATURE", [this](const INDIProperty& prop) {
        handleTemperatureProperty(prop);
    });
}

}  // namespace lithium::client::indi
