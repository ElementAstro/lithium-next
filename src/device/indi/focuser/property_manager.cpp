#include "property_manager.hpp"

namespace lithium::device::indi::focuser {

PropertyManager::PropertyManager(std::shared_ptr<INDIFocuserCore> core)
    : FocuserComponentBase(std::move(core)) {
}

bool PropertyManager::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    core->getLogger()->info("{}: Initializing property manager", getComponentName());
    setupPropertyWatchers();
    return true;
}

void PropertyManager::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("{}: Shutting down property manager", getComponentName());
    }
}

void PropertyManager::setupPropertyWatchers() {
    setupConnectionProperties();
    setupDriverInfoProperties();
    setupConfigurationProperties();
    setupFocusProperties();
    setupTemperatureProperties();
    setupBacklashProperties();
}

void PropertyManager::handlePropertyUpdate(const INDI::Property& property) {
    // For now, we'll handle property updates directly in the watchers
    // This method can be used for centralized property handling if needed
}

void PropertyManager::setupConnectionProperties() {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return;
    }

    auto& device = core->getDevice();

    // Watch CONNECTION property
    device.watchProperty("CONNECTION",
        [this](const INDI::PropertySwitch& property) {
            auto core = getCore();
            if (!core) return;

            bool connected = property[0].getState() == ISS_ON;
            core->setConnected(connected);
            core->getLogger()->info("{} is {}",
                core->getDeviceName(),
                connected ? "connected" : "disconnected");
        },
        INDI::BaseDevice::WATCH_UPDATE);
}

void PropertyManager::setupDriverInfoProperties() {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return;
    }

    auto& device = core->getDevice();

    // Watch DRIVER_INFO property
    device.watchProperty("DRIVER_INFO",
        [this](const INDI::PropertyText& property) {
            auto core = getCore();
            if (!core) return;

            core->getLogger()->debug("Driver info updated for {}", core->getDeviceName());
            // Driver info is typically read-only, so we just log it
        },
        INDI::BaseDevice::WATCH_NEW);
}

void PropertyManager::setupConfigurationProperties() {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return;
    }

    auto& device = core->getDevice();

    // Watch polling period
    device.watchProperty("POLLING_PERIOD",
        [this](const INDI::PropertyNumber& property) {
            auto core = getCore();
            if (!core) return;

            core->getLogger()->debug("Polling period updated for {}", core->getDeviceName());
        },
        INDI::BaseDevice::WATCH_UPDATE);
}

