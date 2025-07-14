#include "main.hpp"
#include "controller.hpp"
#include "components/hardware_interface.hpp"
#include "components/position_manager.hpp"
#include "components/property_manager.hpp"
#include "components/preset_manager.hpp"

namespace lithium::device::ascom::rotator {

ASCOMRotatorMain::ASCOMRotatorMain(const std::string& name) : name_(name) {}

ASCOMRotatorMain::~ASCOMRotatorMain() {
    destroy();
}

auto ASCOMRotatorMain::createRotator(const std::string& name,
                                   const RotatorInitConfig& config)
    -> std::shared_ptr<ASCOMRotatorMain> {
    auto rotator = std::make_shared<ASCOMRotatorMain>(name);
    if (rotator->initialize(config)) {
        return rotator;
    }
    return nullptr;
}

auto ASCOMRotatorMain::createRotatorWithController(const std::string& name,
                                                  std::shared_ptr<ASCOMRotatorController> controller)
    -> std::shared_ptr<ASCOMRotatorMain> {
    auto rotator = std::make_shared<ASCOMRotatorMain>(name);
    rotator->setController(controller);
    rotator->initialized_ = true;
    return rotator;
}

auto ASCOMRotatorMain::initialize(const RotatorInitConfig& config) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) {
        return true;
    }

    try {
        current_config_ = config;

        // Create controller if not already set
        if (!controller_) {
            controller_ = createDefaultController();
        }

        if (!controller_) {
            return false;
        }

        // Setup callbacks
        setupCallbacks();

        initialized_ = true;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

auto ASCOMRotatorMain::destroy() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_) {
        return true;
    }

    try {
        // Remove callbacks
        removeCallbacks();

        // Disconnect and destroy controller
        if (controller_) {
            controller_->disconnect();
            controller_.reset();
        }

        initialized_ = false;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

auto ASCOMRotatorMain::isInitialized() const -> bool {
    return initialized_.load();
}

auto ASCOMRotatorMain::connect(const std::string& deviceIdentifier) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    return controller_->connect(deviceIdentifier);
}

auto ASCOMRotatorMain::connectWithConfig(const std::string& deviceIdentifier,
                                       const RotatorInitConfig& config) -> bool {
    // Apply new configuration without replacing the full structure
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Update only the relevant fields
        current_config_.alpaca_host = config.alpaca_host;
        current_config_.alpaca_port = config.alpaca_port;
        current_config_.alpaca_device_number = config.alpaca_device_number;
        current_config_.connection_type = config.connection_type;
        current_config_.com_prog_id = config.com_prog_id;
    }

    return connect(deviceIdentifier);
}

auto ASCOMRotatorMain::disconnect() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    return controller_->disconnect();
}

auto ASCOMRotatorMain::reconnect() -> bool {
    // Since controller doesn't have reconnect method, implement disconnect then connect
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    if (controller_->disconnect()) {
        // Try to reconnect with the last known device identifier
        // For now, this is a simplified implementation
        return controller_->connect(""); // Empty string for default connection
    }

    return false;
}

auto ASCOMRotatorMain::isConnected() const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    return controller_->isConnected();
}

auto ASCOMRotatorMain::getCurrentPosition() -> std::optional<double> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return std::nullopt;
    }

    return controller_->getPosition();
}

auto ASCOMRotatorMain::moveToAngle(double angle) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    return controller_->moveToAngle(angle);
}

auto ASCOMRotatorMain::rotateByAngle(double angle) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto current_pos = controller_->getPosition();
    if (!current_pos.has_value()) {
        return false;
    }

    double target_angle = current_pos.value() + angle;
    // Normalize to 0-360 degrees
    while (target_angle < 0) target_angle += 360.0;
    while (target_angle >= 360.0) target_angle -= 360.0;

    return controller_->moveToAngle(target_angle);
}

auto ASCOMRotatorMain::syncPosition(double angle) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    return controller_->syncPosition(angle);
}

