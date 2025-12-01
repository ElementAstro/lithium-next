/*
 * config_registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: ConfigRegistry - Central registry for unified configuration system

**************************************************/

#ifndef LITHIUM_CONFIG_CORE_CONFIG_REGISTRY_HPP
#define LITHIUM_CONFIG_CORE_CONFIG_REGISTRY_HPP

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"
#include "config_section.hpp"
#include "configurable.hpp"
#include "manager.hpp"

namespace fs = std::filesystem;

namespace lithium::config {

// Forward declaration
class ConfigRegistryImpl;

/**
 * @brief Configuration file format enumeration
 */
enum class ConfigFormat {
    JSON,   ///< Standard JSON
    JSON5,  ///< JSON5 (with comments and trailing commas)
    YAML,   ///< YAML format
    AUTO    ///< Auto-detect based on file extension
};

/**
 * @brief Configuration loading options
 */
struct ConfigLoadOptions {
    ConfigFormat format{ConfigFormat::AUTO};  ///< File format
    bool strict{true};         ///< Strict validation mode (fail on errors)
    bool mergeWithExisting{true};  ///< Merge with existing config vs replace
    bool enableWatch{false};   ///< Enable file watching for auto-reload
};

/**
 * @brief Section registration information
 */
struct SectionInfo {
    std::string path;                              ///< Configuration path
    std::function<json()> schemaGenerator;         ///< Schema generator
    std::function<json()> defaultGenerator;        ///< Default value generator
    std::function<ConfigValidationResult(const json&)> validator;  ///< Custom validator
    bool supportsRuntimeReconfig{true};           ///< Supports hot-reload
};

/**
 * @brief Central registry for the unified configuration system
 *
 * ConfigRegistry provides a single point of access for all configuration
 * in the application. It:
 * - Registers configuration sections with their schemas
 * - Loads configuration from files (JSON, JSON5, YAML)
 * - Validates configuration against registered schemas
 * - Provides type-safe access to configuration sections
 * - Manages configuration change subscriptions
 * - Supports runtime configuration updates with notifications
 *
 * @thread_safety This class is thread-safe for all public operations
 *
 * @example
 * ```cpp
 * // Get the registry instance
 * auto& registry = ConfigRegistry::instance();
 *
 * // Register configuration sections
 * registry.registerSection<LoggingConfig>();
 * registry.registerSection<ServerConfig>();
 *
 * // Apply defaults
 * registry.applyDefaults();
 *
 * // Load from file
 * registry.loadFromFile("config.yaml");
 *
 * // Get a configuration section
 * auto serverConfig = registry.getSection<ServerConfig>();
 * if (serverConfig) {
 *     std::cout << "Port: " << serverConfig->port << std::endl;
 * }
 *
 * // Subscribe to changes
 * registry.subscribe("/lithium/server/port", [](const json& old, const json& new_) {
 *     std::cout << "Port changed from " << old << " to " << new_ << std::endl;
 * });
 *
 * // Update at runtime
 * registry.updateValue("/lithium/server/port", 9000);
 * ```
 */
class ConfigRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the global ConfigRegistry instance
     */
    [[nodiscard]] static ConfigRegistry& instance();

    /**
     * @brief Destructor
     */
    ~ConfigRegistry();

    // Non-copyable, non-movable singleton
    ConfigRegistry(const ConfigRegistry&) = delete;
    ConfigRegistry& operator=(const ConfigRegistry&) = delete;
    ConfigRegistry(ConfigRegistry&&) = delete;
    ConfigRegistry& operator=(ConfigRegistry&&) = delete;

    // ========================================================================
    // Initialization
    // ========================================================================

    /**
     * @brief Set the underlying ConfigManager instance
     *
     * Must be called before using the registry. The ConfigManager
     * is used for actual configuration storage and validation.
     *
     * @param manager Shared pointer to ConfigManager
     */
    void setConfigManager(std::shared_ptr<ConfigManager> manager);

    /**
     * @brief Get the underlying ConfigManager
     * @return Shared pointer to ConfigManager
     */
    [[nodiscard]] std::shared_ptr<ConfigManager> getConfigManager() const;

    // ========================================================================
    // Section Registration
    // ========================================================================

    /**
     * @brief Register a configuration section type
     *
     * @tparam T ConfigSection-derived type with PATH, serialize, deserialize,
     *           and generateSchema static members
     */
    template <ConfigSectionDerived T>
    void registerSection() {
        SectionInfo info;
        info.path = std::string(T::PATH);
        info.schemaGenerator = []() { return T::generateSchema(); };
        info.defaultGenerator = []() { return T::defaults().toJson(); };
        info.validator = [](const json& j) -> ConfigValidationResult {
            ConfigValidationResult result;
            try {
                T::deserialize(j);
            } catch (const std::exception& e) {
                result.addError(std::string(T::PATH), e.what(), "deserialize");
            }
            return result;
        };
        registerSectionInfo(std::string(T::PATH), std::move(info));
    }

