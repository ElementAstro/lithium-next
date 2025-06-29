/*
 * modular_dome.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "modular_dome.hpp"
#include "core/indi_dome_core.hpp"
#include "property_manager.hpp"
#include "motion_controller.hpp"
#include "shutter_controller.hpp"

#include <spdlog/spdlog.h>

namespace lithium::device::indi {

ModularINDIDome::ModularINDIDome(std::string name) : AtomDome(std::move(name)) {
    // Set dome capabilities
    setDomeCapabilities({
        .canPark = true,
        .canSync = true,
        .canAbort = true,
        .hasShutter = true,
        .hasVariable = false,
        .canSetAzimuth = true,
        .canSetParkPosition = true,
        .hasBacklash = true,
        .minAzimuth = 0.0,
        .maxAzimuth = 360.0
    });

    // Set default dome parameters
    setDomeParameters({
        .diameter = 3.0,
        .height = 2.5,
        .slitWidth = 0.5,
        .slitHeight = 0.8,
        .telescopeRadius = 0.5
    });

    logInfo("ModularINDIDome constructed");
}

ModularINDIDome::~ModularINDIDome() {
    if (isConnected()) {
        destroy();
    }
}

auto ModularINDIDome::initialize() -> bool {
    logInfo("Initializing modular dome");

    try {
        // Create and initialize components
        if (!initializeComponents()) {
            logError("Failed to initialize components");
            return false;
        }

        // Register components with core
        if (!registerComponents()) {
            logError("Failed to register components");
            cleanupComponents();
            return false;
        }

        // Setup callbacks
        if (!setupCallbacks()) {
            logError("Failed to setup callbacks");
            cleanupComponents();
            return false;
        }

        logInfo("Modular dome initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Exception during initialization: " + std::string(ex.what()));
        cleanupComponents();
        return false;
    }
}

auto ModularINDIDome::destroy() -> bool {
    logInfo("Destroying modular dome");

    try {
        if (isConnected()) {
            disconnect();
        }

        cleanupComponents();
        logInfo("Modular dome destroyed successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Exception during destruction: " + std::string(ex.what()));
        return false;
    }
}

auto ModularINDIDome::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    logInfo("Connecting to device: " + deviceName);

    if (!validateComponents()) {
        logError("Components not properly initialized");
        return false;
    }

    return core_->connect(deviceName, timeout, maxRetry);
}

auto ModularINDIDome::disconnect() -> bool {
    logInfo("Disconnecting from device");

    if (!core_) {
        return true;
    }

    return core_->disconnect();
}

auto ModularINDIDome::reconnect(int timeout, int maxRetry) -> bool {
    logInfo("Reconnecting to device");

    if (!core_) {
        logError("Core not initialized");
        return false;
    }

    return core_->reconnect(timeout, maxRetry);
}

auto ModularINDIDome::scan() -> std::vector<std::string> {
    if (!core_) {
        logError("Core not initialized");
        return {};
    }

    return core_->scanForDevices();
}

auto ModularINDIDome::isConnected() const -> bool {
    return core_ && core_->isConnected();
}

// State queries
auto ModularINDIDome::isMoving() const -> bool {
    return motion_controller_ && motion_controller_->isMoving();
}

auto ModularINDIDome::isParked() const -> bool {
    return core_ && core_->isParked();
}

// Azimuth control
auto ModularINDIDome::getAzimuth() -> std::optional<double> {
    if (!motion_controller_) {
        return std::nullopt;
    }
    return motion_controller_->getCurrentAzimuth();
}

auto ModularINDIDome::setAzimuth(double azimuth) -> bool {
    return moveToAzimuth(azimuth);
}

auto ModularINDIDome::moveToAzimuth(double azimuth) -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->moveToAzimuth(azimuth);
}

auto ModularINDIDome::rotateClockwise() -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->rotateClockwise();
}

auto ModularINDIDome::rotateCounterClockwise() -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->rotateCounterClockwise();
}

auto ModularINDIDome::stopRotation() -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->stopRotation();
}

auto ModularINDIDome::abortMotion() -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->abortMotion();
}

auto ModularINDIDome::syncAzimuth(double azimuth) -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->syncAzimuth(azimuth);
}

// Shutter control
auto ModularINDIDome::openShutter() -> bool {
    if (!shutter_controller_) {
        logError("Shutter controller not available");
        return false;
    }
    return shutter_controller_->openShutter();
}

auto ModularINDIDome::closeShutter() -> bool {
    if (!shutter_controller_) {
        logError("Shutter controller not available");
        return false;
    }
    return shutter_controller_->closeShutter();
}

auto ModularINDIDome::abortShutter() -> bool {
    if (!shutter_controller_) {
        logError("Shutter controller not available");
        return false;
    }
    return shutter_controller_->abortShutter();
}

auto ModularINDIDome::getShutterState() -> ShutterState {
    if (!shutter_controller_) {
        return ShutterState::UNKNOWN;
    }
    return shutter_controller_->getShutterState();
}

auto ModularINDIDome::hasShutter() -> bool {
    return shutter_controller_ && shutter_controller_->hasShutter();
}

// Speed control
auto ModularINDIDome::getRotationSpeed() -> std::optional<double> {
    if (!motion_controller_) {
        return std::nullopt;
    }
    return motion_controller_->getRotationSpeed();
}

auto ModularINDIDome::setRotationSpeed(double speed) -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->setRotationSpeed(speed);
}

auto ModularINDIDome::getMaxSpeed() -> double {
    if (!motion_controller_) {
        return 0.0;
    }
    return motion_controller_->getMaxSpeed();
}

auto ModularINDIDome::getMinSpeed() -> double {
    if (!motion_controller_) {
        return 0.0;
    }
    return motion_controller_->getMinSpeed();
}

// Backlash compensation
auto ModularINDIDome::getBacklash() -> double {
    if (!motion_controller_) {
        return 0.0;
    }
    return motion_controller_->getBacklash();
}

auto ModularINDIDome::setBacklash(double backlash) -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->setBacklash(backlash);
}

auto ModularINDIDome::enableBacklashCompensation(bool enable) -> bool {
    if (!motion_controller_) {
        logError("Motion controller not available");
        return false;
    }
    return motion_controller_->enableBacklashCompensation(enable);
}

auto ModularINDIDome::isBacklashCompensationEnabled() -> bool {
    if (!motion_controller_) {
        return false;
    }
    return motion_controller_->isBacklashCompensationEnabled();
}

// Statistics
auto ModularINDIDome::getTotalRotation() -> double {
    if (!motion_controller_) {
        return 0.0;
    }
    return motion_controller_->getTotalRotation();
}

auto ModularINDIDome::resetTotalRotation() -> bool {
    if (!motion_controller_) {
        return false;
    }
    return motion_controller_->resetTotalRotation();
}

auto ModularINDIDome::getShutterOperations() -> uint64_t {
    if (!shutter_controller_) {
        return 0;
    }
    return shutter_controller_->getShutterOperations();
}

auto ModularINDIDome::resetShutterOperations() -> bool {
    if (!shutter_controller_) {
        return false;
    }
    return shutter_controller_->resetShutterOperations();
}

// Stub implementations for remaining methods
auto ModularINDIDome::park() -> bool {
    logWarning("Park functionality not yet implemented");
    return false;
}

auto ModularINDIDome::unpark() -> bool {
    logWarning("Unpark functionality not yet implemented");
    return false;
}

auto ModularINDIDome::getParkPosition() -> std::optional<double> {
    logWarning("Get park position not yet implemented");
    return std::nullopt;
}

auto ModularINDIDome::setParkPosition(double azimuth) -> bool {
    logWarning("Set park position not yet implemented");
    return false;
}

auto ModularINDIDome::canPark() -> bool {
    return false; // Will be implemented with parking controller
}

auto ModularINDIDome::followTelescope(bool enable) -> bool {
    logWarning("Telescope following not yet implemented");
    return false;
}

auto ModularINDIDome::isFollowingTelescope() -> bool {
    return false;
}

auto ModularINDIDome::calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double {
    return telescopeAz; // Simplified calculation
}

auto ModularINDIDome::setTelescopePosition(double az, double alt) -> bool {
    logWarning("Set telescope position not yet implemented");
    return false;
}

auto ModularINDIDome::findHome() -> bool {
    logWarning("Find home not yet implemented");
    return false;
}

auto ModularINDIDome::setHome() -> bool {
    logWarning("Set home not yet implemented");
    return false;
}

auto ModularINDIDome::gotoHome() -> bool {
    logWarning("Goto home not yet implemented");
    return false;
}

auto ModularINDIDome::getHomePosition() -> std::optional<double> {
    return std::nullopt;
}

auto ModularINDIDome::canOpenShutter() -> bool {
    return shutter_controller_ && shutter_controller_->canOpenShutter();
}

auto ModularINDIDome::isSafeToOperate() -> bool {
    return core_ && core_->isSafeToOperate();
}

auto ModularINDIDome::getWeatherStatus() -> std::string {
    return "Unknown"; // Will be implemented with weather manager
}

auto ModularINDIDome::savePreset(int slot, double azimuth) -> bool {
    logWarning("Save preset not yet implemented");
    return false;
}

auto ModularINDIDome::loadPreset(int slot) -> bool {
    logWarning("Load preset not yet implemented");
    return false;
}

auto ModularINDIDome::getPreset(int slot) -> std::optional<double> {
    return std::nullopt;
}

auto ModularINDIDome::deletePreset(int slot) -> bool {
    logWarning("Delete preset not yet implemented");
    return false;
}

// Private initialization methods
auto ModularINDIDome::initializeComponents() -> bool {
    try {
        // Create core first
        core_ = std::make_shared<INDIDomeCore>(getName());
        if (!core_->initialize()) {
            logError("Failed to initialize core");
            return false;
        }

        // Create property manager
        property_manager_ = std::make_shared<PropertyManager>(core_);
        if (!property_manager_->initialize()) {
            logError("Failed to initialize property manager");
            return false;
        }

        // Create motion controller
        motion_controller_ = std::make_shared<MotionController>(core_);
        motion_controller_->setPropertyManager(property_manager_);
        if (!motion_controller_->initialize()) {
            logError("Failed to initialize motion controller");
            return false;
        }

        // Create shutter controller
        shutter_controller_ = std::make_shared<ShutterController>(core_);
        shutter_controller_->setPropertyManager(property_manager_);
        if (!shutter_controller_->initialize()) {
            logError("Failed to initialize shutter controller");
            return false;
        }

        logInfo("All components initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        logError("Exception during component initialization: " + std::string(ex.what()));
        return false;
    }
}

auto ModularINDIDome::registerComponents() -> bool {
    try {
        if (!core_) {
            logError("Core not available for registration");
            return false;
        }

        core_->registerPropertyManager(property_manager_);
        core_->registerMotionController(motion_controller_);
        core_->registerShutterController(shutter_controller_);

        logInfo("Components registered with core");
        return true;
    } catch (const std::exception& ex) {
        logError("Exception during component registration: " + std::string(ex.what()));
        return false;
    }
}

auto ModularINDIDome::setupCallbacks() -> bool {
    try {
        // Setup event callbacks from core to update AtomDome state
        if (core_) {
            core_->setAzimuthCallback([this](double azimuth) {
                this->current_azimuth_ = azimuth;
                this->notifyAzimuthChange(azimuth);
            });

            core_->setShutterCallback([this](ShutterState state) {
                this->updateShutterState(state);
                this->notifyShutterChange(state);
            });

            core_->setParkCallback([this](bool parked) {
                this->is_parked_ = parked;
                this->notifyParkChange(parked);
            });

            core_->setMoveCompleteCallback([this](bool success, const std::string& message) {
                this->notifyMoveComplete(success, message);
            });
        }

        logInfo("Callbacks setup completed");
        return true;
    } catch (const std::exception& ex) {
        logError("Exception during callback setup: " + std::string(ex.what()));
        return false;
    }
}

auto ModularINDIDome::cleanupComponents() -> bool {
    try {
        if (shutter_controller_) {
            shutter_controller_->cleanup();
            shutter_controller_.reset();
        }

        if (motion_controller_) {
            motion_controller_->cleanup();
            motion_controller_.reset();
        }

        if (property_manager_) {
            property_manager_->cleanup();
            property_manager_.reset();
        }

        if (core_) {
            core_->destroy();
            core_.reset();
        }

        logInfo("Components cleaned up");
        return true;
    } catch (const std::exception& ex) {
        logError("Exception during component cleanup: " + std::string(ex.what()));
        return false;
    }
}

auto ModularINDIDome::validateComponents() const -> bool {
    return core_ && property_manager_ && motion_controller_ && shutter_controller_;
}

auto ModularINDIDome::areComponentsInitialized() const -> bool {
    return validateComponents() &&
           core_->isInitialized() &&
           property_manager_->isInitialized() &&
           motion_controller_->isInitialized() &&
           shutter_controller_->isInitialized();
}

void ModularINDIDome::handleComponentError(const std::string& component, const std::string& error) {
    logError("Component error in " + component + ": " + error);
}

void ModularINDIDome::logInfo(const std::string& message) const {
    spdlog::info("[ModularINDIDome] {}", message);
}

void ModularINDIDome::logWarning(const std::string& message) const {
    spdlog::warn("[ModularINDIDome] {}", message);
}

void ModularINDIDome::logError(const std::string& message) const {
    spdlog::error("[ModularINDIDome] {}", message);
}

auto ModularINDIDome::runDiagnostics() -> bool {
    if (!core_) {
        logError("Cannot run diagnostics: core not initialized");
        return false;
    }

    try {
        bool all_passed = true;

        // Test core functionality
        if (!core_->isConnected()) {
            logWarning("Diagnostics: Device not connected");
            all_passed = false;
        }

        // Test motion controller
        if (motion_controller_) {
            // Add specific motion controller diagnostics
            logInfo("Diagnostics: Motion controller available");
        } else {
            logError("Diagnostics: Motion controller not available");
            all_passed = false;
        }

        // Test shutter controller
        if (shutter_controller_) {
            // Add specific shutter controller diagnostics
            logInfo("Diagnostics: Shutter controller available");
        } else {
            logError("Diagnostics: Shutter controller not available");
            all_passed = false;
        }

        // Test property manager
        if (property_manager_) {
            logInfo("Diagnostics: Property manager available");
        } else {
            logError("Diagnostics: Property manager not available");
            all_passed = false;
        }

        logInfo("Diagnostics completed, result: " + (all_passed ? std::string("PASSED") : std::string("FAILED")));
        return all_passed;

    } catch (const std::exception& ex) {
        logError("Diagnostics failed with exception: " + std::string(ex.what()));
        return false;
    }
}

} // namespace lithium::device::indi
