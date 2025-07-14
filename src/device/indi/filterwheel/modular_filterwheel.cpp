#include "modular_filterwheel.hpp"

namespace lithium::device::indi::filterwheel {

ModularINDIFilterWheel::ModularINDIFilterWheel(std::string name)
    : AtomFilterWheel(std::move(name)), core_(std::make_shared<INDIFilterWheelCore>(name_)) {

    core_->getLogger()->info("Creating modular INDI filterwheel: {}", name_);

    // Create component managers with shared core
    propertyManager_ = std::make_unique<PropertyManager>(core_);
    filterController_ = std::make_unique<FilterController>(core_);
    statisticsManager_ = std::make_unique<StatisticsManager>(core_);
    temperatureManager_ = std::make_unique<TemperatureManager>(core_);
    configurationManager_ = std::make_unique<ConfigurationManager>(core_);
    profiler_ = std::make_unique<FilterWheelProfiler>(core_);
}

bool ModularINDIFilterWheel::initialize() {
    core_->getLogger()->info("Initializing modular INDI filterwheel");
    return initializeComponents();
}

bool ModularINDIFilterWheel::destroy() {
    core_->getLogger()->info("Destroying modular INDI filterwheel");
    cleanupComponents();
    return true;
}

bool ModularINDIFilterWheel::connect(const std::string& deviceName, int timeout,
                                     int maxRetry) {
    if (core_->isConnected()) {
        core_->getLogger()->error("{} is already connected.", core_->getDeviceName());
        return false;
    }

    core_->setDeviceName(deviceName);
    core_->getLogger()->info("Connecting to {}...", deviceName);

    setupInitialConnection(deviceName);
    return true;
}

bool ModularINDIFilterWheel::disconnect() {
    if (!core_->isConnected()) {
        core_->getLogger()->warn("Device {} is not connected",
                              core_->getDeviceName());
        return false;
    }

    disconnectServer();
    core_->setConnected(false);
    core_->getLogger()->info("Disconnected from {}", core_->getDeviceName());
    return true;
}

std::vector<std::string> ModularINDIFilterWheel::scan() {
    // INDI doesn't provide a direct scan method
    // This would typically be handled by the INDI server
    core_->getLogger()->warn("Scan method not directly supported by INDI");
    return {};
}

bool ModularINDIFilterWheel::isConnected() const {
    return core_->isConnected();
}

// Filter control methods (delegated to FilterController)
std::optional<int> ModularINDIFilterWheel::getPosition() {
    return filterController_->getPosition();
}

bool ModularINDIFilterWheel::setPosition(int position) {
    int currentPosition = core_->getCurrentSlot();
    bool result = filterController_->setPosition(position);
    if (result) {
        statisticsManager_->recordPositionChange(currentPosition, position);
        // Record move time when move completes
        auto duration = filterController_->getLastMoveDuration();
        statisticsManager_->recordMoveTime(duration);
    }
    return result;
}

int ModularINDIFilterWheel::getFilterCount() {
    return filterController_->getMaxPosition();
}

bool ModularINDIFilterWheel::isValidPosition(int position) {
    return filterController_->isValidPosition(position);
}

bool ModularINDIFilterWheel::isMoving() const {
    return filterController_->isMoving();
}

bool ModularINDIFilterWheel::abortMotion() {
    return filterController_->abortMove();
}

// Filter information methods (delegated to FilterController)
std::optional<std::string> ModularINDIFilterWheel::getSlotName(int slot) {
    return filterController_->getFilterName(slot);
}

bool ModularINDIFilterWheel::setSlotName(int slot, const std::string& name) {
    return filterController_->setFilterName(slot, name);
}

std::vector<std::string> ModularINDIFilterWheel::getAllSlotNames() {
    return filterController_->getFilterNames();
}

std::string ModularINDIFilterWheel::getCurrentFilterName() {
    auto currentPos = getPosition();
    if (currentPos.has_value()) {
        auto name = getSlotName(currentPos.value());
        return name.value_or("Unknown");
    }
    return "Unknown";
}

// Enhanced filter management
std::optional<FilterInfo> ModularINDIFilterWheel::getFilterInfo(int slot) {
    auto name = getSlotName(slot);
    if (name.has_value()) {
        FilterInfo info;
        info.name = name.value();
        info.type = "Unknown"; // Could be extended to store more info
        return info;
    }
    return std::nullopt;
}

bool ModularINDIFilterWheel::setFilterInfo(int slot, const FilterInfo& info) {
    return setSlotName(slot, info.name);
}

std::vector<FilterInfo> ModularINDIFilterWheel::getAllFilterInfo() {
    std::vector<FilterInfo> infos;
    auto names = getAllSlotNames();
    for (size_t i = 0; i < names.size(); ++i) {
        FilterInfo info;
        info.name = names[i];
        info.type = "Unknown";
        infos.push_back(info);
    }
    return infos;
}

// Filter search and selection
std::optional<int> ModularINDIFilterWheel::findFilterByName(const std::string& name) {
    auto names = getAllSlotNames();
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) {
            return static_cast<int>(i + 1); // 1-based indexing
        }
    }
    return std::nullopt;
}

std::vector<int> ModularINDIFilterWheel::findFilterByType(const std::string& type) {
    // For now, return empty as we don't store type information
    // This could be extended in the future
    core_->getLogger()->warn("findFilterByType not implemented yet");
    return {};
}

