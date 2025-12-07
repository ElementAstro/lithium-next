/**
 * @file config.hpp
 * @brief Main aggregated header for Lithium Config library.
 *
 * This is the primary include file for the config library.
 * Include this file to get access to all configuration functionality.
 *
 * @par Usage Example:
 * @code
 * #include "config/config.hpp"
 *
 * using namespace lithium::config;
 *
 * // Create a ConfigManager instance
 * auto manager = ConfigManager::createShared();
 *
 * // Load configuration from file
 * manager->loadFromFile("config.json");
 *
 * // Get configuration value
 * auto port = manager->get_as<int>("server/port");
 *
 * // Set configuration value
 * manager->set("server/host", "localhost");
 *
 * // Use ConfigRegistry for type-safe access
 * auto& registry = ConfigRegistry::instance();
 * registry.registerSection<ServerConfig>();
 * auto serverConfig = registry.getSection<ServerConfig>();
 * @endcode
 *
 * @date 2024-6-4
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_CONFIG_HPP
#define LITHIUM_CONFIG_HPP

#include <memory>
#include <string>

// ============================================================================
// Core Module
// ============================================================================
// Configuration manager, registry, and exception types

#include "core/config_registry.hpp"
#include "core/config_section.hpp"
#include "core/configurable.hpp"
#include "core/types.hpp"

// ============================================================================
// Components Module
// ============================================================================
// Cache, Validator, Serializer, Watcher, YamlParser components

#include "components/components.hpp"

// ============================================================================
// Utils Module
// ============================================================================
// JSON5 parser and helper macros

#include "utils/utils.hpp"

namespace lithium::config {

// ============================================================================
// Library Version
// ============================================================================

/**
 * @brief Lithium Config library version.
 */
inline constexpr const char* CONFIG_VERSION = "1.1.0";

/**
 * @brief Get config library version string.
 * @return Version string.
 */
[[nodiscard]] inline const char* getConfigVersion() noexcept {
    return CONFIG_VERSION;
}

/**
 * @brief Get all module versions as a formatted string.
 * @return Formatted string with all module versions.
 */
[[nodiscard]] inline std::string getAllConfigModuleVersions() {
    std::string result;
    result += "Config: ";
    result += CONFIG_VERSION;
    result += "\n  Core: ";
    result += CORE_VERSION;
    result += "\n  Components: ";
    result += COMPONENTS_VERSION;
    result += "\n  Utils: ";
    result += UTILS_VERSION;
    return result;
}

// ============================================================================
// Convenience Type Aliases
// ============================================================================

/// Shared pointer to ConfigManager
using ConfigManagerPtr = std::shared_ptr<ConfigManager>;

/// Shared pointer to ConfigCache
using ConfigCachePtr = std::shared_ptr<ConfigCache>;

/// Shared pointer to ConfigValidator
using ConfigValidatorPtr = std::shared_ptr<ConfigValidator>;

/// Shared pointer to ConfigSerializer
using ConfigSerializerPtr = std::shared_ptr<ConfigSerializer>;

/// Shared pointer to ConfigWatcher
using ConfigWatcherPtr = std::shared_ptr<ConfigWatcher>;

// ============================================================================
// Factory Functions
// ============================================================================

/**
 * @brief Create a new ConfigManager instance.
 * @return Shared pointer to the new ConfigManager.
 */
[[nodiscard]] inline ConfigManagerPtr createConfigManager() {
    return ConfigManager::createShared();
}

/**
 * @brief Create a new ConfigManager instance with custom options.
 * @param options Configuration options.
 * @return Shared pointer to the new ConfigManager.
 */
[[nodiscard]] inline ConfigManagerPtr createConfigManager(
    const ConfigManager::Options& options) {
    return ConfigManager::createShared(options);
}

/**
 * @brief Create a new ConfigCache instance.
 * @return Shared pointer to the new ConfigCache.
 */
[[nodiscard]] inline ConfigCachePtr createConfigCache() {
    return std::make_shared<ConfigCache>();
}

/**
 * @brief Create a new ConfigCache instance with custom config.
 * @param config Cache configuration.
 * @return Shared pointer to the new ConfigCache.
 */
[[nodiscard]] inline ConfigCachePtr createConfigCache(
    const ConfigCache::Config& config) {
    return std::make_shared<ConfigCache>(config);
}

/**
 * @brief Create a new ConfigValidator instance.
 * @return Shared pointer to the new ConfigValidator.
 */
[[nodiscard]] inline ConfigValidatorPtr createConfigValidator() {
    return std::make_shared<ConfigValidator>();
}

/**
 * @brief Create a new ConfigSerializer instance.
 * @return Shared pointer to the new ConfigSerializer.
 */
[[nodiscard]] inline ConfigSerializerPtr createConfigSerializer() {
    return std::make_shared<ConfigSerializer>();
}

/**
 * @brief Create a new ConfigWatcher instance.
 * @return Shared pointer to the new ConfigWatcher.
 */
[[nodiscard]] inline ConfigWatcherPtr createConfigWatcher() {
    return std::make_shared<ConfigWatcher>();
}

// ============================================================================
// Quick Access Functions
// ============================================================================

/**
 * @brief Create default ConfigManager options.
 * @return Default ConfigManager::Options.
 */
[[nodiscard]] inline ConfigManager::Options createDefaultConfigOptions() {
    return ConfigManager::Options{};
}

/**
 * @brief Create ConfigManager options with caching disabled.
 * @return ConfigManager::Options with caching disabled.
 */
[[nodiscard]] inline ConfigManager::Options createNoCacheConfigOptions() {
    ConfigManager::Options options;
    options.enable_caching = false;
    return options;
}

/**
 * @brief Create ConfigManager options with validation disabled.
 * @return ConfigManager::Options with validation disabled.
 */
[[nodiscard]] inline ConfigManager::Options createNoValidationConfigOptions() {
    ConfigManager::Options options;
    options.enable_validation = false;
    return options;
}

/**
 * @brief Create ConfigManager options with auto-reload disabled.
 * @return ConfigManager::Options with auto-reload disabled.
 */
[[nodiscard]] inline ConfigManager::Options createNoAutoReloadConfigOptions() {
    ConfigManager::Options options;
    options.enable_auto_reload = false;
    return options;
}

}  // namespace lithium::config

// ============================================================================
// Backward Compatibility
// ============================================================================
// These aliases are provided for backward compatibility with code that
// uses the types directly in the lithium namespace

namespace lithium {
using ConfigManager = lithium::config::ConfigManager;
using ConfigRegistry = lithium::config::ConfigRegistry;
using ConfigCache = lithium::config::ConfigCache;
using ConfigValidator = lithium::config::ConfigValidator;
using ConfigSerializer = lithium::config::ConfigSerializer;
using ConfigWatcher = lithium::config::ConfigWatcher;
}  // namespace lithium

#endif  // LITHIUM_CONFIG_HPP
