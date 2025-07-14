#include "main.hpp"
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace lithium::device::ascom::focuser {

// ModuleManager static members
bool ModuleManager::initialized_ = false;
std::vector<std::shared_ptr<Controller>> ModuleManager::controllers_;
std::map<std::string, std::shared_ptr<Controller>> ModuleManager::controller_map_;
bool ModuleManager::logging_enabled_ = true;
int ModuleManager::log_level_ = 0;
std::mutex ModuleManager::controllers_mutex_;

// ConfigManager static members
std::map<std::string, std::string> ConfigManager::config_values_;
std::mutex ConfigManager::config_mutex_;

// ModuleFactory implementation
auto ModuleFactory::getModuleInfo() -> ModuleInfo {
    ModuleInfo info;
    info.name = "ASCOM Focuser";
    info.version = "1.0.0";
    info.description = "Lithium ASCOM Focuser Driver - Modular Architecture";
    info.author = "Max Qian";
    info.contact = "lightapt.com";
    info.license = "MIT";

    // Add supported devices
    info.supportedDevices = {
        "Generic ASCOM Focuser",
        "USB Focuser",
        "Serial Focuser",
        "Network Focuser"
    };

    // Add capabilities
    info.capabilities = {
        {"absolute_positioning", "true"},
        {"relative_positioning", "true"},
        {"temperature_compensation", "true"},
        {"backlash_compensation", "true"},
        {"speed_control", "true"},
        {"position_limits", "true"},
        {"temperature_monitoring", "true"},
        {"property_caching", "true"},
        {"statistics", "true"},
        {"self_test", "true"},
        {"calibration", "true"},
        {"emergency_stop", "true"}
    };

    return info;
}

auto ModuleFactory::createController(const std::string& name) -> std::shared_ptr<Controller> {
    try {
        auto controller = std::make_shared<Controller>(name);

        // Register with module manager
        ModuleManager::registerController(controller);

        return controller;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

auto ModuleFactory::createController(const std::string& name, const ControllerConfig& config) -> std::shared_ptr<Controller> {
    try {
        auto controller = std::make_shared<Controller>(name);

        // Apply configuration
        if (!controller->setControllerConfig(config)) {
            return nullptr;
        }

        // Register with module manager
        ModuleManager::registerController(controller);

        return controller;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

auto ModuleFactory::discoverDevices() -> std::vector<DeviceInfo> {
    std::vector<DeviceInfo> devices;

    // This would typically scan for actual hardware devices
    // For now, return some example devices

    DeviceInfo device1;
    device1.name = "Generic ASCOM Focuser";
    device1.identifier = "ascom.focuser.generic";
    device1.description = "Generic ASCOM compatible focuser";
    device1.manufacturer = "Unknown";
    device1.model = "Generic";
    device1.serialNumber = "N/A";
    device1.firmwareVersion = "1.0.0";
    device1.isConnected = false;
    device1.isAvailable = true;
    device1.properties = {
        {"max_position", "65535"},
        {"min_position", "0"},
        {"step_size", "1.0"},
        {"has_temperature", "false"},
        {"has_backlash", "true"}
    };

    devices.push_back(device1);

    return devices;
}

auto ModuleFactory::isDeviceSupported(const std::string& deviceName) -> bool {
    auto supported = getSupportedDevices();
    return std::find(supported.begin(), supported.end(), deviceName) != supported.end();
}

auto ModuleFactory::getSupportedDevices() -> std::vector<std::string> {
    return {
        "Generic ASCOM Focuser",
        "USB Focuser",
        "Serial Focuser",
        "Network Focuser"
    };
}

auto ModuleFactory::getDeviceCapabilities(const std::string& deviceName) -> std::map<std::string, std::string> {
    std::map<std::string, std::string> capabilities;

    // Return standard capabilities for all devices
    capabilities = {
        {"absolute_positioning", "true"},
        {"relative_positioning", "true"},
        {"temperature_compensation", "true"},
        {"backlash_compensation", "true"},
        {"speed_control", "true"},
        {"position_limits", "true"},
        {"temperature_monitoring", "false"}, // Depends on hardware
        {"property_caching", "true"},
        {"statistics", "true"},
        {"self_test", "true"},
        {"calibration", "true"},
        {"emergency_stop", "true"}
    };

    return capabilities;
}

auto ModuleFactory::validateConfiguration(const ControllerConfig& config) -> bool {
    // Validate configuration parameters
    if (config.deviceName.empty()) {
        return false;
    }

    if (config.connectionTimeout.count() <= 0) {
        return false;
    }

    if (config.movementTimeout.count() <= 0) {
        return false;
    }

    if (config.maxRetries < 0) {
        return false;
    }

    return true;
}

auto ModuleFactory::getDefaultConfiguration() -> ControllerConfig {
    ControllerConfig config;

    config.deviceName = "ASCOM Focuser";
    config.enableTemperatureCompensation = true;
    config.enableBacklashCompensation = true;
    config.enablePositionTracking = true;
    config.enablePropertyCaching = true;
    config.connectionTimeout = std::chrono::seconds(30);
    config.movementTimeout = std::chrono::seconds(60);
    config.temperatureMonitoringInterval = std::chrono::seconds(30);
    config.positionUpdateInterval = std::chrono::milliseconds(100);
    config.propertyUpdateInterval = std::chrono::seconds(1);
    config.maxRetries = 3;
    config.enableLogging = true;
    config.enableStatistics = true;

    return config;
}

// ModuleManager implementation
auto ModuleManager::initialize() -> bool {
    if (initialized_) {
        return true;
    }

    try {
        // Initialize module-level resources
        controllers_.clear();
        controller_map_.clear();

        // Load configuration
        ConfigManager::loadConfiguration("ascom_focuser.conf");

        // Set default logging
        logging_enabled_ = true;
        log_level_ = 0;

        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto ModuleManager::cleanup() -> void {
    if (!initialized_) {
        return;
    }

    try {
        // Cleanup all controllers
        std::lock_guard<std::mutex> lock(controllers_mutex_);

        for (auto& controller : controllers_) {
            if (controller) {
                controller->disconnect();
            }
        }

        controllers_.clear();
        controller_map_.clear();

        // Save configuration
        ConfigManager::saveConfiguration("ascom_focuser.conf");

        initialized_ = false;
    } catch (const std::exception& e) {
        // Log error but continue cleanup
    }
}

auto ModuleManager::isInitialized() -> bool {
    return initialized_;
}

auto ModuleManager::getVersion() -> std::string {
    return "1.0.0";
}

auto ModuleManager::getBuildInfo() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> info;

    info["version"] = getVersion();
    info["build_date"] = __DATE__;
    info["build_time"] = __TIME__;
    info["compiler"] = __VERSION__;
    info["architecture"] = "modular";
    info["components"] = "hardware,movement,temperature,position,backlash,property";

    return info;
}

auto ModuleManager::registerModule() -> bool {
    // Register with the system registry
    return true; // Placeholder
}

auto ModuleManager::unregisterModule() -> void {
    // Unregister from the system registry
}

auto ModuleManager::getActiveControllers() -> std::vector<std::shared_ptr<Controller>> {
    std::lock_guard<std::mutex> lock(controllers_mutex_);
    return controllers_;
}

auto ModuleManager::getController(const std::string& name) -> std::shared_ptr<Controller> {
    std::lock_guard<std::mutex> lock(controllers_mutex_);

    auto it = controller_map_.find(name);
    if (it != controller_map_.end()) {
        return it->second;
    }

    return nullptr;
}

auto ModuleManager::registerController(std::shared_ptr<Controller> controller) -> bool {
    if (!controller) {
        return false;
    }

    std::lock_guard<std::mutex> lock(controllers_mutex_);

    std::string name = controller->getName();

    // Check if controller already exists
    if (controller_map_.find(name) != controller_map_.end()) {
        return false;
    }

    controllers_.push_back(controller);
    controller_map_[name] = controller;

    return true;
}

auto ModuleManager::unregisterController(const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(controllers_mutex_);

    auto it = controller_map_.find(name);
    if (it == controller_map_.end()) {
        return false;
    }

    // Remove from map
    controller_map_.erase(it);

    // Remove from vector
    auto controller = it->second;
    controllers_.erase(std::remove(controllers_.begin(), controllers_.end(), controller), controllers_.end());

    return true;
}

auto ModuleManager::getModuleStatistics() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> stats;

    std::lock_guard<std::mutex> lock(controllers_mutex_);

    stats["total_controllers"] = std::to_string(controllers_.size());
    stats["active_controllers"] = std::to_string(
        std::count_if(controllers_.begin(), controllers_.end(),
                     [](const std::shared_ptr<Controller>& c) { return c->isConnected(); }));
    stats["module_version"] = getVersion();
    stats["initialized"] = initialized_ ? "true" : "false";
    stats["logging_enabled"] = logging_enabled_ ? "true" : "false";
    stats["log_level"] = std::to_string(log_level_);

    return stats;
}

auto ModuleManager::enableLogging(bool enable) -> void {
    logging_enabled_ = enable;
}

auto ModuleManager::isLoggingEnabled() -> bool {
    return logging_enabled_;
}

auto ModuleManager::setLogLevel(int level) -> void {
    log_level_ = level;
}

auto ModuleManager::getLogLevel() -> int {
    return log_level_;
}

// LegacyWrapper implementation
auto LegacyWrapper::createLegacyFocuser(const std::string& name) -> std::shared_ptr<AtomFocuser> {
    auto controller = ModuleFactory::createController(name);
    if (controller) {
        return std::static_pointer_cast<AtomFocuser>(controller);
    }

    return nullptr;
}

auto LegacyWrapper::wrapController(std::shared_ptr<Controller> controller) -> std::shared_ptr<AtomFocuser> {
    if (controller) {
        return std::static_pointer_cast<AtomFocuser>(controller);
    }

    return nullptr;
}

auto LegacyWrapper::isLegacyModeEnabled() -> bool {
    return ConfigManager::getConfigValue("legacy_mode") == "true";
}

auto LegacyWrapper::enableLegacyMode(bool enable) -> void {
    ConfigManager::setConfigValue("legacy_mode", enable ? "true" : "false");
}

auto LegacyWrapper::getLegacyVersion() -> std::string {
    return "1.0.0";
}

auto LegacyWrapper::getLegacyCompatibility() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> compatibility;

    compatibility["interface_version"] = "3";
    compatibility["ascom_version"] = "6.0";
    compatibility["platform_version"] = "6.0";
    compatibility["driver_version"] = "1.0.0";
    compatibility["supported_interfaces"] = "IFocuser,IFocuserV2,IFocuserV3";

    return compatibility;
}

// ConfigManager implementation
auto ConfigManager::loadConfiguration(const std::string& filename) -> bool {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            // Create default configuration
            resetToDefaults();
            return true;
        }

        std::lock_guard<std::mutex> lock(config_mutex_);
        config_values_.clear();

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') {
                continue; // Skip empty lines and comments
            }

            auto pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                // Trim whitespace
                key.erase(key.find_last_not_of(" \t") + 1);
                key.erase(0, key.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));

                config_values_[key] = value;
            }
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto ConfigManager::saveConfiguration(const std::string& filename) -> bool {
    try {
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(config_mutex_);

        file << "# ASCOM Focuser Configuration\n";
        file << "# Generated automatically - do not edit manually\n\n";

        for (const auto& [key, value] : config_values_) {
            file << key << " = " << value << "\n";
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto ConfigManager::getConfigValue(const std::string& key) -> std::string {
    std::lock_guard<std::mutex> lock(config_mutex_);

    auto it = config_values_.find(key);
    if (it != config_values_.end()) {
        return it->second;
    }

    return "";
}

auto ConfigManager::setConfigValue(const std::string& key, const std::string& value) -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_values_[key] = value;
    return true;
}

auto ConfigManager::getAllConfigValues() -> std::map<std::string, std::string> {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_values_;
}

auto ConfigManager::resetToDefaults() -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);

    config_values_.clear();

    // Set default values
    config_values_["device_name"] = "ASCOM Focuser";
    config_values_["enable_temperature_compensation"] = "true";
    config_values_["enable_backlash_compensation"] = "true";
    config_values_["enable_position_tracking"] = "true";
    config_values_["enable_property_caching"] = "true";
    config_values_["connection_timeout"] = "30";
    config_values_["movement_timeout"] = "60";
    config_values_["temperature_monitoring_interval"] = "30";
    config_values_["position_update_interval"] = "100";
    config_values_["property_update_interval"] = "1000";
    config_values_["max_retries"] = "3";
    config_values_["enable_logging"] = "true";
    config_values_["enable_statistics"] = "true";
    config_values_["log_level"] = "0";
    config_values_["legacy_mode"] = "false";

    return true;
}

auto ConfigManager::validateConfiguration() -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);

    // Check required keys
    std::vector<std::string> required_keys = {
        "device_name",
        "connection_timeout",
        "movement_timeout",
        "max_retries"
    };

    for (const auto& key : required_keys) {
        if (config_values_.find(key) == config_values_.end()) {
            return false;
        }
    }

    // Validate specific values
    try {
        int timeout = std::stoi(config_values_["connection_timeout"]);
        if (timeout <= 0) {
            return false;
        }

        int movement_timeout = std::stoi(config_values_["movement_timeout"]);
        if (movement_timeout <= 0) {
            return false;
        }

        int retries = std::stoi(config_values_["max_retries"]);
        if (retries < 0) {
            return false;
        }
    } catch (const std::exception& e) {
        return false;
    }

    return true;
}