    /**
     * @brief Register a configurable component
     *
     * @param component Pointer to IConfigurable component
     * @note The component must outlive the registry
     */
    void registerComponent(IConfigurable* component);

    /**
     * @brief Unregister a configurable component
     *
     * @param component Pointer to IConfigurable component
     */
    void unregisterComponent(IConfigurable* component);

    /**
     * @brief Check if a section is registered
     *
     * @param path Configuration path
     * @return true if the section is registered
     */
    [[nodiscard]] bool hasSection(std::string_view path) const;

    /**
     * @brief Get list of all registered section paths
     *
     * @return Vector of registered configuration paths
     */
    [[nodiscard]] std::vector<std::string> getRegisteredSections() const;

    // ========================================================================
    // Configuration Loading
    // ========================================================================

    /**
     * @brief Apply default values for all registered sections
     *
     * This should be called after registering all sections and before
     * loading configuration files.
     */
    void applyDefaults();

    /**
     * @brief Load configuration from a file
     *
     * The file format is auto-detected based on extension (.json, .json5, .yaml, .yml)
     * or can be explicitly specified in options.
     *
     * @param path Path to configuration file
     * @param options Loading options
     * @return true if loading succeeded
     * @throws ConfigValidationException if strict mode and validation fails
     */
    bool loadFromFile(const fs::path& path,
                      const ConfigLoadOptions& options = {});

    /**
     * @brief Load configuration from multiple files
     *
     * Files are loaded in order, with later files overriding earlier ones.
     *
     * @param paths Vector of file paths
     * @param options Loading options
     * @return Number of successfully loaded files
     */
    size_t loadFromFiles(const std::vector<fs::path>& paths,
                         const ConfigLoadOptions& options = {});

    /**
     * @brief Load configuration from a directory
     *
     * Loads all configuration files in the directory (non-recursive by default).
     *
     * @param dirPath Directory path
     * @param recursive Whether to load from subdirectories
     * @param options Loading options
     * @return Number of successfully loaded files
     */
    size_t loadFromDirectory(const fs::path& dirPath, bool recursive = false,
                             const ConfigLoadOptions& options = {});

    /**
     * @brief Save all configuration to a file
     *
     * @param path Path to save configuration
     * @param format Output format (JSON, JSON5, or YAML)
     * @return true if save succeeded
     */
    bool saveToFile(const fs::path& path,
                    ConfigFormat format = ConfigFormat::YAML) const;

    /**
     * @brief Reload configuration from previously loaded files
     *
     * @return true if reload succeeded
     */
    bool reload();

    // ========================================================================
    // Configuration Access
    // ========================================================================

