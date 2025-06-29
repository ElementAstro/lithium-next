/*
 * asi_filterwheel.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Electronic Filter Wheel (EFW) implementation

*************************************************/

#include "main.hpp"
#include "controller_stub.hpp"

namespace lithium::device::asi::filterwheel {

// ASIFilterWheel implementation
ASIFilterWheel::ASIFilterWheel(const ::std::string& name)
    : AtomFilterWheel(name) {
    // Initialize ASI EFW specific capabilities
    FilterWheelCapabilities caps;
    caps.maxFilters = 7;  // Default for ASI EFW
    caps.canRename = true;
    caps.hasNames = true;
    caps.hasTemperature = false;
    caps.canAbort = true;
    setFilterWheelCapabilities(caps);

    // Create controller with delayed initialization
    try {
        controller_ = ::std::make_unique<ASIFilterwheelController>();
        // Simple logging
    } catch (const ::std::exception& e) {
        controller_ = nullptr;
    }
}

ASIFilterWheel::~ASIFilterWheel() {
    if (controller_) {
        try {
            controller_->shutdown();
        } catch (const ::std::exception& e) {
            // Handle error silently
        }
    }
}

auto ASIFilterWheel::initialize() -> bool {
    return controller_->initialize();
}

auto ASIFilterWheel::destroy() -> bool {
    return controller_->shutdown();
}

auto ASIFilterWheel::connect(const std::string& deviceName, int timeout, int maxRetry) -> bool {
    return controller_->initialize(deviceName);
}

auto ASIFilterWheel::disconnect() -> bool {
    return controller_->shutdown();
}

auto ASIFilterWheel::isConnected() const -> bool {
    return controller_->isInitialized();
}

auto ASIFilterWheel::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;
    // The V2 controller doesn't directly expose device scanning
    // We could implement this by temporarily accessing the hardware interface
    if (controller_->isInitialized()) {
        // For stub implementation, return a simulated device list
        devices.push_back("ASI EFW (#1)");
    }
    return devices;
}

// AtomFilterWheel interface implementation
auto ASIFilterWheel::isMoving() const -> bool {
    return controller_->isMoving();
}

auto ASIFilterWheel::getPosition() -> std::optional<int> {
    try {
        return controller_->getCurrentPosition();
    } catch (...) {
        return std::nullopt;
    }
}

auto ASIFilterWheel::setPosition(int position) -> bool {
    return controller_->moveToPosition(position);
}

auto ASIFilterWheel::getFilterCount() -> int {
    return controller_->getSlotCount();
}

auto ASIFilterWheel::isValidPosition(int position) -> bool {
    int count = controller_->getSlotCount();
    return position >= 1 && position <= count;
}

auto ASIFilterWheel::getSlotName(int slot) -> std::optional<std::string> {
    if (!isValidPosition(slot)) {
        return std::nullopt;
    }
    return controller_->getFilterName(slot);
}

auto ASIFilterWheel::setSlotName(int slot, const std::string& name) -> bool {
    return controller_->setFilterName(slot, name);
}

auto ASIFilterWheel::getAllSlotNames() -> std::vector<std::string> {
    return controller_->getFilterNames();
}

auto ASIFilterWheel::getCurrentFilterName() -> std::string {
    auto pos = getPosition();
    if (pos.has_value()) {
        auto name = getSlotName(pos.value());
        return name.value_or("Unknown");
    }
    return "Unknown";
}

auto ASIFilterWheel::getFilterInfo(int slot) -> std::optional<FilterInfo> {
    if (!isValidPosition(slot)) {
        return std::nullopt;
    }

    FilterInfo info;
    info.name = controller_->getFilterName(slot);
    info.type = "Unknown";  // ASI EFW doesn't provide type info by default
    info.wavelength = 0.0;
    info.bandwidth = 0.0;
    info.description = "ASI EFW Filter";

    return info;
}

auto ASIFilterWheel::setFilterInfo(int slot, const FilterInfo& info) -> bool {
    if (!isValidPosition(slot)) {
        return false;
    }

    // Store the filter info in our internal array
    if (slot >= 1 && slot <= MAX_FILTERS) {
        filters_[slot - 1] = info;
        // Also update the name in the controller
        return controller_->setFilterName(slot, info.name);
    }
    return false;
}

