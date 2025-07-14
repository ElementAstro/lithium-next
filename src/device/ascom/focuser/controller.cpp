#include "controller.hpp"
#include "components/hardware_interface.hpp"
#include "components/movement_controller.hpp"
#include "components/temperature_controller.hpp"
#include "components/position_manager.hpp"
#include "components/backlash_compensator.hpp"
#include "components/property_manager.hpp"
#include <stdexcept>

namespace lithium::device::ascom::focuser {

Controller::Controller(const std::string& name)
    : AtomFocuser(name)
    , initialized_(false)
    , connected_(false)
    , moving_(false)
    , config_{}
{
}

Controller::~Controller() {
    if (initialized_) {
        cleanup();
    }
}

auto Controller::initialize() -> bool {
    if (initialized_) {
        return true;
    }

    try {
        // Initialize configuration
        config_.deviceName = getName();
        config_.enableTemperatureCompensation = true;
        config_.enableBacklashCompensation = true;
        config_.enablePositionTracking = true;
        config_.enablePropertyCaching = true;
        config_.connectionTimeout = std::chrono::seconds(30);
        config_.movementTimeout = std::chrono::seconds(60);
        config_.temperatureMonitoringInterval = std::chrono::seconds(30);
        config_.positionUpdateInterval = std::chrono::milliseconds(100);
        config_.propertyUpdateInterval = std::chrono::seconds(1);
        config_.maxRetries = 3;
        config_.enableLogging = true;
        config_.enableStatistics = true;

        // Create component instances
        hardware_ = std::make_shared<components::HardwareInterface>(config_.deviceName);
        movement_ = std::make_shared<components::MovementController>(hardware_);
        temperature_ = std::make_shared<components::TemperatureController>(hardware_, movement_);
        position_ = std::make_shared<components::PositionManager>(hardware_);
        backlash_ = std::make_shared<components::BacklashCompensator>(hardware_, movement_);
        property_ = std::make_shared<components::PropertyManager>(hardware_);

        // Initialize components
        if (!hardware_->initialize()) {
            return false;
        }

        if (!movement_->initialize()) {
            return false;
        }

        if (!temperature_->initialize()) {
            return false;
        }

        if (!position_->initialize()) {
            return false;
        }

        if (!backlash_->initialize()) {
            return false;
        }

        if (!property_->initialize()) {
            return false;
        }

        // Set up inter-component callbacks
        setupCallbacks();

        // Initialize focuser capabilities
        initializeFocuserCapabilities();

        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        cleanup();
        return false;
    }
}

auto Controller::cleanup() -> void {
    if (!initialized_) {
        return;
    }

    try {
        // Disconnect if connected
        if (connected_) {
            disconnect();
        }

        // Cleanup components in reverse order
        if (property_) {
            property_->destroy();
        }

        if (backlash_) {
            backlash_->destroy();
        }

        if (position_) {
            position_->destroy();
        }

        if (temperature_) {
            temperature_->destroy();
        }

        if (movement_) {
            movement_->destroy();
        }

        if (hardware_) {
            hardware_->destroy();
        }

        // Reset component pointers
        property_.reset();
        backlash_.reset();
        position_.reset();
        temperature_.reset();
        movement_.reset();
        hardware_.reset();

        initialized_ = false;
    } catch (const std::exception& e) {
        // Log error but continue cleanup
    }
}

auto Controller::getControllerConfig() const -> ControllerConfig {
    return config_;
}

auto Controller::setControllerConfig(const ControllerConfig& config) -> bool {
    config_ = config;

    // Update component configurations
    if (hardware_) {
        hardware_->setDeviceName(config.deviceName);
    }

    if (temperature_) {
        components::TemperatureController::CompensationConfig temp_config;
        temp_config.enabled = config.enableTemperatureCompensation;
        temp_config.updateInterval = config.temperatureMonitoringInterval;
        temperature_->setCompensationConfig(temp_config);
    }

    if (property_) {
        components::PropertyManager::PropertyConfig prop_config;
        prop_config.enableCaching = config.enablePropertyCaching;
        prop_config.propertyUpdateInterval = config.propertyUpdateInterval;
        property_->setPropertyConfig(prop_config);
    }

    return true;
}

// Connection management
auto Controller::connect() -> bool {
    if (connected_) {
        return true;
    }

    if (!initialized_) {
        if (!initialize()) {
            return false;
        }
    }

    try {
        // Connect hardware
        if (!hardware_->connect()) {
            return false;
        }

        // Start monitoring threads
        if (config_.enableTemperatureCompensation) {
            temperature_->startMonitoring();
        }

        if (config_.enablePropertyCaching) {
            property_->startMonitoring();
        }

        // Update connection status
        connected_ = true;
        property_->setConnected(true);

        // Synchronize initial state
        synchronizeState();

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto Controller::disconnect() -> bool {
    if (!connected_) {
        return true;
    }

    try {
        // Stop any ongoing movement
        if (moving_) {
            halt();
        }

        // Stop monitoring threads
        if (temperature_) {
            temperature_->stopMonitoring();
        }

        if (property_) {
            property_->stopMonitoring();
        }

        // Disconnect hardware
        if (hardware_) {
            hardware_->disconnect();
        }

        // Update connection status
        connected_ = false;
        property_->setConnected(false);

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto Controller::isConnected() const -> bool {
    return connected_;
}

auto Controller::reconnect() -> bool {
    disconnect();
    return connect();
}

// Movement control
auto Controller::moveToPosition(int position) -> bool {
    if (!connected_) {
        return false;
    }

    if (moving_) {
        return false; // Already moving
    }

    try {
        // Validate position
        if (!position_->validatePosition(position)) {
            return false;
        }

        // Set target position
        if (!position_->setTargetPosition(position)) {
            return false;
        }

        // Calculate backlash compensation
        int current_pos = position_->getCurrentPosition();
        auto direction = (position > current_pos) ?
            components::MovementDirection::OUTWARD :
            components::MovementDirection::INWARD;

        int backlash_steps = 0;
        if (config_.enableBacklashCompensation) {
            backlash_steps = backlash_->calculateBacklashCompensation(position, direction);
        }

        // Start movement
        moving_ = true;
        property_->setProperty("IsMoving", components::PropertyManager::PropertyValue(true));

        // Apply backlash compensation first if needed
        if (backlash_steps > 0) {
            if (!backlash_->applyBacklashCompensation(backlash_steps, direction)) {
                moving_ = false;
                property_->setProperty("IsMoving", components::PropertyManager::PropertyValue(false));
                return false;
            }
        }

        // Execute main movement
        bool success = movement_->moveToPosition(position);

        // Update movement state
        moving_ = false;
        property_->setProperty("IsMoving", components::PropertyManager::PropertyValue(false));

        if (success) {
            // Update position
            position_->setCurrentPosition(position);
            property_->setProperty("Position", components::PropertyManager::PropertyValue(position));

            // Update backlash state
            if (config_.enableBacklashCompensation) {
                backlash_->updateLastDirection(direction);
            }
        }

        return success;
    } catch (const std::exception& e) {
        moving_ = false;
        property_->setProperty("IsMoving", components::PropertyManager::PropertyValue(false));
        return false;
    }
}

auto Controller::moveRelative(int steps) -> bool {
    if (!connected_) {
        return false;
    }

    int current_pos = position_->getCurrentPosition();
    int target_pos = current_pos + steps;

    return moveToPosition(target_pos);
}

auto Controller::halt() -> bool {
    if (!connected_) {
        return false;
    }

    try {
        bool success = movement_->halt();

        if (success) {
            moving_ = false;
            property_->setProperty("IsMoving", components::PropertyManager::PropertyValue(false));

            // Update position after halt
            auto current_pos = hardware_->getCurrentPosition();
            if (current_pos.has_value()) {
                position_->setCurrentPosition(current_pos.value());
                property_->setProperty("Position", components::PropertyManager::PropertyValue(current_pos.value()));
            }
        }

        return success;
    } catch (const std::exception& e) {
        return false;
    }
}

auto Controller::isMoving() const -> bool {
    return moving_;
}

auto Controller::getCurrentPosition() -> std::optional<int> {
    if (!connected_) {
        return std::nullopt;
    }

    return position_->getCurrentPosition();
}

auto Controller::getTargetPosition() -> std::optional<int> {
    if (!connected_) {
        return std::nullopt;
    }

    return position_->getTargetPosition();
}

// Speed control
auto Controller::getSpeed() -> std::optional<double> {
    if (!connected_) {
        return std::nullopt;
    }

    return movement_->getSpeed();
}

auto Controller::setSpeed(double speed) -> bool {
    if (!connected_) {
        return false;
    }

    return movement_->setSpeed(speed);
}

auto Controller::getMaxSpeed() -> int {
    if (!connected_) {
        return 0;
    }

    return movement_->getMaxSpeed();
}

auto Controller::getSpeedRange() -> std::pair<int, int> {
    if (!connected_) {
        return {0, 0};
    }

    return movement_->getSpeedRange();
}

// Direction control
auto Controller::getDirection() -> std::optional<FocusDirection> {
    if (!connected_) {
        return std::nullopt;
    }

    auto direction = movement_->getDirection();
    if (direction.has_value()) {
        switch (direction.value()) {
            case components::MovementDirection::INWARD:
                return FocusDirection::IN;
            case components::MovementDirection::OUTWARD:
                return FocusDirection::OUT;
            default:
                return FocusDirection::NONE;
        }
    }

    return std::nullopt;
}

auto Controller::setDirection(FocusDirection direction) -> bool {
    if (!connected_) {
        return false;
    }

    components::MovementDirection move_dir;
    switch (direction) {
        case FocusDirection::IN:
            move_dir = components::MovementDirection::INWARD;
            break;
        case FocusDirection::OUT:
            move_dir = components::MovementDirection::OUTWARD;
            break;
        default:
            move_dir = components::MovementDirection::NONE;
            break;
    }

    return movement_->setDirection(move_dir);
}

// Limit control
auto Controller::getMaxLimit() -> std::optional<int> {
    if (!connected_) {
        return std::nullopt;
    }

    auto limits = position_->getPositionLimits();
    return limits.maxPosition;
}

auto Controller::setMaxLimit(int limit) -> bool {
    if (!connected_) {
        return false;
    }

    auto limits = position_->getPositionLimits();
    limits.maxPosition = limit;

    return position_->setPositionLimits(limits);
}

auto Controller::getMinLimit() -> std::optional<int> {
    if (!connected_) {
        return std::nullopt;
    }

    auto limits = position_->getPositionLimits();
    return limits.minPosition;
}

auto Controller::setMinLimit(int limit) -> bool {
    if (!connected_) {
        return false;
    }

    auto limits = position_->getPositionLimits();
    limits.minPosition = limit;

    return position_->setPositionLimits(limits);
}

// Temperature control
auto Controller::getTemperature() -> std::optional<double> {
    if (!connected_) {
        return std::nullopt;
    }

    return temperature_->getExternalTemperature();
}

auto Controller::hasTemperatureSensor() -> bool {
    if (!connected_) {
        return false;
    }

    return temperature_->hasTemperatureSensor();
}

auto Controller::getTemperatureCompensation() -> TemperatureCompensation {
    if (!connected_) {
        return TemperatureCompensation{};
    }

    return temperature_->getTemperatureCompensation();
}

auto Controller::setTemperatureCompensation(const TemperatureCompensation& comp) -> bool {
    if (!connected_) {
        return false;
    }

    return temperature_->setTemperatureCompensation(comp);
}

auto Controller::enableTemperatureCompensation(bool enable) -> bool {
    if (!connected_) {
        return false;
    }

    return temperature_->enableTemperatureCompensation(enable);
}

// Backlash control
auto Controller::getBacklashSteps() -> int {
    if (!connected_) {
        return 0;
    }

    return backlash_->getBacklashSteps();
}

auto Controller::setBacklashSteps(int steps) -> bool {
    if (!connected_) {
        return false;
    }

    return backlash_->setBacklashSteps(steps);
}

auto Controller::enableBacklashCompensation(bool enable) -> bool {
    if (!connected_) {
        return false;
    }

    return backlash_->enableBacklashCompensation(enable);
}

auto Controller::isBacklashCompensationEnabled() -> bool {
    if (!connected_) {
        return false;
    }

    return backlash_->isBacklashCompensationEnabled();
}

auto Controller::calibrateBacklash() -> bool {
    if (!connected_) {
        return false;
    }

    return backlash_->calibrateBacklash(100); // Use default test range
}

// Property management
auto Controller::getProperty(const std::string& name) -> std::optional<std::string> {
    if (!connected_) {
        return std::nullopt;
    }

    auto value = property_->getProperty(name);
    if (value.has_value()) {
        // Convert PropertyValue to string
        if (std::holds_alternative<bool>(value.value())) {
            return std::get<bool>(value.value()) ? "true" : "false";
        } else if (std::holds_alternative<int>(value.value())) {
            return std::to_string(std::get<int>(value.value()));
        } else if (std::holds_alternative<double>(value.value())) {
            return std::to_string(std::get<double>(value.value()));
        } else if (std::holds_alternative<std::string>(value.value())) {
            return std::get<std::string>(value.value());
        }
    }

    return std::nullopt;
}

auto Controller::setProperty(const std::string& name, const std::string& value) -> bool {
    if (!connected_) {
        return false;
    }

    // Convert string to PropertyValue based on property type
    // This is a simplified conversion - a real implementation would need
    // to know the expected type for each property

    // Try boolean first
    if (value == "true" || value == "false") {
        return property_->setProperty(name, components::PropertyManager::PropertyValue(value == "true"));
    }

    // Try integer
    try {
        int int_val = std::stoi(value);
        return property_->setProperty(name, components::PropertyManager::PropertyValue(int_val));
    } catch (const std::exception& e) {
        // Not an integer
    }

    // Try double
    try {
        double double_val = std::stod(value);
        return property_->setProperty(name, components::PropertyManager::PropertyValue(double_val));
    } catch (const std::exception& e) {
        // Not a double
    }

    // Default to string
    return property_->setProperty(name, components::PropertyManager::PropertyValue(value));
}

auto Controller::getAllProperties() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> result;

    if (!connected_) {
        return result;
    }

    auto properties = property_->getProperties(property_->getRegisteredProperties());

    for (const auto& [name, value] : properties) {
        if (std::holds_alternative<bool>(value)) {
            result[name] = std::get<bool>(value) ? "true" : "false";
        } else if (std::holds_alternative<int>(value)) {
            result[name] = std::to_string(std::get<int>(value));
        } else if (std::holds_alternative<double>(value)) {
            result[name] = std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            result[name] = std::get<std::string>(value);
        }
    }

    return result;
}

// Statistics and monitoring
auto Controller::getStatistics() -> FocuserStatistics {
    FocuserStatistics stats;

    if (!connected_) {
        return stats;
    }

    // Get component statistics
    auto pos_stats = position_->getPositionStats();
    auto temp_stats = temperature_->getTemperatureStats();
    auto backlash_stats = backlash_->getBacklashStats();

    stats.totalMoves = pos_stats.totalMoves;
    stats.totalDistance = pos_stats.positionRange;
    stats.currentPosition = pos_stats.currentPosition;
    stats.targetPosition = position_->getTargetPosition();
    stats.currentTemperature = temp_stats.currentTemperature;
    stats.temperatureCompensations = temp_stats.totalCompensations;
    stats.backlashCompensations = backlash_stats.totalCompensations;
    stats.uptime = std::chrono::steady_clock::now() - pos_stats.startTime;
    stats.connected = connected_;
    stats.moving = moving_;

    return stats;
}

auto Controller::resetStatistics() -> bool {
    if (!connected_) {
        return false;
    }

    position_->resetPositionStats();
    temperature_->resetTemperatureStats();
    backlash_->resetBacklashStats();

    return true;
}

// Calibration and maintenance
auto Controller::performFullCalibration() -> bool {
    if (!connected_) {
        return false;
    }

    bool success = true;

    // Calibrate backlash
    if (config_.enableBacklashCompensation) {
        if (!backlash_->calibrateBacklash(100)) {
            success = false;
        }
    }

    // Calibrate temperature compensation
    if (config_.enableTemperatureCompensation) {
        // This would involve a more complex calibration process
        // For now, just enable temperature compensation
        temperature_->enableTemperatureCompensation(true);
    }

    // Calibrate position limits
    if (!position_->autoDetectLimits()) {
        success = false;
    }

    return success;
}

auto Controller::performSelfTest() -> bool {
    if (!connected_) {
        return false;
    }

    try {
        // Test hardware communication
        if (!hardware_->performSelfTest()) {
            return false;
        }

        // Test movement
        int current_pos = position_->getCurrentPosition();
        int test_pos = current_pos + 10;

        if (!moveToPosition(test_pos)) {
            return false;
        }

        // Wait for movement to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Return to original position
        if (!moveToPosition(current_pos)) {
            return false;
        }

        // Test temperature sensor if available
        if (hasTemperatureSensor()) {
            auto temp = getTemperature();
            if (!temp.has_value()) {
                return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

// Emergency and safety
auto Controller::emergencyStop() -> bool {
    try {
        // Stop all movement immediately
        if (movement_) {
            movement_->emergencyStop();
        }

        // Update state
        moving_ = false;
        if (property_) {
            property_->setProperty("IsMoving", components::PropertyManager::PropertyValue(false));
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto Controller::getLastError() -> std::string {
    if (hardware_) {
        return hardware_->getLastError();
    }

    return "";
}

auto Controller::clearErrors() -> bool {
    if (hardware_) {
        return hardware_->clearErrors();
    }

    return true;
}

// Private methods

auto Controller::setupCallbacks() -> void {
    // Set up inter-component communication

    // Temperature callbacks
    if (temperature_) {
        temperature_->setTemperatureCallback([this](double temp) {
            handleTemperatureChange(temp);
        });

        temperature_->setCompensationCallback([this](double tempChange, int steps, bool success) {
            handleTemperatureCompensation(tempChange, steps, success);
        });
    }

    // Position callbacks
    if (position_) {
        position_->setPositionCallback([this](int pos) {
            handlePositionChange(pos);
        });

        position_->setLimitCallback([this](int pos, const std::string& limitType) {
            handleLimitReached(pos, limitType);
        });
    }

    // Backlash callbacks
    if (backlash_) {
        backlash_->setCompensationCallback([this](int steps, components::MovementDirection dir, bool success) {
            handleBacklashCompensation(steps, dir, success);
        });
    }

    // Property callbacks
    if (property_) {
        property_->setPropertyChangeCallback([this](const std::string& name,
                                                   const components::PropertyManager::PropertyValue& oldValue,
                                                   const components::PropertyManager::PropertyValue& newValue) {
            handlePropertyChange(name, oldValue, newValue);
        });
    }
}

auto Controller::initializeFocuserCapabilities() -> void {
    FocuserCapabilities caps;

    caps.canAbsoluteMove = true;
    caps.canRelativeMove = true;
    caps.canAbort = true;
    caps.canReverse = movement_->canReverse();
    caps.canSync = false; // Not implemented yet
    caps.hasTemperature = temperature_->hasTemperatureSensor();
    caps.hasBacklash = config_.enableBacklashCompensation;
    caps.hasSpeedControl = true;
    caps.maxPosition = hardware_->getMaxPosition();
    caps.minPosition = hardware_->getMinPosition();

    setFocuserCapabilities(caps);
}

auto Controller::synchronizeState() -> void {
    if (!connected_) {
        return;
    }

    try {
        // Synchronize position
        auto current_pos = hardware_->getCurrentPosition();
        if (current_pos.has_value()) {
            position_->setCurrentPosition(current_pos.value());
        }

        // Synchronize movement state
        moving_ = hardware_->isMoving();

        // Synchronize properties
        property_->synchronizeAllProperties();

        // Update focuser state
        setFocuserState(moving_ ? FocuserState::MOVING : FocuserState::IDLE);
    } catch (const std::exception& e) {
        // Log error but continue
    }
}

auto Controller::handleTemperatureChange(double temperature) -> void {
    // Handle temperature change notifications
    if (property_) {
        property_->setProperty("Temperature", components::PropertyManager::PropertyValue(temperature));
    }
}

auto Controller::handleTemperatureCompensation(double tempChange, int steps, bool success) -> void {
    // Handle temperature compensation notifications
    if (success) {
        // Update position after compensation
        auto current_pos = hardware_->getCurrentPosition();
        if (current_pos.has_value()) {
            position_->setCurrentPosition(current_pos.value());
            if (property_) {
                property_->setProperty("Position", components::PropertyManager::PropertyValue(current_pos.value()));
            }
        }
    }
}

auto Controller::handlePositionChange(int position) -> void {
    // Handle position change notifications
    if (property_) {
        property_->setProperty("Position", components::PropertyManager::PropertyValue(position));
    }
}

auto Controller::handleLimitReached(int position, const std::string& limitType) -> void {
    // Handle limit reached notifications
    // This might trigger an emergency stop or alert
    if (moving_) {
        halt();
    }
}

auto Controller::handleBacklashCompensation(int steps, components::MovementDirection direction, bool success) -> void {
    // Handle backlash compensation notifications
    if (success) {
        // Update position after backlash compensation
        auto current_pos = hardware_->getCurrentPosition();
        if (current_pos.has_value()) {
            position_->setCurrentPosition(current_pos.value());
        }
    }
}

auto Controller::handlePropertyChange(const std::string& name,
                                     const components::PropertyManager::PropertyValue& oldValue,
                                     const components::PropertyManager::PropertyValue& newValue) -> void {
    // Handle property change notifications
    // This could trigger actions based on specific property changes

    if (name == "Connected") {
        if (std::holds_alternative<bool>(newValue)) {
            bool new_connected = std::get<bool>(newValue);
            if (new_connected != connected_) {
                connected_ = new_connected;
            }
        }
    } else if (name == "IsMoving") {
        if (std::holds_alternative<bool>(newValue)) {
            bool new_moving = std::get<bool>(newValue);
            if (new_moving != moving_) {
                moving_ = new_moving;
                setFocuserState(moving_ ? FocuserState::MOVING : FocuserState::IDLE);
            }
        }
    }
}

auto Controller::validateConfiguration() -> bool {
    // Validate controller configuration
    if (config_.deviceName.empty()) {
        return false;
    }

    if (config_.connectionTimeout.count() <= 0) {
        return false;
    }

    if (config_.movementTimeout.count() <= 0) {
        return false;
    }

    if (config_.maxRetries < 0) {
        return false;
    }

    return true;
}

auto Controller::performMaintenanceTasks() -> void {
    // Perform periodic maintenance tasks
    try {
        // Update statistics
        if (config_.enableStatistics) {
            // Statistics are updated automatically by components
        }

        // Check for errors
        auto error = getLastError();
        if (!error.empty()) {
            // Log error
        }

        // Synchronize state periodically
        if (connected_) {
            synchronizeState();
        }
    } catch (const std::exception& e) {
        // Log error but continue
    }
}

} // namespace lithium::device::ascom::focuser
