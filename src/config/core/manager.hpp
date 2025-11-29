/*
 * manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-4

Description: High-performance Configuration Manager with Split Components

**************************************************/

#ifndef LITHIUM_CONFIG_CORE_MANAGER_HPP
#define LITHIUM_CONFIG_CORE_MANAGER_HPP

#include <concepts>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../components/cache.hpp"
#include "../components/serializer.hpp"
#include "../components/validator.hpp"
#include "../components/watcher.hpp"
#include "atom/type/json.hpp"
#include "exception.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace lithium::config {

// Forward declaration
class ConfigManagerImpl;

/**
 * @brief Concept for values that can be stored in a configuration
 */
template <typename T>
concept ConfigValue = requires(T value, json j) {
    { j = value } -> std::convertible_to<json>;
    { j.get<T>() } -> std::convertible_to<T>;
};

/**
 * @brief Configuration Manager with high-performance split architecture
 *
 * The ConfigManager provides a comprehensive configuration management system
 * with the following features:
 * - High-performance caching with LRU eviction and TTL support
 * - JSON Schema-based validation with custom rules
 * - Multi-format serialization (JSON/JSON5/Binary) with streaming support
 * - File watching and auto-reload functionality
 * - Thread-safe operations with optimized locking
 * - Performance monitoring and metrics collection
 * - Comprehensive error handling and logging
 *
 * The architecture is split into specialized components:
 * - ConfigCache: High-performance caching layer
 * - ConfigValidator: JSON Schema validation engine
 * - ConfigSerializer: Multi-format serialization engine
 * - ConfigWatcher: File monitoring and auto-reload system
 *
 * @thread_safety This class is thread-safe for all public operations
 * @performance Optimized for high-throughput configuration access
 */
class ConfigManager {
public:
    /**
     * @brief Configuration options for the ConfigManager
     */
    struct Options {
        ConfigCache::Config cache_options;          ///< Cache configuration
        ConfigValidator::Config validator_options;  ///< Validator configuration
        SerializationOptions serializer_options;  ///< Serializer configuration
        ConfigWatcher::WatcherOptions
            watcher_options;            ///< Watcher configuration
        bool enable_auto_reload{true};  ///< Enable automatic file reloading
        bool enable_validation{true};   ///< Enable configuration validation
        bool enable_caching{true};      ///< Enable configuration caching
        std::chrono::milliseconds auto_save_delay{5000};  ///< Auto-save delay
    };

    /**
     * @brief Performance metrics for the ConfigManager
     */
    struct Metrics {
        size_t total_operations{0};          ///< Total operations performed
        size_t cache_hits{0};                ///< Cache hit count
        size_t cache_misses{0};              ///< Cache miss count
        size_t validation_successes{0};      ///< Successful validations
        size_t validation_failures{0};       ///< Failed validations
        size_t files_loaded{0};              ///< Files loaded count
        size_t files_saved{0};               ///< Files saved count
        size_t auto_reloads{0};              ///< Auto-reload triggers
        double average_access_time_ms{0.0};  ///< Average access time
        double average_save_time_ms{0.0};    ///< Average save time
        std::chrono::steady_clock::time_point
            last_operation;  ///< Last operation timestamp
    };

    /**
     * @brief Default constructor with default options
     */
    ConfigManager();

    /**
     * @brief Constructor with custom options
     * @param options Configuration options for the manager
     */
    explicit ConfigManager(const Options& options);

    /**
     * @brief Destructor - handles cleanup and final saves
     */
    ~ConfigManager();

    // Delete copy operations to ensure proper resource management
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Allow move operations
    ConfigManager(ConfigManager&&) noexcept;
    ConfigManager& operator=(ConfigManager&&) noexcept;

    /**
     * @brief Creates a shared pointer instance of ConfigManager with default
     * options
     * @return std::shared_ptr<ConfigManager> Shared pointer to ConfigManager
     * instance
     */
    [[nodiscard]] static auto createShared() -> std::shared_ptr<ConfigManager>;

