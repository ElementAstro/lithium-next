/*
 * controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Rotator Controller Implementation

*************************************************/

#include "controller.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <thread>

namespace lithium::device::ascom::rotator {

ASCOMRotatorController::ASCOMRotatorController(std::string name, const RotatorConfig& config)
    : AtomRotator(std::move(name)), config_(config) {
    spdlog::info("ASCOMRotatorController constructor called with name: {}", getName());
}

ASCOMRotatorController::~ASCOMRotatorController() {
    spdlog::info("ASCOMRotatorController destructor called");
    destroy();
}

auto ASCOMRotatorController::initialize() -> bool {
    spdlog::info("Initializing ASCOM Rotator Controller");

    if (is_initialized_.load()) {
        spdlog::warn("Controller already initialized");
        return true;
    }

    if (!validateConfiguration(config_)) {
        setLastError("Invalid configuration");
        return false;
    }

    if (!initializeComponents()) {
        setLastError("Failed to initialize components");
        return false;
    }

    setupComponentCallbacks();

    is_initialized_.store(true);
    spdlog::info("ASCOM Rotator Controller initialized successfully");
    return true;
}

auto ASCOMRotatorController::destroy() -> bool {
    spdlog::info("Destroying ASCOM Rotator Controller");

    stopMonitoring();
    disconnect();
    removeComponentCallbacks();

    if (!destroyComponents()) {
        spdlog::warn("Failed to properly destroy all components");
    }

    is_initialized_.store(false);
    return true;
}

auto ASCOMRotatorController::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    spdlog::info("Connecting to ASCOM rotator device: {}", deviceName);

    if (!is_initialized_.load()) {
        setLastError("Controller not initialized");
        return false;
    }

    if (is_connected_.load()) {
        spdlog::warn("Already connected to a device");
        return true;
    }

    // Connect hardware interface
    if (!hardware_interface_->connect(deviceName, config_.connection_type)) {
        setLastError("Failed to connect hardware interface: " + hardware_interface_->getLastError());
        return false;
    }

    // Initialize position manager
    if (!position_manager_->initialize()) {
        setLastError("Failed to initialize position manager: " + position_manager_->getLastError());
        hardware_interface_->disconnect();
        return false;
    }

    // Update device capabilities
    property_manager_->updateDeviceCapabilities();

    // Set position limits if enabled
    if (config_.enable_position_limits) {
        position_manager_->setPositionLimits(config_.min_position, config_.max_position);
    }

    // Configure backlash compensation
    if (config_.enable_backlash_compensation) {
        position_manager_->enableBacklashCompensation(true);
        position_manager_->setBacklashAmount(config_.backlash_amount);
    }

    // Start monitoring if enabled
    if (config_.enable_position_monitoring) {
        position_manager_->startPositionMonitoring(config_.position_monitor_interval_ms);
    }

    if (config_.enable_property_monitoring) {
        // Start property monitoring for key properties
        std::vector<std::string> monitored_props = {"position", "ismoving", "connected"};
        property_manager_->startPropertyMonitoring(monitored_props, config_.property_monitor_interval_ms);
    }

    is_connected_.store(true);
    notifyConnectionChange(true);

    // Start global monitoring
    if (!monitoring_active_.load()) {
        startMonitoring();
    }

    spdlog::info("Successfully connected to rotator device");
    return true;
}

auto ASCOMRotatorController::disconnect() -> bool {
    spdlog::info("Disconnecting from ASCOM rotator device");

    if (!is_connected_.load()) {
        return true;
    }

    // Stop monitoring
    stopMonitoring();

    // Stop position monitoring
    if (position_manager_) {
        position_manager_->stopPositionMonitoring();
    }

    // Stop property monitoring
    if (property_manager_) {
        property_manager_->stopPropertyMonitoring();
    }

    // Disconnect hardware
    if (hardware_interface_) {
        hardware_interface_->disconnect();
    }

    is_connected_.store(false);
    notifyConnectionChange(false);

    spdlog::info("Disconnected from rotator device");
    return true;
}

auto ASCOMRotatorController::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM rotator devices");

    if (!hardware_interface_) {
        return {};
    }

    return hardware_interface_->scanDevices();
}

