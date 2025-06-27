#include "modular_focuser.hpp"

namespace lithium::device::indi::focuser {

ModularINDIFocuser::ModularINDIFocuser(std::string name)
    : AtomFocuser(std::move(name)), core_(std::make_shared<INDIFocuserCore>(name_)) {
    
    core_->getLogger()->info("Creating modular INDI focuser: {}", name_);

    // Create component managers with shared core
    propertyManager_ = std::make_unique<PropertyManager>(core_);
    movementController_ = std::make_unique<MovementController>(core_);
    temperatureManager_ = std::make_unique<TemperatureManager>(core_);
    presetManager_ = std::make_unique<PresetManager>(core_);
    statisticsManager_ = std::make_unique<StatisticsManager>(core_);
}

bool ModularINDIFocuser::initialize() {
    core_->getLogger()->info("Initializing modular INDI focuser");
    return initializeComponents();
}

bool ModularINDIFocuser::destroy() {
    core_->getLogger()->info("Destroying modular INDI focuser");
    cleanupComponents();
    return true;
}

bool ModularINDIFocuser::connect(const std::string& deviceName, int timeout,
                                 int maxRetry) {
    if (core_->isConnected()) {
        core_->getLogger()->error("{} is already connected.", core_->getDeviceName());
        return false;
    }

    core_->setDeviceName(deviceName);
    core_->getLogger()->info("Connecting to {}...", deviceName);

    setupInitialConnection(deviceName);
    return true;
}

bool ModularINDIFocuser::disconnect() {
    if (!core_->isConnected()) {
        core_->getLogger()->warn("Device {} is not connected",
                              core_->getDeviceName());
        return false;
    }

    disconnectServer();
    core_->setConnected(false);
    core_->getLogger()->info("Disconnected from {}", core_->getDeviceName());
    return true;
}

std::vector<std::string> ModularINDIFocuser::scan() {
    // INDI doesn't provide a direct scan method
    // This would typically be handled by the INDI server
    core_->getLogger()->warn("Scan method not directly supported by INDI");
    return {};
}

bool ModularINDIFocuser::isConnected() const {
    return core_->isConnected();
}

// Movement control methods (delegated to MovementController)
bool ModularINDIFocuser::isMoving() const {
    return movementController_->isMoving();
}

std::optional<double> ModularINDIFocuser::getSpeed() {
    return movementController_->getSpeed();
}

bool ModularINDIFocuser::setSpeed(double speed) {
    return movementController_->setSpeed(speed);
}

int ModularINDIFocuser::getMaxSpeed() {
    return movementController_->getMaxSpeed();
}

std::pair<int, int> ModularINDIFocuser::getSpeedRange() {
    return movementController_->getSpeedRange();
}

std::optional<FocusDirection> ModularINDIFocuser::getDirection() {
    return movementController_->getDirection();
}

bool ModularINDIFocuser::setDirection(FocusDirection direction) {
    return movementController_->setDirection(direction);
}

std::optional<int> ModularINDIFocuser::getMaxLimit() {
    return movementController_->getMaxLimit();
}

bool ModularINDIFocuser::setMaxLimit(int maxLimit) {
    return movementController_->setMaxLimit(maxLimit);
}

std::optional<int> ModularINDIFocuser::getMinLimit() {
    return movementController_->getMinLimit();
}

bool ModularINDIFocuser::setMinLimit(int minLimit) {
    return movementController_->setMinLimit(minLimit);
}

std::optional<bool> ModularINDIFocuser::isReversed() {
    return movementController_->isReversed();
}

bool ModularINDIFocuser::setReversed(bool reversed) {
    return movementController_->setReversed(reversed);
}

bool ModularINDIFocuser::moveSteps(int steps) {
    bool result = movementController_->moveSteps(steps);
    if (result) {
        statisticsManager_->recordMovement(steps);
    }
    return result;
}

bool ModularINDIFocuser::moveToPosition(int position) {
    bool result = movementController_->moveToPosition(position);
    if (result) {
        int currentPos = core_->getCurrentPosition();
        int steps = position - currentPos;
        statisticsManager_->recordMovement(steps);
    }
    return result;
}

std::optional<int> ModularINDIFocuser::getPosition() {
    return movementController_->getPosition();
}

bool ModularINDIFocuser::moveForDuration(int durationMs) {
    return movementController_->moveForDuration(durationMs);
}

bool ModularINDIFocuser::abortMove() {
    return movementController_->abortMove();
}

bool ModularINDIFocuser::syncPosition(int position) {
    return movementController_->syncPosition(position);
}

bool ModularINDIFocuser::moveInward(int steps) {
    bool result = movementController_->moveInward(steps);
    if (result) {
        statisticsManager_->recordMovement(steps);
    }
    return result;
}

bool ModularINDIFocuser::moveOutward(int steps) {
    bool result = movementController_->moveOutward(steps);
    if (result) {
        statisticsManager_->recordMovement(steps);
    }
    return result;
}

// Backlash compensation
int ModularINDIFocuser::getBacklash() { return core_->getBacklashSteps(); }

bool ModularINDIFocuser::setBacklash(int backlash) {
    INDI::PropertyNumber property =
        core_->getDevice().getProperty("FOCUS_BACKLASH_STEPS");
    if (!property.isValid()) {
        core_->getLogger()->warn(
            "Unable to find FOCUS_BACKLASH_STEPS property, setting internal "
            "value");
        core_->setBacklashSteps(backlash);
        return true;
    }
    property[0].value = backlash;
    sendNewProperty(property);
    return true;
}

bool ModularINDIFocuser::enableBacklashCompensation(bool enable) {
    INDI::PropertySwitch property =
        core_->getDevice().getProperty("FOCUS_BACKLASH_TOGGLE");
    if (!property.isValid()) {
        core_->getLogger()->warn(
            "Unable to find FOCUS_BACKLASH_TOGGLE property, setting internal "
            "value");
        core_->setBacklashEnabled(enable);
        return true;
    }
    if (enable) {
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
    } else {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
    }
    sendNewProperty(property);
    return true;
}

bool ModularINDIFocuser::isBacklashCompensationEnabled() {
    return core_->isBacklashEnabled();
}

// Temperature management (delegated to TemperatureManager)
std::optional<double> ModularINDIFocuser::getExternalTemperature() {
    return temperatureManager_->getExternalTemperature();
}

std::optional<double> ModularINDIFocuser::getChipTemperature() {
    return temperatureManager_->getChipTemperature();
}

bool ModularINDIFocuser::hasTemperatureSensor() {
    return temperatureManager_->hasTemperatureSensor();
}

TemperatureCompensation ModularINDIFocuser::getTemperatureCompensation() {
    return temperatureManager_->getTemperatureCompensation();
}

bool ModularINDIFocuser::setTemperatureCompensation(
    const TemperatureCompensation& comp) {
    return temperatureManager_->setTemperatureCompensation(comp);
}

bool ModularINDIFocuser::enableTemperatureCompensation(bool enable) {
    return temperatureManager_->enableTemperatureCompensation(enable);
}

// Auto-focus (basic implementation)
bool ModularINDIFocuser::startAutoFocus() {
    // INDI doesn't typically have built-in autofocus
    // This would be handled by client software like Ekos
    core_->getLogger()->warn("Auto-focus not directly supported by INDI drivers");
    isAutoFocusing_.store(true);
    autoFocusProgress_.store(0.0);
    return false;
}

bool ModularINDIFocuser::stopAutoFocus() {
    isAutoFocusing_.store(false);
    autoFocusProgress_.store(0.0);
    return true;
}

bool ModularINDIFocuser::isAutoFocusing() {
    return isAutoFocusing_.load();
}

double ModularINDIFocuser::getAutoFocusProgress() {
    return autoFocusProgress_.load();
}

// Preset management (delegated to PresetManager)
bool ModularINDIFocuser::savePreset(int slot, int position) {
    return presetManager_->savePreset(slot, position);
}

bool ModularINDIFocuser::loadPreset(int slot) {
    auto position = presetManager_->getPreset(slot);
    if (!position.has_value()) {
        return false;
    }
    return moveToPosition(position.value());
}

std::optional<int> ModularINDIFocuser::getPreset(int slot) {
    return presetManager_->getPreset(slot);
}

bool ModularINDIFocuser::deletePreset(int slot) {
    return presetManager_->deletePreset(slot);
}

// Statistics (delegated to StatisticsManager)
uint64_t ModularINDIFocuser::getTotalSteps() {
    return statisticsManager_->getTotalSteps();
}

bool ModularINDIFocuser::resetTotalSteps() {
    return statisticsManager_->resetTotalSteps();
}

int ModularINDIFocuser::getLastMoveSteps() {
    return statisticsManager_->getLastMoveSteps();
}

int ModularINDIFocuser::getLastMoveDuration() {
    return statisticsManager_->getLastMoveDuration();
}

void ModularINDIFocuser::newMessage(INDI::BaseDevice baseDevice,
                                    int messageID) {
    auto message = baseDevice.messageQueue(messageID);
    core_->getLogger()->info("Message from {}: {}", baseDevice.getDeviceName(),
                          message);
}

bool ModularINDIFocuser::initializeComponents() {
    bool success = true;

    success &= propertyManager_->initialize();
    success &= movementController_->initialize();
    success &= temperatureManager_->initialize();
    success &= presetManager_->initialize();
    success &= statisticsManager_->initialize();

    if (success) {
        core_->getLogger()->info("All components initialized successfully");
    } else {
        core_->getLogger()->error("Failed to initialize some components");
    }

    return success;
}

void ModularINDIFocuser::cleanupComponents() {
    if (statisticsManager_)
        statisticsManager_->shutdown();
    if (presetManager_)
        presetManager_->shutdown();
    if (temperatureManager_)
        temperatureManager_->shutdown();
    if (movementController_)
        movementController_->shutdown();
    if (propertyManager_)
        propertyManager_->shutdown();
}

void ModularINDIFocuser::setupDeviceWatchers() {
    watchDevice(core_->getDeviceName().c_str(), [this](INDI::BaseDevice device) {
        core_->setDevice(device);
        core_->getLogger()->info("Device {} discovered", core_->getDeviceName());

        // Setup property watchers
        propertyManager_->setupPropertyWatchers();

        // Setup connection property watcher
        device.watchProperty(
            "CONNECTION",
            [this](INDI::Property) {
                core_->getLogger()->info("Connecting to {}...",
                                      core_->getDeviceName());
                connectDevice(name_.c_str());
            },
            INDI::BaseDevice::WATCH_NEW);
    });
}

void ModularINDIFocuser::setupInitialConnection(const std::string& deviceName) {
    setupDeviceWatchers();

    // Start statistics session
    statisticsManager_->startSession();

    core_->getLogger()->info("Setup complete for device: {}", deviceName);
}

}  // namespace lithium::device::indi::focuser
