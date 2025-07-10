/*
 * controller.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Modular ASCOM Filter Wheel Controller Implementation

*************************************************/

#include "controller.hpp"

#include <spdlog/spdlog.h>

#include "components/calibration_system.hpp"
#include "components/configuration_manager.hpp"
#include "components/hardware_interface.hpp"
#include "components/monitoring_system.hpp"
#include "components/position_manager.hpp"

namespace lithium::device::ascom::filterwheel {

ASCOMFilterwheelController::ASCOMFilterwheelController(std::string name)
    : AtomFilterWheel(std::move(name)) {
    spdlog::info("ASCOMFilterwheelController constructor called with name: {}",
                 getName());
}

ASCOMFilterwheelController::~ASCOMFilterwheelController() {
    spdlog::info("ASCOMFilterwheelController destructor called");
    destroy();
}

auto ASCOMFilterwheelController::initialize() -> bool {
    spdlog::info("Initializing ASCOM FilterWheel Controller");

    if (is_initialized_.load()) {
        spdlog::warn("Controller already initialized");
        return true;
    }

    if (!initializeComponents()) {
        setError("Failed to initialize controller components");
        return false;
    }

    is_initialized_.store(true);
    spdlog::info("ASCOM FilterWheel Controller initialized successfully");
    return true;
}

auto ASCOMFilterwheelController::destroy() -> bool {
    spdlog::info("Destroying ASCOM FilterWheel Controller");

    if (!is_initialized_.load()) {
        return true;
    }

    disconnect();
    destroyComponents();
    is_initialized_.store(false);

    spdlog::info("ASCOM FilterWheel Controller destroyed successfully");
    return true;
}

auto ASCOMFilterwheelController::connect(const std::string& deviceName,
                                         int timeout, int maxRetry) -> bool {
    if (!is_initialized_.load()) {
        setError("Controller not initialized");
        return false;
    }

    if (!hardware_interface_) {
        setError("Hardware interface not available");
        return false;
    }

    spdlog::info("Connecting to ASCOM filterwheel device: {}", deviceName);

    // Determine connection type and delegate to hardware interface
    bool success = hardware_interface_->connect(deviceName);
    if (success && monitoring_system_) {
        monitoring_system_->startMonitoring();
    }

    return success;
}

auto ASCOMFilterwheelController::disconnect() -> bool {
    spdlog::info("Disconnecting ASCOM FilterWheel");

    if (monitoring_system_) {
        monitoring_system_->stopMonitoring();
    }

    if (hardware_interface_) {
        return hardware_interface_->disconnect();
    }

    return true;
}

auto ASCOMFilterwheelController::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM filterwheel devices");

    std::vector<std::string> devices;

    if (hardware_interface_) {
        devices = hardware_interface_->scanDevices();
    }

    return devices;
}

auto ASCOMFilterwheelController::isConnected() const -> bool {
    return hardware_interface_ ? hardware_interface_->isConnected() : false;
}

auto ASCOMFilterwheelController::isMoving() const -> bool {
    return position_manager_ ? position_manager_->isMoving() : false;
}

auto ASCOMFilterwheelController::getPosition() -> std::optional<int> {
    return position_manager_ ? position_manager_->getCurrentPosition()
                             : std::nullopt;
}

auto ASCOMFilterwheelController::setPosition(int position) -> bool {
    return position_manager_ ? position_manager_->moveToPosition(position)
                             : false;
}

auto ASCOMFilterwheelController::getFilterCount() -> int {
    return position_manager_ ? position_manager_->getFilterCount() : 0;
}

auto ASCOMFilterwheelController::isValidPosition(int position) -> bool {
    return position_manager_ ? position_manager_->isValidPosition(position)
                             : false;
}

auto ASCOMFilterwheelController::getSlotName(int slot)
    -> std::optional<std::string> {
    return configuration_manager_ ? configuration_manager_->getFilterName(slot)
                                  : std::nullopt;
}

auto ASCOMFilterwheelController::setSlotName(int slot, const std::string& name)
    -> bool {
    return configuration_manager_
               ? configuration_manager_->setFilterName(slot, name)
               : false;
}

auto ASCOMFilterwheelController::getAllSlotNames() -> std::vector<std::string> {
    if (!configuration_manager_) {
        return {};
    }

    std::vector<std::string> names;
    int count = getFilterCount();
    for (int i = 0; i < count; ++i) {
        auto name = configuration_manager_->getFilterName(i);
        names.push_back(name ? *name : ("Filter " + std::to_string(i + 1)));
    }

    return names;
}

auto ASCOMFilterwheelController::getCurrentFilterName() -> std::string {
    auto position = getPosition();
    if (!position) {
        return "Unknown";
    }

    auto name = getSlotName(*position);
    return name ? *name : ("Filter " + std::to_string(*position + 1));
}