auto ASCOMRotatorController::isConnected() const -> bool {
    return is_connected_.load();
}

auto ASCOMRotatorController::isMoving() const -> bool {
    if (!position_manager_) {
        return false;
    }
    return position_manager_->isMoving();
}

auto ASCOMRotatorController::getPosition() -> std::optional<double> {
    if (!position_manager_) {
        return std::nullopt;
    }
    return position_manager_->getCurrentPosition();
}

auto ASCOMRotatorController::setPosition(double angle) -> bool {
    return moveToAngle(angle);
}

auto ASCOMRotatorController::moveToAngle(double angle) -> bool {
    if (!position_manager_) {
        setLastError("Position manager not available");
        return false;
    }

    components::MovementParams params;
    params.target_angle = angle;
    params.speed = config_.default_speed;
    params.acceleration = config_.default_acceleration;
    params.tolerance = config_.position_tolerance;
    params.timeout_ms = config_.movement_timeout_ms;

    return position_manager_->moveToAngle(angle, params);
}

auto ASCOMRotatorController::rotateByAngle(double angle) -> bool {
    if (!position_manager_) {
        setLastError("Position manager not available");
        return false;
    }

    components::MovementParams params;
    params.speed = config_.default_speed;
    params.acceleration = config_.default_acceleration;
    params.tolerance = config_.position_tolerance;
    params.timeout_ms = config_.movement_timeout_ms;

    return position_manager_->rotateByAngle(angle, params);
}

auto ASCOMRotatorController::abortMove() -> bool {
    if (!position_manager_) {
        return false;
    }
    return position_manager_->abortMove();
}

auto ASCOMRotatorController::syncPosition(double angle) -> bool {
    if (!position_manager_) {
        setLastError("Position manager not available");
        return false;
    }
    return position_manager_->syncPosition(angle);
}

auto ASCOMRotatorController::getDirection() -> std::optional<RotatorDirection> {
    if (!position_manager_) {
        return std::nullopt;
    }
    return position_manager_->getDirection();
}

auto ASCOMRotatorController::setDirection(RotatorDirection direction) -> bool {
    if (!position_manager_) {
        return false;
    }
    return position_manager_->setDirection(direction);
}

auto ASCOMRotatorController::isReversed() -> bool {
    if (!position_manager_) {
        return false;
    }
    return position_manager_->isReversed();
}

auto ASCOMRotatorController::setReversed(bool reversed) -> bool {
    if (!position_manager_) {
        return false;
    }
    return position_manager_->setReversed(reversed);
}

auto ASCOMRotatorController::getSpeed() -> std::optional<double> {
    if (!position_manager_) {
        return std::nullopt;
    }
    return position_manager_->getSpeed();
}

auto ASCOMRotatorController::setSpeed(double speed) -> bool {
    if (!position_manager_) {
        return false;
    }

    if (position_manager_->setSpeed(speed)) {
        config_.default_speed = speed;
        return true;
    }
    return false;
}

auto ASCOMRotatorController::getMaxSpeed() -> double {
    if (!position_manager_) {
        return 50.0; // Default max speed
    }
    return position_manager_->getMaxSpeed();
}

auto ASCOMRotatorController::getMinSpeed() -> double {
    if (!position_manager_) {
        return 0.1; // Default min speed
    }
    return position_manager_->getMinSpeed();
}

auto ASCOMRotatorController::getMinPosition() -> double {
    if (!position_manager_) {
        return 0.0;
    }
    auto limits = position_manager_->getPositionLimits();
    return limits.first;
}

auto ASCOMRotatorController::getMaxPosition() -> double {
    if (!position_manager_) {
        return 360.0;
    }
    auto limits = position_manager_->getPositionLimits();
    return limits.second;
}

auto ASCOMRotatorController::setLimits(double min, double max) -> bool {
    if (!position_manager_) {
        return false;
    }

    if (position_manager_->setPositionLimits(min, max)) {
        config_.enable_position_limits = true;
        config_.min_position = min;
        config_.max_position = max;
        return true;
    }
    return false;
}

