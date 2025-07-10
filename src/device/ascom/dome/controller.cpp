/*
 * controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Modular Controller auto ASCOMDomeController::stopRotation() -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->stopAzimuthMovement(); // Use public method
}mentation

*************************************************/

#include "controller.hpp"

#include <spdlog/spdlog.h>

namespace lithium::ascom::dome {

ASCOMDomeController::ASCOMDomeController(std::string name)
    : AtomDome(std::move(name)) {
    spdlog::info("Initializing ASCOM Dome Controller: {}", getName());
    
    // Initialize components
    hardware_interface_ = std::make_shared<components::HardwareInterface>();
    azimuth_manager_ = std::make_shared<components::AzimuthManager>(hardware_interface_);
    shutter_manager_ = std::make_shared<components::ShutterManager>(hardware_interface_);
    parking_manager_ = std::make_shared<components::ParkingManager>(hardware_interface_, azimuth_manager_);
    telescope_coordinator_ = std::make_shared<components::TelescopeCoordinator>(hardware_interface_, azimuth_manager_);
    weather_monitor_ = std::make_shared<components::WeatherMonitor>();
    configuration_manager_ = std::make_shared<components::ConfigurationManager>();
    
    // Setup component callbacks
    setupComponentCallbacks();
}

ASCOMDomeController::~ASCOMDomeController() {
    spdlog::info("Destroying ASCOM Dome Controller");
    destroy();
}

auto ASCOMDomeController::initialize() -> bool {
    spdlog::info("Initializing ASCOM Dome Controller");
    
    if (!hardware_interface_->initialize()) {
        spdlog::error("Failed to initialize hardware interface");
        return false;
    }
    
    // Load configuration
    std::string config_path = configuration_manager_->getDefaultConfigPath();
    if (!configuration_manager_->loadConfiguration(config_path)) {
        spdlog::warn("Failed to load configuration, using defaults");
        configuration_manager_->loadDefaultConfiguration();
    }
    
    // Apply configuration to components
    applyConfiguration();
    
    // Start weather monitoring if enabled
    if (configuration_manager_->getBool("weather", "safety_enabled", true)) {
        weather_monitor_->startMonitoring();
    }
    
    spdlog::info("ASCOM Dome Controller initialized successfully");
    return true;
}

auto ASCOMDomeController::destroy() -> bool {
    spdlog::info("Destroying ASCOM Dome Controller");
    
    // Stop monitoring
    if (weather_monitor_) {
        weather_monitor_->stopMonitoring();
    }
    
    if (telescope_coordinator_) {
        telescope_coordinator_->stopAutomaticFollowing();
    }
    
    // Disconnect hardware
    if (hardware_interface_) {
        hardware_interface_->disconnect();
        hardware_interface_->destroy();
    }
    
    // Save configuration if needed
    if (configuration_manager_ && configuration_manager_->hasUnsavedChanges()) {
        configuration_manager_->saveConfiguration(configuration_manager_->getDefaultConfigPath());
    }
    
    return true;
}

auto ASCOMDomeController::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    spdlog::info("Connecting to ASCOM dome: {}", deviceName);
    
    if (!hardware_interface_) {
        spdlog::error("Hardware interface not initialized");
        return false;
    }
    
    // Determine connection type from device name
    components::HardwareInterface::ConnectionType type;
    if (deviceName.find("://") != std::string::npos) {
        type = components::HardwareInterface::ConnectionType::ALPACA_REST;
    } else {
        type = components::HardwareInterface::ConnectionType::COM_DRIVER;
    }
    
    if (hardware_interface_->connect(deviceName, type, timeout)) {
        // Update dome capabilities from hardware interface
        hardware_interface_->updateCapabilities();
        
        spdlog::info("Successfully connected to dome: {}", deviceName);
        return true;
    }
    
    spdlog::error("Failed to connect to dome: {}", deviceName);
    return false;
}

auto ASCOMDomeController::disconnect() -> bool {
    spdlog::info("Disconnecting from ASCOM dome");
    
    if (hardware_interface_) {
        return hardware_interface_->disconnect();
    }
    
    return true;
}

auto ASCOMDomeController::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM dome devices");
    
    if (hardware_interface_) {
        return hardware_interface_->scan();
    }
    
    return {};
}

auto ASCOMDomeController::isConnected() const -> bool {
    return hardware_interface_ && hardware_interface_->isConnected();
}

// Dome state methods
auto ASCOMDomeController::isMoving() const -> bool {
    return azimuth_manager_ && azimuth_manager_->isMoving();
}