    /**
     * @brief Creates a shared pointer instance of ConfigManager with custom
     * options
     * @param options Configuration options for the manager
     * @return std::shared_ptr<ConfigManager> Shared pointer to ConfigManager
     * instance
     */
    [[nodiscard]] static auto createShared(const Options& options)
        -> std::shared_ptr<ConfigManager>;

    /**
     * @brief Creates a unique pointer instance of ConfigManager with default
     * options
     * @return std::unique_ptr<ConfigManager> Unique pointer to ConfigManager
     * instance
     */
    [[nodiscard]] static auto createUnique() -> std::unique_ptr<ConfigManager>;

    /**
     * @brief Creates a unique pointer instance of ConfigManager with custom
     * options
     * @param options Configuration options for the manager
     * @return std::unique_ptr<ConfigManager> Unique pointer to ConfigManager
     * instance
     */
    [[nodiscard]] static auto createUnique(const Options& options)
        -> std::unique_ptr<ConfigManager>;

    /**
     * @brief Retrieves the value associated with the given key path with
     * caching
     * @param key_path The path to the configuration value
     * @return std::optional<json> The optional JSON value if found
     * @note This method uses caching for improved performance
     */
    [[nodiscard]] auto get(std::string_view key_path) const
        -> std::optional<json>;

    /**
     * @brief Retrieves a typed value from the configuration with caching
     * @tparam T The expected type of the value
     * @param key_path The path to the configuration value
     * @return std::optional<T> The typed value if found and convertible
     * @note This method includes automatic type validation and caching
     */
    template <ConfigValue T>
    [[nodiscard]] auto get_as(std::string_view key_path) const
        -> std::optional<T> {
        auto value = get(key_path);
        if (!value.has_value()) {
            return std::nullopt;
        }

        try {
            return value->get<T>();
        } catch (const json::exception& e) {
            logTypeConversionError(key_path, typeid(T).name(), e.what());
            return std::nullopt;
        }
    }

    /**
     * @brief Sets the value for the specified key path with validation and
     * caching
     * @param key_path The path to set the configuration value
     * @param value The JSON value to set
     * @return bool True if the value was successfully set, false otherwise
     * @note This method includes automatic validation and cache updates
     */
    auto set(std::string_view key_path, const json& value) -> bool;

    /**
     * @brief Sets the value for the specified key path with move semantics
     * @param key_path The path to set the configuration value
     * @param value The JSON value to set (moved)
     * @return bool True if the value was successfully set, false otherwise
     * @note This method uses move semantics for optimal performance
     */
    auto set(std::string_view key_path, json&& value) -> bool;

    /**
     * @brief Sets any compatible value for the specified key path with
     * validation
     * @tparam T Type of the value to set (must be compatible with json)
     * @param key_path The path to set the configuration value
     * @param value The value to set
     * @return bool True if the value was successfully set, false otherwise
     * @note This method includes automatic validation and type checking
     */
    template <ConfigValue T>
    auto set_value(std::string_view key_path, T&& value) -> bool {
        return set(key_path, json(std::forward<T>(value)));
    }

    /**
     * @brief Appends a value to an array at the specified key path with
     * validation
     * @param key_path The path to the array
     * @param value The JSON value to append
     * @return bool True if the value was successfully appended, false otherwise
     * @note This method includes automatic array validation and cache updates
     */
    auto append(std::string_view key_path, const json& value) -> bool;

    /**
     * @brief Appends any compatible value to an array at the specified key path
     * @tparam T Type of the value to append (must be compatible with json)
     * @param key_path The path to the array
     * @param value The value to append
     * @return bool True if the value was successfully appended, false otherwise
     * @note This method includes automatic type conversion and validation
     */
    template <ConfigValue T>
    auto append_value(std::string_view key_path, T&& value) -> bool {
        return append(key_path, json(std::forward<T>(value)));
    }

