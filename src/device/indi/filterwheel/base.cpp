/*
 * base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Base INDI FilterWheel implementation

*************************************************/

#include "base.hpp"

#include <chrono>
#include <thread>

#include <spdlog/sinks/stdout_color_sinks.h>

INDIFilterwheelBase::INDIFilterwheelBase(std::string name) : AtomFilterWheel(name) {
    logger_ = spdlog::get("filterwheel_indi") 
                  ? spdlog::get("filterwheel_indi")
                  : spdlog::stdout_color_mt("filterwheel_indi");
}

auto INDIFilterwheelBase::initialize() -> bool {
    logger_->info("Initializing INDI filterwheel: {}", name_);
    // Initialize filter capabilities
    FilterWheelCapabilities caps;
    caps.maxFilters = 8;
    caps.canRename = true;
    caps.hasNames = true;
    caps.hasTemperature = false;
    caps.canAbort = true;
    setFilterWheelCapabilities(caps);
    
    return true;
}

auto INDIFilterwheelBase::destroy() -> bool {
    logger_->info("Destroying INDI filterwheel: {}", name_);
    if (isConnected()) {
        disconnect();
    }
    return true;
}

auto INDIFilterwheelBase::isConnected() const -> bool {
    return isConnected_.load();
}

auto INDIFilterwheelBase::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    if (isConnected_.load()) {
        logger_->error("{} is already connected.", deviceName);
        return false;
    }

    deviceName_ = deviceName;
    logger_->info("Connecting to {}...", deviceName_);
    
    // Watch for device and set up property watchers
    watchDevice(deviceName_.c_str(), [this](INDI::BaseDevice device) {
        device_ = device;
        setupPropertyWatchers();
    });

    return true;
}

auto INDIFilterwheelBase::disconnect() -> bool {
    if (!isConnected_.load()) {
        logger_->warn("Device {} is not connected", deviceName_);
        return false;
    }

    try {
        logger_->info("Disconnecting from {}...", deviceName_);
        disconnectDevice(deviceName_.c_str());
        device_ = INDI::BaseDevice();
        isConnected_.store(false);
        logger_->info("Successfully disconnected from {}", deviceName_);
        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to disconnect from {}: {}", deviceName_, e.what());
        return false;
    }
}

auto INDIFilterwheelBase::scan() -> std::vector<std::string> {
    logger_->info("Scanning for filter wheel devices...");
    std::vector<std::string> devices;
    logger_->debug("Device scanning not implemented - use INDI client tools");
    return devices;
}

auto INDIFilterwheelBase::watchAdditionalProperty() -> bool {
    logger_->debug("Watching additional properties");
    return true;
}

void INDIFilterwheelBase::setPropertyNumber(std::string_view propertyName, double value) {
    if (!device_.isValid()) {
        logger_->error("Device not valid for property setting");
        return;
    }
    
    INDI::PropertyNumber property = device_.getProperty(propertyName.data());
    if (!property.isValid()) {
        logger_->error("Property {} not found", propertyName);
        return;
    }
    
    property[0].value = value;
    sendNewProperty(property);
}

void INDIFilterwheelBase::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    auto message = baseDevice.messageQueue(messageID);
    logger_->info("Message from {}: {}", baseDevice.getDeviceName(), message);
}

void INDIFilterwheelBase::setupPropertyWatchers() {
    logger_->debug("Setting up property watchers for {}", deviceName_);

    // Connection property
    device_.watchProperty("CONNECTION",
        [this](INDI::Property) {
            logger_->info("Connecting to {}...", deviceName_);
            connectDevice(name_.c_str());
        },
        INDI::BaseDevice::WATCH_NEW);

    device_.watchProperty("CONNECTION",
        [this](const INDI::PropertySwitch &property) {
            handleConnectionProperty(property);
        },
        INDI::BaseDevice::WATCH_UPDATE);

    // Driver info
    device_.watchProperty("DRIVER_INFO",
        [this](const INDI::PropertyText &property) {
            handleDriverInfoProperty(property);
        },
        INDI::BaseDevice::WATCH_NEW);

    // Debug
    device_.watchProperty("DEBUG",
        [this](const INDI::PropertySwitch &property) {
            handleDebugProperty(property);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    // Polling period
    device_.watchProperty("POLLING_PERIOD",
        [this](const INDI::PropertyNumber &property) {
            handlePollingProperty(property);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    // Device auto search
    device_.watchProperty("DEVICE_AUTO_SEARCH",
        [this](const INDI::PropertySwitch &property) {
            if (property.isValid()) {
                deviceAutoSearch_ = property[0].getState() == ISS_ON;
                logger_->info("Auto search is {}", deviceAutoSearch_ ? "ON" : "OFF");
            }
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    // Device port scan
    device_.watchProperty("DEVICE_PORT_SCAN",
        [this](const INDI::PropertySwitch &property) {
            if (property.isValid()) {
                devicePortScan_ = property[0].getState() == ISS_ON;
                logger_->info("Device port scan is {}", devicePortScan_ ? "ON" : "OFF");
            }
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    // Filter slot
    device_.watchProperty("FILTER_SLOT",
        [this](const INDI::PropertyNumber &property) {
            handleFilterSlotProperty(property);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

    // Filter names
    device_.watchProperty("FILTER_NAME",
        [this](const INDI::PropertyText &property) {
            handleFilterNameProperty(property);
        },
        INDI::BaseDevice::WATCH_NEW_OR_UPDATE);
}

void INDIFilterwheelBase::handleConnectionProperty(const INDI::PropertySwitch &property) {
    isConnected_ = property[0].getState() == ISS_ON;
    if (isConnected_.load()) {
        logger_->info("{} is connected.", deviceName_);
    } else {
        logger_->info("{} is disconnected.", deviceName_);
    }
}

void INDIFilterwheelBase::handleDriverInfoProperty(const INDI::PropertyText &property) {
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
}

void INDIFilterwheelBase::handleDebugProperty(const INDI::PropertySwitch &property) {
    if (property.isValid()) {
        isDebug_.store(property[0].getState() == ISS_ON);
        logger_->info("Debug is {}", isDebug_.load() ? "ON" : "OFF");
    }
}

void INDIFilterwheelBase::handlePollingProperty(const INDI::PropertyNumber &property) {
    if (property.isValid()) {
        auto period = property[0].getValue();
        logger_->info("Current polling period: {}", period);
        if (period != currentPollingPeriod_.load()) {
            logger_->info("Polling period changed to: {}", period);
            currentPollingPeriod_ = period;
        }
    }
}

void INDIFilterwheelBase::handleFilterSlotProperty(const INDI::PropertyNumber &property) {
    if (property.isValid()) {
        logger_->info("Current filter slot: {}", property[0].getValue());
        currentSlot_ = static_cast<int>(property[0].getValue());
        maxSlot_ = static_cast<int>(property[0].getMax());
        minSlot_ = static_cast<int>(property[0].getMin());
        
        int slotIndex = currentSlot_.load();
        if (slotIndex >= 0 && slotIndex < static_cast<int>(slotNames_.size())) {
            currentSlotName_ = slotNames_[slotIndex];
            logger_->info("Current filter slot name: {}", currentSlotName_);
        }
    }
}

void INDIFilterwheelBase::handleFilterNameProperty(const INDI::PropertyText &property) {
    if (property.isValid()) {
        slotNames_.clear();
        for (const auto &filter : property) {
            logger_->info("Filter name: {}", filter.getText());
            slotNames_.emplace_back(filter.getText());
        }
    }
}