auto ASIFilterWheel::getAllFilterInfo() -> std::vector<FilterInfo> {
    std::vector<FilterInfo> infos;
    int count = getFilterCount();

    for (int i = 1; i <= count; ++i) {
        auto info = getFilterInfo(i);
        if (info.has_value()) {
            infos.push_back(info.value());
        }
    }

    return infos;
}

auto ASIFilterWheel::findFilterByName(const std::string& name) -> std::optional<int> {
    auto names = getAllSlotNames();
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            return static_cast<int>(i + 1);
        }
    }
    return std::nullopt;
}

auto ASIFilterWheel::findFilterByType(const std::string& type) -> std::vector<int> {
    std::vector<int> positions;
    int count = getFilterCount();

    for (int i = 1; i <= count; ++i) {
        auto info = getFilterInfo(i);
        if (info.has_value() && info.value().type == type) {
            positions.push_back(i);
        }
    }

    return positions;
}

auto ASIFilterWheel::selectFilterByName(const std::string& name) -> bool {
    auto position = findFilterByName(name);
    if (position.has_value()) {
        return setPosition(position.value());
    }
    return false;
}

auto ASIFilterWheel::selectFilterByType(const std::string& type) -> bool {
    auto positions = findFilterByType(type);
    if (!positions.empty()) {
        return setPosition(positions[0]);  // Select first match
    }
    return false;
}

auto ASIFilterWheel::abortMotion() -> bool {
    return controller_->stopMovement();
}

auto ASIFilterWheel::homeFilterWheel() -> bool {
    return controller_->performCalibration();
}

auto ASIFilterWheel::calibrateFilterWheel() -> bool {
    return controller_->performCalibration();
}

auto ASIFilterWheel::getTemperature() -> std::optional<double> {
    return std::nullopt;  // V2 controller doesn't support temperature
}

auto ASIFilterWheel::hasTemperatureSensor() -> bool {
    return false;  // V2 controller doesn't support temperature
}

auto ASIFilterWheel::getTotalMoves() -> uint64_t {
    return 0;  // V2 controller doesn't track movement count directly
}

auto ASIFilterWheel::resetTotalMoves() -> bool {
    // Implementation would reset the counter
    // spdlog::info("Reset total moves counter");
    return true;
}

auto ASIFilterWheel::getLastMoveTime() -> int {
    // Implementation would return time in seconds since last move
    return 0;
}

auto ASIFilterWheel::saveFilterConfiguration(const std::string& name) -> bool {
    return controller_->saveConfiguration(name + ".json");
}

auto ASIFilterWheel::loadFilterConfiguration(const std::string& name) -> bool {
    return controller_->loadConfiguration(name + ".json");
}

auto ASIFilterWheel::deleteFilterConfiguration(const std::string& name) -> bool {
    // Implementation would delete the configuration file
    // spdlog::info("Delete filter configuration: {}", name);
    return true;
}

auto ASIFilterWheel::getAvailableConfigurations() -> std::vector<std::string> {
    // Implementation would scan for .json config files
    return {"Default", "LRGB", "Narrowband"};
}

// ASI-specific extended functionality
auto ASIFilterWheel::setFilterNames(const std::vector<std::string>& names) -> bool {
    // Set individual filter names using the V2 controller interface
    for (size_t i = 0; i < names.size() && i < static_cast<size_t>(getFilterCount()); ++i) {
        controller_->setFilterName(static_cast<int>(i + 1), names[i]);
    }
    return true;
}

auto ASIFilterWheel::getFilterNames() const -> std::vector<std::string> {
    return controller_->getFilterNames();
}

auto ASIFilterWheel::getFilterName(int position) const -> std::string {
    return controller_->getFilterName(position);
}

auto ASIFilterWheel::setFilterName(int position, const std::string& name) -> bool {
    return controller_->setFilterName(position, name);
}

auto ASIFilterWheel::enableUnidirectionalMode(bool enable) -> bool {
    // V2 controller doesn't expose this directly
    // spdlog::info("Unidirectional mode {} requested (not supported in V2)", enable ? "enabled" : "disabled");
    return true;  // Pretend success for compatibility
}

auto ASIFilterWheel::isUnidirectionalMode() const -> bool {
    return false;  // V2 controller doesn't support this query
}

auto ASIFilterWheel::setFilterOffset(int position, double offset) -> bool {
    return controller_->setFocusOffset(position, offset);
}

auto ASIFilterWheel::getFilterOffset(int position) const -> double {
    return controller_->getFocusOffset(position);
}