auto ASCOMFilterwheelController::getFilterInfo(int slot)
    -> std::optional<FilterInfo> {
    return configuration_manager_ ? configuration_manager_->getFilterInfo(slot)
                                  : std::nullopt;
}

auto ASCOMFilterwheelController::setFilterInfo(int slot, const FilterInfo& info)
    -> bool {
    return configuration_manager_
               ? configuration_manager_->setFilterInfo(slot, info)
               : false;
}

auto ASCOMFilterwheelController::getAllFilterInfo() -> std::vector<FilterInfo> {
    if (!configuration_manager_) {
        return {};
    }

    std::vector<FilterInfo> filters;
    int count = getFilterCount();
    for (int i = 0; i < count; ++i) {
        auto info = configuration_manager_->getFilterInfo(i);
        if (info) {
            filters.push_back(*info);
        }
    }

    return filters;
}

auto ASCOMFilterwheelController::findFilterByName(const std::string& name)
    -> std::optional<int> {
    return configuration_manager_
               ? configuration_manager_->findFilterByName(name)
               : std::nullopt;
}

auto ASCOMFilterwheelController::findFilterByType(const std::string& type)
    -> std::vector<int> {
    return configuration_manager_
               ? configuration_manager_->findFiltersByType(type)
               : std::vector<int>{};
}

auto ASCOMFilterwheelController::selectFilterByName(const std::string& name)
    -> bool {
    auto position = findFilterByName(name);
    return position ? setPosition(*position) : false;
}

auto ASCOMFilterwheelController::selectFilterByType(const std::string& type)
    -> bool {
    auto matches = findFilterByType(type);
    return !matches.empty() ? setPosition(matches[0]) : false;
}

auto ASCOMFilterwheelController::abortMotion() -> bool {
    return position_manager_ ? position_manager_->abortMovement() : false;
}

auto ASCOMFilterwheelController::homeFilterWheel() -> bool {
    return position_manager_ ? position_manager_->homeFilterWheel() : false;
}

auto ASCOMFilterwheelController::calibrateFilterWheel() -> bool {
    return calibration_system_
               ? calibration_system_->performQuickCalibration().status ==
                     components::CalibrationStatus::COMPLETED
               : false;
}

auto ASCOMFilterwheelController::getTemperature() -> std::optional<double> {
    return hardware_interface_ ? hardware_interface_->getTemperature()
                               : std::nullopt;
}

auto ASCOMFilterwheelController::hasTemperatureSensor() -> bool {
    return hardware_interface_ ? hardware_interface_->hasTemperatureSensor()
                               : false;
}

auto ASCOMFilterwheelController::getTotalMoves() -> uint64_t {
    return position_manager_ ? position_manager_->getTotalMoves() : 0;
}

auto ASCOMFilterwheelController::resetTotalMoves() -> bool {
    if (position_manager_) {
        position_manager_->resetMoveCounter();
        return true;
    }
    return false;
}

auto ASCOMFilterwheelController::getLastMoveTime() -> int {
    if (position_manager_) {
        return static_cast<int>(position_manager_->getLastMoveTime().count());
    }
    return 0;
}

auto ASCOMFilterwheelController::saveFilterConfiguration(
    const std::string& name) -> bool {
    return configuration_manager_ ? configuration_manager_->createProfile(name)
                                  : false;
}

auto ASCOMFilterwheelController::loadFilterConfiguration(
    const std::string& name) -> bool {
    return configuration_manager_ ? configuration_manager_->loadProfile(name)
                                  : false;
}

auto ASCOMFilterwheelController::deleteFilterConfiguration(
    const std::string& name) -> bool {
    return configuration_manager_ ? configuration_manager_->deleteProfile(name)
                                  : false;
}

auto ASCOMFilterwheelController::getAvailableConfigurations()
    -> std::vector<std::string> {
    return configuration_manager_
               ? configuration_manager_->getAvailableProfiles()
               : std::vector<std::string>{};
}

// ASCOM-specific functionality
auto ASCOMFilterwheelController::getASCOMDriverInfo()
    -> std::optional<std::string> {
    return hardware_interface_ ? hardware_interface_->getDriverInfo()
                               : std::nullopt;
}

auto ASCOMFilterwheelController::getASCOMVersion()
    -> std::optional<std::string> {
    return hardware_interface_ ? hardware_interface_->getDriverVersion()
                               : std::nullopt;
}

auto ASCOMFilterwheelController::getASCOMInterfaceVersion()
    -> std::optional<int> {
    return hardware_interface_ ? hardware_interface_->getInterfaceVersion()
                               : std::nullopt;
}

auto ASCOMFilterwheelController::setASCOMClientID(const std::string& clientId)
    -> bool {
    return hardware_interface_ ? hardware_interface_->setClientID(clientId)
                               : false;
}