    /**
     * @brief Deletes the value associated with the given key path
     * @param key_path The path to the configuration value to delete
     * @return bool True if the value was successfully deleted, false otherwise
     * @note This method automatically updates the cache and triggers callbacks
     */
    auto remove(std::string_view key_path) -> bool;

    /**
     * @brief Checks if a value exists for the given key path with caching
     * @param key_path The path to check
     * @return bool True if a value exists for the key path, false otherwise
     * @note This method uses caching for improved performance
     */
    [[nodiscard]] auto has(std::string_view key_path) const -> bool;

    /**
     * @brief Retrieves all keys in the configuration with caching
     * @return std::vector<std::string> A vector of keys in the configuration
     * @note This method uses caching for improved performance
     */
    [[nodiscard]] auto getKeys() const -> std::vector<std::string>;

    /**
     * @brief Lists all configuration files in specified directory
     * @return std::vector<std::string> A vector of paths to configuration files
     * @note This method scans for multiple configuration formats
     */
    [[nodiscard]] auto listPaths() const -> std::vector<std::string>;

    /**
     * @brief Loads configuration data from a file with validation and caching
     * @param path The path to the file containing configuration data
     * @return bool True if the file was successfully loaded, false otherwise
     * @note This method includes automatic format detection, validation, and
     * caching
     */
    auto loadFromFile(const fs::path& path) -> bool;

    /**
     * @brief Loads configuration data from multiple files in parallel
     * @param paths The paths to the files containing configuration data
     * @return size_t The number of successfully loaded files
     * @note This method uses parallel processing for improved performance
     */
    auto loadFromFiles(std::span<const fs::path> paths) -> size_t;

    /**
     * @brief Loads configuration data from a directory with auto-watching
     * @param dir_path The path to the directory containing configuration files
     * @param recursive Flag indicating whether to recursively load from
     * subdirectories
     * @return bool True if the directory was successfully loaded, false
     * otherwise
     * @note This method can optionally enable auto-watching for file changes
     */
    auto loadFromDir(const fs::path& dir_path, bool recursive = false) -> bool;

    /**
     * @brief Saves the current configuration to a file with format selection
     * @param file_path The path to save the configuration file
     * @return bool True if the configuration was successfully saved, false
     * otherwise
     * @note This method automatically selects format based on file extension
     */
    [[nodiscard]] auto save(const fs::path& file_path) const -> bool;

    /**
     * @brief Saves all configuration data to files in the specified directory
     * @param dir_path The path to the directory to save configuration files
     * @return bool True if all configuration data was successfully saved, false
     * otherwise
     * @note This method uses the configured serialization format
     */
    [[nodiscard]] auto saveAll(const fs::path& dir_path) const -> bool;

    /**
     * @brief Cleans up the configuration by removing unused entries and
     * optimizing data
     * @note This method optimizes cache usage and validates configuration
     * integrity
     */
    void tidy();

    /**
     * @brief Clears all configuration data and resets components
     * @note This method clears cache, validation state, and all stored data
     */
    void clear();

    /**
     * @brief Merges the current configuration with the provided JSON data
     * @param src The JSON object to merge into the current configuration
     * @note This method includes validation and cache updates for merged data
     */
    void merge(const json& src);

    /**
     * @brief Registers a callback for configuration changes
     * @param callback Function to call when configuration changes
     * @return size_t ID of the registered callback
     * @note Callbacks are thread-safe and called asynchronously
     */
    size_t onChanged(std::function<void(std::string_view path)> callback);

    /**
     * @brief Unregisters a configuration change callback
     * @param id ID of the callback to remove
     * @return bool True if callback was removed, false if not found
     */
    bool removeCallback(size_t id);

    // Component access methods

    /**
     * @brief Get direct access to the cache component
     * @return const ConfigCache& Reference to the cache component
     * @note Use for advanced cache operations and statistics
     */
    [[nodiscard]] const ConfigCache& getCache() const;

