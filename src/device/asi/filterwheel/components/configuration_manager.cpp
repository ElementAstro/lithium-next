#include "configuration_manager.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>

namespace lithium::device::asi::filterwheel {

ConfigurationManager::ConfigurationManager()
    : current_profile_("Default")
    , move_timeout_ms_(30000)
    , auto_focus_correction_(true)
    , auto_exposure_correction_(false) {

    initializeDefaultSettings();
    spdlog::info( "ConfigurationManager initialized");
}

ConfigurationManager::~ConfigurationManager() {
    spdlog::info( "ConfigurationManager destroyed");
}

bool ConfigurationManager::createProfile(const std::string& name, const std::string& description) {
    if (name.empty()) {
        spdlog::error( "Profile name cannot be empty");
        return false;
    }

    if (profiles_.find(name) != profiles_.end()) {
        spdlog::warn( "Profile '{}' already exists", name.c_str());
        return false;
    }

    profiles_[name] = FilterProfile(name, description);
    spdlog::info( "Created profile '{}'", name.c_str());
    return true;
}

bool ConfigurationManager::deleteProfile(const std::string& name) {
    if (name == "Default") {
        spdlog::error( "Cannot delete default profile");
        return false;
    }

    auto it = profiles_.find(name);
    if (it == profiles_.end()) {
        spdlog::error( "Profile '{}' not found", name.c_str());
        return false;
    }

    profiles_.erase(it);

    // Switch to default if current profile was deleted
    if (current_profile_ == name) {
        current_profile_ = "Default";
    }

    spdlog::info( "Deleted profile '{}'", name.c_str());
    return true;
}

bool ConfigurationManager::setCurrentProfile(const std::string& name) {
    if (profiles_.find(name) == profiles_.end()) {
        spdlog::error( "Profile '{}' not found", name.c_str());
        return false;
    }

    current_profile_ = name;
    spdlog::info( "Set current profile to '{}'", name.c_str());
    return true;
}

std::string ConfigurationManager::getCurrentProfileName() const {
    return current_profile_;
}

std::vector<std::string> ConfigurationManager::getProfileNames() const {
    std::vector<std::string> names;
    for (const auto& [name, profile] : profiles_) {
        names.push_back(name);
    }
    return names;
}

bool ConfigurationManager::profileExists(const std::string& name) const {
    return profiles_.find(name) != profiles_.end();
}

bool ConfigurationManager::setFilterSlot(int slot_id, const FilterSlotConfig& config) {
    if (!isValidSlotId(slot_id)) {
        spdlog::error( "Invalid slot ID: {}", slot_id);
        return false;
    }

    FilterProfile* profile = getCurrentProfile();
    if (!profile) {
        spdlog::error( "No current profile available");
        return false;
    }

    // Ensure slots vector is large enough
    if (static_cast<size_t>(slot_id) >= profile->slots.size()) {
        profile->slots.resize(slot_id + 1);
    }

    profile->slots[slot_id] = config;
    profile->slots[slot_id].slot_id = slot_id; // Ensure slot ID is correct

    spdlog::info( "Set filter slot {}: name='{}', offset={:.2f}",
          slot_id, config.name.c_str(), config.focus_offset);
    return true;
}

std::optional<FilterSlotConfig> ConfigurationManager::getFilterSlot(int slot_id) const {
    if (!isValidSlotId(slot_id)) {
        return std::nullopt;
    }

    const FilterProfile* profile = getCurrentProfile();
    if (!profile || static_cast<size_t>(slot_id) >= profile->slots.size()) {
        return std::nullopt;
    }

    return profile->slots[slot_id];
}

bool ConfigurationManager::setFilterName(int slot_id, const std::string& name) {
    auto slot_config = getFilterSlot(slot_id);
    if (!slot_config) {
        // Create new slot config if it doesn't exist
        slot_config = FilterSlotConfig(slot_id, name);
    } else {
        slot_config->name = name;
    }

    return setFilterSlot(slot_id, *slot_config);
}

std::string ConfigurationManager::getFilterName(int slot_id) const {
    auto slot_config = getFilterSlot(slot_id);
    if (slot_config) {
        return slot_config->name;
    }
    return "Slot " + std::to_string(slot_id);
}

bool ConfigurationManager::setFocusOffset(int slot_id, double offset) {
    auto slot_config = getFilterSlot(slot_id);
    if (!slot_config) {
        slot_config = FilterSlotConfig(slot_id);
    }

    slot_config->focus_offset = offset;
    return setFilterSlot(slot_id, *slot_config);
}

double ConfigurationManager::getFocusOffset(int slot_id) const {
    auto slot_config = getFilterSlot(slot_id);
    if (slot_config) {
        return slot_config->focus_offset;
    }
    return 0.0;
}

bool ConfigurationManager::setExposureMultiplier(int slot_id, double multiplier) {
    auto slot_config = getFilterSlot(slot_id);
    if (!slot_config) {
        slot_config = FilterSlotConfig(slot_id);
    }

    slot_config->exposure_multiplier = multiplier;
    return setFilterSlot(slot_id, *slot_config);
}

double ConfigurationManager::getExposureMultiplier(int slot_id) const {
    auto slot_config = getFilterSlot(slot_id);
    if (slot_config) {
        return slot_config->exposure_multiplier;
    }
    return 1.0;
}

bool ConfigurationManager::setSlotEnabled(int slot_id, bool enabled) {
    auto slot_config = getFilterSlot(slot_id);
    if (!slot_config) {
        slot_config = FilterSlotConfig(slot_id);
    }

    slot_config->enabled = enabled;
    return setFilterSlot(slot_id, *slot_config);
}

bool ConfigurationManager::isSlotEnabled(int slot_id) const {
    auto slot_config = getFilterSlot(slot_id);
    if (slot_config) {
        return slot_config->enabled;
    }
    return true; // Default to enabled
}

void ConfigurationManager::setMoveTimeout(int timeout_ms) {
    move_timeout_ms_ = timeout_ms;
    spdlog::info( "Move timeout set to {} ms", timeout_ms);
}

int ConfigurationManager::getMoveTimeout() const {
    return move_timeout_ms_;
}

void ConfigurationManager::setAutoFocusCorrection(bool enabled) {
    auto_focus_correction_ = enabled;
    spdlog::info( "Auto focus correction {}", enabled ? "enabled" : "disabled");
}

bool ConfigurationManager::isAutoFocusCorrectionEnabled() const {
    return auto_focus_correction_;
}

void ConfigurationManager::setAutoExposureCorrection(bool enabled) {
    auto_exposure_correction_ = enabled;
    spdlog::info( "Auto exposure correction {}", enabled ? "enabled" : "disabled");
}

bool ConfigurationManager::isAutoExposureCorrectionEnabled() const {
    return auto_exposure_correction_;
}

std::vector<int> ConfigurationManager::getEnabledSlots() const {
    std::vector<int> enabled_slots;
    const FilterProfile* profile = getCurrentProfile();

    if (profile) {
        for (size_t i = 0; i < profile->slots.size(); ++i) {
            if (profile->slots[i].enabled) {
                enabled_slots.push_back(static_cast<int>(i));
            }
        }
    }

    return enabled_slots;
}

std::vector<FilterSlotConfig> ConfigurationManager::getAllSlots() const {
    const FilterProfile* profile = getCurrentProfile();
    if (profile) {
        return profile->slots;
    }
    return {};
}

int ConfigurationManager::findSlotByName(const std::string& name) const {
    const FilterProfile* profile = getCurrentProfile();
    if (!profile) {
        return -1;
    }

    for (size_t i = 0; i < profile->slots.size(); ++i) {
        if (profile->slots[i].name == name) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

std::vector<std::string> ConfigurationManager::getFilterNames() const {
    std::vector<std::string> names;
    const FilterProfile* profile = getCurrentProfile();

    if (profile) {
        for (const auto& slot : profile->slots) {
            names.push_back(slot.name.empty() ? "Slot " + std::to_string(slot.slot_id) : slot.name);
        }
    }

    return names;
}

bool ConfigurationManager::saveConfiguration(const std::string& filepath) {
    std::string path = filepath.empty() ? getDefaultConfigPath() : filepath;

    try {
        // Create directory if it doesn't exist
        std::filesystem::path file_path(path);
        std::filesystem::create_directories(file_path.parent_path());

        // Write to file in simple format
        std::ofstream file(path);
        if (!file.is_open()) {
            spdlog::error( "Failed to open config file for writing: {}", path.c_str());
            return false;
        }

        // Write header
        file << "# ASI Filterwheel Configuration\n";
        file << "# Generated automatically - do not edit manually\n\n";

        // Write settings
        file << "[settings]\n";
        file << "move_timeout_ms=" << move_timeout_ms_ << "\n";
        file << "auto_focus_correction=" << (auto_focus_correction_ ? "true" : "false") << "\n";
        file << "auto_exposure_correction=" << (auto_exposure_correction_ ? "true" : "false") << "\n";
        file << "current_profile=" << current_profile_ << "\n\n";

        // Write profiles
        for (const auto& [name, profile] : profiles_) {
            file << "[profile:" << name << "]\n";
            file << "name=" << profile.name << "\n";
            file << "description=" << profile.description << "\n";

            // Write slots
            for (const auto& slot : profile.slots) {
                file << "slot_" << slot.slot_id << "_name=" << slot.name << "\n";
                file << "slot_" << slot.slot_id << "_description=" << slot.description << "\n";
                file << "slot_" << slot.slot_id << "_focus_offset=" << slot.focus_offset << "\n";
                file << "slot_" << slot.slot_id << "_exposure_multiplier=" << slot.exposure_multiplier << "\n";
                file << "slot_" << slot.slot_id << "_enabled=" << (slot.enabled ? "true" : "false") << "\n";
            }
            file << "\n";
        }

        spdlog::info( "Configuration saved to: {}", path.c_str());
        return true;

    } catch (const std::exception& e) {
        spdlog::error( "Failed to save configuration: {}", e.what());
        return false;
    }
}

bool ConfigurationManager::loadConfiguration(const std::string& filepath) {
    std::string path = filepath.empty() ? getDefaultConfigPath() : filepath;

    if (!std::filesystem::exists(path)) {
        spdlog::warn( "Configuration file not found: {}", path.c_str());
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::error( "Failed to open config file for reading: {}", path.c_str());
            return false;
        }

        std::string line;
        std::string current_section;
        FilterProfile* current_profile = nullptr;

        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Check for section headers
            if (line[0] == '[' && line.back() == ']') {
                current_section = line.substr(1, line.length() - 2);

                if (current_section.starts_with("profile:")) {
                    std::string profile_name = current_section.substr(8);
                    profiles_[profile_name] = FilterProfile(profile_name);
                    current_profile = &profiles_[profile_name];
                } else {
                    current_profile = nullptr;
                }
                continue;
            }

            // Parse key=value pairs
            size_t pos = line.find('=');
            if (pos == std::string::npos) {
                continue;
            }

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Handle settings section
            if (current_section == "settings") {
                if (key == "move_timeout_ms") {
                    move_timeout_ms_ = std::stoi(value);
                } else if (key == "auto_focus_correction") {
                    auto_focus_correction_ = (value == "true");
                } else if (key == "auto_exposure_correction") {
                    auto_exposure_correction_ = (value == "true");
                } else if (key == "current_profile") {
                    current_profile_ = value;
                }
            }
            // Handle profile sections
            else if (current_profile && current_section.starts_with("profile:")) {
                if (key == "name") {
                    current_profile->name = value;
                } else if (key == "description") {
                    current_profile->description = value;
                } else if (key.starts_with("slot_")) {
                    // Parse slot configuration
                    size_t first_underscore = key.find('_', 5);
                    if (first_underscore != std::string::npos) {
                        int slot_id = std::stoi(key.substr(5, first_underscore - 5));
                        std::string slot_key = key.substr(first_underscore + 1);

                        // Ensure slots vector is large enough
                        if (static_cast<size_t>(slot_id) >= current_profile->slots.size()) {
                            current_profile->slots.resize(slot_id + 1);
                            current_profile->slots[slot_id].slot_id = slot_id;
                        }

                        if (slot_key == "name") {
                            current_profile->slots[slot_id].name = value;
                        } else if (slot_key == "description") {
                            current_profile->slots[slot_id].description = value;
                        } else if (slot_key == "focus_offset") {
                            current_profile->slots[slot_id].focus_offset = std::stod(value);
                        } else if (slot_key == "exposure_multiplier") {
                            current_profile->slots[slot_id].exposure_multiplier = std::stod(value);
                        } else if (slot_key == "enabled") {
                            current_profile->slots[slot_id].enabled = (value == "true");
                        }
                    }
                }
            }
        }

        spdlog::info( "Configuration loaded from: {}", path.c_str());
        return true;

    } catch (const std::exception& e) {
        spdlog::error( "Failed to load configuration: {}", e.what());
        return false;
    }
}

std::string ConfigurationManager::getDefaultConfigPath() const {
    if (config_path_.empty()) {
        config_path_ = generateConfigPath();
    }
    return config_path_;
}

bool ConfigurationManager::validateConfiguration() const {
    // Basic validation - can be extended
    if (profiles_.empty()) {
        return false;
    }

    if (profiles_.find(current_profile_) == profiles_.end()) {
        return false;
    }

    return true;
}

std::vector<std::string> ConfigurationManager::getValidationErrors() const {
    std::vector<std::string> errors;

    if (profiles_.empty()) {
        errors.push_back("No profiles defined");
    }

    if (profiles_.find(current_profile_) == profiles_.end()) {
        errors.push_back("Current profile '" + current_profile_ + "' not found");
    }

    return errors;
}

void ConfigurationManager::resetToDefaults() {
    profiles_.clear();
    current_profile_ = "Default";
    move_timeout_ms_ = 30000;
    auto_focus_correction_ = true;
    auto_exposure_correction_ = false;

    initializeDefaultSettings();
    spdlog::info( "Configuration reset to defaults");
}

void ConfigurationManager::createDefaultProfile(int slot_count) {
    FilterProfile default_profile("Default", "Default filter profile");

    for (int i = 0; i < slot_count; ++i) {
        FilterSlotConfig slot(i, "Filter " + std::to_string(i + 1),
                            "Default filter slot " + std::to_string(i + 1));
        default_profile.slots.push_back(slot);
    }

    profiles_["Default"] = default_profile;
    current_profile_ = "Default";

    spdlog::info( "Created default profile with {} slots", slot_count);
}

FilterProfile* ConfigurationManager::getCurrentProfile() {
    auto it = profiles_.find(current_profile_);
    return (it != profiles_.end()) ? &it->second : nullptr;
}

const FilterProfile* ConfigurationManager::getCurrentProfile() const {
    auto it = profiles_.find(current_profile_);
    return (it != profiles_.end()) ? &it->second : nullptr;
}

bool ConfigurationManager::isValidSlotId(int slot_id) const {
    return slot_id >= 0 && slot_id < 32; // Reasonable upper limit
}

void ConfigurationManager::initializeDefaultSettings() {
    if (profiles_.empty()) {
        createDefaultProfile(8); // Default 8-slot filterwheel
    }
}

std::string ConfigurationManager::generateConfigPath() const {
    std::filesystem::path config_dir;

    // Try to use XDG config directory or fallback to home
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        config_dir = std::filesystem::path(xdg_config) / "lithium";
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            config_dir = std::filesystem::path(home) / ".config" / "lithium";
        } else {
            config_dir = std::filesystem::current_path() / "config";
        }
    }

    return (config_dir / "asi_filterwheel_config.json").string();
}

} // namespace lithium::device::asi::filterwheel