auto ASCOMRotatorMain::abortMove() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    return controller_->abortMove();
}

auto ASCOMRotatorMain::isMoving() const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    return controller_->isMoving();
}

auto ASCOMRotatorMain::setSpeed(double speed) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto position_manager = controller_->getPositionManager();
    if (position_manager) {
        return position_manager->setSpeed(speed);
    }

    return false;
}

auto ASCOMRotatorMain::getSpeed() -> std::optional<double> {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return std::nullopt;
    }

    auto position_manager = controller_->getPositionManager();
    if (position_manager) {
        return position_manager->getSpeed();
    }

    return std::nullopt;
}

auto ASCOMRotatorMain::setReversed(bool reversed) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto position_manager = controller_->getPositionManager();
    if (position_manager) {
        return position_manager->setReversed(reversed);
    }

    return false;
}

auto ASCOMRotatorMain::isReversed() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto position_manager = controller_->getPositionManager();
    if (position_manager) {
        return position_manager->isReversed();
    }

    return false;
}

auto ASCOMRotatorMain::enableBacklashCompensation(bool enable) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto position_manager = controller_->getPositionManager();
    if (position_manager) {
        return position_manager->enableBacklashCompensation(enable);
    }

    return false;
}

auto ASCOMRotatorMain::setBacklashAmount(double amount) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto position_manager = controller_->getPositionManager();
    if (position_manager) {
        return position_manager->setBacklashAmount(amount);
    }

    return false;
}

auto ASCOMRotatorMain::saveCurrentAsPreset(int slot, const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto preset_manager = controller_->getPresetManager();
    if (preset_manager) {
        return preset_manager->saveCurrentPosition(slot, name);
    }

    return false;
}

auto ASCOMRotatorMain::moveToPreset(int slot) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto preset_manager = controller_->getPresetManager();
    if (preset_manager) {
        return preset_manager->moveToPreset(slot);
    }

    return false;
}

auto ASCOMRotatorMain::deletePreset(int slot) -> bool {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return false;
    }

    auto preset_manager = controller_->getPresetManager();
    if (preset_manager) {
        return preset_manager->deletePreset(slot);
    }

    return false;
}

auto ASCOMRotatorMain::getPresetNames() -> std::map<int, std::string> {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, std::string> names;

    if (!controller_) {
        return names;
    }

    auto preset_manager = controller_->getPresetManager();
    if (preset_manager) {
        auto used_slots = preset_manager->getUsedSlots();
        for (int slot : used_slots) {
            auto name = preset_manager->getPresetName(slot);
            if (name.has_value()) {
                names[slot] = name.value();
            }
        }
    }

    return names;
}

auto ASCOMRotatorMain::getLastError() -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!controller_) {
        return "Controller not initialized";
    }

    // Get error from status since controller doesn't have getLastError method
    auto status = controller_->getStatus();
    return status.last_error;
}

auto ASCOMRotatorMain::clearLastError() -> void {
    // Since controller doesn't have clearLastError, this is a no-op for now
    // Errors are managed internally by the controller
}

auto ASCOMRotatorMain::getController() -> std::shared_ptr<ASCOMRotatorController> {
    std::lock_guard<std::mutex> lock(mutex_);
    return controller_;
}

auto ASCOMRotatorMain::setController(std::shared_ptr<ASCOMRotatorController> controller) -> void {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove callbacks from old controller
    if (controller_) {
        removeCallbacks();
    }

    controller_ = controller;

    // Setup callbacks for new controller
    if (controller_ && initialized_) {
        setupCallbacks();
    }
}

