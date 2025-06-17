#include "property_manager.hpp"

#include <algorithm>

namespace lithium::device::indi::focuser {

bool PropertyManager::initialize(FocuserState &state) {
    state_ = &state;
    state_->logger_->info("{}: Initializing property manager",
                          getComponentName());
    return true;
}

void PropertyManager::cleanup() {
    if (state_) {
        state_->logger_->info("{}: Cleaning up property manager",
                              getComponentName());
    }
    state_ = nullptr;
}

void PropertyManager::setupPropertyWatchers(INDI::BaseDevice &device,
                                            FocuserState &state) {
    setupConnectionProperties(device, state);
    setupDriverInfoProperties(device, state);
    setupConfigurationProperties(device, state);
    setupFocusProperties(device, state);
    setupTemperatureProperties(device, state);
    setupBacklashProperties(device, state);
}

void PropertyManager::setupConnectionProperties(INDI::BaseDevice &device,
                                                FocuserState &state) {
    device.watchProperty(
        "CONNECTION",
        [&state](const INDI::PropertySwitch &property) {
            state.isConnected_.store(property[0].getState() == ISS_ON,
                                     std::memory_order_relaxed);
            state.logger_->info(
                "{} is {}.", state.deviceName_,
                state.isConnected_.load(std::memory_order_relaxed)
                    ? "connected"
                    : "disconnected");
        },
        INDI::BaseDevice::WATCH_UPDATE);
}

void PropertyManager::setupDriverInfoProperties(INDI::BaseDevice &device,
                                                FocuserState &state) {
    device.watchProperty(
        "DRIVER_INFO",
        [&state](const INDI::PropertyText &property) {
            if (!property.isValid())
                return;
            state.logger_->info("Driver name: {}", property[0].getText());
            state.logger_->info("Driver executable: {}", property[1].getText());
            state.logger_->info("Driver version: {}", property[2].getText());
            state.logger_->info("Driver interface: {}", property[3].getText());
            state.driverExec_ = property[1].getText();
            state.driverVersion_ = property[2].getText();
            state.driverInterface_ = property[3].getText();
        },
        INDI::BaseDevice::WATCH_NEW);
}

void PropertyManager::setupConfigurationProperties(INDI::BaseDevice &device,
                                                   FocuserState &state) {
    device.watchProperty(
        "DEBUG",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            state.isDebug_.store(property[0].getState() == ISS_ON,
                                 std::memory_order_relaxed);
            state.logger_->info(
                "Debug is {}",
                state.isDebug_.load(std::memory_order_relaxed) ? "ON" : "OFF");
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "POLLING_PERIOD",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto period = property[0].getValue();
            auto prev = state.currentPollingPeriod_.exchange(
                period, std::memory_order_relaxed);
            if (period != prev) {
                state.logger_->info("Polling period changed to: {}", period);
            }
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "DEVICE_AUTO_SEARCH",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            bool autoSearch = property[0].getState() == ISS_ON;
            state.deviceAutoSearch_ = autoSearch;
            state.logger_->info("Auto search is {}", autoSearch ? "ON" : "OFF");
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "DEVICE_PORT_SCAN",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            bool portScan = property[0].getState() == ISS_ON;
            state.devicePortScan_ = portScan;
            state.logger_->info("Device port scan is {}",
                                portScan ? "ON" : "OFF");
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "BAUD_RATE",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            auto it = std::ranges::find_if(property, [](const auto &item) {
                return item.getState() == ISS_ON;
            });
            if (it != property.end()) {
                int idx = std::distance(property.begin(), it);
                state.logger_->info("Baud rate is {}", it->getLabel());
                state.baudRate_ = static_cast<BAUD_RATE>(idx);
            }
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);
}

void PropertyManager::setupFocusProperties(INDI::BaseDevice &device,
                                           FocuserState &state) {
    device.watchProperty(
        "Mode",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            auto it = std::ranges::find_if(property, [](const auto &item) {
                return item.getState() == ISS_ON;
            });
            if (it != property.end()) {
                int idx = std::distance(property.begin(), it);
                state.logger_->info("Focuser mode is {}", it->getLabel());
                state.focusMode_ = static_cast<FocusMode>(idx);
            }
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "FOCUS_MOTION",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            auto it = std::ranges::find_if(property, [](const auto &item) {
                return item.getState() == ISS_ON;
            });
            if (it != property.end()) {
                int idx = std::distance(property.begin(), it);
                state.logger_->info("Focuser motion is {}", it->getLabel());
                state.focusDirection_ = static_cast<FocusDirection>(idx);
            }
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "FOCUS_SPEED",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto speed = property[0].getValue();
            state.logger_->info("Current focuser speed: {}", speed);
            state.currentFocusSpeed_.store(speed, std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "REL_FOCUS_POSITION",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto position = static_cast<int>(property[0].getValue());
            state.logger_->info("Current relative focuser position: {}",
                                position);
            state.realRelativePosition_.store(position,
                                              std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "ABS_FOCUS_POSITION",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto position = static_cast<int>(property[0].getValue());
            state.logger_->info("Current absolute focuser position: {}",
                                position);
            state.realAbsolutePosition_.store(position,
                                              std::memory_order_relaxed);
            state.currentPosition_.store(position, std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "FOCUS_MAX",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto maxlimit = static_cast<int>(property[0].getValue());
            state.logger_->info("Current focuser max limit: {}", maxlimit);
            state.maxPosition_ = maxlimit;
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "FOCUS_REVERSE_MOTION",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            bool reversed = property[0].getState() == ISS_ON;
            state.logger_->info("Focuser is {}",
                                reversed ? "reversed" : "not reversed");
            state.isReverse_.store(reversed, std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "FOCUS_TIMER",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto timer = property[0].getValue();
            state.logger_->info("Current focuser timer: {}", timer);
            state.focusTimer_.store(timer, std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "FOCUS_ABORT_MOTION",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            bool aborting = property[0].getState() == ISS_ON;
            state.logger_->info("Focuser is {}",
                                aborting ? "aborting" : "not aborting");
            state.isFocuserMoving_.store(!aborting, std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "DELAY",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto delay = static_cast<int>(property[0].getValue());
            state.logger_->info("Current focuser delay: {}", delay);
            state.delay_msec_ = delay;
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);
}

void PropertyManager::setupTemperatureProperties(INDI::BaseDevice &device,
                                                 FocuserState &state) {
    device.watchProperty(
        "FOCUS_TEMPERATURE",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto temperature = property[0].getValue();
            state.logger_->info("Current focuser temperature: {}", temperature);
            state.temperature_.store(temperature, std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "CHIP_TEMPERATURE",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto temperature = property[0].getValue();
            state.logger_->info("Current chip temperature: {}", temperature);
            state.chipTemperature_.store(temperature,
                                         std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);
}

void PropertyManager::setupBacklashProperties(INDI::BaseDevice &device,
                                              FocuserState &state) {
    device.watchProperty(
        "FOCUS_BACKLASH_TOGGLE",
        [&state](const INDI::PropertySwitch &property) {
            if (!property.isValid())
                return;
            bool enabled = property[0].getState() == ISS_ON;
            state.logger_->info("Backlash is {}",
                                enabled ? "enabled" : "disabled");
            state.backlashEnabled_.store(enabled, std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    device.watchProperty(
        "FOCUS_BACKLASH_STEPS",
        [&state](const INDI::PropertyNumber &property) {
            if (!property.isValid())
                return;
            auto backlash = static_cast<int>(property[0].getValue());
            state.logger_->info("Current focuser backlash: {}", backlash);
            state.backlashSteps_.store(backlash, std::memory_order_relaxed);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);
}

}  // namespace lithium::device::indi::focuser