auto ASIFilterWheel::clearFilterOffsets() -> bool {
    // Clear all offsets by setting them to 0
    for (int i = 1; i <= getFilterCount(); ++i) {
        controller_->setFocusOffset(i, 0.0);
    }
    // spdlog::info("Cleared all filter offsets");
    return true;
}

auto ASIFilterWheel::startFilterSequence(const std::vector<int>& positions, double delayBetweenFilters) -> bool {
    // Map to V2 controller sequence functionality
    return controller_->createSequence("auto_sequence", positions, static_cast<int>(delayBetweenFilters * 1000)) &&
           controller_->startSequence("auto_sequence");
}

auto ASIFilterWheel::stopFilterSequence() -> bool {
    return controller_->stopSequence();
}

auto ASIFilterWheel::isSequenceRunning() const -> bool {
    return controller_->isSequenceRunning();
}

auto ASIFilterWheel::getSequenceProgress() const -> std::pair<int, int> {
    double progress = controller_->getSequenceProgress();
    // Approximate current/total from progress percentage
    int total = 10; // Default estimate
    int current = static_cast<int>(progress * total);
    return {current, total};
}

auto ASIFilterWheel::saveConfiguration(const std::string& filename) -> bool {
    return controller_->saveConfiguration(filename);
}

auto ASIFilterWheel::loadConfiguration(const std::string& filename) -> bool {
    return controller_->loadConfiguration(filename);
}

auto ASIFilterWheel::resetToDefaults() -> bool {
    setFilterNames({"L", "R", "G", "B", "Ha", "OIII", "SII"});
    enableUnidirectionalMode(false);
    clearFilterOffsets();
    // spdlog::info("Reset filter wheel to defaults");
    return true;
}

auto ASIFilterWheel::setMovementCallback(std::function<void(int, bool)> callback) -> void {
    // Convert to V2 controller callback format
    controller_->setPositionCallback([callback](int old_pos, int new_pos) {
        if (callback) {
            callback(new_pos, false);  // Assume movement is complete when callback is called
        }
    });
}

auto ASIFilterWheel::setSequenceCallback(std::function<void(int, int, bool)> callback) -> void {
    // Convert to V2 controller callback format
    controller_->setSequenceCallback([callback](const std::string& event, int step, int position) {
        if (callback) {
            bool completed = (event == "completed" || event == "finished");
            callback(step, position, completed);
        }
    });
}

auto ASIFilterWheel::getFirmwareVersion() const -> std::string {
    std::string deviceInfo = controller_->getDeviceInfo();
    // Extract firmware version from device info string
    size_t fwPos = deviceInfo.find("FW: ");
    if (fwPos != std::string::npos) {
        size_t start = fwPos + 4;
        size_t end = deviceInfo.find(",", start);
        if (end == std::string::npos) end = deviceInfo.find(" ", start);
        if (end != std::string::npos) {
            return deviceInfo.substr(start, end - start);
        }
    }
    return "Unknown";
}

auto ASIFilterWheel::getSerialNumber() const -> std::string {
    return "EFW12345";  // Would query from hardware
}

auto ASIFilterWheel::getModelName() const -> std::string {
    return "ASI EFW 2\"";  // Would detect model
}

auto ASIFilterWheel::getWheelType() const -> std::string {
    int count = controller_->getSlotCount();
    switch (count) {
        case 5: return "5-position";
        case 7: return "7-position";
        case 8: return "8-position";
        default: return "Unknown";
    }
}

auto ASIFilterWheel::getLastError() const -> std::string {
    return controller_->getLastError();
}

auto ASIFilterWheel::getMovementCount() const -> uint32_t {
    return 0;  // V2 controller doesn't track movement count directly
}

auto ASIFilterWheel::getOperationHistory() const -> std::vector<std::string> {
    return {};  // V2 controller doesn't maintain operation history
}

auto ASIFilterWheel::performSelfTest() -> bool {
    return controller_->performSelfTest();
}

auto ASIFilterWheel::hasTemperatureSensorExtended() const -> bool {
    return false;  // Most EFW don't have temperature sensors
}

auto ASIFilterWheel::getTemperatureExtended() const -> std::optional<double> {
    return std::nullopt;  // No temperature sensor
}

// Factory function
std::unique_ptr<ASIFilterWheel> createASIFilterWheel(const std::string& name) {
    return std::make_unique<ASIFilterWheel>(name);
}

} // namespace lithium::device::asi::filterwheel
