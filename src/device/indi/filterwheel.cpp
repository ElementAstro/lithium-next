#include "filterwheel.hpp"

#include <optional>
#include <thread>
#include <tuple>
#include <chrono>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "atom/utils/qtimer.hpp"
#include "atom/components/component.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#endif

INDIFilterwheel::INDIFilterwheel(std::string name) : AtomFilterWheel(name) {
    logger_ = spdlog::get("filterwheel_indi") 
                  ? spdlog::get("filterwheel_indi")
                  : spdlog::stdout_color_mt("filterwheel_indi");
}

auto INDIFilterwheel::initialize() -> bool {
    // Implement initialization logic here
    return true;
}

auto INDIFilterwheel::destroy() -> bool {
    // Implement destruction logic here
    return true;
}

auto INDIFilterwheel::isConnected() const -> bool {
    return isConnected_.load();
}

auto INDIFilterwheel::connect(const std::string &deviceName, int timeout,
                              int maxRetry) -> bool {
    if (isConnected_.load()) {
        logger_->error("{} is already connected.", deviceName_);
        return false;
    }

    deviceName_ = deviceName;
    logger_->info("Connecting to {}...", deviceName_);
    // Max: need to get initial parameters and then register corresponding
    // callback functions
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

        // Max: this parameter is actually quite important, but except for
        // planetary cameras, it does not need to be adjusted, the default is
        // fine
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
            "FILTER_SLOT",
            [this](const INDI::PropertyNumber &property) {
                if (property.isValid()) {
                    logger_->info("Current filter slot: {}",
                          property[0].getValue());
                    currentSlot_ = property[0].getValue();
                    maxSlot_ = property[0].getMax();
                    minSlot_ = property[0].getMin();
                    currentSlotName_ =
                        slotNames_[static_cast<int>(property[0].getValue())];
                    logger_->info("Current filter slot name: {}",
                          currentSlotName_);
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);

        device_.watchProperty(
            "FILTER_NAME",
            [this](const INDI::PropertyText &property) {
                if (property.isValid()) {
                    slotNames_.clear();
                    for (const auto &filter : property) {
                        logger_->info("Filter name: {}", filter.getText());
                        slotNames_.emplace_back(filter.getText());
                    }
                }
            },
            INDI::BaseDevice::WATCH_NEW_OR_UPDATE);
    });

    return true;
}

auto INDIFilterwheel::disconnect() -> bool {
    if (!isConnected_.load()) {
        logger_->warn("Device {} is not connected", deviceName_);
        return false;
    }

    try {
        logger_->info("Disconnecting from {}...", deviceName_);
        
        // Disconnect from the device
        disconnectDevice(deviceName_.c_str());
        
        // Clear device state
        device_ = INDI::BaseDevice();
        isConnected_.store(false);
        
        logger_->info("Successfully disconnected from {}", deviceName_);
        return true;
    } catch (const std::exception& e) {
        logger_->error("Failed to disconnect from {}: {}", deviceName_, e.what());
        return false;
    }
}

auto INDIFilterwheel::watchAdditionalProperty() -> bool {
    // Implement additional property watching logic here
    return true;
}

void INDIFilterwheel::setPropertyNumber(std::string_view propertyName,
                                        double value) {
    // Implement setting property number logic here
}

auto INDIFilterwheel::getPositionDetails()
    -> std::optional<std::tuple<double, double, double>> {
    INDI::PropertyNumber property = device_.getProperty("FILTER_SLOT");
    if (!property.isValid()) {
        logger_->error("Unable to find FILTER_SLOT property...");
        return std::nullopt;
    }
    return std::make_tuple(property[0].getValue(), property[0].getMin(),
                           property[0].getMax());
}

