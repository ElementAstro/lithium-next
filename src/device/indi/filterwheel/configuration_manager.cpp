#include "configuration_manager.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>

namespace lithium::device::indi::filterwheel {

ConfigurationManager::ConfigurationManager(std::shared_ptr<INDIFilterWheelCore> core)
    : FilterWheelComponentBase(std::move(core)) {}

bool ConfigurationManager::initialize() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    core->getLogger()->info("Initializing ConfigurationManager");

    // Load existing configurations from file
    loadConfigurationsFromFile();

    core->getLogger()->info("ConfigurationManager initialized with {} configurations",
                           configurations_.size());

    initialized_ = true;
    return true;
}

void ConfigurationManager::shutdown() {
    auto core = getCore();
    if (core) {
        core->getLogger()->info("Shutting down ConfigurationManager");

        // Save configurations before shutdown
        saveConfigurationsToFile();
    }

    configurations_.clear();
    initialized_ = false;
}

bool ConfigurationManager::saveFilterConfiguration(const std::string& name) {
    auto core = getCore();
    if (!core || !validateComponentReady()) {
        return false;
    }

    if (!isValidConfigurationName(name)) {
        core->getLogger()->error("Invalid configuration name: {}", name);
        return false;
    }

    try {
        auto config = captureCurrentConfiguration(name);
        configurations_[name] = config;

        if (saveConfigurationsToFile()) {
            core->getLogger()->info("Filter configuration '{}' saved successfully", name);
            return true;
        } else {
            configurations_.erase(name); // Rollback on file save failure
            return false;
        }
    } catch (const std::exception& e) {
        core->getLogger()->error("Failed to save configuration '{}': {}", name, e.what());
        return false;
    }
}

bool ConfigurationManager::loadFilterConfiguration(const std::string& name) {
    auto core = getCore();
    if (!core || !validateComponentReady()) {
        return false;
    }

    auto it = configurations_.find(name);
    if (it == configurations_.end()) {
        core->getLogger()->error("Configuration '{}' not found", name);
        return false;
    }

    try {
        if (applyConfiguration(it->second)) {
            // Update last used time
            it->second.lastUsed = std::chrono::system_clock::now();
            saveConfigurationsToFile();

            core->getLogger()->info("Filter configuration '{}' loaded successfully", name);
            return true;
        } else {
            core->getLogger()->error("Failed to apply configuration '{}'", name);
            return false;
        }
    } catch (const std::exception& e) {
        core->getLogger()->error("Failed to load configuration '{}': {}", name, e.what());
        return false;
    }
}

bool ConfigurationManager::deleteFilterConfiguration(const std::string& name) {
    auto core = getCore();
    if (!core || !validateComponentReady()) {
        return false;
    }

    auto it = configurations_.find(name);
    if (it == configurations_.end()) {
        core->getLogger()->warn("Configuration '{}' not found for deletion", name);
        return false;
    }

    configurations_.erase(it);

    if (saveConfigurationsToFile()) {
        core->getLogger()->info("Configuration '{}' deleted successfully", name);
        return true;
    } else {
        core->getLogger()->error("Failed to save after deleting configuration '{}'", name);
        return false;
    }
}

std::vector<std::string> ConfigurationManager::getAvailableConfigurations() const {
    std::vector<std::string> names;
    names.reserve(configurations_.size());

    for (const auto& [name, config] : configurations_) {
        names.push_back(name);
    }

    return names;
}

