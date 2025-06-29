/*
 * asi_focuser.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Electronic Auto Focuser (EAF) implementation

*************************************************/

#include "main.hpp"

#include <spdlog/spdlog.h>
#include "atom/log/loguru.hpp"

#include "controller.hpp"

namespace lithium::device::asi::focuser {

// ASIFocuser implementation
ASIFocuser::ASIFocuser(const std::string& name)
    : AtomFocuser(name),
      controller_(std::make_unique<controller::ASIFocuserController>(this)) {
    // Initialize ASI EAF specific capabilities
    FocuserCapabilities caps;
    caps.canAbsoluteMove = true;
    caps.canRelativeMove = true;
    caps.canAbort = true;
    caps.canReverse = true;
    caps.canSync = false;
    caps.hasTemperature = true;
    caps.hasBacklash = true;
    caps.hasSpeedControl = false;
    caps.maxPosition = 31000;
    caps.minPosition = 0;
    setFocuserCapabilities(caps);

    LOG_F(INFO, "Created ASI Focuser: {}", name);
    spdlog::info("Created ASI Focuser: {}", name);
}

ASIFocuser::~ASIFocuser() {
    if (controller_) {
        controller_->destroy();
    }
    LOG_F(INFO, "Destroyed ASI Focuser");
    spdlog::info("Destroyed ASI Focuser");
}

auto ASIFocuser::initialize() -> bool {
    spdlog::debug("Initializing ASI Focuser");
    return controller_->initialize();
}

auto ASIFocuser::destroy() -> bool {
    spdlog::debug("Destroying ASI Focuser");
    return controller_->destroy();
}

auto ASIFocuser::connect(const std::string& deviceName, int timeout,
                         int maxRetry) -> bool {
    spdlog::info("Connecting to device: {}, timeout: {}, maxRetry: {}", deviceName, timeout, maxRetry);
    return controller_->connect(deviceName, timeout, maxRetry);
}

auto ASIFocuser::disconnect() -> bool {
    spdlog::info("Disconnecting focuser");
    return controller_->disconnect();
}

auto ASIFocuser::isConnected() const -> bool {
    bool connected = controller_->isConnected();
    spdlog::debug("isConnected: {}", connected);
    return connected;
}

auto ASIFocuser::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASI focuser devices");
    std::vector<std::string> devices;
    controller_->scan(devices);
    spdlog::debug("Found {} devices", devices.size());
    return devices;
}

// AtomFocuser interface implementation
auto ASIFocuser::isMoving() const -> bool {
    bool moving = controller_->isMoving();
    spdlog::debug("isMoving: {}", moving);
    return moving;
}

// Speed control
auto ASIFocuser::getSpeed() -> std::optional<double> {
    double speed = controller_->getSpeed();
    spdlog::debug("getSpeed: {}", speed);
    return speed;
}

auto ASIFocuser::setSpeed(double speed) -> bool {
    spdlog::info("setSpeed: {}", speed);
    return controller_->setSpeed(speed);
}

auto ASIFocuser::getMaxSpeed() -> int {
    int maxSpeed = controller_->getMaxSpeed();
    spdlog::debug("getMaxSpeed: {}", maxSpeed);
    return maxSpeed;
}

auto ASIFocuser::getSpeedRange() -> std::pair<int, int> {
    auto range = controller_->getSpeedRange();
    spdlog::debug("getSpeedRange: {} - {}", range.first, range.second);
    return range;
}

// Direction control
auto ASIFocuser::getDirection() -> std::optional<FocusDirection> {
    auto dir = controller_->isDirectionReversed() ? FocusDirection::IN : FocusDirection::OUT;
    spdlog::debug("getDirection: {}", dir == FocusDirection::IN ? "IN" : "OUT");
    return dir;
}

auto ASIFocuser::setDirection(FocusDirection direction) -> bool {
    spdlog::info("setDirection: {}", direction == FocusDirection::IN ? "IN" : "OUT");
    return controller_->setDirection(direction == FocusDirection::IN);
}

