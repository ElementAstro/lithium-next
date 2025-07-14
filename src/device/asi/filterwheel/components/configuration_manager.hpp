#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace lithium::device::asi::filterwheel {

/**
 * @brief Configuration data for a single filter slot
 */
struct FilterSlotConfig {
    int slot_id;
    std::string name;
    std::string description;
    double focus_offset;        // Focus offset for this filter
    double exposure_multiplier; // Exposure multiplier for this filter
    bool enabled;

    FilterSlotConfig(int id = 0, const std::string& filter_name = "",
                    const std::string& desc = "", double offset = 0.0,
                    double multiplier = 1.0, bool is_enabled = true)
        : slot_id(id), name(filter_name), description(desc)
        , focus_offset(offset), exposure_multiplier(multiplier), enabled(is_enabled) {}
};

/**
 * @brief Profile containing configuration for all filter slots
 */
struct FilterProfile {
    std::string name;
    std::string description;
    std::vector<FilterSlotConfig> slots;
    std::unordered_map<std::string, std::string> metadata;

    FilterProfile(const std::string& profile_name = "Default",
                 const std::string& desc = "Default filter profile")
        : name(profile_name), description(desc) {}
};

/**
 * @brief Manages filterwheel configuration including filter profiles,
 *        slot configurations, and operational settings
 */
class ConfigurationManager {
public:
    ConfigurationManager();
    ~ConfigurationManager();

    // Profile management
    bool createProfile(const std::string& name, const std::string& description = "");
    bool deleteProfile(const std::string& name);
    bool setCurrentProfile(const std::string& name);
    std::string getCurrentProfileName() const;
    std::vector<std::string> getProfileNames() const;
    bool profileExists(const std::string& name) const;

    // Filter slot configuration
    bool setFilterSlot(int slot_id, const FilterSlotConfig& config);
    std::optional<FilterSlotConfig> getFilterSlot(int slot_id) const;
    bool setFilterName(int slot_id, const std::string& name);
    std::string getFilterName(int slot_id) const;
    bool setFocusOffset(int slot_id, double offset);
    double getFocusOffset(int slot_id) const;
    bool setExposureMultiplier(int slot_id, double multiplier);
    double getExposureMultiplier(int slot_id) const;
    bool setSlotEnabled(int slot_id, bool enabled);
    bool isSlotEnabled(int slot_id) const;

    // Operational settings
    void setMoveTimeout(int timeout_ms);
    int getMoveTimeout() const;
    void setAutoFocusCorrection(bool enabled);
    bool isAutoFocusCorrectionEnabled() const;
    void setAutoExposureCorrection(bool enabled);
    bool isAutoExposureCorrectionEnabled() const;

    // Filter discovery
    std::vector<int> getEnabledSlots() const;
    std::vector<FilterSlotConfig> getAllSlots() const;
    int findSlotByName(const std::string& name) const;
    std::vector<std::string> getFilterNames() const;

    // Configuration persistence
    bool saveConfiguration(const std::string& filepath = "");
    bool loadConfiguration(const std::string& filepath = "");
    std::string getDefaultConfigPath() const;

    // Validation
    bool validateConfiguration() const;
    std::vector<std::string> getValidationErrors() const;

    // Reset and defaults
    void resetToDefaults();
    void createDefaultProfile(int slot_count);

private:
    std::unordered_map<std::string, FilterProfile> profiles_;
    std::string current_profile_;

    // Operational settings
    int move_timeout_ms_;
    bool auto_focus_correction_;
    bool auto_exposure_correction_;

    // Default configuration path
    mutable std::string config_path_;

    // Helper methods
    FilterProfile* getCurrentProfile();
    const FilterProfile* getCurrentProfile() const;
    bool isValidSlotId(int slot_id) const;
    void initializeDefaultSettings();
    std::string generateConfigPath() const;
};

} // namespace lithium::device::asi::filterwheel