auto ASCOMRotatorController::getBacklash() -> double {
    if (!position_manager_) {
        return 0.0;
    }
    return position_manager_->getBacklashAmount();
}

auto ASCOMRotatorController::setBacklash(double backlash) -> bool {
    if (!position_manager_) {
        return false;
    }

    if (position_manager_->setBacklashAmount(backlash)) {
        config_.backlash_amount = backlash;
        return true;
    }
    return false;
}

auto ASCOMRotatorController::enableBacklashCompensation(bool enable) -> bool {
    if (!position_manager_) {
        return false;
    }

    if (position_manager_->enableBacklashCompensation(enable)) {
        config_.enable_backlash_compensation = enable;
        return true;
    }
    return false;
}

auto ASCOMRotatorController::isBacklashCompensationEnabled() -> bool {
    if (!position_manager_) {
        return false;
    }
    return position_manager_->isBacklashCompensationEnabled();
}

auto ASCOMRotatorController::getTemperature() -> std::optional<double> {
    if (!property_manager_) {
        return std::nullopt;
    }
    return property_manager_->getDoubleProperty("temperature");
}

auto ASCOMRotatorController::hasTemperatureSensor() -> bool {
    if (!property_manager_) {
        return false;
    }
    auto capabilities = property_manager_->getDeviceCapabilities();
    return capabilities.has_temperature_sensor;
}

auto ASCOMRotatorController::savePreset(int slot, double angle) -> bool {
    if (!preset_manager_) {
        return false;
    }
    return preset_manager_->savePreset(slot, angle);
}

auto ASCOMRotatorController::loadPreset(int slot) -> bool {
    if (!preset_manager_) {
        return false;
    }
    return preset_manager_->loadPreset(slot);
}

auto ASCOMRotatorController::getPreset(int slot) -> std::optional<double> {
    if (!preset_manager_) {
        return std::nullopt;
    }
    return preset_manager_->getPresetAngle(slot);
}

auto ASCOMRotatorController::deletePreset(int slot) -> bool {
    if (!preset_manager_) {
        return false;
    }
    return preset_manager_->deletePreset(slot);
}

auto ASCOMRotatorController::getTotalRotation() -> double {
    if (!position_manager_) {
        return 0.0;
    }
    return position_manager_->getTotalRotation();
}

auto ASCOMRotatorController::resetTotalRotation() -> bool {
    if (!position_manager_) {
        return false;
    }
    return position_manager_->resetTotalRotation();
}

auto ASCOMRotatorController::getLastMoveAngle() -> double {
    if (!position_manager_) {
        return 0.0;
    }
    auto [angle, duration] = position_manager_->getLastMoveInfo();
    return angle;
}

auto ASCOMRotatorController::getLastMoveDuration() -> int {
    if (!position_manager_) {
        return 0;
    }
    auto [angle, duration] = position_manager_->getLastMoveInfo();
    return static_cast<int>(duration.count());
}

auto ASCOMRotatorController::getStatus() -> RotatorStatus {
    RotatorStatus status;

    status.connected = isConnected();
    status.moving = isMoving();
    status.emergency_stop_active = isEmergencyStopActive();
    status.last_error = getLastError();
    status.last_update = std::chrono::steady_clock::now();

    if (position_manager_) {
        auto pos = position_manager_->getCurrentPosition();
        if (pos) status.current_position = *pos;

        status.target_position = position_manager_->getTargetPosition();

        auto mech_pos = position_manager_->getMechanicalPosition();
        if (mech_pos) status.mechanical_position = *mech_pos;

        status.movement_state = position_manager_->getMovementState();
    }

    status.temperature = getTemperature();

    return status;
}

auto ASCOMRotatorController::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

// Private helper methods

