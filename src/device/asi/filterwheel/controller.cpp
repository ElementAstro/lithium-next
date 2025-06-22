#include "controller.hpp"

#include <sstream>

#include <spdlog/spdlog.h>

#include "components/hardware_interface.hpp"
#include "components/position_manager.hpp"

namespace lithium::device::asi::filterwheel {

ASIFilterwheelController::ASIFilterwheelController()
    : initialized_(false), last_position_(-1) {
    spdlog::info("ASIFilterwheelController created");
}

ASIFilterwheelController::~ASIFilterwheelController() {
    shutdown();
    spdlog::info("ASIFilterwheelController destroyed");
}

bool ASIFilterwheelController::initialize(const std::string& device_path) {
    if (initialized_) {
        spdlog::warn("Controller already initialized");
        return true;
    }

    spdlog::info("Initializing ASI Filterwheel Controller V2");

    try {
        // Initialize components in the correct order
        if (!initializeComponents()) {
            setLastError("Failed to initialize components");
            return false;
        }

        // Connect hardware
        if (!hardware_interface_->connectToDevice(device_path)) {
            setLastError("Failed to connect to filterwheel hardware");
            cleanupComponents();
            return false;
        }

        // Setup inter-component callbacks
        setupCallbacks();

        // Validate all components are ready
        if (!validateComponentsReady()) {
            setLastError("Component validation failed");
            cleanupComponents();
            return false;
        }

        // Load configuration if available
        configuration_manager_->loadConfiguration();

        // Get initial position
        last_position_ = hardware_interface_->getCurrentPosition();

        initialized_ = true;
        spdlog::info("ASI Filterwheel Controller V2 initialized successfully");
        return true;

    } catch (const std::exception& e) {
        setLastError("Exception during initialization: " +
                     std::string(e.what()));
        spdlog::error("Exception during initialization: {}", e.what());
        cleanupComponents();
        return false;
    }
}

bool ASIFilterwheelController::shutdown() {
    if (!initialized_) {
        return true;
    }

    spdlog::info("Shutting down ASI Filterwheel Controller V2");

    try {
        // Stop any running operations
        if (sequence_manager_ && sequence_manager_->isSequenceRunning()) {
            sequence_manager_->stopSequence();
        }

        if (monitoring_system_ &&
            monitoring_system_->isHealthMonitoringActive()) {
            monitoring_system_->stopHealthMonitoring();
        }

        // Save configuration
        if (configuration_manager_) {
            configuration_manager_->saveConfiguration();
        }

        // Cleanup components
        cleanupComponents();

        initialized_ = false;
        spdlog::info("ASI Filterwheel Controller V2 shut down successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception during shutdown: {}", e.what());
        return false;
    }
}

bool ASIFilterwheelController::isInitialized() const { return initialized_; }

bool ASIFilterwheelController::moveToPosition(int position) {
    if (!initialized_ || !position_manager_) {
        setLastError(
            "Controller not initialized or position manager unavailable");
        return false;
    }

    if (monitoring_system_) {
        monitoring_system_->startOperationTimer("move_to_position");
    }

    bool success = position_manager_->setPosition(position);

    if (monitoring_system_) {
        monitoring_system_->endOperationTimer(success,
                                              success ? "" : "Move failed");
    }

    if (success) {
        notifyPositionChange(position);
    } else {
        setLastError("Failed to move to position " + std::to_string(position));
    }

    return success;
}

int ASIFilterwheelController::getCurrentPosition() const {
    if (!initialized_ || !hardware_interface_) {
        return -1;
    }

    return hardware_interface_->getCurrentPosition();
}

bool ASIFilterwheelController::isMoving() const {
    if (!initialized_ || !position_manager_) {
        return false;
    }

    return position_manager_->isMoving();
}

bool ASIFilterwheelController::stopMovement() {
    if (!initialized_ || !position_manager_) {
        setLastError(
            "Controller not initialized or position manager unavailable");
        return false;
    }

    position_manager_->stopMovement();
    return true;
}

bool ASIFilterwheelController::waitForMovement(int timeout_ms) {
    if (!initialized_ || !position_manager_) {
        setLastError(
            "Controller not initialized or position manager unavailable");
        return false;
    }

    return position_manager_->waitForMovement(timeout_ms);
}

int ASIFilterwheelController::getSlotCount() const {
    if (!initialized_ || !hardware_interface_) {
        return 0;
    }

    return hardware_interface_->getFilterCount();
}

bool ASIFilterwheelController::setFilterName(int slot,
                                               const std::string& name) {
    if (!initialized_ || !configuration_manager_) {
        setLastError(
            "Controller not initialized or configuration manager unavailable");
        return false;
    }

    return configuration_manager_->setFilterName(slot, name);
}

std::string ASIFilterwheelController::getFilterName(int slot) const {
    if (!initialized_ || !configuration_manager_) {
        return "Slot " + std::to_string(slot);
    }

    return configuration_manager_->getFilterName(slot);
}

std::vector<std::string> ASIFilterwheelController::getFilterNames() const {
    if (!initialized_ || !configuration_manager_) {
        return {};
    }

    return configuration_manager_->getFilterNames();
}

bool ASIFilterwheelController::setFocusOffset(int slot, double offset) {
    if (!initialized_ || !configuration_manager_) {
        setLastError(
            "Controller not initialized or configuration manager unavailable");
        return false;
    }

    return configuration_manager_->setFocusOffset(slot, offset);
}

double ASIFilterwheelController::getFocusOffset(int slot) const {
    if (!initialized_ || !configuration_manager_) {
        return 0.0;
    }

    return configuration_manager_->getFocusOffset(slot);
}

bool ASIFilterwheelController::createProfile(const std::string& name,
                                               const std::string& description) {
    if (!initialized_ || !configuration_manager_) {
        setLastError(
            "Controller not initialized or configuration manager unavailable");
        return false;
    }

    return configuration_manager_->createProfile(name, description);
}

bool ASIFilterwheelController::setCurrentProfile(const std::string& name) {
    if (!initialized_ || !configuration_manager_) {
        setLastError(
            "Controller not initialized or configuration manager unavailable");
        return false;
    }

    return configuration_manager_->setCurrentProfile(name);
}

std::string ASIFilterwheelController::getCurrentProfile() const {
    if (!initialized_ || !configuration_manager_) {
        return "Default";
    }

    return configuration_manager_->getCurrentProfileName();
}

std::vector<std::string> ASIFilterwheelController::getProfiles() const {
    if (!initialized_ || !configuration_manager_) {
        return {};
    }

    return configuration_manager_->getProfileNames();
}

bool ASIFilterwheelController::deleteProfile(const std::string& name) {
    if (!initialized_ || !configuration_manager_) {
        setLastError(
            "Controller not initialized or configuration manager unavailable");
        return false;
    }

    return configuration_manager_->deleteProfile(name);
}

bool ASIFilterwheelController::createSequence(
    const std::string& name, const std::vector<int>& positions,
    int dwell_time_ms) {
    if (!initialized_ || !sequence_manager_) {
        setLastError(
            "Controller not initialized or sequence manager unavailable");
        return false;
    }

    return sequence_manager_->createCustomSequence(name, positions,
                                                   dwell_time_ms);
}

bool ASIFilterwheelController::startSequence(const std::string& name) {
    if (!initialized_ || !sequence_manager_) {
        setLastError(
            "Controller not initialized or sequence manager unavailable");
        return false;
    }

    return sequence_manager_->startSequence(name);
}

bool ASIFilterwheelController::pauseSequence() {
    if (!initialized_ || !sequence_manager_) {
        setLastError(
            "Controller not initialized or sequence manager unavailable");
        return false;
    }

    return sequence_manager_->pauseSequence();
}

bool ASIFilterwheelController::resumeSequence() {
    if (!initialized_ || !sequence_manager_) {
        setLastError(
            "Controller not initialized or sequence manager unavailable");
        return false;
    }

    return sequence_manager_->resumeSequence();
}

bool ASIFilterwheelController::stopSequence() {
    if (!initialized_ || !sequence_manager_) {
        setLastError(
            "Controller not initialized or sequence manager unavailable");
        return false;
    }

    return sequence_manager_->stopSequence();
}

bool ASIFilterwheelController::isSequenceRunning() const {
    if (!initialized_ || !sequence_manager_) {
        return false;
    }

    return sequence_manager_->isSequenceRunning();
}

double ASIFilterwheelController::getSequenceProgress() const {
    if (!initialized_ || !sequence_manager_) {
        return 0.0;
    }

    return sequence_manager_->getSequenceProgress();
}

bool ASIFilterwheelController::performCalibration() {
    if (!initialized_ || !calibration_system_) {
        setLastError(
            "Controller not initialized or calibration system unavailable");
        return false;
    }

    return calibration_system_->performFullCalibration();
}

bool ASIFilterwheelController::performSelfTest() {
    if (!initialized_ || !calibration_system_) {
        setLastError(
            "Controller not initialized or calibration system unavailable");
        return false;
    }

    return calibration_system_->performQuickSelfTest();
}

bool ASIFilterwheelController::testPosition(int position) {
    if (!initialized_ || !calibration_system_) {
        setLastError(
            "Controller not initialized or calibration system unavailable");
        return false;
    }

    return calibration_system_->testPosition(position);
}

std::string ASIFilterwheelController::getCalibrationStatus() const {
    if (!initialized_ || !calibration_system_) {
        return "Calibration system unavailable";
    }

    return calibration_system_->getCalibrationStatus();
}

bool ASIFilterwheelController::hasValidCalibration() const {
    if (!initialized_ || !calibration_system_) {
        return false;
    }

    return calibration_system_->hasValidCalibration();
}

double ASIFilterwheelController::getSuccessRate() const {
    if (!initialized_ || !monitoring_system_) {
        return 0.0;
    }

    return monitoring_system_->getSuccessRate();
}

int ASIFilterwheelController::getConsecutiveFailures() const {
    if (!initialized_ || !monitoring_system_) {
        return 0;
    }

    return monitoring_system_->getConsecutiveFailures();
}

std::string ASIFilterwheelController::getHealthStatus() const {
    if (!initialized_ || !monitoring_system_) {
        return "Monitoring system unavailable";
    }

    return monitoring_system_->generateHealthSummary();
}

bool ASIFilterwheelController::isHealthy() const {
    if (!initialized_ || !monitoring_system_) {
        return false;
    }

    return monitoring_system_->isHealthy();
}

void ASIFilterwheelController::startHealthMonitoring(int interval_ms) {
    if (!initialized_ || !monitoring_system_) {
        spdlog::error(
              "Cannot start health monitoring: controller not initialized or "
              "monitoring system unavailable");
        return;
    }

    monitoring_system_->startHealthMonitoring(interval_ms);
}

void ASIFilterwheelController::stopHealthMonitoring() {
    if (monitoring_system_) {
        monitoring_system_->stopHealthMonitoring();
    }
}

bool ASIFilterwheelController::saveConfiguration(
    const std::string& filepath) {
    if (!initialized_ || !configuration_manager_) {
        setLastError(
            "Controller not initialized or configuration manager unavailable");
        return false;
    }

    return configuration_manager_->saveConfiguration(filepath);
}

bool ASIFilterwheelController::loadConfiguration(
    const std::string& filepath) {
    if (!initialized_ || !configuration_manager_) {
        setLastError(
            "Controller not initialized or configuration manager unavailable");
        return false;
    }

    return configuration_manager_->loadConfiguration(filepath);
}

void ASIFilterwheelController::setPositionCallback(
    PositionCallback callback) {
    position_callback_ = std::move(callback);
}

void ASIFilterwheelController::setSequenceCallback(
    SequenceCallback callback) {
    sequence_callback_ = std::move(callback);
}

void ASIFilterwheelController::setHealthCallback(HealthCallback callback) {
    health_callback_ = std::move(callback);
}

void ASIFilterwheelController::clearCallbacks() {
    position_callback_ = nullptr;
    sequence_callback_ = nullptr;
    health_callback_ = nullptr;
}

std::string ASIFilterwheelController::getDeviceInfo() const {
    if (!initialized_ || !hardware_interface_) {
        return "Device not initialized";
    }

    auto deviceInfo = hardware_interface_->getDeviceInfo();
    if (deviceInfo.has_value()) {
        const auto& info = deviceInfo.value();
        std::ostringstream ss;
        ss << "Device: " << info.name 
           << " (ID: " << info.id << ")"
           << ", Slots: " << info.slotCount
           << ", FW: " << info.firmwareVersion
           << ", Driver: " << info.driverVersion;
        return ss.str();
    }
    
    return "Device information unavailable";
}

std::string ASIFilterwheelController::getVersion() const {
    return "ASI Filterwheel Controller V2.0.0";
}

std::string ASIFilterwheelController::getLastError() const {
    return last_error_;
}

// Component access methods
std::shared_ptr<components::HardwareInterface>
ASIFilterwheelController::getHardwareInterface() const {
    return hardware_interface_;
}

std::shared_ptr<components::PositionManager>
ASIFilterwheelController::getPositionManager() const {
    return position_manager_;
}

std::shared_ptr<ConfigurationManager>
ASIFilterwheelController::getConfigurationManager() const {
    return configuration_manager_;
}

std::shared_ptr<components::SequenceManager>
ASIFilterwheelController::getSequenceManager() const {
    return sequence_manager_;
}

std::shared_ptr<MonitoringSystem>
ASIFilterwheelController::getMonitoringSystem() const {
    return monitoring_system_;
}

std::shared_ptr<CalibrationSystem>
ASIFilterwheelController::getCalibrationSystem() const {
    return calibration_system_;
}

// Private methods

bool ASIFilterwheelController::initializeComponents() {
    spdlog::info("Initializing filterwheel components");

    try {
        // Create components in dependency order
        hardware_interface_ = std::make_shared<components::HardwareInterface>();
        if (!hardware_interface_) {
            spdlog::error("Failed to create hardware interface");
            return false;
        }

        position_manager_ =
            std::make_shared<components::PositionManager>(hardware_interface_);
        if (!position_manager_) {
            spdlog::error("Failed to create position manager");
            return false;
        }

        configuration_manager_ = std::make_shared<ConfigurationManager>();
        if (!configuration_manager_) {
            spdlog::error("Failed to create configuration manager");
            return false;
        }

        sequence_manager_ =
            std::make_shared<components::SequenceManager>(position_manager_);
        if (!sequence_manager_) {
            spdlog::error("Failed to create sequence manager");
            return false;
        }

        monitoring_system_ =
            std::make_shared<MonitoringSystem>(hardware_interface_);
        if (!monitoring_system_) {
            spdlog::error("Failed to create monitoring system");
            return false;
        }

        calibration_system_ = std::make_shared<CalibrationSystem>(
            hardware_interface_, position_manager_);
        if (!calibration_system_) {
            spdlog::error("Failed to create calibration system");
            return false;
        }

        spdlog::info("All filterwheel components created successfully");
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Exception while creating components: {}", e.what());
        return false;
    }
}

void ASIFilterwheelController::setupCallbacks() {
    // Setup sequence manager callback
    if (sequence_manager_) {
        sequence_manager_->setSequenceCallback(
            [this](const std::string& event, int step, int position) {
                onSequenceEvent(event, step, position);
            });
    }

    // Setup monitoring system callbacks
    if (monitoring_system_) {
        monitoring_system_->setHealthCallback(
            [this](const HealthMetrics& metrics) {
                onHealthUpdate("Health update",
                               metrics.is_connected && metrics.is_responding);
            });

        monitoring_system_->setAlertCallback(
            [this](const std::string& alert_type, const std::string& message) {
                onHealthUpdate("Alert: " + alert_type + " - " + message, false);
            });
    }
}

void ASIFilterwheelController::cleanupComponents() {
    spdlog::info("Cleaning up filterwheel components");

    if (hardware_interface_) {
        hardware_interface_->disconnect();
    }

    // Reset all shared pointers
    calibration_system_.reset();
    monitoring_system_.reset();
    sequence_manager_.reset();
    configuration_manager_.reset();
    position_manager_.reset();
    hardware_interface_.reset();
}

bool ASIFilterwheelController::validateComponentsReady() const {
    if (!hardware_interface_) {
        spdlog::error("Hardware interface not ready");
        return false;
    }

    if (!hardware_interface_->isConnected()) {
        spdlog::error("Hardware not connected");
        return false;
    }

    if (!position_manager_) {
        spdlog::error("Position manager not ready");
        return false;
    }

    if (!configuration_manager_) {
        spdlog::error("Configuration manager not ready");
        return false;
    }

    return true;
}

void ASIFilterwheelController::setLastError(const std::string& error) {
    last_error_ = error;
    spdlog::error("Controller error: {}", error);
}

void ASIFilterwheelController::notifyPositionChange(int new_position) {
    if (new_position != last_position_) {
        if (position_callback_) {
            try {
                position_callback_(last_position_, new_position);
            } catch (const std::exception& e) {
                spdlog::error("Exception in position callback: {}", e.what());
            }
        }
        last_position_ = new_position;
    }
}

void ASIFilterwheelController::onSequenceEvent(const std::string& event,
                                                 int step, int position) {
    if (sequence_callback_) {
        try {
            sequence_callback_(event, step, position);
        } catch (const std::exception& e) {
            spdlog::error("Exception in sequence callback: {}", e.what());
        }
    }
}

void ASIFilterwheelController::onHealthUpdate(const std::string& status,
                                                bool is_healthy) {
    if (health_callback_) {
        try {
            health_callback_(status, is_healthy);
        } catch (const std::exception& e) {
            spdlog::error("Exception in health callback: {}", e.what());
        }
    }
}

bool ASIFilterwheelController::validateConfiguration() const {
    if (!configuration_manager_) {
        return false;
    }

    return configuration_manager_->validateConfiguration();
}

std::vector<std::string> ASIFilterwheelController::getComponentErrors()
    const {
    std::vector<std::string> errors;

    if (!hardware_interface_) {
        errors.push_back("Hardware interface not available");
    }

    if (!position_manager_) {
        errors.push_back("Position manager not available");
    }

    if (!configuration_manager_) {
        errors.push_back("Configuration manager not available");
    } else {
        auto config_errors = configuration_manager_->getValidationErrors();
        errors.insert(errors.end(), config_errors.begin(), config_errors.end());
    }

    if (calibration_system_) {
        auto cal_errors = calibration_system_->getConfigurationErrors();
        errors.insert(errors.end(), cal_errors.begin(), cal_errors.end());
    }

    return errors;
}

}  // namespace lithium::device::asi::filterwheel