// Limits
auto ASIFocuser::getMaxLimit() -> std::optional<int> {
    int maxLimit = controller_->getMaxPosition();
    spdlog::debug("getMaxLimit: {}", maxLimit);
    return maxLimit;
}

auto ASIFocuser::setMaxLimit(int maxLimit) -> bool {
    spdlog::info("setMaxLimit: {}", maxLimit);
    return controller_->setMaxLimit(maxLimit);
}

auto ASIFocuser::getMinLimit() -> std::optional<int> {
    int minLimit = controller_->getMinPosition();
    spdlog::debug("getMinLimit: {}", minLimit);
    return minLimit;
}

auto ASIFocuser::setMinLimit(int minLimit) -> bool {
    spdlog::info("setMinLimit: {}", minLimit);
    return controller_->setMinLimit(minLimit);
}

// Reverse control
auto ASIFocuser::isReversed() -> std::optional<bool> {
    auto reversed = controller_->isDirectionReversed();
    spdlog::debug("isReversed: {}", reversed ? "true" : "false");
    return reversed;
}

auto ASIFocuser::setReversed(bool reversed) -> bool {
    spdlog::info("setReversed: {}", reversed ? "true" : "false");
    return controller_->setDirection(reversed);
}

// Movement control
auto ASIFocuser::moveSteps(int steps) -> bool {
    spdlog::info("moveSteps: {}", steps);
    return controller_->moveSteps(steps);
}

auto ASIFocuser::moveToPosition(int position) -> bool {
    spdlog::info("moveToPosition: {}", position);
    return controller_->moveToPosition(position);
}

auto ASIFocuser::getPosition() -> std::optional<int> {
    try {
        int pos = controller_->getPosition();
        spdlog::debug("getPosition: {}", pos);
        return pos;
    } catch (...) {
        spdlog::error("getPosition: exception thrown");
        return std::nullopt;
    }
}

auto ASIFocuser::moveForDuration(int durationMs) -> bool {
    double speed = controller_->getSpeed();
    int steps = static_cast<int>(speed * durationMs / 1000.0);
    spdlog::info("moveForDuration: {} ms (calculated steps: {})", durationMs, steps);
    return controller_->moveSteps(steps);
}

auto ASIFocuser::abortMove() -> bool {
    spdlog::warn("abortMove called");
    return controller_->abortMove();
}

auto ASIFocuser::syncPosition(int position) -> bool {
    spdlog::info("syncPosition: {}", position);
    return controller_->syncPosition(position);
}

// Relative movement
auto ASIFocuser::moveInward(int steps) -> bool {
    spdlog::info("moveInward: {}", steps);
    return controller_->moveSteps(-steps);
}

auto ASIFocuser::moveOutward(int steps) -> bool {
    spdlog::info("moveOutward: {}", steps);
    return controller_->moveSteps(steps);
}

// Backlash compensation
auto ASIFocuser::getBacklash() -> int {
    int backlash = controller_->getBacklash();
    spdlog::debug("getBacklash: {}", backlash);
    return backlash;
}

auto ASIFocuser::setBacklash(int backlash) -> bool {
    spdlog::info("setBacklash: {}", backlash);
    return controller_->setBacklash(backlash);
}

auto ASIFocuser::enableBacklashCompensation(bool enable) -> bool {
    spdlog::info("enableBacklashCompensation: {}", enable ? "true" : "false");
    return controller_->enableBacklashCompensation(enable);
}

auto ASIFocuser::isBacklashCompensationEnabled() -> bool {
    bool enabled = controller_->isBacklashCompensationEnabled();
    spdlog::debug("isBacklashCompensationEnabled: {}", enabled);
    return enabled;
}

// Temperature monitoring
auto ASIFocuser::getExternalTemperature() -> std::optional<double> {
    auto temp = controller_->getTemperature();
    spdlog::debug("getExternalTemperature: {}", temp ? std::to_string(*temp) : "n/a");
    return temp;
}

