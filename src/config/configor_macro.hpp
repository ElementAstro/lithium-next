#ifndef LITHIUM_CONFIG_CONFIGOR_MACRO_HPP
#define LITHIUM_CONFIG_CONFIGOR_MACRO_HPP

#include <string_view>
#include <type_traits>
#include "atom/log/loguru.hpp"
#include "configor.hpp"

namespace lithium {
// Forward declaration
template <typename T>
concept ConfigurationType =
    std::is_same_v<T, int> || std::is_same_v<T, float> ||
    std::is_same_v<T, double> || std::is_same_v<T, bool> ||
    std::is_same_v<T, std::string>;

// Type-safe wrapper for getting configuration values
template <ConfigurationType T>
[[nodiscard]] inline auto GetConfigValue(
    const std::shared_ptr<ConfigManager> &configManager,
    std::string_view path) -> T {
    if (!configManager) {
        LOG_F(ERROR, "Config manager is null");
        throw std::runtime_error("Config manager is null");
    }

    auto opt = configManager->get_as<T>(path);
    if (!opt.has_value()) {
        LOG_F(ERROR, "Config value for {} not found or wrong type", path);
        throw std::runtime_error(std::string("Config value for ") +
                                 std::string(path) +
                                 " not found or wrong type");
    }

    return opt.value();
}
}  // namespace lithium

// Modern C++20 macros that are safer and more maintainable
#define GetIntConfig(path)                                                 \
    lithium::GetConfigValue<int>(                                          \
        GetPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER).value(), \
        path)

#define GetFloatConfig(path)                                               \
    lithium::GetConfigValue<float>(                                        \
        GetPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER).value(), \
        path)

#define GetBoolConfig(path)                                                \
    lithium::GetConfigValue<bool>(                                         \
        GetPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER).value(), \
        path)

#define GetDoubleConfig(path)                                              \
    lithium::GetConfigValue<double>(                                       \
        GetPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER).value(), \
        path)

#define GetStringConfig(path)                                              \
    lithium::GetConfigValue<std::string>(                                  \
        GetPtr<lithium::ConfigManager>(Constants::CONFIG_MANAGER).value(), \
        path)

// Enhanced macro for getting config values with better error handling
#define GET_CONFIG_VALUE(configManager, path, type, outputVar)                \
    type outputVar;                                                           \
    do {                                                                      \
        try {                                                                 \
            auto opt = (configManager)->get_as<type>(path);                   \
            if (!opt.has_value()) {                                           \
                LOG_F(ERROR, "Config value for {} not found or wrong type",   \
                      path);                                                  \
                THROW_BAD_CONFIG_EXCEPTION("Config value for " +              \
                                           std::string(path) + " not found"); \
            }                                                                 \
            outputVar = opt.value();                                          \
        } catch (const std::bad_optional_access &e) {                         \
            LOG_F(ERROR, "Bad access to config value for {}: {}", path,       \
                  e.what());                                                  \
            THROW_BAD_CONFIG_EXCEPTION(e.what());                             \
        } catch (const json::exception &e) {                                  \
            LOG_F(ERROR, "Invalid config value for {}: {}", path, e.what());  \
            THROW_INVALID_CONFIG_EXCEPTION(e.what());                         \
        } catch (const std::exception &e) {                                   \
            LOG_F(ERROR,                                                      \
                  "Unknown exception when getting config value for {}: {}",   \
                  path, e.what());                                            \
            THROW_UNKOWN(e.what());                                           \
        }                                                                     \
    } while (0)

// Safe setters with type checking
#define SET_CONFIG_VALUE(configManager, path, value)                           \
    do {                                                                       \
        try {                                                                  \
            if (!(configManager)->set_value(path, value)) {                    \
                LOG_F(ERROR, "Failed to set config value for {}", path);       \
                THROW_BAD_CONFIG_EXCEPTION("Failed to set config value for " + \
                                           std::string(path));                 \
            }                                                                  \
        } catch (const std::exception &e) {                                    \
            LOG_F(ERROR, "Exception when setting config value for {}: {}",     \
                  path, e.what());                                             \
            THROW_UNKOWN(e.what());                                            \
        }                                                                      \
    } while (0)

#endif  // LITHIUM_CONFIG_CONFIGOR_MACRO_HPP
