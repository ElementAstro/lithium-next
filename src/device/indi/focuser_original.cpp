#include "focuser.hpp"
#include "focuser_main.hpp"

#include <optional>

#include <spdlog/spdlog.h>

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "device/template/focuser.hpp"

// Legacy wrapper for the original INDIFocuser interface
// This maintains backward compatibility while using the new modular implementation
class LegacyINDIFocuser : public INDI::BaseClient, public AtomFocuser {
public:
    explicit LegacyINDIFocuser(std::string name) 
        : AtomFocuser(name), modularFocuser_(std::move(name)) {}
    
    ~LegacyINDIFocuser() override = default;

    // Non-copyable, non-movable
    LegacyINDIFocuser(const LegacyINDIFocuser& other) = delete;
    LegacyINDIFocuser& operator=(const LegacyINDIFocuser& other) = delete;
    LegacyINDIFocuser(LegacyINDIFocuser&& other) = delete;
    LegacyINDIFocuser& operator=(LegacyINDIFocuser&& other) = delete;

auto INDIFocuser::initialize() -> bool { return true; }

auto INDIFocuser::destroy() -> bool { return true; }

auto INDIFocuser::connect(const std::string &deviceName, int timeout,
                          int maxRetry) -> bool {
    if (isConnected_.load()) {
        logger_->error("{} is already connected.", deviceName_);
        return false;
    }

    deviceName_ = deviceName;
    logger_->info("Connecting to {}...", deviceName_);
    // Max: 需要获取初始的参数，然后再注册对应的回调函数
    watchDevice(deviceName_.c_str(), [this](INDI::BaseDevice device) {
        device_ = device;  // save device

        // wait for the availability of the "CONNECTION" property
        device.watchProperty(
            "CONNECTION",
            [this](INDI::Property) {
                logger_->info("Connecting to {}...", deviceName_);
                connectDevice(name_.c_str());
            },
            INDI::BaseDevice::WATCH_NEW);

        device.watchProperty(
            "CONNECTION",
            [this](const INDI::PropertySwitch &property) {
                isConnected_ = property[0].getState() == ISS_ON;
                if (isConnected_.load()) {
                    logger_->info("{} is connected.", deviceName_);
                } else {
                    logger_->info("{} is disconnected.", deviceName_);
                }
            },
            INDI::BaseDevice::WATCH_UPDATE);

        device.watchProperty(
            "DRIVER_INFO",
            [this](const INDI::PropertyText &property) {
                if (property.isValid()) {
                    const auto *driverName = property[0].getText();
                    logger_->info("Driver name: {}", driverName);

                    const auto *driverExec = property[1].getText();
                    logger_->info("Driver executable: {}", driverExec);
                    driverExec_ = driverExec;
                    const auto *driverVersion = property[2].getText();
                    logger_->info("Driver version: {}", driverVersion);
                    driverVersion_ = driverVersion;
                    const auto *driverInterface = property[3].getText();
                    logger_->info("Driver interface: {}", driverInterface);
                    driverInterface_ = driverInterface;
                }
            },
            INDI::BaseDevice::WATCH_NEW);

        device.watchProperty(
            "DEBUG",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    isDebug_.store(property[0].getState() == ISS_ON);
                    logger_->info("Debug is {}", isDebug_.load() ? "ON" : "OFF");
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        // Max: 这个参数其实挺重要的，但是除了行星相机都不需要调整，默认就好
        device.watchProperty(
            "POLLING_PERIOD",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto period = property[0].getValue();
                    logger_->info("Current polling period: {}", period);
                    if (period != currentPollingPeriod_.load()) {
                        logger_->info("Polling period change to: {}", period);
                        currentPollingPeriod_ = period;
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "DEVICE_AUTO_SEARCH",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    deviceAutoSearch_ = property[0].getState() == ISS_ON;
                    logger_->info("Auto search is {}",
                          deviceAutoSearch_ ? "ON" : "OFF");
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "DEVICE_PORT_SCAN",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    devicePortScan_ = property[0].getState() == ISS_ON;
                    logger_->info("Device port scan is {}",
                          devicePortScan_ ? "On" : "Off");
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "BAUD_RATE",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    for (int i = 0; i < property.size(); i++) {
                        if (property[i].getState() == ISS_ON) {
                            logger_->info("Baud rate is {}",
                                  property[i].getLabel());
                            baudRate_ = static_cast<BAUD_RATE>(i);
                        }
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "Mode",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    for (int i = 0; i < property.size(); i++) {
                        if (property[i].getState() == ISS_ON) {
                            logger_->info("Focuser mode is {}",
                                  property[i].getLabel());
                            focusMode_ = static_cast<FocusMode>(i);
                        }
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_MOTION",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    for (int i = 0; i < property.size(); i++) {
                        if (property[i].getState() == ISS_ON) {
                            logger_->info("Focuser motion is {}",
                                  property[i].getLabel());
                            focusDirection_ = static_cast<FocusDirection>(i);
                        }
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_SPEED",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto speed = property[0].getValue();
                    logger_->info("Current focuser speed: {}", speed);
                    currentFocusSpeed_ = speed;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "REL_FOCUS_POSITION",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto position = property[0].getValue();
                    logger_->info("Current relative focuser position: {}",
                          position);
                    realRelativePosition_ = position;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "ABS_FOCUS_POSITION",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto position = property[0].getValue();
                    logger_->info("Current absolute focuser position: {}",
                          position);
                    realAbsolutePosition_ = position;
                    current_position_ = position;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_MAX",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto maxlimit = property[0].getValue();
                    logger_->info("Current focuser max limit: {}", maxlimit);
                    maxPosition_ = maxlimit;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_BACKLASH_TOGGLE",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    if (property[0].getState() == ISS_ON) {
                        logger_->info("Backlash is enabled");
                        backlashEnabled_ = true;
                    } else {
                        logger_->info("Backlash is disabled");
                        backlashEnabled_ = false;
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_BACKLASH_STEPS",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto backlash = property[0].getValue();
                    logger_->info("Current focuser backlash: {}", backlash);
                    backlashSteps_ = backlash;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_TEMPERATURE",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto temperature = property[0].getValue();
                    logger_->info("Current focuser temperature: {}", temperature);
                    temperature_ = temperature;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "CHIP_TEMPERATURE",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto temperature = property[0].getValue();
                    logger_->info("Current chip temperature: {}", temperature);
                    chipTemperature_ = temperature;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "DELAY",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto delay = property[0].getValue();
                    logger_->info("Current focuser delay: {}", delay);
                    delay_msec_ = delay;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device.watchProperty(
            "FOCUS_REVERSE_MOTION",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    if (property[0].getState() == ISS_ON) {
                        logger_->info("Focuser is reversed");
                        isReverse_ = true;
                    } else {
                        logger_->info("Focuser is not reversed");
                        isReverse_ = false;
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_TIMER",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto timer = property[0].getValue();
                    logger_->info("Current focuser timer: {}", timer);
                    focusTimer_ = timer;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_ABORT_MOTION",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    if (property[0].getState() == ISS_ON) {
                        logger_->info("Focuser is aborting");
                        isFocuserMoving_ = false;
                        updateFocuserState(FocuserState::IDLE);
                    } else {
                        logger_->info("Focuser is not aborting");
                        isFocuserMoving_ = true;
                        updateFocuserState(FocuserState::MOVING);
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);
    });

    return true;
}
auto INDIFocuser::disconnect() -> bool { 
    if (!isConnected_.load()) {
        logger_->warn("Device {} is not connected", deviceName_);
        return false;
    }
    
    disconnectServer();
    isConnected_ = false;
    logger_->info("Disconnected from {}", deviceName_);
    return true;
}

auto INDIFocuser::reconnect(int timeout, int maxRetry) -> bool {
    if (disconnect()) {
        return connect(deviceName_, timeout, maxRetry);
    }
    return false;
}

auto INDIFocuser::scan() -> std::vector<std::string> {
    // INDI doesn't provide a direct scan method
    // This would typically be handled by the INDI server
    logger_->warn("Scan method not directly supported by INDI");
    return {};
}

auto INDIFocuser::isConnected() const -> bool {
    return isConnected_.load();
}

auto INDIFocuser::watchAdditionalProperty() -> bool { return true; }

void INDIFocuser::setPropertyNumber(std::string_view propertyName,
                                    double value) {}

auto INDIFocuser::getSpeed() -> std::optional<double> {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_SPEED property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

auto INDIFocuser::setSpeed(double speed) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_SPEED property...");
        return false;
    }
    property[0].value = speed;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::getDirection() -> std::optional<FocusDirection> {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_MOTION");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_MOTION property...");
        return std::nullopt;
    }
    if (property[0].getState() == ISS_ON) {
        return FocusDirection::IN;
    }
    return FocusDirection::OUT;
}

auto INDIFocuser::setDirection(FocusDirection direction) -> bool {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_MOTION");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_MOTION property...");
        return false;
    }
    if (FocusDirection::IN == direction) {
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
    } else {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
    }
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::getMaxLimit() -> std::optional<int> {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_MAX");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_MAX property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

auto INDIFocuser::setMaxLimit(int maxlimit) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_MAX");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_MAX property...");
        return false;
    }
    property[0].value = maxlimit;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::isReversed() -> std::optional<bool> {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_REVERSE_MOTION");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_REVERSE_MOTION property...");
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

auto INDIFocuser::setReversed(bool reversed) -> bool {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_REVERSE_MOTION");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_REVERSE_MOTION property...");
        return false;
    }
    if (reversed) {
        property[0].setState(ISS_ON);
        property[1].setState(ISS_OFF);
    } else {
        property[0].setState(ISS_OFF);
        property[1].setState(ISS_ON);
    }
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::moveSteps(int steps) -> bool {
    INDI::PropertyNumber property = device_.getProperty("REL_FOCUS_POSITION");
    if (!property.isValid()) {
        logger_->error("Unable to find REL_FOCUS_POSITION property...");
        return false;
    }
    property[0].value = steps;
    sendNewProperty(property);
    lastMoveSteps_ = steps;
    totalSteps_ += std::abs(steps);
    return true;
}

auto INDIFocuser::moveToPosition(int position) -> bool {
    INDI::PropertyNumber property = device_.getProperty("ABS_FOCUS_POSITION");
    if (!property.isValid()) {
        logger_->error("Unable to find ABS_FOCUS_POSITION property...");
        return false;
    }
    lastMoveSteps_ = position - current_position_;
    property[0].value = position;
    sendNewProperty(property);
    target_position_ = position;
    totalSteps_ += std::abs(lastMoveSteps_);
    return true;
}

auto INDIFocuser::getPosition() -> std::optional<int> {
    INDI::PropertyNumber property = device_.getProperty("ABS_FOCUS_POSITION");
    if (!property.isValid()) {
        logger_->error("Unable to find ABS_FOCUS_POSITION property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

auto INDIFocuser::moveForDuration(int durationMs) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_TIMER");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_TIMER property...");
        return false;
    }
    property[0].value = durationMs;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::abortMove() -> bool {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_ABORT_MOTION");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_ABORT_MOTION property...");
        return false;
    }
    property[0].setState(ISS_ON);
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::syncPosition(int position) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_SYNC");
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_SYNC property...");
        return false;
    }
    property[0].value = position;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::getExternalTemperature() -> std::optional<double> {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_TEMPERATURE");
    sendNewProperty(property);
    if (!property.isValid()) {
        logger_->error("Unable to find FOCUS_TEMPERATURE property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

auto INDIFocuser::getChipTemperature() -> std::optional<double> {
    INDI::PropertyNumber property = device_.getProperty("CHIP_TEMPERATURE");
    sendNewProperty(property);
    if (!property.isValid()) {
        logger_->error("Unable to find CHIP_TEMPERATURE property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

// Additional methods implementation

auto INDIFocuser::isMoving() const -> bool {
    return isFocuserMoving_.load();
}

auto INDIFocuser::getMaxSpeed() -> int {
    // Most INDI focusers don't have a specific max speed property
    // Return a reasonable default
    return 100;
}

auto INDIFocuser::getSpeedRange() -> std::pair<int, int> {
    // Standard INDI focuser speed range
    return {1, 100};
}

auto INDIFocuser::getMinLimit() -> std::optional<int> {
    // Most INDI focusers don't have a minimum limit property
    // Return the internal minimum position
    return minPosition_;
}

auto INDIFocuser::setMinLimit(int minLimit) -> bool {
    minPosition_ = minLimit;
    return true;
}

auto INDIFocuser::moveInward(int steps) -> bool {
    setDirection(FocusDirection::IN);
    return moveSteps(steps);
}

auto INDIFocuser::moveOutward(int steps) -> bool {
    setDirection(FocusDirection::OUT);
    return moveSteps(steps);
}

auto INDIFocuser::getBacklash() -> int {
    return backlashSteps_.load();
}

auto INDIFocuser::setBacklash(int backlash) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_BACKLASH_STEPS");
    if (!property.isValid()) {
        logger_->warn("Unable to find FOCUS_BACKLASH_STEPS property, setting internal value");
        backlashSteps_ = backlash;
        return true;
    }
    property[0].value = backlash;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::enableBacklashCompensation(bool enable) -> bool {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_BACKLASH_TOGGLE");
    if (!property.isValid()) {
        logger_->warn("Unable to find FOCUS_BACKLASH_TOGGLE property, setting internal value");
        backlashEnabled_ = enable;
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

auto INDIFocuser::isBacklashCompensationEnabled() -> bool {
    return backlashEnabled_.load();
}

auto INDIFocuser::hasTemperatureSensor() -> bool {
    INDI::PropertyNumber tempProperty = device_.getProperty("FOCUS_TEMPERATURE");
    INDI::PropertyNumber chipProperty = device_.getProperty("CHIP_TEMPERATURE");
    return tempProperty.isValid() || chipProperty.isValid();
}

auto INDIFocuser::getTemperatureCompensation() -> TemperatureCompensation {
    return tempCompensation_;
}

auto INDIFocuser::setTemperatureCompensation(const TemperatureCompensation& comp) -> bool {
    tempCompensation_ = comp;
    logger_->info("Temperature compensation set: enabled={}, coefficient={}", 
                 comp.enabled, comp.coefficient);
    return true;
}

auto INDIFocuser::enableTemperatureCompensation(bool enable) -> bool {
    tempCompensationEnabled_ = enable;
    tempCompensation_.enabled = enable;
    logger_->info("Temperature compensation {}", enable ? "enabled" : "disabled");
    return true;
}

auto INDIFocuser::startAutoFocus() -> bool {
    // INDI doesn't typically have built-in autofocus
    // This would be handled by client software like Ekos
    logger_->warn("Auto-focus not directly supported by INDI drivers");
    isAutoFocusing_ = true;
    autoFocusProgress_ = 0.0;
    return false;
}

auto INDIFocuser::stopAutoFocus() -> bool {
    isAutoFocusing_ = false;
    autoFocusProgress_ = 0.0;
    return true;
}

auto INDIFocuser::isAutoFocusing() -> bool {
    return isAutoFocusing_.load();
}

auto INDIFocuser::getAutoFocusProgress() -> double {
    return autoFocusProgress_.load();
}

auto INDIFocuser::savePreset(int slot, int position) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) {
        logger_->error("Invalid preset slot: {}", slot);
        return false;
    }
    presets_[slot] = position;
    logger_->info("Saved preset {} with position {}", slot, position);
    return true;
}

auto INDIFocuser::loadPreset(int slot) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) {
        logger_->error("Invalid preset slot: {}", slot);
        return false;
    }
    if (!presets_[slot].has_value()) {
        logger_->error("Preset slot {} is empty", slot);
        return false;
    }
    return moveToPosition(presets_[slot].value());
}

auto INDIFocuser::getPreset(int slot) -> std::optional<int> {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) {
        return std::nullopt;
    }
    return presets_[slot];
}

auto INDIFocuser::deletePreset(int slot) -> bool {
    if (slot < 0 || slot >= static_cast<int>(presets_.size())) {
        logger_->error("Invalid preset slot: {}", slot);
        return false;
    }
    presets_[slot].reset();
    logger_->info("Deleted preset {}", slot);
    return true;
}

auto INDIFocuser::getTotalSteps() -> uint64_t {
    return totalSteps_.load();
}

auto INDIFocuser::resetTotalSteps() -> bool {
    totalSteps_ = 0;
    logger_->info("Reset total steps counter");
    return true;
}

auto INDIFocuser::getLastMoveSteps() -> int {
    return lastMoveSteps_.load();
}

auto INDIFocuser::getLastMoveDuration() -> int {
    return lastMoveDuration_.load();
}

void INDIFocuser::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    auto message = baseDevice.messageQueue(messageID);
    logger_->info("Message from {}: {}", baseDevice.getDeviceName(), message);
}

ATOM_MODULE(focuser_indi, [](Component &component) {
    auto logger = spdlog::get("focuser");
    if (!logger) {
        logger = spdlog::default_logger();
    }
    logger->info("Registering focuser_indi module...");
    component.doc("INDI Focuser");
    component.def("initialize", &INDIFocuser::initialize, "device",
                  "Initialize a focuser device.");
    component.def("destroy", &INDIFocuser::destroy, "device",
                  "Destroy a focuser device.");
    component.def("connect", &INDIFocuser::connect, "device",
                  "Connect to a focuser device.");
    component.def("disconnect", &INDIFocuser::disconnect, "device",
                  "Disconnect from a focuser device.");
    component.def("reconnect", &INDIFocuser::reconnect, "device",
                  "Reconnect to a focuser device.");
    component.def("scan", &INDIFocuser::scan, "device",
                  "Scan for focuser devices.");
    component.def("is_connected", &INDIFocuser::isConnected, "device",
                  "Check if a focuser device is connected.");

    component.def("get_focuser_speed", &INDIFocuser::getSpeed, "device",
                  "Get the focuser speed.");
    component.def("set_focuser_speed", &INDIFocuser::setSpeed, "device",
                  "Set the focuser speed.");

    component.def("get_move_direction", &INDIFocuser::getDirection, "device",
                  "Get the focuser mover direction.");
    component.def("set_move_direction", &INDIFocuser::setDirection, "device",
                  "Set the focuser mover direction.");

    component.def("get_max_limit", &INDIFocuser::getMaxLimit, "device",
                  "Get the focuser max limit.");
    component.def("set_max_limit", &INDIFocuser::setMaxLimit, "device",
                  "Set the focuser max limit.");

    component.def("is_reversed", &INDIFocuser::isReversed, "device",
                  "Get whether the focuser reverse is enabled.");
    component.def("set_reversed", &INDIFocuser::setReversed, "device",
                  "Set whether the focuser reverse is enabled.");

    component.def("move_steps", &INDIFocuser::moveSteps, "device",
                  "Move the focuser steps.");
    component.def("move_to_position", &INDIFocuser::moveToPosition, "device",
                  "Move the focuser to absolute position.");
    component.def("get_position", &INDIFocuser::getPosition, "device",
                  "Get the focuser absolute position.");
    component.def("move_for_duration", &INDIFocuser::moveForDuration, "device",
                  "Move the focuser with time.");
    component.def("abort_move", &INDIFocuser::abortMove, "device",
                  "Abort the focuser move.");
    component.def("sync_position", &INDIFocuser::syncPosition, "device",
                  "Sync the focuser position.");
    component.def("get_external_temperature",
                  &INDIFocuser::getExternalTemperature, "device",
                  "Get the focuser external temperature.");
    component.def("get_chip_temperature", &INDIFocuser::getChipTemperature,
                  "device", "Get the focuser chip temperature.");

    // Additional method registrations
    component.def("is_moving", &INDIFocuser::isMoving, "device",
                  "Check if focuser is currently moving.");
    component.def("get_max_speed", &INDIFocuser::getMaxSpeed, "device",
                  "Get maximum focuser speed.");
    component.def("get_speed_range", &INDIFocuser::getSpeedRange, "device",
                  "Get focuser speed range.");
    component.def("move_inward", &INDIFocuser::moveInward, "device",
                  "Move focuser inward by steps.");
    component.def("move_outward", &INDIFocuser::moveOutward, "device",
                  "Move focuser outward by steps.");
    component.def("get_backlash", &INDIFocuser::getBacklash, "device",
                  "Get backlash compensation steps.");
    component.def("set_backlash", &INDIFocuser::setBacklash, "device",
                  "Set backlash compensation steps.");
    component.def("enable_backlash_compensation", &INDIFocuser::enableBacklashCompensation, "device",
                  "Enable/disable backlash compensation.");
    component.def("has_temperature_sensor", &INDIFocuser::hasTemperatureSensor, "device",
                  "Check if focuser has temperature sensor.");
    component.def("save_preset", &INDIFocuser::savePreset, "device",
                  "Save current position to preset slot.");
    component.def("load_preset", &INDIFocuser::loadPreset, "device",
                  "Load position from preset slot.");
    component.def("get_total_steps", &INDIFocuser::getTotalSteps, "device",
                  "Get total steps moved since reset.");
    component.def("reset_total_steps", &INDIFocuser::resetTotalSteps, "device",
                  "Reset total steps counter.");

    component.def(
        "create_instance",
        [](const std::string &name) {
            std::shared_ptr<AtomFocuser> instance =
                std::make_shared<INDIFocuser>(name);
            return instance;
        },
        "device", "Create a new focuser instance.");
    component.defType<INDIFocuser>("focuser_indi", "device",
                                   "Define a new focuser instance.");

    logger->info("Registered focuser_indi module.");
});