void PropertyManager::setupFocusProperties() {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return;
    }

    auto& device = core->getDevice();

    // Watch absolute position
    device.watchProperty("ABS_FOCUS_POSITION",
        [this](const INDI::PropertyNumber& property) {
            auto core = getCore();
            if (!core) return;

            int position = static_cast<int>(property[0].getValue());
            core->setCurrentPosition(position);
            core->getLogger()->debug("Absolute position updated: {}", position);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch relative position
    device.watchProperty("REL_FOCUS_POSITION",
        [this](const INDI::PropertyNumber& property) {
            auto core = getCore();
            if (!core) return;

            int relPosition = static_cast<int>(property[0].getValue());
            core->setRelativePosition(relPosition);
            core->getLogger()->debug("Relative position updated: {}", relPosition);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch focus speed
    device.watchProperty("FOCUS_SPEED",
        [this](const INDI::PropertyNumber& property) {
            auto core = getCore();
            if (!core) return;

            double speed = property[0].getValue();
            core->setCurrentSpeed(speed);
            core->getLogger()->debug("Focus speed updated: {}", speed);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch focus direction
    device.watchProperty("FOCUS_MOTION",
        [this](const INDI::PropertySwitch& property) {
            auto core = getCore();
            if (!core) return;

            FocusDirection direction = FocusDirection::IN;
            // Check which switch element is on
            for (int i = 0; i < property.count(); i++) {
                if (property[i].getState() == ISS_ON) {
                    if (strcmp(property[i].getName(), "FOCUS_INWARD") == 0) {
                        direction = FocusDirection::IN;
                    } else if (strcmp(property[i].getName(), "FOCUS_OUTWARD") == 0) {
                        direction = FocusDirection::OUT;
                    }
                    break;
                }
            }
            core->setDirection(direction);
            core->getLogger()->debug("Focus direction updated: {}",
                direction == FocusDirection::IN ? "IN" : "OUT");
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch focus limits
    device.watchProperty("FOCUS_MAX",
        [this](const INDI::PropertyNumber& property) {
            auto core = getCore();
            if (!core) return;

            int maxPos = static_cast<int>(property[0].getValue());
            core->setMaxPosition(maxPos);
            core->getLogger()->debug("Max position updated: {}", maxPos);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch focus reverse
    device.watchProperty("FOCUS_REVERSE",
        [this](const INDI::PropertySwitch& property) {
            auto core = getCore();
            if (!core) return;

            bool reversed = false;
            // Find the enabled switch element
            for (int i = 0; i < property.count(); i++) {
                if (property[i].getState() == ISS_ON &&
                    strcmp(property[i].getName(), "INDI_ENABLED") == 0) {
                    reversed = true;
                    break;
                }
            }
            core->setReversed(reversed);
            core->getLogger()->debug("Focus reverse updated: {}", reversed);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch focus state (moving/idle)
    device.watchProperty("FOCUS_STATE",
        [this](const INDI::PropertySwitch& property) {
            auto core = getCore();
            if (!core) return;

            bool moving = false;
            // Find the busy switch element
            for (int i = 0; i < property.count(); i++) {
                if (property[i].getState() == ISS_ON &&
                    strcmp(property[i].getName(), "FOCUS_BUSY") == 0) {
                    moving = true;
                    break;
                }
            }
            core->setMoving(moving);
            core->getLogger()->debug("Focus state updated: {}",
                moving ? "MOVING" : "IDLE");
        },
        INDI::BaseDevice::WATCH_UPDATE);
}

void PropertyManager::setupTemperatureProperties() {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return;
    }

    auto& device = core->getDevice();

    // Watch temperature reading
    device.watchProperty("FOCUS_TEMPERATURE",
        [this](const INDI::PropertyNumber& property) {
            auto core = getCore();
            if (!core) return;

            double temperature = property[0].getValue();
            core->setTemperature(temperature);
            core->getLogger()->debug("Temperature updated: {:.2f}°C", temperature);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch chip temperature if available
    device.watchProperty("CHIP_TEMPERATURE",
        [this](const INDI::PropertyNumber& property) {
            auto core = getCore();
            if (!core) return;

            double chipTemp = property[0].getValue();
            core->setChipTemperature(chipTemp);
            core->getLogger()->debug("Chip temperature updated: {:.2f}°C", chipTemp);
        },
        INDI::BaseDevice::WATCH_UPDATE);
}

void PropertyManager::setupBacklashProperties() {
    auto core = getCore();
    if (!core || !core->getDevice().isValid()) {
        return;
    }

    auto& device = core->getDevice();

    // Watch backlash enable/disable
    device.watchProperty("FOCUS_BACKLASH_TOGGLE",
        [this](const INDI::PropertySwitch& property) {
            auto core = getCore();
            if (!core) return;

            bool enabled = false;
            // Find the enabled switch element
            for (int i = 0; i < property.count(); i++) {
                if (property[i].getState() == ISS_ON &&
                    strcmp(property[i].getName(), "INDI_ENABLED") == 0) {
                    enabled = true;
                    break;
                }
            }
            core->setBacklashEnabled(enabled);
            core->getLogger()->debug("Backlash compensation: {}",
                enabled ? "ENABLED" : "DISABLED");
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch backlash steps
    device.watchProperty("FOCUS_BACKLASH_STEPS",
        [this](const INDI::PropertyNumber& property) {
            auto core = getCore();
            if (!core) return;

            int steps = static_cast<int>(property[0].getValue());
            core->setBacklashSteps(steps);
            core->getLogger()->debug("Backlash steps updated: {}", steps);
        },
        INDI::BaseDevice::WATCH_UPDATE);
}

void PropertyManager::handleSwitchPropertyUpdate(const INDI::PropertySwitch& property) {
    auto core = getCore();
    if (!core) return;

    const std::string& name = property.getName();
    core->getLogger()->debug("Switch property '{}' updated", name);

    // Handle specific switch properties
    if (name == "CONNECTION") {
        bool connected = property[0].getState() == ISS_ON;
        core->setConnected(connected);
    } else if (name == "FOCUS_MOTION") {
        FocusDirection direction = FocusDirection::IN;
        for (int i = 0; i < property.count(); i++) {
            if (property[i].getState() == ISS_ON) {
                if (strcmp(property[i].getName(), "FOCUS_INWARD") == 0) {
                    direction = FocusDirection::IN;
                } else if (strcmp(property[i].getName(), "FOCUS_OUTWARD") == 0) {
                    direction = FocusDirection::OUT;
                }
                break;
            }
        }
        core->setDirection(direction);
    } else if (name == "FOCUS_REVERSE") {
        bool reversed = false;
        for (int i = 0; i < property.count(); i++) {
            if (property[i].getState() == ISS_ON &&
                strcmp(property[i].getName(), "INDI_ENABLED") == 0) {
                reversed = true;
                break;
            }
        }
        core->setReversed(reversed);
    } else if (name == "FOCUS_STATE") {
        bool moving = false;
        for (int i = 0; i < property.count(); i++) {
            if (property[i].getState() == ISS_ON &&
                strcmp(property[i].getName(), "FOCUS_BUSY") == 0) {
                moving = true;
                break;
            }
        }
        core->setMoving(moving);
    } else if (name == "FOCUS_BACKLASH_TOGGLE") {
        bool enabled = false;
        for (int i = 0; i < property.count(); i++) {
            if (property[i].getState() == ISS_ON &&
                strcmp(property[i].getName(), "INDI_ENABLED") == 0) {
                enabled = true;
                break;
            }
        }
        core->setBacklashEnabled(enabled);
    }
}

void PropertyManager::handleNumberPropertyUpdate(const INDI::PropertyNumber& property) {
    auto core = getCore();
    if (!core) return;

    const std::string& name = property.getName();
    core->getLogger()->debug("Number property '{}' updated", name);

    // Handle specific number properties
    if (name == "ABS_FOCUS_POSITION") {
        int position = static_cast<int>(property[0].getValue());
        core->setCurrentPosition(position);
    } else if (name == "REL_FOCUS_POSITION") {
        int relPosition = static_cast<int>(property[0].getValue());
        core->setRelativePosition(relPosition);
    } else if (name == "FOCUS_SPEED") {
        double speed = property[0].getValue();
        core->setCurrentSpeed(speed);
    } else if (name == "FOCUS_MAX") {
        int maxPos = static_cast<int>(property[0].getValue());
        core->setMaxPosition(maxPos);
    } else if (name == "FOCUS_TEMPERATURE") {
        double temperature = property[0].getValue();
        core->setTemperature(temperature);
    } else if (name == "CHIP_TEMPERATURE") {
        double chipTemp = property[0].getValue();
        core->setChipTemperature(chipTemp);
    } else if (name == "FOCUS_BACKLASH_STEPS") {
        int steps = static_cast<int>(property[0].getValue());
        core->setBacklashSteps(steps);
    }
}

void PropertyManager::handleTextPropertyUpdate(const INDI::PropertyText& property) {
    auto core = getCore();
    if (!core) return;

    const std::string& name = property.getName();
    core->getLogger()->debug("Text property '{}' updated", name);

    // Handle specific text properties if needed
    // Most text properties are informational (like DRIVER_INFO)
}

}  // namespace lithium::device::indi::focuser