auto ASCOMFilterwheelController::getASCOMClientID()
    -> std::optional<std::string> {
    // This would need to be implemented in hardware interface
    return std::nullopt;
}

auto ASCOMFilterwheelController::connectToCOMDriver(const std::string& progId)
    -> bool {
    return hardware_interface_ ? hardware_interface_->connectToCOM(progId)
                               : false;
}

auto ASCOMFilterwheelController::connectToAlpacaDevice(const std::string& host,
                                                       int port,
                                                       int deviceNumber)
    -> bool {
    return hardware_interface_
               ? hardware_interface_->connectToAlpaca(host, port, deviceNumber)
               : false;
}

auto ASCOMFilterwheelController::discoverAlpacaDevices()
    -> std::vector<std::string> {
    return hardware_interface_ ? hardware_interface_->discoverAlpacaDevices()
                               : std::vector<std::string>{};
}

auto ASCOMFilterwheelController::performSelfTest() -> bool {
    return calibration_system_
               ? calibration_system_->performQuickCalibration().status ==
                     components::CalibrationStatus::COMPLETED
               : false;
}

auto ASCOMFilterwheelController::getConnectionType() -> std::string {
    if (!hardware_interface_) {
        return "None";
    }

    switch (hardware_interface_->getConnectionType()) {
        case components::ConnectionType::COM_DRIVER:
            return "COM Driver";
        case components::ConnectionType::ALPACA_REST:
            return "Alpaca REST";
        default:
            return "Unknown";
    }
}

auto ASCOMFilterwheelController::getConnectionStatus() -> std::string {
    return isConnected() ? "Connected" : "Disconnected";
}

auto ASCOMFilterwheelController::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(error_mutex_);
    return last_error_;
}

auto ASCOMFilterwheelController::clearError() -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
}

// Private implementation methods
auto ASCOMFilterwheelController::initializeComponents() -> bool {
    try {
        // Create components in dependency order
        hardware_interface_ = std::make_unique<components::HardwareInterface>();
        if (!hardware_interface_->initialize()) {
            setError("Failed to initialize hardware interface");
            return false;
        }

        position_manager_ = std::make_unique<components::PositionManager>(
            std::shared_ptr<components::HardwareInterface>(
                hardware_interface_.get(), [](auto*) {}));
        if (!position_manager_->initialize()) {
            setError("Failed to initialize position manager");
            return false;
        }

        configuration_manager_ =
            std::make_unique<components::ConfigurationManager>();
        if (!configuration_manager_->initialize()) {
            setError("Failed to initialize configuration manager");
            return false;
        }

        monitoring_system_ = std::make_unique<components::MonitoringSystem>(
            std::shared_ptr<components::HardwareInterface>(
                hardware_interface_.get(), [](auto*) {}),
            std::shared_ptr<components::PositionManager>(
                position_manager_.get(), [](auto*) {}));
        if (!monitoring_system_->initialize()) {
            setError("Failed to initialize monitoring system");
            return false;
        }

        calibration_system_ = std::make_unique<components::CalibrationSystem>(
            std::shared_ptr<components::HardwareInterface>(
                hardware_interface_.get(), [](auto*) {}),
            std::shared_ptr<components::PositionManager>(
                position_manager_.get(), [](auto*) {}),
            std::shared_ptr<components::MonitoringSystem>(
                monitoring_system_.get(), [](auto*) {}));
        if (!calibration_system_->initialize()) {
            setError("Failed to initialize calibration system");
            return false;
        }

        alpaca_client_ = std::make_unique<components::AlpacaClient>();
        if (!alpaca_client_->initialize()) {
            setError("Failed to initialize Alpaca client");
            return false;
        }

#ifdef _WIN32
        com_helper_ = std::make_unique<components::COMHelper>();
        if (!com_helper_->initialize()) {
            setError("Failed to initialize COM helper");
            return false;
        }
#endif

        return true;
    } catch (const std::exception& e) {
        setError("Exception during component initialization: " +
                 std::string(e.what()));
        return false;
    }
}

auto ASCOMFilterwheelController::destroyComponents() -> void {
    // Destroy components in reverse order
    calibration_system_.reset();
    monitoring_system_.reset();
    configuration_manager_.reset();
    position_manager_.reset();
    hardware_interface_.reset();
}

auto ASCOMFilterwheelController::checkComponentHealth() -> bool {
    return hardware_interface_ && position_manager_ && configuration_manager_ &&
           monitoring_system_ && calibration_system_;
}

auto ASCOMFilterwheelController::setError(const std::string& error) -> void {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = error;
    spdlog::error("ASCOMFilterwheelController error: {}", error);
}

}  // namespace lithium::device::ascom::filterwheel
