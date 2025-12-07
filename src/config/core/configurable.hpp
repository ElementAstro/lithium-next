/*
 * configurable.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: IConfigurable interface for unified configuration system

**************************************************/

#ifndef LITHIUM_CONFIG_CORE_CONFIGURABLE_HPP
#define LITHIUM_CONFIG_CORE_CONFIGURABLE_HPP

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "atom/type/json.hpp"

namespace lithium::config {

using json = nlohmann::json;

/**
 * @brief Interface for components that support unified configuration
 *
 * This interface defines the contract for components that want to participate
 * in the unified configuration system. Components implementing this interface
 * can:
 * - Register their configuration schema with the ConfigRegistry
 * - Receive configuration from files or runtime updates
 * - Be notified of configuration changes for hot-reload support
 *
 * @example
 * ```cpp
 * class MyServer : public IConfigurable {
 * public:
 *     std::string_view getConfigPath() const noexcept override {
 *         return "/lithium/myserver";
 *     }
 *
 *     json getConfigSchema() const override {
 *         return {
 *             {"type", "object"},
 *             {"properties", {
 *                 {"port", {{"type", "integer"}, {"default", 8080}}}
 *             }}
 *         };
 *     }
 *
 *     json getDefaultConfig() const override {
 *         return {{"port", 8080}};
 *     }
 *
 *     bool applyConfig(const json& config) override {
 *         port_ = config.value("port", 8080);
 *         return true;
 *     }
 *
 *     json exportConfig() const override {
 *         return {{"port", port_}};
 *     }
 *
 *     void onConfigChanged(std::string_view path,
 *                          const std::optional<json>& newValue) override {
 *         if (path == "/lithium/myserver/port" && newValue) {
 *             port_ = newValue->get<int>();
 *         }
 *     }
 *
 * private:
 *     int port_ = 8080;
 * };
 * ```
 *
 * @thread_safety Implementations should be thread-safe for concurrent access
 */
class IConfigurable {
public:
    virtual ~IConfigurable() = default;

    /**
     * @brief Get the configuration path prefix for this component
     *
     * The path is used to locate this component's configuration within
     * the global configuration tree. Paths should follow the format:
     * "/lithium/<module>/<component>" (e.g., "/lithium/server/websocket")
     *
     * @return std::string_view Configuration path prefix
     * @note The returned string_view must remain valid for the lifetime
     *       of the object (typically a static constexpr string)
     */
    [[nodiscard]] virtual std::string_view getConfigPath() const noexcept = 0;

    /**
     * @brief Get the JSON Schema for this component's configuration
     *
     * The schema is used to validate configuration before it's applied.
     * It should follow the JSON Schema specification (draft-07 or later).
     *
     * @return json JSON Schema object
     * @see https://json-schema.org/
     */
    [[nodiscard]] virtual json getConfigSchema() const = 0;

    /**
     * @brief Get default configuration values
     *
     * Returns a JSON object containing all configuration options with
     * their default values. This is used to initialize the configuration
     * before loading from files.
     *
     * @return json Default configuration as JSON object
     */
    [[nodiscard]] virtual json getDefaultConfig() const = 0;

    /**
     * @brief Apply configuration from JSON
     *
     * Called when configuration is loaded from a file or updated at runtime.
     * The configuration passed will have already been validated against
     * the schema returned by getConfigSchema().
     *
     * @param config Configuration JSON object
     * @return true if configuration was applied successfully
     * @return false if configuration could not be applied (component should
     *         log the reason)
     */
    virtual bool applyConfig(const json& config) = 0;

    /**
     * @brief Export current configuration to JSON
     *
     * Returns the current configuration state as a JSON object.
     * This is used for configuration persistence and debugging.
     *
     * @return json Current configuration as JSON object
     */
    [[nodiscard]] virtual json exportConfig() const = 0;

    /**
     * @brief Handle configuration change notification
     *
     * Called when a specific configuration value changes at runtime.
     * This allows components to respond to configuration changes without
     * requiring a full reconfiguration.
     *
     * The default implementation does nothing, suitable for components
     * that don't support hot-reload.
     *
     * @param path Full path to the changed configuration value
     * @param newValue New value (nullopt if the value was deleted)
     */
    virtual void onConfigChanged(
        [[maybe_unused]] std::string_view path,
        [[maybe_unused]] const std::optional<json>& newValue) {}

    /**
     * @brief Check if the component supports runtime configuration changes
     *
     * Components that return true can have their configuration modified
     * at runtime through the ConfigRegistry. Components returning false
     * require a restart to apply configuration changes.
     *
     * @return true if runtime reconfiguration is supported
     * @return false if restart is required for configuration changes
     */
    [[nodiscard]] virtual bool supportsRuntimeReconfig() const noexcept {
        return true;
    }

    /**
     * @brief Get the component name for logging/debugging
     *
     * @return std::string_view Human-readable component name
     */
    [[nodiscard]] virtual std::string_view getComponentName() const noexcept {
        return "Unknown";
    }
};

/**
 * @brief Change notification callback type
 *
 * @param path Full configuration path that changed
 * @param oldValue Previous value (nullopt if newly added)
 * @param newValue New value (nullopt if deleted)
 */
using ConfigChangeCallback = std::function<void(
    std::string_view path, const std::optional<json>& oldValue,
    const std::optional<json>& newValue)>;

/**
 * @brief Configuration validation result
 */
struct ConfigValidationError {
    std::string path;     ///< Path to the invalid value
    std::string message;  ///< Error description
    std::string keyword;  ///< JSON Schema keyword that failed (optional)
};

/**
 * @brief Result of configuration validation
 */
struct ConfigValidationResult {
    bool valid{true};                           ///< Whether validation passed
    std::vector<ConfigValidationError> errors;  ///< List of validation errors

    [[nodiscard]] bool isValid() const noexcept { return valid; }

    [[nodiscard]] explicit operator bool() const noexcept { return valid; }

    void addError(std::string path, std::string message,
                  std::string keyword = "") {
        valid = false;
        errors.push_back(
            {std::move(path), std::move(message), std::move(keyword)});
    }
};

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_CORE_CONFIGURABLE_HPP