auto ConfigManager::getConfigurationSchema() -> std::map<std::string, std::string> {
    std::map<std::string, std::string> schema;

    schema["device_name"] = "string:Device name";
    schema["enable_temperature_compensation"] = "boolean:Enable temperature compensation";
    schema["enable_backlash_compensation"] = "boolean:Enable backlash compensation";
    schema["enable_position_tracking"] = "boolean:Enable position tracking";
    schema["enable_property_caching"] = "boolean:Enable property caching";
    schema["connection_timeout"] = "integer:Connection timeout (seconds)";
    schema["movement_timeout"] = "integer:Movement timeout (seconds)";
    schema["temperature_monitoring_interval"] = "integer:Temperature monitoring interval (seconds)";
    schema["position_update_interval"] = "integer:Position update interval (milliseconds)";
    schema["property_update_interval"] = "integer:Property update interval (milliseconds)";
    schema["max_retries"] = "integer:Maximum retry attempts";
    schema["enable_logging"] = "boolean:Enable logging";
    schema["enable_statistics"] = "boolean:Enable statistics";
    schema["log_level"] = "integer:Log level (0-5)";
    schema["legacy_mode"] = "boolean:Enable legacy compatibility mode";

    return schema;
}

} // namespace lithium::device::ascom::focuser

