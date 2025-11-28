/*
 * macro.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration Helper Macros

**************************************************/

#ifndef LITHIUM_CONFIG_UTILS_MACRO_HPP
#define LITHIUM_CONFIG_UTILS_MACRO_HPP

#include <string_view>
#include <type_traits>

#include "atom/log/spdlog_logger.hpp"
#include "../core/manager.hpp"
#include "../core/exception.hpp"

namespace lithium::config {

/**
 * @brief Concept for configuration value types
 */
template <typename T>
concept ConfigurationType =
    std::is_same_v<T, int> || std::is_same_v<T, float> ||
    std::is_same_v<T, double> || std::is_same_v<T, bool> ||
    std::is_same_v<T, std::string>;

/**
 * @brief Type-safe wrapper for getting configuration values
 * @tparam T Configuration value type
 * @param configManager Shared pointer to ConfigManager
 * @param path Configuration path
 * @return Configuration value
 * @throws std::runtime_error if config manager is null or value not found
 */
template <ConfigurationType T>
[[nodiscard]] inline auto GetConfigValue(
    const std::shared_ptr<ConfigManager>& configManager,
    std::string_view path) -> T {
    if (!configManager) {
        LOG_ERROR( "Config manager is null");
        throw std::runtime_error("Config manager is null");
    }

    auto opt = configManager->get_as<T>(path);
    if (!opt.has_value()) {
        LOG_ERROR( "Config value for {} not found or wrong type", path);
        throw std::runtime_error(std::string("Config value for ") +
                                 std::string(path) +
                                 " not found or wrong type");
    }

    return opt.value();
}

}  // namespace lithium::config

// Backward compatibility - expose in lithium namespace
namespace lithium {
using lithium::config::ConfigurationType;
using lithium::config::GetConfigValue;
}  // namespace lithium

// ============================================================================
// Modern C++20 macros that are safer and more maintainable
// ============================================================================

#define GetIntConfig(path)                                                     \
    lithium::config::GetConfigValue<int>(                                      \
        GetPtr<lithium::config::ConfigManager>(Constants::CONFIG_MANAGER)      \
            .value(),                                                          \
        path)

#define GetFloatConfig(path)                                                   \
    lithium::config::GetConfigValue<float>(                                    \
        GetPtr<lithium::config::ConfigManager>(Constants::CONFIG_MANAGER)      \
            .value(),                                                          \
        path)

#define GetBoolConfig(path)                                                    \
    lithium::config::GetConfigValue<bool>(                                     \
        GetPtr<lithium::config::ConfigManager>(Constants::CONFIG_MANAGER)      \
            .value(),                                                          \
        path)

#define GetDoubleConfig(path)                                                  \
    lithium::config::GetConfigValue<double>(                                   \
        GetPtr<lithium::config::ConfigManager>(Constants::CONFIG_MANAGER)      \
            .value(),                                                          \
        path)

#define GetStringConfig(path)                                                  \
    lithium::config::GetConfigValue<std::string>(                              \
        GetPtr<lithium::config::ConfigManager>(Constants::CONFIG_MANAGER)      \
            .value(),                                                          \
        path)

// ============================================================================
// Enhanced macro for getting config values with better error handling
// ============================================================================

#define GET_CONFIG_VALUE(configManager, path, type, outputVar)                 \
    type outputVar;                                                            \
    do {                                                                       \
        try {                                                                  \
            auto opt = (configManager)->get_as<type>(path);                    \
            if (!opt.has_value()) {                                            \
                LOG_ERROR( "Config value for {} not found or wrong type",    \
                      path);                                                   \
                THROW_BAD_CONFIG_EXCEPTION("Config value for " +               \
                                           std::string(path) + " not found");  \
            }                                                                  \
            outputVar = opt.value();                                           \
        } catch (const std::bad_optional_access& e) {                          \
            LOG_ERROR( "Bad access to config value for {}: {}", path,        \
                  e.what());                                                   \
            THROW_BAD_CONFIG_EXCEPTION(e.what());                              \
        } catch (const json::exception& e) {                                   \
            LOG_ERROR( "Invalid config value for {}: {}", path, e.what());   \
            THROW_INVALID_CONFIG_EXCEPTION(e.what());                          \
        } catch (const std::exception& e) {                                    \
            LOG_ERROR(                                                       \
                  "Unknown exception when getting config value for {}: {}",    \
                  path, e.what());                                             \
            throw;                                                             \
        }                                                                      \
    } while (0)

// ============================================================================
// Safe setters with type checking
// ============================================================================

#define SET_CONFIG_VALUE(configManager, path, value)                           \
    do {                                                                       \
        try {                                                                  \
            if (!(configManager)->set_value(path, value)) {                    \
                LOG_ERROR( "Failed to set config value for {}", path);       \
                THROW_BAD_CONFIG_EXCEPTION("Failed to set config value for " + \
                                           std::string(path));                 \
            }                                                                  \
        } catch (const std::exception& e) {                                    \
            LOG_ERROR( "Exception when setting config value for {}: {}",     \
                  path, e.what());                                             \
            throw;                                                             \
        }                                                                      \
    } while (0)

#endif  // LITHIUM_CONFIG_UTILS_MACRO_HPP