auto ASCOMDomeController::isParked() const -> bool {
    return parking_manager_ && parking_manager_->isParked();
}

// Azimuth control methods
auto ASCOMDomeController::getAzimuth() -> std::optional<double> {
    if (!azimuth_manager_) {
        return std::nullopt;
    }
    return azimuth_manager_->getCurrentAzimuth();
}

auto ASCOMDomeController::setAzimuth(double azimuth) -> bool {
    return moveToAzimuth(azimuth);
}

auto ASCOMDomeController::moveToAzimuth(double azimuth) -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->moveToAzimuth(azimuth);
}

auto ASCOMDomeController::rotateClockwise() -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->rotateClockwise(); // No argument
}

auto ASCOMDomeController::rotateCounterClockwise() -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->rotateCounterClockwise(); // No argument
}

auto ASCOMDomeController::stopRotation() -> bool {
    return abortMotion();
}

auto ASCOMDomeController::abortMotion() -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->stopMovement();
}

auto ASCOMDomeController::syncAzimuth(double azimuth) -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->syncAzimuth(azimuth); // Use correct method name
}
}

// Parking methods
auto ASCOMDomeController::park() -> bool {
    if (!parking_manager_) {
        return false;
    }
    return parking_manager_->park();
}

auto ASCOMDomeController::unpark() -> bool {
    if (!parking_manager_) {
        return false;
    }
    return parking_manager_->unpark();
}

auto ASCOMDomeController::getParkPosition() -> std::optional<double> {
    if (!parking_manager_) {
        return std::nullopt;
    }
    return parking_manager_->getParkPosition();
}

auto ASCOMDomeController::setParkPosition(double azimuth) -> bool {
    if (!parking_manager_) {
        return false;
    }
    return parking_manager_->setParkPosition(azimuth);
}

auto ASCOMDomeController::canPark() -> bool {
    return parking_manager_ && parking_manager_->canPark();
}

// Shutter control methods
auto ASCOMDomeController::openShutter() -> bool {
    if (!shutter_manager_) {
        return false;
    }
    return shutter_manager_->openShutter();
}

auto ASCOMDomeController::closeShutter() -> bool {
    if (!shutter_manager_) {
        return false;
    }
    return shutter_manager_->closeShutter();
}

auto ASCOMDomeController::abortShutter() -> bool {
    if (!shutter_manager_) {
        return false;
    }
    return shutter_manager_->abortShutter();
}

auto ASCOMDomeController::getShutterState() -> ShutterState {
    if (!shutter_manager_) {
        return ShutterState::UNKNOWN;
    }
    
    auto state = shutter_manager_->getShutterState();
    // Convert from component enum to AtomDome enum
    switch (state) {
        case components::ShutterState::OPEN: return ShutterState::OPEN;
        case components::ShutterState::CLOSED: return ShutterState::CLOSED;
        case components::ShutterState::OPENING: return ShutterState::OPENING;
        case components::ShutterState::CLOSING: return ShutterState::CLOSING;
        case components::ShutterState::ERROR: return ShutterState::ERROR;
        default: return ShutterState::UNKNOWN;
    }
}

auto ASCOMDomeController::hasShutter() -> bool {
    return shutter_manager_ && shutter_manager_->hasShutter();
}

// Speed control methods
auto ASCOMDomeController::getRotationSpeed() -> std::optional<double> {
    if (!azimuth_manager_) {
        return std::nullopt;
    }
    return azimuth_manager_->getRotationSpeed();
}

auto ASCOMDomeController::setRotationSpeed(double speed) -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->setRotationSpeed(speed);
}

auto ASCOMDomeController::getMaxSpeed() -> double {
    if (!azimuth_manager_) {
        return 10.0; // Default
    }
    return azimuth_manager_->getSpeedRange().second;
}

auto ASCOMDomeController::getMinSpeed() -> double {
    if (!azimuth_manager_) {
        return 1.0; // Default
    }
    return azimuth_manager_->getSpeedRange().first;
}

// Telescope coordination methods
auto ASCOMDomeController::followTelescope(bool enable) -> bool {
    if (!telescope_coordinator_) {
        return false;
    }
    return telescope_coordinator_->followTelescope(enable);
}

auto ASCOMDomeController::isFollowingTelescope() -> bool {
    return telescope_coordinator_ && telescope_coordinator_->isFollowingTelescope();
}

auto ASCOMDomeController::calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double {
    if (!telescope_coordinator_) {
        return telescopeAz; // Simple pass-through
    }
    return telescope_coordinator_->calculateDomeAzimuth(telescopeAz, telescopeAlt);
}

