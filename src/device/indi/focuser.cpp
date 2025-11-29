#include "focuser.hpp"

#include <optional>
#include <tuple>

#include "atom/log/spdlog_logger.hpp"

#include "atom/components/component.hpp"
#include "atom/components/registry.hpp"
#include "device/template/focuser.hpp"

INDIFocuser::INDIFocuser(std::string name) : AtomFocuser(name) {}

auto INDIFocuser::initialize() -> bool { return true; }

auto INDIFocuser::destroy() -> bool { return true; }

auto INDIFocuser::connect(const std::string &deviceName, int timeout,
                          int maxRetry) -> bool {
    if (isConnected_.load()) {
        LOG_ERROR("{} is already connected.", deviceName_);
        return false;
    }

    deviceName_ = deviceName;
    LOG_INFO("Connecting to {}...", deviceName_);
    // Max: 需要获取初始的参数，然后再注册对应的回调函数
    watchDevice(deviceName_.c_str(), [this](INDI::BaseDevice device) {
        device_ = device;  // save device

        // wait for the availability of the "CONNECTION" property
        device.watchProperty(
            "CONNECTION",
            [this](INDI::Property) {
                LOG_INFO("Connecting to {}...", deviceName_);
                connectDevice(name_.c_str());
            },
            INDI::BaseDevice::WATCH_NEW);

        device.watchProperty(
            "CONNECTION",
            [this](const INDI::PropertySwitch &property) {
                isConnected_ = property[0].getState() == ISS_ON;
                if (isConnected_.load()) {
                    LOG_INFO("{} is connected.", deviceName_);
                } else {
                    LOG_INFO("{} is disconnected.", deviceName_);
                }
            },
            INDI::BaseDevice::WATCH_UPDATE);

        device.watchProperty(
            "DRIVER_INFO",
            [this](const INDI::PropertyText &property) {
                if (property.isValid()) {
                    const auto *driverName = property[0].getText();
                    LOG_INFO("Driver name: {}", driverName);

                    const auto *driverExec = property[1].getText();
                    LOG_INFO("Driver executable: {}", driverExec);
                    driverExec_ = driverExec;
                    const auto *driverVersion = property[2].getText();
                    LOG_INFO("Driver version: {}", driverVersion);
                    driverVersion_ = driverVersion;
                    const auto *driverInterface = property[3].getText();
                    LOG_INFO("Driver interface: {}", driverInterface);
                    driverInterface_ = driverInterface;
                }
            },
            INDI::BaseDevice::WATCH_NEW);

        device.watchProperty(
            "DEBUG",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    isDebug_.store(property[0].getState() == ISS_ON);
                    LOG_INFO("Debug is {}", isDebug_.load() ? "ON" : "OFF");
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        // Max: 这个参数其实挺重要的，但是除了行星相机都不需要调整，默认就好
        device.watchProperty(
            "POLLING_PERIOD",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto period = property[0].getValue();
                    LOG_INFO("Current polling period: {}", period);
                    if (period != currentPollingPeriod_.load()) {
                        LOG_INFO("Polling period change to: {}", period);
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
                    LOG_INFO("Auto search is {}",
                             deviceAutoSearch_ ? "ON" : "OFF");
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "DEVICE_PORT_SCAN",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    devicePortScan_ = property[0].getState() == ISS_ON;
                    LOG_INFO("Device port scan is {}",
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
                            LOG_INFO("Baud rate is {}", property[i].getLabel());
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
                            LOG_INFO("Focuser mode is {}",
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
                            LOG_INFO("Focuser motion is {}",
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
                    LOG_INFO("Current focuser speed: {}", speed);
                    currentFocusSpeed_ = speed;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "REL_FOCUS_POSITION",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto position = property[0].getValue();
                    LOG_INFO("Current relative focuser position: {}", position);
                    realRelativePosition_ = position;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "ABS_FOCUS_POSITION",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto position = property[0].getValue();
                    LOG_INFO("Current absolute focuser position: {}", position);
                    realAbsolutePosition_ = position;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_MAX",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto maxlimit = property[0].getValue();
                    LOG_INFO("Current focuser max limit: {}", maxlimit);
                    maxPosition_ = maxlimit;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_BACKLASH_TOGGLE",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    if (property[0].getState() == ISS_ON) {
                        LOG_INFO("Backlash is enabled");
                        backlashEnabled_ = true;
                    } else {
                        LOG_INFO("Backlash is disabled");
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
                    LOG_INFO("Current focuser backlash: {}", backlash);
                    backlashSteps_ = backlash;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_TEMPERATURE",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto temperature = property[0].getValue();
                    LOG_INFO("Current focuser temperature: {}", temperature);
                    temperature_ = temperature;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "CHIP_TEMPERATURE",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto temperature = property[0].getValue();
                    LOG_INFO("Current chip temperature: {}", temperature);
                    chipTemperature_ = temperature;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "DELAY",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    auto delay = property[0].getValue();
                    LOG_INFO("Current focuser delay: {}", delay);
                    delay_msec_ = delay;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device.watchProperty(
            "FOCUS_REVERSE_MOTION",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    if (property[0].getState() == ISS_ON) {
                        LOG_INFO("Focuser is reversed");
                        isReverse_ = true;
                    } else {
                        LOG_INFO("Focuser is not reversed");
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
                    LOG_INFO("Current focuser timer: {}", timer);
                    focusTimer_ = timer;
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FOCUS_ABORT_MOTION",
            [this](const INDI::PropertySwitch &property) {
                if (property.isValid()) {
                    if (property[0].getState() == ISS_ON) {
                        LOG_INFO("Focuser is aborting");
                        isFocuserMoving_ = false;
                    } else {
                        LOG_INFO("Focuser is not aborting");
                        isFocuserMoving_ = true;
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);
    });

    return true;
}
auto INDIFocuser::disconnect() -> bool { return true; }

auto INDIFocuser::watchAdditionalProperty() -> bool { return true; }

void INDIFocuser::setPropertyNumber(std::string_view propertyName,
                                    double value) {}

auto INDIFocuser::getSpeed() -> std::optional<double> {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find FOCUS_SPEED property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

auto INDIFocuser::setSpeed(double speed) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_SPEED");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find FOCUS_SPEED property...");
        return false;
    }
    property[0].value = speed;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::getDirection() -> std::optional<FocusDirection> {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_MOTION");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find FOCUS_MOTION property...");
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
        LOG_ERROR("Unable to find FOCUS_MOTION property...");
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
        LOG_ERROR("Unable to find FOCUS_MAX property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

auto INDIFocuser::setMaxLimit(int maxlimit) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_MAX");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find FOCUS_MAX property...");
        return false;
    }
    property[0].value = maxlimit;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::isReversed() -> std::optional<bool> {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_REVERSE_MOTION");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find FOCUS_REVERSE_MOTION property...");
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
        LOG_ERROR("Unable to find FOCUS_REVERSE_MOTION property...");
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
        LOG_ERROR("Unable to find REL_FOCUS_POSITION property...");
        return false;
    }
    property[0].value = steps;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::moveToPosition(int position) -> bool {
    INDI::PropertyNumber property = device_.getProperty("ABS_FOCUS_POSITION");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find ABS_FOCUS_POSITION property...");
        return false;
    }
    property[0].value = position;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::getPosition() -> std::optional<int> {
    INDI::PropertyNumber property = device_.getProperty("ABS_FOCUS_POSITION");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find ABS_FOCUS_POSITION property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

auto INDIFocuser::moveForDuration(int durationMs) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_TIMER");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find FOCUS_TIMER property...");
        return false;
    }
    property[0].value = durationMs;
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::abortMove() -> bool {
    INDI::PropertySwitch property = device_.getProperty("FOCUS_ABORT_MOTION");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find FOCUS_ABORT_MOTION property...");
        return false;
    }
    property[0].setState(ISS_ON);
    sendNewProperty(property);
    return true;
}

auto INDIFocuser::syncPosition(int position) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FOCUS_SYNC");
    if (!property.isValid()) {
        LOG_ERROR("Unable to find FOCUS_SYNC property...");
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
        LOG_ERROR("Unable to find FOCUS_TEMPERATURE property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

auto INDIFocuser::getChipTemperature() -> std::optional<double> {
    INDI::PropertyNumber property = device_.getProperty("CHIP_TEMPERATURE");
    sendNewProperty(property);
    if (!property.isValid()) {
        LOG_ERROR("Unable to find CHIP_TEMPERATURE property...");
        return std::nullopt;
    }
    return property[0].getValue();
}

ATOM_MODULE(focuser_indi, [](Component &component) {
    LOG_INFO("Registering focuser_indi module...");
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

    LOG_INFO("Registered focuser_indi module.");
});