// Helper methods
auto ASCOMRotatorMain::setupCallbacks() -> void {
    if (!controller_) {
        return;
    }

    // Position change callback (setPositionCallback takes current and target position)
    controller_->setPositionCallback([this](double current, double target) {
        if (position_changed_callback_) {
            position_changed_callback_(current);
        }
    });

    // Movement state callback (monitors IDLE, MOVING, etc.)
    controller_->setMovementStateCallback([this](components::MovementState state) {
        if (state == components::MovementState::MOVING && movement_started_callback_) {
            movement_started_callback_();
        } else if (state == components::MovementState::IDLE && movement_completed_callback_) {
            movement_completed_callback_();
        }
    });

    // Error callback
    controller_->setErrorCallback([this](const std::string& error) {
        if (error_callback_) {
            error_callback_(error);
        }
    });
}

auto ASCOMRotatorMain::removeCallbacks() -> void {
    if (!controller_) {
        return;
    }

    controller_->setPositionCallback(nullptr);
    controller_->setMovementStateCallback(nullptr);
    controller_->setConnectionCallback(nullptr);
    controller_->setErrorCallback(nullptr);
}

auto ASCOMRotatorMain::createDefaultController() -> std::shared_ptr<ASCOMRotatorController> {
    try {
        // Create modular components with default configuration
        auto hardware = std::make_shared<components::HardwareInterface>(
            current_config_.device_name, "");
        auto position_manager = std::make_shared<components::PositionManager>(hardware);
        auto property_manager = std::make_shared<components::PropertyManager>(hardware, position_manager);
        auto preset_manager = std::make_shared<components::PresetManager>(hardware, position_manager);

        // Create controller
        auto controller = std::make_shared<ASCOMRotatorController>(
            current_config_.device_name, hardware, position_manager, property_manager, preset_manager);

        return controller;
    } catch (const std::exception&) {
        return nullptr;
    }
}

auto ASCOMRotatorMain::validateConfig(const RotatorInitConfig& config) -> bool {
    // Basic validation
    if (config.device_name.empty()) {
        return false;
    }

    if (config.alpaca_port <= 0 || config.alpaca_port > 65535) {
        return false;
    }

    if (config.alpaca_device_number < 0) {
        return false;
    }

    return true;
}

// Event handling methods
auto ASCOMRotatorMain::onPositionChanged(std::function<void(double)> callback) -> void {
    std::lock_guard<std::mutex> lock(mutex_);
    position_changed_callback_ = callback;
}

auto ASCOMRotatorMain::onMovementStarted(std::function<void()> callback) -> void {
    std::lock_guard<std::mutex> lock(mutex_);
    movement_started_callback_ = callback;
}

auto ASCOMRotatorMain::onMovementCompleted(std::function<void()> callback) -> void {
    std::lock_guard<std::mutex> lock(mutex_);
    movement_completed_callback_ = callback;
}

auto ASCOMRotatorMain::onError(std::function<void(const std::string&)> callback) -> void {
    std::lock_guard<std::mutex> lock(mutex_);
    error_callback_ = callback;
}

// Registry implementation
auto ASCOMRotatorRegistry::getInstance() -> ASCOMRotatorRegistry& {
    static ASCOMRotatorRegistry instance;
    return instance;
}

auto ASCOMRotatorRegistry::registerRotator(const std::string& name,
                                         std::shared_ptr<ASCOMRotatorMain> rotator) -> bool {
    std::unique_lock<std::shared_mutex> lock(registry_mutex_);

    if (rotators_.find(name) != rotators_.end()) {
        return false; // Already exists
    }

    rotators_[name] = rotator;
    return true;
}

auto ASCOMRotatorRegistry::unregisterRotator(const std::string& name) -> bool {
    std::unique_lock<std::shared_mutex> lock(registry_mutex_);

    auto it = rotators_.find(name);
    if (it != rotators_.end()) {
        rotators_.erase(it);
        return true;
    }

    return false;
}

auto ASCOMRotatorRegistry::getRotator(const std::string& name) -> std::shared_ptr<ASCOMRotatorMain> {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);

    auto it = rotators_.find(name);
    if (it != rotators_.end()) {
        return it->second;
    }

    return nullptr;
}