auto ASCOMDomeController::setTelescopePosition(double az, double alt) -> bool {
    if (!telescope_coordinator_) {
        return false;
    }
    return telescope_coordinator_->setTelescopePosition(az, alt);
}

// Home position methods
auto ASCOMDomeController::findHome() -> bool {
    if (!parking_manager_) {
        return false;
    }
    return parking_manager_->findHome();
}

auto ASCOMDomeController::setHome() -> bool {
    if (!parking_manager_) {
        return false;
    }
    return parking_manager_->setHome();
}

auto ASCOMDomeController::gotoHome() -> bool {
    if (!parking_manager_) {
        return false;
    }
    return parking_manager_->gotoHome();
}

auto ASCOMDomeController::getHomePosition() -> std::optional<double> {
    if (!parking_manager_) {
        return std::nullopt;
    }
    return parking_manager_->getHomePosition();
}

// Backlash compensation methods
auto ASCOMDomeController::getBacklash() -> double {
    if (!azimuth_manager_) {
        return 0.0;
    }
    return azimuth_manager_->getBacklashCompensation();
}

auto ASCOMDomeController::setBacklash(double backlash) -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->setBacklashCompensation(backlash);
}

auto ASCOMDomeController::enableBacklashCompensation(bool enable) -> bool {
    if (!azimuth_manager_) {
        return false;
    }
    return azimuth_manager_->enableBacklashCompensation(enable);
}

auto ASCOMDomeController::isBacklashCompensationEnabled() -> bool {
    return azimuth_manager_ && azimuth_manager_->isBacklashCompensationEnabled();
}

// Weather monitoring methods
auto ASCOMDomeController::canOpenShutter() -> bool {
    if (!weather_monitor_ || !shutter_manager_) {
        return false;
    }
    return weather_monitor_->isSafeToOperate() && shutter_manager_->canOpenShutter();
}

auto ASCOMDomeController::isSafeToOperate() -> bool {
    if (!weather_monitor_) {
        return true; // Default to safe if no weather monitoring
    }
    return weather_monitor_->isSafeToOperate();
}

auto ASCOMDomeController::getWeatherStatus() -> std::string {
    if (!weather_monitor_) {
        return "No weather monitoring";
    }
    return weather_monitor_->getWeatherStatus();
}

// Statistics methods (placeholder implementations)
auto ASCOMDomeController::getTotalRotation() -> double {
    return total_rotation_.load();
}

auto ASCOMDomeController::resetTotalRotation() -> bool {
    total_rotation_.store(0.0);
    return true;
}

auto ASCOMDomeController::getShutterOperations() -> uint64_t {
    if (!shutter_manager_) {
        return 0;
    }
    return shutter_manager_->getOperationsCount();
}

auto ASCOMDomeController::resetShutterOperations() -> bool {
    if (!shutter_manager_) {
        return false;
    }
    return shutter_manager_->resetOperationsCount();
}

// Preset methods (placeholder implementations)
auto ASCOMDomeController::savePreset(int slot, double azimuth) -> bool {
    presets_[slot] = azimuth;
    return true;
}

auto ASCOMDomeController::loadPreset(int slot) -> bool {
    if (slot >= 0 && slot < static_cast<int>(presets_.size()) && presets_[slot].has_value()) {
        return moveToAzimuth(presets_[slot].value());
    }
    return false;
}

auto ASCOMDomeController::getPreset(int slot) -> std::optional<double> {
    if (slot >= 0 && slot < static_cast<int>(presets_.size())) {
        return presets_[slot];
    }
    return std::nullopt;
}

auto ASCOMDomeController::deletePreset(int slot) -> bool {
    if (slot >= 0 && slot < static_cast<int>(presets_.size())) {
        presets_[slot] = std::nullopt;
        return true;
    }
    return false;
}