auto INDIFilterwheel::setPosition(int position) -> bool {
    INDI::PropertyNumber property = device_.getProperty("FILTER_SLOT");
    if (!property.isValid()) {
        logger_->error("Unable to find FILTER_SLOT property...");
        return false;
    }
    property[0].value = position;
    sendNewProperty(property);
    atom::utils::ElapsedTimer t;
    t.start();
    int timeout = 10000;
    while (t.elapsed() < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (property.getState() == IPS_OK) {
            break;  // it will not wait the motor arrived
        }
    }
    if (t.elapsed() > timeout) {
        logger_->error("setPosition | ERROR : timeout ");
        return false;
    }
    
    // Update statistics
    total_moves_++;
    last_move_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    return true;
}

// Implementation of AtomFilterWheel interface methods

auto INDIFilterwheel::isMoving() const -> bool {
    return filterwheel_state_ == FilterWheelState::MOVING;
}

auto INDIFilterwheel::getFilterCount() -> int {
    return slotNames_.size();
}

auto INDIFilterwheel::isValidPosition(int position) -> bool {
    return position >= minSlot_ && position <= maxSlot_;
}

auto INDIFilterwheel::getSlotName(int slot) -> std::optional<std::string> {
    if (!isValidSlot(slot) || slot >= static_cast<int>(slotNames_.size())) {
        logger_->error("Invalid slot index: {}", slot);
        return std::nullopt;
    }
    return slotNames_[slot];
}

auto INDIFilterwheel::setSlotName(int slot, const std::string& name) -> bool {
    if (!isValidSlot(slot)) {
        logger_->error("Invalid slot index: {}", slot);
        return false;
    }
    
    INDI::PropertyText property = device_.getProperty("FILTER_NAME");
    if (!property.isValid()) {
        logger_->error("Unable to find FILTER_NAME property");
        return false;
    }
    
    if (slot < static_cast<int>(property.size())) {
        property[slot].setText(name.c_str());
        sendNewProperty(property);
        
        // Update local cache
        if (slot < static_cast<int>(slotNames_.size())) {
            slotNames_[slot] = name;
        }
        return true;
    }
    
    logger_->error("Slot {} out of range for property", slot);
    return false;
}

auto INDIFilterwheel::getAllSlotNames() -> std::vector<std::string> {
    return slotNames_;
}

auto INDIFilterwheel::getCurrentFilterName() -> std::string {
    int currentPos = currentSlot_.load();
    if (currentPos >= 0 && currentPos < static_cast<int>(slotNames_.size())) {
        return slotNames_[currentPos];
    }
    return "Unknown";
}

auto INDIFilterwheel::getFilterInfo(int slot) -> std::optional<FilterInfo> {
    if (!isValidSlot(slot)) {
        logger_->error("Invalid slot index: {}", slot);
        return std::nullopt;
    }
    
    // For now, return basic info based on slot name
    // This could be enhanced to store more detailed filter information
    FilterInfo info;
    if (slot < static_cast<int>(slotNames_.size())) {
        info.name = slotNames_[slot];
        info.type = "Unknown";
        info.wavelength = 0.0;
        info.bandwidth = 0.0;
        info.description = "Filter at slot " + std::to_string(slot);
    }
    
    return info;
}

auto INDIFilterwheel::setFilterInfo(int slot, const FilterInfo& info) -> bool {
    if (!isValidSlot(slot)) {
        logger_->error("Invalid slot index: {}", slot);
        return false;
    }
    
    // Store the filter info in the protected array
    if (slot < MAX_FILTERS) {
        filters_[slot] = info;
        
        // Also update the slot name if it's different
        if (slot < static_cast<int>(slotNames_.size()) && slotNames_[slot] != info.name) {
            return setSlotName(slot, info.name);
        }
        return true;
    }
    
    return false;
}

auto INDIFilterwheel::getAllFilterInfo() -> std::vector<FilterInfo> {
    std::vector<FilterInfo> infos;
    for (int i = 0; i < getFilterCount(); ++i) {
        auto info = getFilterInfo(i);
        if (info) {
            infos.push_back(*info);
        }
    }
    return infos;
}

auto INDIFilterwheel::findFilterByName(const std::string& name) -> std::optional<int> {
    for (int i = 0; i < static_cast<int>(slotNames_.size()); ++i) {
        if (slotNames_[i] == name) {
            return i;
        }
    }
    return std::nullopt;
}

