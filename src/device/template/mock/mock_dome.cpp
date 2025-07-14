/*
 * mock_dome.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "mock_dome.hpp"

#include <cmath>

MockDome::MockDome(const std::string& name)
    : AtomDome(name), gen_(rd_()), noise_dist_(-0.1, 0.1) {
    // Set default capabilities
    DomeCapabilities caps;
    caps.canPark = true;
    caps.canSync = true;
    caps.canAbort = true;
    caps.hasShutter = true;
    caps.hasVariable = false;
    caps.canSetAzimuth = true;
    caps.canSetParkPosition = true;
    caps.hasBacklash = true;
    caps.minAzimuth = 0.0;
    caps.maxAzimuth = 360.0;
    setDomeCapabilities(caps);

    // Set default parameters
    DomeParameters params;
    params.diameter = 3.0;
    params.height = 2.5;
    params.slitWidth = 1.0;
    params.slitHeight = 1.2;
    params.telescopeRadius = 0.5;
    setDomeParameters(params);

    // Initialize state
    current_azimuth_ = 0.0;
    shutter_state_ = ShutterState::CLOSED;
    park_position_ = 0.0;
    home_position_ = 0.0;
}

bool MockDome::initialize() {
    setState(DeviceState::IDLE);
    updateDomeState(DomeState::IDLE);
    updateShutterState(ShutterState::CLOSED);
    return true;
}

bool MockDome::destroy() {
    if (is_dome_moving_) {
        abortMotion();
    }
    if (is_shutter_moving_) {
        abortShutter();
    }
    setState(DeviceState::UNKNOWN);
    return true;
}

bool MockDome::connect(const std::string& port, int timeout, int maxRetry) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!isSimulated()) {
        return false;
    }

    connected_ = true;
    setState(DeviceState::IDLE);
    updateDomeState(DomeState::IDLE);
    return true;
}

bool MockDome::disconnect() {
    if (is_dome_moving_) {
        abortMotion();
    }
    if (is_shutter_moving_) {
        abortShutter();
    }
    connected_ = false;
    setState(DeviceState::UNKNOWN);
    return true;
}

std::vector<std::string> MockDome::scan() {
    if (isSimulated()) {
        return {"MockDome_1", "MockDome_2"};
    }
    return {};
}

bool MockDome::isMoving() const {
    std::lock_guard<std::mutex> lock(move_mutex_);
    return is_dome_moving_;
}

bool MockDome::isParked() const {
    return is_parked_;
}

auto MockDome::getAzimuth() -> std::optional<double> {
    if (!isConnected()) return std::nullopt;

    addPositionNoise();
    return current_azimuth_;
}

auto MockDome::setAzimuth(double azimuth) -> bool {
    return moveToAzimuth(azimuth);
}

auto MockDome::moveToAzimuth(double azimuth) -> bool {
    if (!isConnected()) return false;
    if (isMoving()) return false;

    double normalized_azimuth = normalizeAzimuth(azimuth);
    target_azimuth_ = normalized_azimuth;

    updateDomeState(DomeState::MOVING);

    if (dome_move_thread_.joinable()) {
        dome_move_thread_.join();
    }

    dome_move_thread_ = std::thread(&MockDome::simulateDomeMove, this, normalized_azimuth);
    return true;
}

auto MockDome::rotateClockwise() -> bool {
    if (!isConnected()) return false;

    double new_azimuth = normalizeAzimuth(current_azimuth_ + 10.0);
    return moveToAzimuth(new_azimuth);
}

auto MockDome::rotateCounterClockwise() -> bool {
    if (!isConnected()) return false;

    double new_azimuth = normalizeAzimuth(current_azimuth_ - 10.0);
    return moveToAzimuth(new_azimuth);
}

auto MockDome::stopRotation() -> bool {
    return abortMotion();
}

auto MockDome::abortMotion() -> bool {
    if (!isConnected()) return false;

    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_dome_moving_ = false;
    }

    if (dome_move_thread_.joinable()) {
        dome_move_thread_.join();
    }

    updateDomeState(DomeState::IDLE);
    return true;
}

auto MockDome::syncAzimuth(double azimuth) -> bool {
    if (!isConnected()) return false;
    if (isMoving()) return false;

    current_azimuth_ = normalizeAzimuth(azimuth);
    return true;
}

auto MockDome::park() -> bool {
    if (!isConnected()) return false;

    updateDomeState(DomeState::PARKING);

    // Move to park position and close shutter
    bool success = moveToAzimuth(park_position_);
    if (success) {
        // Wait for movement to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        closeShutter();
        is_parked_ = true;
        updateDomeState(DomeState::PARKED);

        if (park_callback_) {
            park_callback_(true);
        }
    }

    return success;
}

auto MockDome::unpark() -> bool {
    if (!isConnected()) return false;
    if (!is_parked_) return true;

    is_parked_ = false;
    updateDomeState(DomeState::IDLE);

    if (park_callback_) {
        park_callback_(false);
    }

    return true;
}

auto MockDome::getParkPosition() -> std::optional<double> {
    if (!isConnected()) return std::nullopt;
    return park_position_;
}

auto MockDome::setParkPosition(double azimuth) -> bool {
    if (!isConnected()) return false;

    park_position_ = normalizeAzimuth(azimuth);
    return true;
}

auto MockDome::canPark() -> bool {
    return dome_capabilities_.canPark;
}

auto MockDome::openShutter() -> bool {
    if (!isConnected()) return false;
    if (!dome_capabilities_.hasShutter) return false;
    if (!checkWeatherSafety()) return false;

    if (shutter_state_ == ShutterState::OPEN) return true;

    updateShutterState(ShutterState::OPENING);

    if (shutter_thread_.joinable()) {
        shutter_thread_.join();
    }

    shutter_thread_ = std::thread(&MockDome::simulateShutterOperation, this, ShutterState::OPEN);
    return true;
}

auto MockDome::closeShutter() -> bool {
    if (!isConnected()) return false;
    if (!dome_capabilities_.hasShutter) return false;

    if (shutter_state_ == ShutterState::CLOSED) return true;

    updateShutterState(ShutterState::CLOSING);

    if (shutter_thread_.joinable()) {
        shutter_thread_.join();
    }

    shutter_thread_ = std::thread(&MockDome::simulateShutterOperation, this, ShutterState::CLOSED);
    return true;
}

auto MockDome::abortShutter() -> bool {
    if (!isConnected()) return false;

    {
        std::lock_guard<std::mutex> lock(shutter_mutex_);
        is_shutter_moving_ = false;
    }

    if (shutter_thread_.joinable()) {
        shutter_thread_.join();
    }

    updateShutterState(ShutterState::ERROR);
    return true;
}

auto MockDome::getShutterState() -> ShutterState {
    return shutter_state_;
}

auto MockDome::hasShutter() -> bool {
    return dome_capabilities_.hasShutter;
}

auto MockDome::getRotationSpeed() -> std::optional<double> {
    if (!isConnected()) return std::nullopt;
    return rotation_speed_;
}

auto MockDome::setRotationSpeed(double speed) -> bool {
    if (!isConnected()) return false;
    if (speed < getMinSpeed() || speed > getMaxSpeed()) return false;

    rotation_speed_ = speed;
    return true;
}

auto MockDome::getMaxSpeed() -> double {
    return 20.0; // degrees per second
}

auto MockDome::getMinSpeed() -> double {
    return 1.0; // degrees per second
}

auto MockDome::followTelescope(bool enable) -> bool {
    if (!isConnected()) return false;

    is_following_telescope_ = enable;
    return true;
}

auto MockDome::isFollowingTelescope() -> bool {
    return is_following_telescope_;
}

auto MockDome::calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double {
    // Simplified dome azimuth calculation
    // In reality, this would consider dome geometry, telescope offset, etc.
    return normalizeAzimuth(telescopeAz);
}

auto MockDome::setTelescopePosition(double az, double alt) -> bool {
    if (!isConnected()) return false;

    telescope_azimuth_ = normalizeAzimuth(az);
    telescope_altitude_ = alt;

    // If following telescope, move dome
    if (is_following_telescope_) {
        double dome_az = calculateDomeAzimuth(az, alt);
        return moveToAzimuth(dome_az);
    }

    return true;
}

auto MockDome::findHome() -> bool {
    if (!isConnected()) return false;

    // Simulate finding home position
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    home_position_ = 0.0;
    return true;
}

auto MockDome::setHome() -> bool {
    if (!isConnected()) return false;

    home_position_ = current_azimuth_;
    return true;
}

auto MockDome::gotoHome() -> bool {
    if (!isConnected()) return false;

    return moveToAzimuth(home_position_);
}

auto MockDome::getHomePosition() -> std::optional<double> {
    if (!isConnected()) return std::nullopt;
    return home_position_;
}

auto MockDome::getBacklash() -> double {
    return backlash_amount_;
}

auto MockDome::setBacklash(double backlash) -> bool {
    backlash_amount_ = std::abs(backlash);
    return true;
}

auto MockDome::enableBacklashCompensation(bool enable) -> bool {
    backlash_enabled_ = enable;
    return true;
}

auto MockDome::isBacklashCompensationEnabled() -> bool {
    return backlash_enabled_;
}

auto MockDome::canOpenShutter() -> bool {
    return checkWeatherSafety() && dome_capabilities_.hasShutter;
}

auto MockDome::isSafeToOperate() -> bool {
    return checkWeatherSafety();
}

auto MockDome::getWeatherStatus() -> std::string {
    if (weather_safe_) {
        return "Weather conditions are safe for operation";
    } else {
        return "Weather conditions are unsafe - high winds detected";
    }
}

auto MockDome::getTotalRotation() -> double {
    return total_rotation_;
}

auto MockDome::resetTotalRotation() -> bool {
    total_rotation_ = 0.0;
    return true;
}

auto MockDome::getShutterOperations() -> uint64_t {
    return shutter_operations_;
}

auto MockDome::resetShutterOperations() -> bool {
    shutter_operations_ = 0;
    return true;
}

auto MockDome::savePreset(int slot, double azimuth) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) return false;

    presets_[slot] = normalizeAzimuth(azimuth);
    return true;
}

auto MockDome::loadPreset(int slot) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) return false;
    if (!presets_[slot].has_value()) return false;

    return moveToAzimuth(*presets_[slot]);
}

auto MockDome::getPreset(int slot) -> std::optional<double> {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) return std::nullopt;
    return presets_[slot];
}

auto MockDome::deletePreset(int slot) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) return false;

    presets_[slot].reset();
    return true;
}

void MockDome::simulateDomeMove(double target_azimuth) {
    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_dome_moving_ = true;
    }

    double start_position = current_azimuth_;
    auto [total_distance, direction] = getShortestPath(current_azimuth_, target_azimuth);

    // Calculate move duration based on speed
    double move_duration = total_distance / rotation_speed_;
    auto move_duration_ms = std::chrono::milliseconds(static_cast<int>(move_duration * 1000));

    // Simulate gradual movement
    const int steps = 15;
    auto step_duration = move_duration_ms / steps;
    double step_azimuth = total_distance / steps;

    if (direction == DomeMotion::COUNTER_CLOCKWISE) {
        step_azimuth = -step_azimuth;
    }

    for (int i = 0; i < steps; ++i) {
        {
            std::lock_guard<std::mutex> lock(move_mutex_);
            if (!is_dome_moving_) break;
        }

        std::this_thread::sleep_for(step_duration);
        current_azimuth_ = normalizeAzimuth(current_azimuth_ + step_azimuth);

        if (azimuth_callback_) {
            azimuth_callback_(current_azimuth_);
        }
    }

    current_azimuth_ = target_azimuth;
    total_rotation_ += getAzimuthalDistance(start_position, target_azimuth);

    {
        std::lock_guard<std::mutex> lock(move_mutex_);
        is_dome_moving_ = false;
    }

    updateDomeState(DomeState::IDLE);

    if (move_complete_callback_) {
        move_complete_callback_(true, "Dome movement completed");
    }
}

void MockDome::simulateShutterOperation(ShutterState target_state) {
    {
        std::lock_guard<std::mutex> lock(shutter_mutex_);
        is_shutter_moving_ = true;
    }

    // Simulate shutter operation time
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    {
        std::lock_guard<std::mutex> lock(shutter_mutex_);
        if (is_shutter_moving_) {
            shutter_state_ = target_state;
            shutter_operations_++;
            is_shutter_moving_ = false;

            if (shutter_callback_) {
                shutter_callback_(target_state);
            }
        }
    }
}

void MockDome::addPositionNoise() {
    current_azimuth_ += noise_dist_(gen_);
    current_azimuth_ = normalizeAzimuth(current_azimuth_);
}

bool MockDome::checkWeatherSafety() const {
    // Simulate random weather conditions
    std::uniform_real_distribution<> weather_dist(0.0, 1.0);
    return weather_dist(gen_) > 0.1; // 90% chance of good weather
}