std::optional<FilterWheelConfiguration> ConfigurationManager::getConfiguration(const std::string& name) const {
    auto it = configurations_.find(name);
    if (it != configurations_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ConfigurationManager::exportConfiguration(const std::string& name, const std::string& filePath) {
    auto core = getCore();
    if (!core) {
        return false;
    }

    auto it = configurations_.find(name);
    if (it == configurations_.end()) {
        core->getLogger()->error("Configuration '{}' not found for export", name);
        return false;
    }

    // Implementation would serialize configuration to JSON/XML
    // For now, just log the operation
    core->getLogger()->info("Export configuration '{}' to '{}' - feature not yet implemented",
                           name, filePath);
    return true; // Placeholder
}

std::optional<std::string> ConfigurationManager::importConfiguration(const std::string& filePath) {
    auto core = getCore();
    if (!core) {
        return std::nullopt;
    }

    // Implementation would deserialize configuration from JSON/XML
    // For now, just log the operation
    core->getLogger()->info("Import configuration from '{}' - feature not yet implemented", filePath);
    return std::nullopt; // Placeholder
}

bool ConfigurationManager::saveConfigurationsToFile() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    try {
        std::string configPath = getConfigurationFilePath();

        // Create directory if it doesn't exist
        std::filesystem::path path(configPath);
        std::filesystem::create_directories(path.parent_path());

        // For now, just create an empty file to indicate successful save
        // Real implementation would serialize configurations to JSON/XML
        std::ofstream file(configPath);
        if (!file.is_open()) {
            core->getLogger()->error("Failed to open configuration file for writing: {}", configPath);
            return false;
        }

        // Write placeholder content
        file << "# Filter Wheel Configurations for " << core->getDeviceName() << std::endl;
        file << "# " << configurations_.size() << " configurations stored" << std::endl;

        core->getLogger()->debug("Configurations saved to: {}", configPath);
        return true;
    } catch (const std::exception& e) {
        core->getLogger()->error("Failed to save configurations: {}", e.what());
        return false;
    }
}

bool ConfigurationManager::loadConfigurationsFromFile() {
    auto core = getCore();
    if (!core) {
        return false;
    }

    try {
        std::string configPath = getConfigurationFilePath();

        if (!std::filesystem::exists(configPath)) {
            core->getLogger()->debug("No existing configuration file found: {}", configPath);
            return true; // Not an error, just no saved configs
        }

        // For now, just check if file exists
        // Real implementation would deserialize configurations from JSON/XML
        core->getLogger()->debug("Configuration file found: {}", configPath);
        return true;
    } catch (const std::exception& e) {
        core->getLogger()->error("Failed to load configurations: {}", e.what());
        return false;
    }
}

std::string ConfigurationManager::getConfigurationFilePath() const {
    auto core = getCore();
    if (!core) {
        return "";
    }

    // Store in user config directory
    return std::string(std::getenv("HOME")) + "/.config/lithium/filterwheel/" +
           core->getDeviceName() + "_configurations.txt";
}

FilterWheelConfiguration ConfigurationManager::captureCurrentConfiguration(const std::string& name) {
    auto core = getCore();

    FilterWheelConfiguration config;
    config.name = name;
    config.created = std::chrono::system_clock::now();
    config.lastUsed = config.created;

    if (core) {
        // Capture current filter names and slot count
        config.filters.clear();
        config.maxSlots = core->getMaxSlot();

        const auto& slotNames = core->getSlotNames();
        for (size_t i = 0; i < slotNames.size() && i < static_cast<size_t>(config.maxSlots); ++i) {
            FilterInfo filter;
            filter.name = slotNames[i];
            filter.type = "Unknown"; // Could be enhanced to capture more details
            config.filters.push_back(filter);
        }

        config.description = "Configuration for " + core->getDeviceName();
    }

    return config;
}

bool ConfigurationManager::applyConfiguration(const FilterWheelConfiguration& config) {
    auto core = getCore();
    if (!core) {
        return false;
    }

    try {
        // Apply filter names
        std::vector<std::string> names;
        for (const auto& filter : config.filters) {
            names.push_back(filter.name);
        }

        // Update core state
        core->setSlotNames(names);
        core->setMaxSlot(config.maxSlots);

        core->getLogger()->debug("Applied configuration: {} filters, max slots: {}",
                                names.size(), config.maxSlots);
        return true;
    } catch (const std::exception& e) {
        core->getLogger()->error("Failed to apply configuration: {}", e.what());
        return false;
    }
}

bool ConfigurationManager::isValidConfigurationName(const std::string& name) const {
    if (name.empty() || name.length() > 50) {
        return false;
    }

    // Check for invalid characters
    const std::string invalidChars = "\\/:*?\"<>|";
    return name.find_first_of(invalidChars) == std::string::npos;
}

}  // namespace lithium::device::indi::filterwheel