// C interface implementation
extern "C" {
    const char* lithium_ascom_focuser_get_module_info() {
        static std::string info_str;
        auto info = lithium::device::ascom::focuser::ModuleFactory::getModuleInfo();
        info_str = info.name + " " + info.version + " - " + info.description;
        return info_str.c_str();
    }

    void* lithium_ascom_focuser_create(const char* name) {
        std::string device_name = name ? name : "ASCOM Focuser";
        auto controller = lithium::device::ascom::focuser::ModuleFactory::createController(device_name);
        if (controller) {
            return new std::shared_ptr<lithium::device::ascom::focuser::Controller>(controller);
        }
        return nullptr;
    }

    void lithium_ascom_focuser_destroy(void* instance) {
        if (instance) {
            delete static_cast<std::shared_ptr<lithium::device::ascom::focuser::Controller>*>(instance);
        }
    }

    int lithium_ascom_focuser_initialize() {
        return lithium::device::ascom::focuser::ModuleManager::initialize() ? 1 : 0;
    }

    void lithium_ascom_focuser_cleanup() {
        lithium::device::ascom::focuser::ModuleManager::cleanup();
    }

    const char* lithium_ascom_focuser_get_version() {
        static std::string version = lithium::device::ascom::focuser::ModuleManager::getVersion();
        return version.c_str();
    }

    int lithium_ascom_focuser_discover_devices(char** devices, int max_devices) {
        auto discovered = lithium::device::ascom::focuser::ModuleFactory::discoverDevices();

        int count = std::min(static_cast<int>(discovered.size()), max_devices);
        for (int i = 0; i < count; ++i) {
            if (devices[i]) {
                std::strncpy(devices[i], discovered[i].name.c_str(), 255);
                devices[i][255] = '\0';
            }
        }

        return count;
    }

    int lithium_ascom_focuser_is_device_supported(const char* device_name) {
        std::string name = device_name ? device_name : "";
        return lithium::device::ascom::focuser::ModuleFactory::isDeviceSupported(name) ? 1 : 0;
    }
}