auto ASCOMRotatorRegistry::getAllRotators() -> std::map<std::string, std::shared_ptr<ASCOMRotatorMain>> {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);
    return rotators_;
}

auto ASCOMRotatorRegistry::getRotatorNames() -> std::vector<std::string> {
    std::shared_lock<std::shared_mutex> lock(registry_mutex_);

    std::vector<std::string> names;
    names.reserve(rotators_.size());

    for (const auto& [name, rotator] : rotators_) {
        names.push_back(name);
    }

    return names;
}

auto ASCOMRotatorRegistry::clear() -> void {
    std::unique_lock<std::shared_mutex> lock(registry_mutex_);
    rotators_.clear();
}

// Utility functions
namespace utils {

auto createQuickRotator(const std::string& device_identifier)
    -> std::shared_ptr<ASCOMRotatorMain> {
    ASCOMRotatorMain::RotatorInitConfig config;
    config.device_name = "Quick Rotator";

    // Parse device identifier (assuming format: host:port/device_number)
    size_t colon_pos = device_identifier.find(':');
    size_t slash_pos = device_identifier.find('/');

    if (colon_pos != std::string::npos) {
        config.alpaca_host = device_identifier.substr(0, colon_pos);

        if (slash_pos != std::string::npos && slash_pos > colon_pos) {
            try {
                config.alpaca_port = std::stoi(device_identifier.substr(colon_pos + 1, slash_pos - colon_pos - 1));
                config.alpaca_device_number = std::stoi(device_identifier.substr(slash_pos + 1));
            } catch (const std::exception&) {
                // Use defaults
            }
        } else if (slash_pos == std::string::npos) {
            try {
                config.alpaca_port = std::stoi(device_identifier.substr(colon_pos + 1));
            } catch (const std::exception&) {
                // Use defaults
            }
        }
    }

    auto rotator = ASCOMRotatorMain::createRotator("quick_rotator", config);
    if (rotator) {
        rotator->connect(device_identifier);
    }

    return rotator;
}

auto normalizeAngle(double angle) -> double {
    while (angle < 0.0) angle += 360.0;
    while (angle >= 360.0) angle -= 360.0;
    return angle;
}

auto angleDifference(double angle1, double angle2) -> double {
    double diff = angle2 - angle1;
    diff = normalizeAngle(diff);
    if (diff > 180.0) {
        diff -= 360.0;
    }
    return diff;
}

auto shortestRotationPath(double from_angle, double to_angle) -> std::pair<double, bool> {
    double diff = angleDifference(from_angle, to_angle);
    return {std::abs(diff), diff >= 0};
}

auto validateRotatorConfig(const ASCOMRotatorMain::RotatorInitConfig& config) -> bool {
    if (config.device_name.empty()) return false;
    if (config.alpaca_port <= 0 || config.alpaca_port > 65535) return false;
    if (config.alpaca_device_number < 0) return false;
    if (config.position_update_interval_ms <= 0) return false;
    if (config.property_cache_duration_ms <= 0) return false;
    if (config.movement_timeout_ms <= 0) return false;
    return true;
}

auto getDefaultAlpacaConfig() -> ASCOMRotatorMain::RotatorInitConfig {
    ASCOMRotatorMain::RotatorInitConfig config;
    config.connection_type = components::ConnectionType::ALPACA_REST;
    config.alpaca_host = "localhost";
    config.alpaca_port = 11111;
    config.alpaca_device_number = 0;
    return config;
}

auto getDefaultCOMConfig(const std::string& prog_id) -> ASCOMRotatorMain::RotatorInitConfig {
    ASCOMRotatorMain::RotatorInitConfig config;
    config.connection_type = components::ConnectionType::COM_DRIVER;
    config.com_prog_id = prog_id;
    return config;
}

} // namespace utils

} // namespace lithium::device::ascom::rotator