auto ASCOMRotatorController::initializeComponents() -> bool {
    try {
        // Create components
        hardware_interface_ = std::make_shared<components::HardwareInterface>();
        position_manager_ = std::make_shared<components::PositionManager>(hardware_interface_);
        property_manager_ = std::make_shared<components::PropertyManager>(hardware_interface_);

        if (config_.enable_presets) {
            preset_manager_ = std::make_shared<components::PresetManager>(hardware_interface_, position_manager_);
        }

        // Initialize components
        if (!hardware_interface_->initialize()) {
            setLastError("Failed to initialize hardware interface");
            return false;
        }

        if (!property_manager_->initialize()) {
            setLastError("Failed to initialize property manager");
            return false;
        }

        if (preset_manager_ && !preset_manager_->initialize()) {
            setLastError("Failed to initialize preset manager");
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        setLastError("Exception during component initialization: " + std::string(e.what()));
        return false;
    }
}

auto ASCOMRotatorController::destroyComponents() -> bool {
    bool success = true;

    if (preset_manager_) {
        if (!preset_manager_->destroy()) {
            success = false;
        }
        preset_manager_.reset();
    }

    if (position_manager_) {
        if (!position_manager_->destroy()) {
            success = false;
        }
        position_manager_.reset();
    }

    if (property_manager_) {
        if (!property_manager_->destroy()) {
            success = false;
        }
        property_manager_.reset();
    }

    if (hardware_interface_) {
        if (!hardware_interface_->destroy()) {
            success = false;
        }
        hardware_interface_.reset();
    }

    return success;
}

auto ASCOMRotatorController::setupComponentCallbacks() -> void {
    if (position_manager_) {
        position_manager_->setPositionCallback(
            [this](double current, double target) {
                notifyPositionChange(current, target);
            }
        );

        position_manager_->setMovementCallback(
            [this](components::MovementState state) {
                notifyMovementStateChange(state);
            }
        );
    }
}

auto ASCOMRotatorController::validateConfiguration(const RotatorConfig& config) -> bool {
    if (config.device_name.empty()) {
        setLastError("Device name cannot be empty");
        return false;
    }

    if (config.default_speed <= 0 || config.default_speed > 100) {
        setLastError("Invalid default speed");
        return false;
    }

    if (config.enable_position_limits && config.min_position >= config.max_position) {
        setLastError("Invalid position limits");
        return false;
    }

    return true;
}

auto ASCOMRotatorController::setLastError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("ASCOMRotatorController error: {}", error);
    notifyError(error);
}

auto ASCOMRotatorController::notifyPositionChange(double current, double target) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (position_callback_) {
        position_callback_(current, target);
    }
}

auto ASCOMRotatorController::notifyMovementStateChange(components::MovementState state) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (movement_state_callback_) {
        movement_state_callback_(state);
    }
}

auto ASCOMRotatorController::notifyConnectionChange(bool connected) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (connection_callback_) {
        connection_callback_(connected);
    }
}

auto ASCOMRotatorController::notifyError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    if (error_callback_) {
        error_callback_(error);
    }
}

auto ASCOMRotatorController::startMonitoring() -> bool {
    if (monitoring_active_.load()) {
        return true;
    }

    monitoring_active_.store(true);
    monitor_thread_ = std::make_unique<std::thread>(&ASCOMRotatorController::monitoringLoop, this);

    spdlog::info("Started rotator monitoring");
    return true;
}

auto ASCOMRotatorController::stopMonitoring() -> bool {
    if (!monitoring_active_.load()) {
        return true;
    }

    monitoring_active_.store(false);

    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    monitor_thread_.reset();

    spdlog::info("Stopped rotator monitoring");
    return true;
}

auto ASCOMRotatorController::monitoringLoop() -> void {
    spdlog::debug("Rotator monitoring loop started");

    while (monitoring_active_.load()) {
        try {
            updateStatus();
            checkComponentHealth();
        } catch (const std::exception& e) {
            spdlog::warn("Error in monitoring loop: {}", e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(monitor_interval_ms_));
    }

    spdlog::debug("Rotator monitoring loop ended");
}

auto ASCOMRotatorController::updateStatus() -> void {
    // Status is updated on-demand via getStatus()
    // This could be used for periodic health checks
}

auto ASCOMRotatorController::checkComponentHealth() -> bool {
    // Basic health check - ensure all components are still valid
    if (!hardware_interface_ || !position_manager_ || !property_manager_) {
        setLastError("Critical component failure detected");
        return false;
    }

    return true;
}

} // namespace lithium::device::ascom::rotator