auto INDIFilterwheel::findFilterByType(const std::string& type) -> std::vector<int> {
    std::vector<int> matches;
    for (int i = 0; i < MAX_FILTERS && i < static_cast<int>(slotNames_.size()); ++i) {
        if (filters_[i].type == type) {
            matches.push_back(i);
        }
    }
    return matches;
}

auto INDIFilterwheel::selectFilterByName(const std::string& name) -> bool {
    auto slot = findFilterByName(name);
    if (slot) {
        return setPosition(*slot);
    }
    logger_->error("Filter '{}' not found", name);
    return false;
}

auto INDIFilterwheel::selectFilterByType(const std::string& type) -> bool {
    auto slots = findFilterByType(type);
    if (!slots.empty()) {
        return setPosition(slots[0]);  // Select first match
    }
    logger_->error("No filter of type '{}' found", type);
    return false;
}

auto INDIFilterwheel::abortMotion() -> bool {
    INDI::PropertySwitch property = device_.getProperty("FILTER_ABORT_MOTION");
    if (!property.isValid()) {
        logger_->warn("FILTER_ABORT_MOTION property not available");
        return false;
    }
    
    property[0].s = ISS_ON;
    sendNewProperty(property);
    
    updateFilterWheelState(FilterWheelState::IDLE);
    logger_->info("Filter wheel motion aborted");
    return true;
}

auto INDIFilterwheel::homeFilterWheel() -> bool {
    INDI::PropertySwitch property = device_.getProperty("FILTER_HOME");
    if (!property.isValid()) {
        logger_->warn("FILTER_HOME property not available");
        return false;
    }
    
    property[0].s = ISS_ON;
    sendNewProperty(property);
    
    updateFilterWheelState(FilterWheelState::MOVING);
    logger_->info("Homing filter wheel...");
    return true;
}

auto INDIFilterwheel::calibrateFilterWheel() -> bool {
    INDI::PropertySwitch property = device_.getProperty("FILTER_CALIBRATE");
    if (!property.isValid()) {
        logger_->warn("FILTER_CALIBRATE property not available");
        return false;
    }
    
    property[0].s = ISS_ON;
    sendNewProperty(property);
    
    updateFilterWheelState(FilterWheelState::MOVING);
    logger_->info("Calibrating filter wheel...");
    return true;
}

auto INDIFilterwheel::getTemperature() -> std::optional<double> {
    INDI::PropertyNumber property = device_.getProperty("FILTER_TEMPERATURE");
    if (!property.isValid()) {
        return std::nullopt;
    }
    
    return property[0].getValue();
}

auto INDIFilterwheel::hasTemperatureSensor() -> bool {
    INDI::PropertyNumber property = device_.getProperty("FILTER_TEMPERATURE");
    return property.isValid();
}

auto INDIFilterwheel::getTotalMoves() -> uint64_t {
    return total_moves_;
}

auto INDIFilterwheel::resetTotalMoves() -> bool {
    total_moves_ = 0;
    logger_->info("Total moves counter reset");
    return true;
}

auto INDIFilterwheel::getLastMoveTime() -> int {
    return last_move_time_;
}

auto INDIFilterwheel::saveFilterConfiguration(const std::string& name) -> bool {
    // This would typically save configuration to a file or database
    logger_->info("Saving filter configuration: {}", name);
    // Placeholder implementation
    return true;
}

auto INDIFilterwheel::loadFilterConfiguration(const std::string& name) -> bool {
    // This would typically load configuration from a file or database
    logger_->info("Loading filter configuration: {}", name);
    // Placeholder implementation
    return true;
}

auto INDIFilterwheel::deleteFilterConfiguration(const std::string& name) -> bool {
    // This would typically delete configuration from a file or database
    logger_->info("Deleting filter configuration: {}", name);
    // Placeholder implementation
    return true;
}

