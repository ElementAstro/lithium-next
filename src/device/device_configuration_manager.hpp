/*
 * device_configuration_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-29

Description: Advanced Device Configuration Management with versioning and validation

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

namespace lithium {

// Configuration value types
enum class ConfigValueType {
    BOOLEAN,
    INTEGER,
    DOUBLE,
    STRING,
    ARRAY,
    OBJECT,
    BINARY
};

// Configuration validation level
enum class ValidationLevel {
    NONE,
    BASIC,
    STRICT,
    CUSTOM
};

// Configuration source
enum class ConfigSource {
    DEFAULT,
    FILE,
    DATABASE,
    NETWORK,
    USER_INPUT,
    ENVIRONMENT,
    COMMAND_LINE
};

// Configuration change type
enum class ConfigChangeType {
    ADDED,
    MODIFIED,
    REMOVED,
    RESET,
    IMPORTED,
    MIGRATED
};

// Configuration value with metadata
struct ConfigValue {
    std::string key;
    std::string value;
    ConfigValueType type{ConfigValueType::STRING};
    ConfigSource source{ConfigSource::DEFAULT};
    
    std::string description;
    std::string unit;
    std::string default_value;
    
    bool is_readonly{false};
    bool is_sensitive{false};
    bool requires_restart{false};
    bool is_deprecated{false};
    
    std::string min_value;
    std::string max_value;
    std::vector<std::string> allowed_values;
    std::string validation_pattern;
    
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point modified_at;
    std::string modified_by;
    
    std::unordered_map<std::string, std::string> metadata;
    int version{1};
    std::string checksum;
};

// Configuration section
struct ConfigSection {
    std::string name;
    std::string description;
    std::unordered_map<std::string, ConfigValue> values;
    
    bool is_readonly{false};
    bool is_system{false};
    int priority{0};
    
    std::vector<std::string> dependencies;
    std::vector<std::string> conflicts;
    
    std::function<bool(const ConfigSection&)> validator;
    std::function<void(const ConfigSection&)> change_handler;
};

// Configuration profile
struct ConfigProfile {
    std::string name;
    std::string description;
    std::string version;
    std::string author;
    
    std::unordered_map<std::string, ConfigSection> sections;
    
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point modified_at;
    
    bool is_default{false};
    bool is_system{false};
    bool is_locked{false};
    
    std::vector<std::string> tags;
    std::unordered_map<std::string, std::string> metadata;
};

// Configuration change record
struct ConfigChangeRecord {
    std::string device_name;
    std::string key;
    std::string old_value;
    std::string new_value;
    ConfigChangeType change_type;
    
    std::chrono::system_clock::time_point timestamp;
    std::string changed_by;
    std::string reason;
    std::string session_id;
    
    bool was_successful{true};
    std::string error_message;
    
    ConfigSource source{ConfigSource::USER_INPUT};
    std::string source_detail;
};

// Configuration validation result
struct ConfigValidationResult {
    bool is_valid{true};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions;
    
    std::unordered_map<std::string, std::string> fixed_values;
    std::vector<std::string> deprecated_keys;
    std::vector<std::string> missing_required_keys;
};

// Configuration manager settings
struct ConfigManagerSettings {
    std::string config_directory{"./config"};
    std::string backup_directory{"./config/backups"};
    std::string cache_directory{"./config/cache"};
    
    ValidationLevel validation_level{ValidationLevel::STRICT};
    bool enable_auto_backup{true};
    bool enable_change_tracking{true};
    bool enable_encryption{false};
    bool enable_compression{true};
    
    size_t max_backup_count{10};
    size_t max_change_history{1000};
    std::chrono::seconds auto_save_interval{300};
    std::chrono::seconds cache_ttl{3600};
    
    std::string encryption_key;
    std::string config_file_extension{".json"};
    std::string backup_file_extension{".bak"};
};

class DeviceConfigurationManager {
public:
    DeviceConfigurationManager();
    explicit DeviceConfigurationManager(const ConfigManagerSettings& settings);
    ~DeviceConfigurationManager();
    
    // Configuration manager setup
    void setSettings(const ConfigManagerSettings& settings);
    ConfigManagerSettings getSettings() const;
    
    bool initialize();
    void shutdown();
    bool isInitialized() const;
    
    // Device configuration management
    bool createDeviceConfig(const std::string& device_name, const ConfigProfile& profile);
    bool loadDeviceConfig(const std::string& device_name, const std::string& file_path = "");
    bool saveDeviceConfig(const std::string& device_name, const std::string& file_path = "");
    bool deleteDeviceConfig(const std::string& device_name);
    
    std::vector<std::string> getConfiguredDevices() const;
    bool isDeviceConfigured(const std::string& device_name) const;
    
    // Configuration value operations
    bool setValue(const std::string& device_name, const std::string& key, 
                 const std::string& value, ConfigSource source = ConfigSource::USER_INPUT);
    
    std::string getValue(const std::string& device_name, const std::string& key, 
                        const std::string& default_value = "") const;
    
    bool hasValue(const std::string& device_name, const std::string& key) const;
    bool removeValue(const std::string& device_name, const std::string& key);
    
    // Typed value operations
    bool setBoolValue(const std::string& device_name, const std::string& key, bool value);
    bool getBoolValue(const std::string& device_name, const std::string& key, bool default_value = false) const;
    
    bool setIntValue(const std::string& device_name, const std::string& key, int value);
    int getIntValue(const std::string& device_name, const std::string& key, int default_value = 0) const;
    
    bool setDoubleValue(const std::string& device_name, const std::string& key, double value);
    double getDoubleValue(const std::string& device_name, const std::string& key, double default_value = 0.0) const;
    
    // Batch operations
    bool setMultipleValues(const std::string& device_name, 
                          const std::unordered_map<std::string, std::string>& values,
                          ConfigSource source = ConfigSource::USER_INPUT);
    
    std::unordered_map<std::string, std::string> getMultipleValues(
        const std::string& device_name, const std::vector<std::string>& keys) const;
    
    // Configuration sections
    bool addSection(const std::string& device_name, const ConfigSection& section);
    bool removeSection(const std::string& device_name, const std::string& section_name);
    ConfigSection getSection(const std::string& device_name, const std::string& section_name) const;
    std::vector<std::string> getSectionNames(const std::string& device_name) const;
    
    // Configuration profiles
    bool createProfile(const ConfigProfile& profile);
    bool saveProfile(const std::string& profile_name, const std::string& file_path = "");
    bool loadProfile(const std::string& profile_name, const std::string& file_path = "");
    bool deleteProfile(const std::string& profile_name);
    
    ConfigProfile getProfile(const std::string& profile_name) const;
    std::vector<std::string> getAvailableProfiles() const;
    
    bool applyProfile(const std::string& device_name, const std::string& profile_name);
    bool createProfileFromDevice(const std::string& device_name, const std::string& profile_name);
    
    // Configuration validation
    ConfigValidationResult validateDeviceConfig(const std::string& device_name) const;
    ConfigValidationResult validateProfile(const std::string& profile_name) const;
    ConfigValidationResult validateValue(const std::string& device_name, 
                                       const std::string& key, 
                                       const std::string& value) const;
    
    // Validation rules
    void addValidationRule(const std::string& key, std::function<bool(const std::string&)> validator);
    void removeValidationRule(const std::string& key);
    void clearValidationRules();
    
    // Configuration templates
    bool createTemplate(const std::string& template_name, const ConfigProfile& profile);
    bool applyTemplate(const std::string& device_name, const std::string& template_name);
    std::vector<std::string> getAvailableTemplates() const;
    
    // Configuration migration
    bool migrateConfig(const std::string& device_name, const std::string& from_version, 
                      const std::string& to_version);
    void addMigrationRule(const std::string& from_version, const std::string& to_version,
                         std::function<bool(ConfigProfile&)> migrator);
    
    // Configuration backup and restore
    std::string createBackup(const std::string& device_name = "");
    bool restoreBackup(const std::string& backup_id, const std::string& device_name = "");
    std::vector<std::string> getAvailableBackups() const;
    bool deleteBackup(const std::string& backup_id);
    
    // Change tracking
    std::vector<ConfigChangeRecord> getChangeHistory(const std::string& device_name, 
                                                   size_t max_records = 100) const;
    void clearChangeHistory(const std::string& device_name = "");
    
    // Configuration comparison
    struct ConfigDifference {
        std::string key;
        std::string old_value;
        std::string new_value;
        ConfigChangeType change_type;
    };
    
    std::vector<ConfigDifference> compareConfigs(const std::string& device1, 
                                               const std::string& device2) const;
    std::vector<ConfigDifference> compareWithProfile(const std::string& device_name, 
                                                   const std::string& profile_name) const;
    
    // Configuration synchronization
    bool syncWithRemote(const std::string& remote_url, const std::string& device_name = "");
    bool pushToRemote(const std::string& remote_url, const std::string& device_name = "");
    bool pullFromRemote(const std::string& remote_url, const std::string& device_name = "");
    
    // Configuration export/import
    std::string exportConfig(const std::string& device_name, const std::string& format = "json") const;
    bool importConfig(const std::string& device_name, const std::string& config_data, 
                     const std::string& format = "json");
    
    // Configuration monitoring
    void enableConfigMonitoring(bool enable);
    bool isConfigMonitoringEnabled() const;
    
    using ConfigChangeCallback = std::function<void(const ConfigChangeRecord&)>;
    using ConfigErrorCallback = std::function<void(const std::string&, const std::string&)>;
    
    void setConfigChangeCallback(ConfigChangeCallback callback);
    void setConfigErrorCallback(ConfigErrorCallback callback);
    
    // Configuration caching
    void enableCaching(bool enable);
    bool isCachingEnabled() const;
    void clearCache(const std::string& device_name = "");
    void refreshCache(const std::string& device_name = "");
    
    // Configuration search
    std::vector<std::string> searchKeys(const std::string& pattern) const;
    std::vector<std::string> searchValues(const std::string& pattern) const;
    std::unordered_map<std::string, std::string> findKeysWithValue(const std::string& value) const;
    
    // Configuration statistics
    struct ConfigStatistics {
        size_t total_devices{0};
        size_t total_keys{0};
        size_t total_sections{0};
        size_t total_profiles{0};
        size_t total_changes{0};
        size_t total_backups{0};
        
        std::chrono::system_clock::time_point last_modified;
        std::chrono::system_clock::time_point last_backup;
        
        std::unordered_map<ConfigSource, size_t> changes_by_source;
        std::unordered_map<ConfigChangeType, size_t> changes_by_type;
    };
    
    ConfigStatistics getStatistics() const;
    void resetStatistics();
    
    // Configuration optimization
    void optimizeStorage();
    void compactChangeHistory();
    void cleanupOldBackups();
    
    // Debugging and diagnostics
    std::string getManagerStatus() const;
    std::string getDeviceConfigInfo(const std::string& device_name) const;
    void dumpConfigData(const std::string& output_path) const;
    
    // Maintenance
    void runMaintenance();
    bool validateIntegrity();
    bool repairCorruption();
    
private:
    class Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // Internal methods
    void setupDefaultValidators();
    void processConfigQueue();
    void performAutoBackup();
    void monitorConfigChanges();
    
    // Validation helpers
    bool validateConfigKey(const std::string& key) const;
    bool validateConfigValue(const ConfigValue& value) const;
    bool validateConfigSection(const ConfigSection& section) const;
    
    // Serialization helpers
    std::string serializeProfile(const ConfigProfile& profile) const;
    ConfigProfile deserializeProfile(const std::string& data) const;
    
    // Security helpers
    std::string encryptValue(const std::string& value) const;
    std::string decryptValue(const std::string& encrypted_value) const;
    std::string calculateChecksum(const std::string& data) const;
    
    // File system helpers
    std::string getDeviceConfigPath(const std::string& device_name) const;
    std::string getProfilePath(const std::string& profile_name) const;
    std::string getBackupPath(const std::string& backup_id) const;
};

// Utility functions
namespace config_utils {
    std::string valueTypeToString(ConfigValueType type);
    ConfigValueType stringToValueType(const std::string& type_str);
    
    std::string sourceToString(ConfigSource source);
    ConfigSource stringToSource(const std::string& source_str);
    
    bool isValidKey(const std::string& key);
    bool isValidValue(const std::string& value, ConfigValueType type);
    
    std::string formatConfigValue(const ConfigValue& value);
    std::string formatConfigSection(const ConfigSection& section);
    std::string formatChangeRecord(const ConfigChangeRecord& record);
    
    // Type conversion utilities
    bool stringToBool(const std::string& str);
    std::string boolToString(bool value);
    
    int stringToInt(const std::string& str);
    std::string intToString(int value);
    
    double stringToDouble(const std::string& str);
    std::string doubleToString(double value);
    
    // Validation utilities
    bool validateRange(const std::string& value, const std::string& min, const std::string& max);
    bool validatePattern(const std::string& value, const std::string& pattern);
    bool validateEnum(const std::string& value, const std::vector<std::string>& allowed_values);
    
    // Configuration merging
    ConfigProfile mergeProfiles(const ConfigProfile& base, const ConfigProfile& overlay);
    ConfigSection mergeSections(const ConfigSection& base, const ConfigSection& overlay);
    
    // Configuration filtering
    ConfigProfile filterProfile(const ConfigProfile& profile, 
                              const std::function<bool(const std::string&, const ConfigValue&)>& filter);
    
    // Configuration path utilities
    std::vector<std::string> splitConfigPath(const std::string& path);
    std::string joinConfigPath(const std::vector<std::string>& parts);
    bool isValidConfigPath(const std::string& path);
}

} // namespace lithium