    /**
     * @brief Get direct access to the validator component
     * @return const ConfigValidator& Reference to the validator component
     * @note Use for schema management and validation operations
     */
    [[nodiscard]] const ConfigValidator& getValidator() const;

    /**
     * @brief Get direct access to the serializer component
     * @return const ConfigSerializer& Reference to the serializer component
     * @note Use for format-specific operations and batch processing
     */
    [[nodiscard]] const ConfigSerializer& getSerializer() const;

    /**
     * @brief Get direct access to the watcher component
     * @return const ConfigWatcher& Reference to the watcher component
     * @note Use for file monitoring configuration and statistics
     */
    [[nodiscard]] const ConfigWatcher& getWatcher() const;

    // Configuration and metrics methods

    /**
     * @brief Update configuration options at runtime
     * @param options New configuration options to apply
     * @note This method may restart components if necessary
     */
    void updateOptions(const Options& options);

    /**
     * @brief Get current configuration options
     * @return const Options& Current configuration options
     */
    [[nodiscard]] const Options& getOptions() const noexcept;

    /**
     * @brief Get performance metrics
     * @return Metrics Current performance metrics
     * @note This method provides comprehensive performance statistics
     */
    [[nodiscard]] Metrics getMetrics() const;

    /**
     * @brief Reset performance metrics
     * @note This method clears all performance counters and statistics
     */
    void resetMetrics();

    // Validation methods

    /**
     * @brief Set JSON schema for validation
     * @param schema_path Path to configuration section (e.g.,
     * "database/connection")
     * @param schema JSON schema object
     * @return bool True if schema was successfully set
     */
    bool setSchema(std::string_view schema_path, const json& schema);

    /**
     * @brief Load schema from file
     * @param schema_path Path to configuration section
     * @param file_path Path to schema file
     * @return bool True if schema was successfully loaded
     */
    bool loadSchema(std::string_view schema_path, const fs::path& file_path);

    /**
     * @brief Validate configuration section against schema
     * @param config_path Path to configuration section to validate
     * @return ValidationResult Validation result with details
     */
    [[nodiscard]] ValidationResult validate(std::string_view config_path) const;

    /**
     * @brief Validate entire configuration
     * @return ValidationResult Overall validation result
     */
    [[nodiscard]] ValidationResult validateAll() const;

    // File watching methods

    /**
     * @brief Enable auto-reload for a specific file
     * @param file_path Path to file to watch
     * @return bool True if watching was enabled successfully
     */
    bool enableAutoReload(const fs::path& file_path);

    /**
     * @brief Disable auto-reload for a specific file
     * @param file_path Path to file to stop watching
     * @return bool True if watching was disabled successfully
     */
    bool disableAutoReload(const fs::path& file_path);

    /**
     * @brief Check if auto-reload is enabled for a file
     * @param file_path Path to check
     * @return bool True if file is being watched for changes
     */
    [[nodiscard]] bool isAutoReloadEnabled(const fs::path& file_path) const;

    // ========================================================================
    // Hook Support
    // ========================================================================

    /**
     * @brief Configuration manager event types for hooks
     */
    enum class ConfigEvent {
        VALUE_GET,        ///< Value was retrieved
        VALUE_SET,        ///< Value was set
        VALUE_REMOVED,    ///< Value was removed
        FILE_LOADED,      ///< Configuration file was loaded
        FILE_SAVED,       ///< Configuration file was saved
        FILE_RELOADED,    ///< Configuration file was auto-reloaded
        VALIDATION_DONE,  ///< Validation was performed
        CACHE_HIT,        ///< Cache hit occurred
        CACHE_MISS,       ///< Cache miss occurred
        CONFIG_CLEARED,   ///< Configuration was cleared
        CONFIG_MERGED     ///< Configuration was merged
    };

