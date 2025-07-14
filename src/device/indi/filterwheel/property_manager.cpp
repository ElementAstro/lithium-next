#include "property_manager.hpp"

namespace lithium::device::indi::filterwheel {

PropertyManager::PropertyManager(std::shared_ptr<INDIFilterWheelCore> core)
    : FilterWheelComponentBase(std::move(core)) {}

bool PropertyManager::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    core->getLogger()->info("Initializing PropertyManager");

    if (core->isConnected()) {
        setupPropertyWatchers();
        syncFromProperties();
    }

    initialized_ = true;
    return true;
}

void PropertyManager::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("Shutting down PropertyManager");
    }
    initialized_ = false;
}

void PropertyManager::setupPropertyWatchers() {
    auto core = getCore();
    if (!core || !core->isConnected()) {
        return;
    }

    auto& device = core->getDevice();

    // Watch CONNECTION property
    device.watchProperty("CONNECTION",
        [this](const INDI::PropertySwitch& property) {
            handleConnectionProperty(property);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch DRIVER_INFO property
    device.watchProperty("DRIVER_INFO",
        [this](const INDI::PropertyText& property) {
            handleDriverInfoProperty(property);
        },
        INDI::BaseDevice::WATCH_NEW);

    // Watch DEBUG property
    device.watchProperty("DEBUG",
        [this](const INDI::PropertySwitch& property) {
            handleDebugProperty(property);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch POLLING_PERIOD property
    device.watchProperty("POLLING_PERIOD",
        [this](const INDI::PropertyNumber& property) {
            handlePollingProperty(property);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch FILTER_SLOT property
    device.watchProperty("FILTER_SLOT",
        [this](const INDI::PropertyNumber& property) {
            handleFilterSlotProperty(property);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Watch FILTER_NAME property
    device.watchProperty("FILTER_NAME",
        [this](const INDI::PropertyText& property) {
            handleFilterNameProperty(property);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    core->getLogger()->debug("PropertyManager: Property watchers set up");
}

void PropertyManager::syncFromProperties() {
    auto core = getCore();
    if (!core || !core->isConnected()) {
        return;
    }

    auto& device = core->getDevice();

    // Sync current filter slot
    INDI::PropertyNumber slotProp = device.getProperty("FILTER_SLOT");
    if (slotProp.isValid()) {
        handleFilterSlotProperty(slotProp);
    }

    // Sync filter names
    INDI::PropertyText nameProp = device.getProperty("FILTER_NAME");
    if (nameProp.isValid()) {
        handleFilterNameProperty(nameProp);
    }

    // Sync polling period
    INDI::PropertyNumber pollingProp = device.getProperty("POLLING_PERIOD");
    if (pollingProp.isValid()) {
        handlePollingProperty(pollingProp);
    }

    // Sync debug state
    INDI::PropertySwitch debugProp = device.getProperty("DEBUG");
    if (debugProp.isValid()) {
        handleDebugProperty(debugProp);
    }

    core->getLogger()->debug("PropertyManager: Properties synchronized");
}

void PropertyManager::handleConnectionProperty(const INDI::PropertySwitch& property) {
    auto core = getCore();
    if (!core) return;

    if (property.getState() == IPS_OK) {
        if (auto connectSwitch = property.findWidgetByName("CONNECT");
            connectSwitch && connectSwitch->getState() == ISS_ON) {
            core->setConnected(true);
            core->getLogger()->info("FilterWheel connected");
        } else {
            core->setConnected(false);
            core->getLogger()->info("FilterWheel disconnected");
        }
    }
}

void PropertyManager::handleDriverInfoProperty(const INDI::PropertyText& property) {
    auto core = getCore();
    if (!core) return;

    for (int i = 0; i < property.count(); ++i) {
        const auto& widget = property[i];
        const std::string name = widget.getName();
        const std::string value = widget.getText();

        if (name == "DRIVER_NAME") {
            core->setDriverExec(value);
        } else if (name == "DRIVER_VERSION") {
            core->setDriverVersion(value);
        } else if (name == "DRIVER_INTERFACE") {
            core->setDriverInterface(value);
        }
    }

    core->getLogger()->debug("Driver info updated: {} v{}",
                            core->getDriverExec(), core->getDriverVersion());
}

void PropertyManager::handleDebugProperty(const INDI::PropertySwitch& property) {
    auto core = getCore();
    if (!core) return;

    if (auto enableSwitch = property.findWidgetByName("ENABLE");
        enableSwitch && enableSwitch->getState() == ISS_ON) {
        core->setDebugEnabled(true);
        core->getLogger()->debug("Debug mode enabled");
    } else {
        core->setDebugEnabled(false);
        core->getLogger()->debug("Debug mode disabled");
    }
}

void PropertyManager::handlePollingProperty(const INDI::PropertyNumber& property) {
    auto core = getCore();
    if (!core) return;

    if (property.count() > 0) {
        double period = property[0].getValue();
        core->setPollingPeriod(period);
        core->getLogger()->debug("Polling period set to: {} ms", period);
    }
}

void PropertyManager::handleFilterSlotProperty(const INDI::PropertyNumber& property) {
    auto core = getCore();
    if (!core) return;

    if (property.count() > 0) {
        int slot = static_cast<int>(property[0].getValue());
        core->setCurrentSlot(slot);

        // Update movement state based on property state
        core->setMoving(property.getState() == IPS_BUSY);

        // Update current slot name if available
        const auto& slotNames = core->getSlotNames();
        if (slot > 0 && slot <= static_cast<int>(slotNames.size())) {
            core->setCurrentSlotName(slotNames[slot - 1]);
        }

        core->getLogger()->debug("Filter slot changed to: {} ({})",
                                slot, core->getCurrentSlotName());
    }
}

void PropertyManager::handleFilterNameProperty(const INDI::PropertyText& property) {
    auto core = getCore();
    if (!core) return;

    std::vector<std::string> names;
    names.reserve(property.count());

    for (int i = 0; i < property.count(); ++i) {
        names.emplace_back(property[i].getText());
    }

    core->setSlotNames(names);
    core->setMaxSlot(static_cast<int>(names.size()));

    core->getLogger()->debug("Filter names updated: {} filters", names.size());
}

}  // namespace lithium::device::indi::filterwheel
