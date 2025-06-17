#include "modular_focuser.hpp"

namespace lithium::device::indi::focuser {

ModularINDIFocuser::ModularINDIFocuser(std::string name)
    : AtomFocuser(std::move(name)), state_(std::make_unique<FocuserState>()) {
    // Initialize logger
    state_->logger_ = spdlog::get("focuser");
    if (!state_->logger_) {
        state_->logger_ = spdlog::default_logger();
    }

    state_->logger_->info("Creating modular INDI focuser: {}", name_);

    // Create component managers
    propertyManager_ = std::make_unique<PropertyManager>();
    movementController_ = std::make_unique<MovementController>();
    temperatureManager_ = std::make_unique<TemperatureManager>();
    presetManager_ = std::make_unique<PresetManager>();
    statisticsManager_ = std::make_unique<StatisticsManager>();
}

bool ModularINDIFocuser::initialize() {
    state_->logger_->info("Initializing modular INDI focuser");
    return initializeComponents();
}

bool ModularINDIFocuser::destroy() {
    state_->logger_->info("Destroying modular INDI focuser");
    cleanupComponents();
    return true;
}

bool ModularINDIFocuser::connect(const std::string& deviceName, int timeout,
                                 int maxRetry) {
    if (state_->isConnected_.load()) {
        state_->logger_->error("{} is already connected.", state_->deviceName_);
        return false;
    }

    state_->deviceName_ = deviceName;
    state_->logger_->info("Connecting to {}...", deviceName);

    setupInitialConnection(deviceName);
    return true;
}

bool ModularINDIFocuser::disconnect() {
    if (!state_->isConnected_.load()) {
        state_->logger_->warn("Device {} is not connected",
                              state_->deviceName_);
        return false;
    }

    disconnectServer();
    state_->isConnected_ = false;
    state_->logger_->info("Disconnected from {}", state_->deviceName_);
    return true;
}

std::vector<std::string> ModularINDIFocuser::scan() {
    // INDI doesn't provide a direct scan method
    // This would typically be handled by the INDI server
    state_->logger_->warn("Scan method not directly supported by INDI");
    return {};
}

bool ModularINDIFocuser::isConnected() const {
    return state_->isConnected_.load();
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
        int currentPos = state_->currentPosition_.load();
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
int ModularINDIFocuser::getBacklash() { return state_->backlashSteps_.load(); }

bool ModularINDIFocuser::setBacklash(int backlash) {
    INDI::PropertyNumber property =
        state_->device_.getProperty("FOCUS_BACKLASH_STEPS");
    if (!property.isValid()) {
        state_->logger_->warn(
            "Unable to find FOCUS_BACKLASH_STEPS property, setting internal "
            "value");
        state_->backlashSteps_ = backlash;
        return true;
    }
    property[0].value = backlash;
    sendNewProperty(property);
    return true;
}

bool ModularINDIFocuser::enableBacklashCompensation(bool enable) {
    INDI::PropertySwitch property =
        state_->device_.getProperty("FOCUS_BACKLASH_TOGGLE");
    if (!property.isValid()) {
        state_->logger_->warn(
            "Unable to find FOCUS_BACKLASH_TOGGLE property, setting internal "
            "value");
        state_->backlashEnabled_ = enable;
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
    return state_->backlashEnabled_.load();
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
    state_->logger_->warn("Auto-focus not directly supported by INDI drivers");
    state_->isAutoFocusing_ = true;
    state_->autoFocusProgress_ = 0.0;
    return false;
}

bool ModularINDIFocuser::stopAutoFocus() {
    state_->isAutoFocusing_ = false;
    state_->autoFocusProgress_ = 0.0;
    return true;
}

bool ModularINDIFocuser::isAutoFocusing() {
    return state_->isAutoFocusing_.load();
}

double ModularINDIFocuser::getAutoFocusProgress() {
    return state_->autoFocusProgress_.load();
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
    state_->logger_->info("Message from {}: {}", baseDevice.getDeviceName(),
                          message);
}

bool ModularINDIFocuser::initializeComponents() {
    bool success = true;

    success &= propertyManager_->initialize(*state_);
    success &= movementController_->initialize(*state_);
    success &= temperatureManager_->initialize(*state_);
    success &= presetManager_->initialize(*state_);
    success &= statisticsManager_->initialize(*state_);

    if (success) {
        state_->logger_->info("All components initialized successfully");
    } else {
        state_->logger_->error("Failed to initialize some components");
    }

    return success;
}

void ModularINDIFocuser::cleanupComponents() {
    if (statisticsManager_)
        statisticsManager_->cleanup();
    if (presetManager_)
        presetManager_->cleanup();
    if (temperatureManager_)
        temperatureManager_->cleanup();
    if (movementController_)
        movementController_->cleanup();
    if (propertyManager_)
        propertyManager_->cleanup();
}

void ModularINDIFocuser::setupDeviceWatchers() {
    watchDevice(state_->deviceName_.c_str(), [this](INDI::BaseDevice device) {
        state_->device_ = device;
        state_->logger_->info("Device {} discovered", state_->deviceName_);

        // Setup property watchers
        propertyManager_->setupPropertyWatchers(device, *state_);

        // Setup connection property watcher
        device.watchProperty(
            "CONNECTION",
            [this](INDI::Property) {
                state_->logger_->info("Connecting to {}...",
                                      state_->deviceName_);
                connectDevice(name_.c_str());
            },
            INDI::BaseDevice::WATCH_NEW);
    });
}

void ModularINDIFocuser::setupInitialConnection(const std::string& deviceName) {
    setupDeviceWatchers();

    // Start statistics session
    statisticsManager_->startSession();

    state_->logger_->info("Setup complete for device: {}", deviceName);
}

}  // namespace lithium::device::indi::focuser