// Private helper methods
auto ASCOMDomeController::setupComponentCallbacks() -> void {
    // Setup callbacks for component coordination
    if (azimuth_manager_) {
        azimuth_manager_->setMovementCallback([this](double current_azimuth, bool is_moving) {
            if (monitoring_system_) {
                monitoring_system_->updateAzimuthStatus(current_azimuth, is_moving);
            }
        });
    }
    
    if (shutter_manager_) {
        shutter_manager_->setStatusCallback([this](ShutterState state) {
            if (monitoring_system_) {
                monitoring_system_->updateShutterStatus(state);
            }
        });
    }
    
    if (weather_monitor_) {
        weather_monitor_->setWeatherCallback([this](const components::WeatherConditions& conditions) {
            if (monitoring_system_) {
                monitoring_system_->updateWeatherConditions(conditions);
            }
            
            // Auto-close shutter if unsafe conditions
            if (!conditions.is_safe && shutter_manager_) {
                spdlog::warn("Unsafe weather conditions detected, closing shutter");
                shutter_manager_->closeShutter();
            }
        });
    }
    
    if (telescope_coordinator_) {
        telescope_coordinator_->setFollowingCallback([this](double target_azimuth) {
            if (azimuth_manager_) {
                azimuth_manager_->moveToAzimuth(target_azimuth);
            }
        });
    }
}

auto ASCOMDomeController::applyConfiguration() -> void {
    if (!configuration_manager_) {
        return;
    }
    
    // Apply azimuth settings
    if (azimuth_manager_) {
        components::AzimuthManager::AzimuthSettings settings;
        settings.default_speed = configuration_manager_->getDouble("movement", "default_speed", 5.0);
        settings.max_speed = configuration_manager_->getDouble("movement", "max_speed", 10.0);
        settings.min_speed = configuration_manager_->getDouble("movement", "min_speed", 1.0);
        settings.position_tolerance = configuration_manager_->getDouble("movement", "position_tolerance", 0.5);
        settings.movement_timeout = configuration_manager_->getInt("movement", "movement_timeout", 300);
        settings.backlash_compensation = configuration_manager_->getDouble("movement", "backlash_compensation", 0.0);
        settings.backlash_enabled = configuration_manager_->getBool("movement", "backlash_enabled", false);
        
        azimuth_manager_->setAzimuthSettings(settings);
    }
    
    // Apply telescope coordination settings
    if (telescope_coordinator_) {
        components::TelescopeCoordinator::TelescopeParameters params;
        params.radius_from_center = configuration_manager_->getDouble("telescope", "radius_from_center", 0.0);
        params.height_offset = configuration_manager_->getDouble("telescope", "height_offset", 0.0);
        params.azimuth_offset = configuration_manager_->getDouble("telescope", "azimuth_offset", 0.0);
        params.altitude_offset = configuration_manager_->getDouble("telescope", "altitude_offset", 0.0
    
    // Apply parking settings
    if (parking_manager_) {
        double park_pos = configuration_manager_->getDouble("dome", "park_position", 0.0);
        parking_manager_->setParkPosition(park_pos);
    }
}

auto ASCOMDomeController::updateDomeCapabilities(const components::ASCOMDomeCapabilities& capabilities) -> void {
    DomeCapabilities dome_caps;
    dome_caps.canPark = capabilities.can_park;
    dome_caps.canSync = capabilities.can_sync_azimuth;
    dome_caps.canAbort = true; // Assume always available
    dome_caps.hasShutter = capabilities.can_set_shutter;
    dome_caps.canSetAzimuth = capabilities.can_set_azimuth;
    dome_caps.canSetParkPosition = capabilities.can_set_park;
    dome_caps.hasBacklash = true; // Software implementation
    
    setDomeCapabilities(dome_caps);
}

auto ASCOMDomeController::setupComponentCallbacks() -> void {
    // Setup callbacks for component coordination
    if (azimuth_manager_) {
        azimuth_manager_->setMovementCallback([this](double current_azimuth, bool is_moving) {
            if (monitoring_system_) {
                monitoring_system_->updateAzimuthStatus(current_azimuth, is_moving);
            }
        });
    }
    
    if (shutter_manager_) {
        shutter_manager_->setStatusCallback([this](ShutterState state) {
            if (monitoring_system_) {
                monitoring_system_->updateShutterStatus(state);
            }
        });
    }
    
    if (weather_monitor_) {
        weather_monitor_->setWeatherCallback([this](const components::WeatherConditions& conditions) {
            if (monitoring_system_) {
                monitoring_system_->updateWeatherConditions(conditions);
            }
            
            // Auto-close shutter if unsafe conditions
            if (!conditions.is_safe && shutter_manager_) {
                spdlog::warn("Unsafe weather conditions detected, closing shutter");
                shutter_manager_->closeShutter();
            }
        });
    }
    
    if (telescope_coordinator_) {
        telescope_coordinator_->setFollowingCallback([this](double target_azimuth) {
            if (azimuth_manager_) {
                azimuth_manager_->moveToAzimuth(target_azimuth);
            }
        });
    }
}

} // namespace lithium::ascom::dome