auto ASIFocuser::getChipTemperature() -> std::optional<double> {
    auto temp = controller_->getTemperature();
    spdlog::debug("getChipTemperature: {}", temp ? std::to_string(*temp) : "n/a");
    return temp;
}

auto ASIFocuser::hasTemperatureSensor() -> bool {
    bool hasSensor = controller_->hasTemperatureSensor();
    spdlog::debug("hasTemperatureSensor: {}", hasSensor);
    return hasSensor;
}

// Temperature compensation
auto ASIFocuser::getTemperatureCompensation() -> TemperatureCompensation {
    TemperatureCompensation comp;
    comp.enabled = controller_->isTemperatureCompensationEnabled();
    comp.coefficient = controller_->getTemperatureCoefficient();
    comp.temperature = controller_->getTemperature().value_or(0.0);
    comp.compensationOffset = 0.0;
    spdlog::debug("getTemperatureCompensation: enabled={}, coefficient={}, temperature={}",
                  comp.enabled, comp.coefficient, comp.temperature);
    return comp;
}

auto ASIFocuser::setTemperatureCompensation(const TemperatureCompensation& comp)
    -> bool {
    spdlog::info("setTemperatureCompensation: enabled={}, coefficient={}", comp.enabled, comp.coefficient);
    bool success = true;
    success &= controller_->setTemperatureCoefficient(comp.coefficient);
    success &= controller_->enableTemperatureCompensation(comp.enabled);
    return success;
}

auto ASIFocuser::enableTemperatureCompensation(bool enable) -> bool {
    spdlog::info("enableTemperatureCompensation: {}", enable ? "true" : "false");
    return controller_->enableTemperatureCompensation(enable);
}

// Auto focus
auto ASIFocuser::startAutoFocus() -> bool {
    LOG_F(INFO, "Starting auto focus");
    spdlog::info("Starting auto focus");
    // Implementation would start auto focus routine
    return true;
}

auto ASIFocuser::stopAutoFocus() -> bool {
    LOG_F(INFO, "Stopping auto focus");
    spdlog::info("Stopping auto focus");
    // Implementation would stop auto focus routine
    return true;
}

auto ASIFocuser::isAutoFocusing() -> bool {
    spdlog::debug("isAutoFocusing: false");
    return false;  // Would query from auto focus routine
}

auto ASIFocuser::getAutoFocusProgress() -> double {
    spdlog::debug("getAutoFocusProgress: 0.0");
    return 0.0;  // Would return progress from auto focus routine
}

// Presets
auto ASIFocuser::savePreset(int slot, int position) -> bool {
    LOG_F(INFO, "Saving preset {} at position {}", slot, position);
    spdlog::info("Saving preset {} at position {}", slot, position);
    // Implementation would save preset
    return true;
}

auto ASIFocuser::loadPreset(int slot) -> bool {
    LOG_F(INFO, "Loading preset {}", slot);
    spdlog::info("Loading preset {}", slot);
    // Implementation would load preset
    return true;
}

auto ASIFocuser::getPreset(int slot) -> std::optional<int> {
    spdlog::debug("getPreset: slot {}", slot);
    // Implementation would return preset position
    return std::nullopt;
}

auto ASIFocuser::deletePreset(int slot) -> bool {
    LOG_F(INFO, "Deleting preset {}", slot);
    spdlog::info("Deleting preset {}", slot);
    // Implementation would delete preset
    return true;
}

// Statistics
auto ASIFocuser::getTotalSteps() -> uint64_t {
    uint64_t steps = controller_->getTotalSteps();
    spdlog::debug("getTotalSteps: {}", steps);
    return steps;
}

auto ASIFocuser::resetTotalSteps() -> bool {
    LOG_F(INFO, "Reset total steps counter");
    spdlog::info("Reset total steps counter");
    return true;
}