bool ModularINDIFilterWheel::selectFilterByName(const std::string& name) {
    auto position = findFilterByName(name);
    if (position.has_value()) {
        return setPosition(position.value());
    }
    return false;
}

bool ModularINDIFilterWheel::selectFilterByType(const std::string& type) {
    auto positions = findFilterByType(type);
    if (!positions.empty()) {
        return setPosition(positions[0]);
    }
    return false;
}

// Motion control
bool ModularINDIFilterWheel::homeFilterWheel() {
    core_->getLogger()->warn("homeFilterWheel not directly supported by INDI");
    return false;
}

bool ModularINDIFilterWheel::calibrateFilterWheel() {
    core_->getLogger()->warn("calibrateFilterWheel not directly supported by INDI");
    return false;
}

// Temperature (delegated to TemperatureManager)
std::optional<double> ModularINDIFilterWheel::getTemperature() {
    return temperatureManager_->getTemperature();
}

bool ModularINDIFilterWheel::hasTemperatureSensor() {
    return temperatureManager_->hasTemperatureSensor();
}

// Statistics methods (delegated to StatisticsManager)
uint64_t ModularINDIFilterWheel::getTotalMoves() {
    return statisticsManager_->getTotalPositionChanges();
}

bool ModularINDIFilterWheel::resetTotalMoves() {
    return statisticsManager_->resetStatistics();
}

int ModularINDIFilterWheel::getLastMoveTime() {
    auto duration = filterController_->getLastMoveDuration();
    return static_cast<int>(duration.count());
}

// Configuration presets (delegated to ConfigurationManager)
bool ModularINDIFilterWheel::saveFilterConfiguration(const std::string& name) {
    return configurationManager_->saveFilterConfiguration(name);
}

bool ModularINDIFilterWheel::loadFilterConfiguration(const std::string& name) {
    return configurationManager_->loadFilterConfiguration(name);
}

bool ModularINDIFilterWheel::deleteFilterConfiguration(const std::string& name) {
    return configurationManager_->deleteFilterConfiguration(name);
}

std::vector<std::string> ModularINDIFilterWheel::getAvailableConfigurations() {
    return configurationManager_->getAvailableConfigurations();
}

// Advanced profiling and performance monitoring
FilterPerformanceStats ModularINDIFilterWheel::getPerformanceStats() {
    return profiler_->getPerformanceStats();
}

std::chrono::milliseconds ModularINDIFilterWheel::predictMoveDuration(int fromSlot, int toSlot) {
    return profiler_->predictMoveDuration(fromSlot, toSlot);
}

bool ModularINDIFilterWheel::hasPerformanceDegraded() {
    return profiler_->hasPerformanceDegraded();
}

std::vector<std::string> ModularINDIFilterWheel::getOptimizationRecommendations() {
    return profiler_->getOptimizationRecommendations();
}

bool ModularINDIFilterWheel::exportProfilingData(const std::string& filePath) {
    return profiler_->exportToCSV(filePath);
}

void ModularINDIFilterWheel::setProfiling(bool enabled) {
    profiler_->setProfiling(enabled);
}

bool ModularINDIFilterWheel::isProfilingEnabled() {
    return profiler_->isProfilingEnabled();
}

void ModularINDIFilterWheel::newMessage(INDI::BaseDevice baseDevice,
                                        int messageID) {
    auto message = baseDevice.messageQueue(messageID);
    core_->getLogger()->info("Message from {}: {}", baseDevice.getDeviceName(),
                          message);
}

bool ModularINDIFilterWheel::initializeComponents() {
    bool success = true;

    success &= propertyManager_->initialize();
    success &= filterController_->initialize();
    success &= statisticsManager_->initialize();
    success &= temperatureManager_->initialize();
    success &= configurationManager_->initialize();
    success &= profiler_->initialize();

    if (success) {
        core_->getLogger()->info("All components initialized successfully");
    } else {
        core_->getLogger()->error("Failed to initialize some components");
    }

    return success;
}

void ModularINDIFilterWheel::cleanupComponents() {
    if (profiler_)
        profiler_->shutdown();
    if (configurationManager_)
        configurationManager_->shutdown();
    if (temperatureManager_)
        temperatureManager_->shutdown();
    if (statisticsManager_)
        statisticsManager_->shutdown();
    if (filterController_)
        filterController_->shutdown();
    if (propertyManager_)
        propertyManager_->shutdown();
}

void ModularINDIFilterWheel::setupDeviceWatchers() {
    watchDevice(core_->getDeviceName().c_str(), [this](INDI::BaseDevice device) {
        core_->setDevice(device);
        core_->getLogger()->info("Device {} discovered", core_->getDeviceName());

        // Setup property watchers
        propertyManager_->setupPropertyWatchers();

        // Setup connection property watcher
        device.watchProperty(
            "CONNECTION",
            [this](INDI::Property) {
                core_->getLogger()->info("Connecting to {}...",
                                      core_->getDeviceName());
                connectDevice(name_.c_str());
            },
            INDI::BaseDevice::WATCH_NEW);
    });
}

void ModularINDIFilterWheel::setupInitialConnection(const std::string& deviceName) {
    setupDeviceWatchers();

    // Start statistics session
    statisticsManager_->startSession();

    core_->getLogger()->info("Setup complete for device: {}", deviceName);
}

}  // namespace lithium::device::indi::filterwheel