    /**
     * @brief Configuration hook callback signature
     * @param event Type of configuration event
     * @param path Configuration path involved
     * @param value Optional value (for GET/SET events)
     */
    using ConfigHook =
        std::function<void(ConfigEvent event, std::string_view path,
                           const std::optional<json>& value)>;

    /**
     * @brief Register a configuration event hook
     * @param hook Callback function for configuration events
     * @return Hook ID for later removal
     */
    size_t addHook(ConfigHook hook);

    /**
     * @brief Remove a registered hook
     * @param hookId ID of the hook to remove
     * @return True if hook was removed
     */
    bool removeHook(size_t hookId);

    /**
     * @brief Clear all registered hooks
     */
    void clearHooks();

    /**
     * @brief Get the number of registered hooks
     * @return Number of hooks
     */
    [[nodiscard]] size_t getHookCount() const;

    // ========================================================================
    // Additional Utility Methods
    // ========================================================================

    /**
     * @brief Get configuration as flattened key-value pairs
     * @return Map of path to value
     */
    [[nodiscard]] std::unordered_map<std::string, json> flatten() const;

    /**
     * @brief Import configuration from flattened key-value pairs
     * @param flatConfig Flattened configuration map
     * @return Number of values imported
     */
    size_t unflatten(const std::unordered_map<std::string, json>& flatConfig);

    /**
     * @brief Export configuration to a specific format
     * @param format Target serialization format
     * @return Serialized configuration string
     */
    [[nodiscard]] std::string exportAs(SerializationFormat format) const;

    /**
     * @brief Import configuration from a string
     * @param data Configuration data string
     * @param format Source serialization format
     * @return True if import was successful
     */
    bool importFrom(std::string_view data, SerializationFormat format);

    /**
     * @brief Get configuration diff between current and provided config
     * @param other Configuration to compare against
     * @return JSON object describing differences
     */
    [[nodiscard]] json diff(const json& other) const;

    /**
     * @brief Apply a patch to the configuration
     * @param patch JSON patch object
     * @return True if patch was applied successfully
     */
    bool applyPatch(const json& patch);

    /**
     * @brief Create a snapshot of current configuration
     * @return Snapshot ID
     */
    [[nodiscard]] std::string createSnapshot();

    /**
     * @brief Restore configuration from a snapshot
     * @param snapshotId ID of snapshot to restore
     * @return True if restore was successful
     */
    bool restoreSnapshot(const std::string& snapshotId);

    /**
     * @brief List all available snapshots
     * @return Vector of snapshot IDs
     */
    [[nodiscard]] std::vector<std::string> listSnapshots() const;

    /**
     * @brief Delete a snapshot
     * @param snapshotId ID of snapshot to delete
     * @return True if snapshot was deleted
     */
    bool deleteSnapshot(const std::string& snapshotId);

private:
    std::unique_ptr<ConfigManagerImpl>
        impl_;  ///< Implementation-specific pointer

    /**
     * @brief Internal helper for merge operations
     * @param src Source JSON to merge
     * @param target Target JSON to merge into
     */
    void merge(const json& src, json& target);

    /**
     * @brief Log type conversion errors using spdlog
     * @param key_path Configuration key path
     * @param type_name Target type name
     * @param error_message Error details
     */
    void logTypeConversionError(std::string_view key_path,
                                const char* type_name,
                                const char* error_message) const;

    /**
     * @brief Internal callback for file change events
     * @param file_path Path of changed file
     * @param event Type of file event
     */
    void onFileChanged(const fs::path& file_path, FileEvent event);

    /**
     * @brief Update metrics for operation timing
     * @param operation_type Type of operation performed
     * @param duration_ms Operation duration in milliseconds
     */
    void updateMetrics(const std::string& operation_type, double duration_ms);
};

}  // namespace lithium::config

// Backward compatibility alias
namespace lithium {
using ConfigManager = lithium::config::ConfigManager;
}

#endif  // LITHIUM_CONFIG_CORE_MANAGER_HPP