    /**
     * @brief Get a configuration section by type
     *
     * @tparam T ConfigSection-derived type
     * @return Configuration instance or nullopt if not found/invalid
     */
    template <ConfigSectionDerived T>
    [[nodiscard]] std::optional<T> getSection() const {
        auto jsonValue = getValue(T::PATH);
        if (!jsonValue) {
            return std::nullopt;
        }
        try {
            return T::deserialize(*jsonValue);
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Get a configuration section with default fallback
     *
     * @tparam T ConfigSection-derived type
     * @return Configuration instance (defaults if not found)
     */
    template <ConfigSectionDerived T>
    [[nodiscard]] T getSectionOrDefault() const {
        return getSection<T>().value_or(T::defaults());
    }

    /**
     * @brief Get a raw JSON value by path
     *
     * @param path Configuration path (e.g., "/lithium/server/port")
     * @return JSON value or nullopt if not found
     */
    [[nodiscard]] std::optional<json> getValue(std::string_view path) const;

    /**
     * @brief Get a typed value by path
     *
     * @tparam T Expected type
     * @param path Configuration path
     * @return Typed value or nullopt if not found or wrong type
     */
    template <typename T>
    [[nodiscard]] std::optional<T> getValueAs(std::string_view path) const {
        auto value = getValue(path);
        if (!value) {
            return std::nullopt;
        }
        try {
            return value->get<T>();
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Get a typed value with default fallback
     *
     * @tparam T Expected type
     * @param path Configuration path
     * @param defaultValue Value to return if not found
     * @return Value or default
     */
    template <typename T>
    [[nodiscard]] T getValueOr(std::string_view path, T defaultValue) const {
        return getValueAs<T>(path).value_or(std::move(defaultValue));
    }

    // ========================================================================
    // Configuration Updates
    // ========================================================================

    /**
     * @brief Update a configuration section
     *
     * @tparam T ConfigSection-derived type
     * @param config New configuration
     * @return Validation result
     */
    template <ConfigSectionDerived T>
    ConfigValidationResult updateSection(const T& config) {
        return updateSectionJson(T::PATH, config.toJson());
    }

    /**
     * @brief Update a configuration section with a modifier function
     *
     * This provides a safe read-modify-write pattern.
     *
     * @tparam T ConfigSection-derived type
     * @param modifier Function that modifies the config in place
     * @return Validation result
     */
    template <ConfigSectionDerived T>
    ConfigValidationResult updateSection(std::function<void(T&)> modifier) {
        T config = getSectionOrDefault<T>();
        modifier(config);
        return updateSection(config);
    }

    /**
     * @brief Update a single value by path
     *
     * @param path Configuration path
     * @param value New value
     * @return Validation result
     */
    ConfigValidationResult updateValue(std::string_view path, const json& value);

    /**
     * @brief Delete a configuration value
     *
     * @param path Configuration path
     * @return true if value was deleted
     */
    bool deleteValue(std::string_view path);

    // ========================================================================
    // Change Subscriptions
    // ========================================================================

    /**
     * @brief Subscribe to changes on a specific path
     *
     * @param path Configuration path to watch (supports wildcards with *)
     * @param callback Function called on change (oldValue, newValue)
     * @return Subscription ID for later removal
     */
    size_t subscribe(
        std::string_view path,
        std::function<void(const json& oldValue, const json& newValue)> callback);

    /**
     * @brief Subscribe to changes on a configuration section type
     *
     * @tparam T ConfigSection-derived type
     * @param callback Function called on change
     * @return Subscription ID
     */
    template <ConfigSectionDerived T>
    size_t subscribeSection(
        std::function<void(const T& oldConfig, const T& newConfig)> callback) {
        return subscribe(T::PATH, [callback](const json& oldVal, const json& newVal) {
            T oldConfig = oldVal.empty() ? T::defaults() : T::deserialize(oldVal);
            T newConfig = newVal.empty() ? T::defaults() : T::deserialize(newVal);
            callback(oldConfig, newConfig);
        });
    }

    /**
     * @brief Unsubscribe from changes
     *
     * @param subscriptionId ID returned by subscribe
     * @return true if subscription was removed
     */
    bool unsubscribe(size_t subscriptionId);

    /**
     * @brief Remove all subscriptions for a path
     *
     * @param path Configuration path
     */
    void unsubscribeAll(std::string_view path);

    // ========================================================================
    // Validation
    // ========================================================================

    /**
     * @brief Validate a specific section
     *
     * @param path Configuration path
     * @return Validation result
     */
    [[nodiscard]] ConfigValidationResult validateSection(
        std::string_view path) const;

    /**
     * @brief Validate all configuration
     *
     * @return Combined validation result
     */
    [[nodiscard]] ConfigValidationResult validateAll() const;

    /**
     * @brief Generate combined JSON Schema for all registered sections
     *
     * @return JSON Schema object
     */
    [[nodiscard]] json generateFullSchema() const;

    // ========================================================================
    // Utilities
    // ========================================================================

    /**
     * @brief Get all configuration as a single JSON object
     *
     * @return JSON object with all configuration
     */
    [[nodiscard]] json exportAll() const;

    /**
     * @brief Import configuration from a JSON object
     *
     * @param config Configuration to import
     * @param validate Whether to validate before importing
     * @return Validation result (empty if validate=false)
     */
    ConfigValidationResult importAll(const json& config, bool validate = true);

    /**
     * @brief Clear all configuration (reset to defaults)
     */
    void clear();

    /**
     * @brief Get statistics about registered sections and subscriptions
     *
     * @return JSON object with statistics
     */
    [[nodiscard]] json getStats() const;

private:
    ConfigRegistry();

    void registerSectionInfo(std::string path, SectionInfo info);
    ConfigValidationResult updateSectionJson(std::string_view path,
                                              const json& value);
    void notifySubscribers(std::string_view path, const json& oldValue,
                           const json& newValue);

    std::unique_ptr<ConfigRegistryImpl> impl_;
};

/**
 * @brief Exception thrown on configuration validation failure in strict mode
 */
class ConfigValidationException : public std::runtime_error {
public:
    explicit ConfigValidationException(const ConfigValidationResult& result)
        : std::runtime_error(formatErrors(result)), result_(result) {}

    [[nodiscard]] const ConfigValidationResult& getResult() const noexcept {
        return result_;
    }

private:
    static std::string formatErrors(const ConfigValidationResult& result) {
        std::string msg = "Configuration validation failed:\n";
        for (const auto& error : result.errors) {
            msg += "  - " + error.path + ": " + error.message + "\n";
        }
        return msg;
    }

    ConfigValidationResult result_;
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_CORE_CONFIG_REGISTRY_HPP