auto ASIFocuser::getLastMoveSteps() -> int {
    int steps = controller_->getLastMoveSteps();
    spdlog::debug("getLastMoveSteps: {}", steps);
    return steps;
}

auto ASIFocuser::getLastMoveDuration() -> int {
    int duration = controller_->getLastMoveDuration();
    spdlog::debug("getLastMoveDuration: {}", duration);
    return duration;
}

// Callbacks
void ASIFocuser::setPositionCallback(PositionCallback callback) {
    spdlog::debug("setPositionCallback set");
    controller_->setPositionCallback(callback);
}

void ASIFocuser::setTemperatureCallback(TemperatureCallback callback) {
    spdlog::debug("setTemperatureCallback set");
    controller_->setTemperatureCallback(callback);
}

void ASIFocuser::setMoveCompleteCallback(MoveCompleteCallback callback) {
    spdlog::debug("setMoveCompleteCallback set");
    controller_->setMoveCompleteCallback([callback](bool success) {
        if (callback) {
            callback(success,
                     success ? "Move completed successfully" : "Move failed");
        }
    });
}

// ASI-specific extended functionality
auto ASIFocuser::setPosition(int position) -> bool {
    spdlog::info("setPosition: {}", position);
    return controller_->moveToPosition(position);
}

auto ASIFocuser::getMaxPosition() const -> int {
    int maxPos = controller_->getMaxPosition();
    spdlog::debug("getMaxPosition: {}", maxPos);
    return maxPos;
}

auto ASIFocuser::stopMovement() -> bool {
    spdlog::warn("stopMovement called");
    return controller_->abortMove();
}

auto ASIFocuser::setStepSize(int stepSize) -> bool {
    LOG_F(INFO, "Set step size to: {}", stepSize);
    spdlog::info("Set step size to: {}", stepSize);
    return true;
}

auto ASIFocuser::getStepSize() const -> int {
    spdlog::debug("getStepSize: 1");
    return 1;  // Default step size
}

auto ASIFocuser::homeToZero() -> bool {
    spdlog::info("homeToZero called");
    return controller_->homeToZero();
}

auto ASIFocuser::setHomePosition() -> bool {
    spdlog::info("setHomePosition called");
    return controller_->setHomePosition();
}

auto ASIFocuser::calibrateFocuser() -> bool {
    spdlog::info("calibrateFocuser called");
    return controller_->calibrateFocuser();
}

auto ASIFocuser::findOptimalPosition(int startPos, int endPos, int stepSize)
    -> std::optional<int> {
    LOG_F(INFO, "Finding optimal position from {} to {} with step size {}",
          startPos, endPos, stepSize);
    spdlog::info("Finding optimal position from {} to {} with step size {}", startPos, endPos, stepSize);
    // Implementation would perform focus sweep and find optimal position
    return std::nullopt;
}

// Advanced features
auto ASIFocuser::setTemperatureCoefficient(double coefficient) -> bool {
    spdlog::info("setTemperatureCoefficient: {}", coefficient);
    return controller_->setTemperatureCoefficient(coefficient);
}

auto ASIFocuser::getTemperatureCoefficient() const -> double {
    double coeff = controller_->getTemperatureCoefficient();
    spdlog::debug("getTemperatureCoefficient: {}", coeff);
    return coeff;
}

auto ASIFocuser::setMovementDirection(bool reverse) -> bool {
    spdlog::info("setMovementDirection: {}", reverse ? "reverse" : "normal");
    return controller_->setDirection(reverse);
}

auto ASIFocuser::isDirectionReversed() const -> bool {
    bool reversed = controller_->isDirectionReversed();
    spdlog::debug("isDirectionReversed: {}", reversed);
    return reversed;
}

auto ASIFocuser::enableBeep(bool enable) -> bool {
    spdlog::info("enableBeep: {}", enable ? "true" : "false");
    return controller_->enableBeep(enable);
}

auto ASIFocuser::isBeepEnabled() const -> bool {
    bool enabled = controller_->isBeepEnabled();
    spdlog::debug("isBeepEnabled: {}", enabled);
    return enabled;
}