auto INDIFilterwheel::getAvailableConfigurations() -> std::vector<std::string> {
    // This would typically return available configurations from storage
    logger_->debug("Getting available configurations");
    // Placeholder implementation
    return std::vector<std::string>{};
}



auto INDIFilterwheel::scan() -> std::vector<std::string> {
    logger_->info("Scanning for filter wheel devices...");
    std::vector<std::string> devices;
    
    // This is a placeholder implementation - actual scanning would need to
    // interact with INDI server to discover available filter wheel devices
    // For now, return empty vector as scanning is typically handled by the client
    logger_->debug("Device scanning not implemented - use INDI client tools");
    
    return devices;
}

void INDIFilterwheel::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    auto message = baseDevice.messageQueue(messageID);
    logger_->info("Message from {}: {}", baseDevice.getDeviceName(), message);
}

ATOM_MODULE(filterwheel_indi, [](Component &component) {
    auto logger = spdlog::get("filterwheel_indi") 
                      ? spdlog::get("filterwheel_indi")
                      : spdlog::stdout_color_mt("filterwheel_indi");
    
    logger->info("Registering filterwheel_indi module...");
    component.def("connect", &INDIFilterwheel::connect, "device",
                  "Connect to a filterwheel device.");
    component.def("disconnect", &INDIFilterwheel::disconnect, "device",
                  "Disconnect from a filterwheel device.");
    component.def("scan", &INDIFilterwheel::scan,
                  "Scan for filterwheel devices.");
    component.def("is_connected", &INDIFilterwheel::isConnected,
                  "Check if a filterwheel device is connected.");

    component.def("initialize", &INDIFilterwheel::initialize, "device",
                  "Initialize a filterwheel device.");
    component.def("destroy", &INDIFilterwheel::destroy, "device",
                  "Destroy a filterwheel device.");

    component.def("get_position", &INDIFilterwheel::getPosition, "device",
                  "Get the current filter position.");
    component.def("get_position_details", &INDIFilterwheel::getPositionDetails, "device",
                  "Get detailed filter position information.");
    component.def("set_position", &INDIFilterwheel::setPosition, "device",
                  "Set the current filter position.");
    component.def("get_slot_name", 
                  static_cast<std::optional<std::string>(INDIFilterwheel::*)(int)>(&INDIFilterwheel::getSlotName), 
                  "device", "Get the current filter slot name.");
    component.def("set_slot_name", 
                  static_cast<bool(INDIFilterwheel::*)(int, const std::string&)>(&INDIFilterwheel::setSlotName), 
                  "device", "Set the current filter slot name.");
    
    // Enhanced filter wheel methods
    component.def("is_moving", &INDIFilterwheel::isMoving, "device",
                  "Check if the filter wheel is moving.");
    component.def("get_filter_count", &INDIFilterwheel::getFilterCount, "device",
                  "Get the total number of filters.");
    component.def("get_current_filter_name", &INDIFilterwheel::getCurrentFilterName, "device",
                  "Get the current filter name.");
    component.def("select_filter_by_name", &INDIFilterwheel::selectFilterByName, "device",
                  "Select filter by name.");
    component.def("abort_motion", &INDIFilterwheel::abortMotion, "device",
                  "Abort filter wheel motion.");
    component.def("home_filter_wheel", &INDIFilterwheel::homeFilterWheel, "device",
                  "Home the filter wheel.");
    component.def("get_total_moves", &INDIFilterwheel::getTotalMoves, "device",
                  "Get total number of moves.");
    component.def("reset_total_moves", &INDIFilterwheel::resetTotalMoves, "device",
                  "Reset total moves counter.");

    component.def(
        "create_instance",
        [](const std::string &name) {
            std::shared_ptr<AtomFilterWheel> instance =
                std::make_shared<INDIFilterwheel>(name);
            return instance;
        },
        "device", "Create a new filterwheel instance.");
    component.defType<INDIFilterwheel>("filterwheel_indi", "device",
                                       "Define a new filterwheel instance.");

    logger->info("Registered filterwheel_indi module.");
});