// Focusing sequences
auto ASIFocuser::performFocusSequence(const std::vector<int>& positions,
                                      std::function<double(int)> qualityMeasure)
    -> std::optional<int> {
    LOG_F(INFO, "Performing focus sequence with {} positions",
          positions.size());
    spdlog::info("Performing focus sequence with {} positions", positions.size());
    // Implementation would perform focus sequence
    return std::nullopt;
}

auto ASIFocuser::performCoarseFineAutofocus(int coarseStepSize,
                                            int fineStepSize, int searchRange)
    -> std::optional<int> {
    LOG_F(INFO,
          "Performing coarse-fine autofocus: coarse={}, fine={}, range={}",
          coarseStepSize, fineStepSize, searchRange);
    spdlog::info("Performing coarse-fine autofocus: coarse={}, fine={}, range={}", coarseStepSize, fineStepSize, searchRange);
    // Implementation would perform coarse-fine autofocus
    return std::nullopt;
}

auto ASIFocuser::performVCurveFocus(int startPos, int endPos, int stepCount)
    -> std::optional<int> {
    LOG_F(INFO, "Performing V-curve focus from {} to {} with {} steps",
          startPos, endPos, stepCount);
    spdlog::info("Performing V-curve focus from {} to {} with {} steps", startPos, endPos, stepCount);
    // Implementation would perform V-curve focus
    return std::nullopt;
}

// Configuration management
auto ASIFocuser::saveConfiguration(const std::string& filename) -> bool {
    spdlog::info("saveConfiguration: {}", filename);
    return controller_->saveConfiguration(filename);
}

auto ASIFocuser::loadConfiguration(const std::string& filename) -> bool {
    spdlog::info("loadConfiguration: {}", filename);
    return controller_->loadConfiguration(filename);
}

auto ASIFocuser::resetToDefaults() -> bool {
    controller_->setBacklash(0);
    controller_->enableBacklashCompensation(false);
    controller_->setTemperatureCoefficient(0.0);
    controller_->enableTemperatureCompensation(false);
    controller_->setDirection(false);
    controller_->enableBeep(false);
    controller_->enableHighResolutionMode(false);
    LOG_F(INFO, "Reset focuser to defaults");
    spdlog::info("Reset focuser to defaults");
    return true;
}

// Hardware information
auto ASIFocuser::getFirmwareVersion() const -> std::string {
    auto version = controller_->getFirmwareVersion();
    spdlog::debug("getFirmwareVersion: {}", version);
    return version;
}

auto ASIFocuser::getSerialNumber() const -> std::string {
    auto serial = controller_->getSerialNumber();
    spdlog::debug("getSerialNumber: {}", serial);
    return serial;
}

auto ASIFocuser::setDeviceAlias(const std::string& alias) -> bool {
    spdlog::info("setDeviceAlias: {}", alias);
    return controller_->setDeviceAlias(alias);
}

auto ASIFocuser::getSDKVersion() -> std::string {
    auto version = controller_->getSDKVersion();
    spdlog::debug("getSDKVersion: {}", version);
    return version;
}

auto ASIFocuser::resetFocuserPosition(int position) -> bool {
    spdlog::info("resetFocuserPosition: {}", position);
    return controller_->resetPosition(position);
}

auto ASIFocuser::setMaxStepPosition(int maxStep) -> bool {
    spdlog::info("setMaxStepPosition: {}", maxStep);
    return controller_->setMaxStep(maxStep);
}

auto ASIFocuser::getMaxStepPosition() -> int {
    int maxStep = controller_->getMaxStep();
    spdlog::debug("getMaxStepPosition: {}", maxStep);
    return maxStep;
}

auto ASIFocuser::getStepRange() -> int {
    int range = controller_->getStepRange();
    spdlog::debug("getStepRange: {}", range);
    return range;
}

}  // namespace lithium::device::asi::focuser
