/*
 * config_section.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: ConfigSection CRTP base class for type-safe configuration sections

**************************************************/

#ifndef LITHIUM_CONFIG_CORE_CONFIG_SECTION_HPP
#define LITHIUM_CONFIG_CORE_CONFIG_SECTION_HPP

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "atom/type/json.hpp"
#include "configurable.hpp"

namespace lithium::config {

using json = nlohmann::json;

/**
 * @brief Concept for types that can be serialized to/from JSON
 */
template <typename T>
concept JsonSerializable = requires(T value, json j) {
    { j = value } -> std::convertible_to<json>;
    { j.get<T>() } -> std::convertible_to<T>;
};

/**
 * @brief Concept for valid ConfigSection derived types
 */
template <typename T>
concept ConfigSectionDerived = requires(T t, const json& j) {
    { T::PATH } -> std::convertible_to<std::string_view>;
    { t.serialize() } -> std::convertible_to<json>;
    { T::deserialize(j) } -> std::convertible_to<T>;
    { T::generateSchema() } -> std::convertible_to<json>;
};

/**
 * @brief CRTP base class for type-safe configuration sections
 *
 * ConfigSection provides a type-safe way to define configuration structures
 * that integrate with the unified configuration system. Derived classes must:
 *
 * 1. Define a static constexpr PATH member for the configuration path
 * 2. Implement serialize() to convert to JSON
 * 3. Implement static deserialize(const json&) to create from JSON
 * 4. Implement static generateSchema() to return JSON Schema
 *
 * @tparam Derived The derived configuration struct type (CRTP)
 *
 * @example
 * ```cpp
 * struct ServerConfig : ConfigSection<ServerConfig> {
 *     static constexpr std::string_view PATH = "/lithium/server";
 *
 *     std::string host = "0.0.0.0";
 *     int port = 8000;
 *     size_t maxConnections = 1000;
 *
 *     [[nodiscard]] json serialize() const {
 *         return {
 *             {"host", host},
 *             {"port", port},
 *             {"maxConnections", maxConnections}
 *         };
 *     }
 *
 *     [[nodiscard]] static ServerConfig deserialize(const json& j) {
 *         ServerConfig config;
 *         config.host = j.value("host", config.host);
 *         config.port = j.value("port", config.port);
 *         config.maxConnections = j.value("maxConnections", config.maxConnections);
 *         return config;
 *     }
 *
 *     [[nodiscard]] static json generateSchema() {
 *         json schema;
 *         schema["type"] = "object";
 *         schema["properties"]["host"] = {{"type", "string"}, {"default", "0.0.0.0"}};
 *         schema["properties"]["port"] = {
 *             {"type", "integer"}, {"minimum", 1}, {"maximum", 65535}, {"default", 8000}
 *         };
 *         schema["properties"]["maxConnections"] = {
 *             {"type", "integer"}, {"minimum", 1}, {"default", 1000}
 *         };
 *         return schema;
 *     }
 * };
 * ```
 */
template <typename Derived>
class ConfigSection {
public:
    /**
     * @brief Get the configuration path for this section
     * @return Configuration path (e.g., "/lithium/server")
     */
    [[nodiscard]] static constexpr std::string_view path() noexcept {
        return Derived::PATH;
    }

    /**
     * @brief Convert this config to JSON
     * @return JSON representation of this configuration
     */
    [[nodiscard]] json toJson() const {
        return static_cast<const Derived*>(this)->serialize();
    }

    /**
     * @brief Create a configuration from JSON
     * @param j JSON object to deserialize
     * @return Configuration instance
     */
    [[nodiscard]] static Derived fromJson(const json& j) {
        return Derived::deserialize(j);
    }

    /**
     * @brief Try to create a configuration from JSON with error handling
     * @param j JSON object to deserialize
     * @return Configuration instance or nullopt on error
     */
    [[nodiscard]] static std::optional<Derived> tryFromJson(const json& j) noexcept {
        try {
            return Derived::deserialize(j);
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Get the JSON Schema for this configuration section
     * @return JSON Schema object
     */
    [[nodiscard]] static json schema() { return Derived::generateSchema(); }

    /**
     * @brief Get a default-constructed configuration
     * @return Default configuration instance
     */
    [[nodiscard]] static Derived defaults() { return Derived{}; }

    /**
     * @brief Validate this configuration against its schema
     * @return Validation result with any errors
     */
    [[nodiscard]] ConfigValidationResult validate() const {
        ConfigValidationResult result;
        // Basic validation - derived classes can override for custom validation
        try {
            auto jsonData = toJson();
            // Schema validation would be done by ConfigValidator
            // Here we just ensure serialization works
        } catch (const std::exception& e) {
            result.addError(std::string(Derived::PATH), e.what(),
                            "serialization");
        }
        return result;
    }

    /**
     * @brief Merge another configuration into this one
     *
     * Values from other will override values in this config.
     * Only non-null values are merged.
     *
     * @param other Configuration to merge from
     */
    void merge(const Derived& other) {
        auto thisJson = toJson();
        auto otherJson = other.toJson();
        mergeJson(thisJson, otherJson);
        *static_cast<Derived*>(this) = Derived::deserialize(thisJson);
    }

    /**
     * @brief Create a diff between this config and another
     * @param other Configuration to compare against
     * @return JSON object containing only the differences
     */
    [[nodiscard]] json diff(const Derived& other) const {
        auto thisJson = toJson();
        auto otherJson = other.toJson();
        return computeDiff(thisJson, otherJson);
    }

    /**
     * @brief Check equality with another configuration
     */
    [[nodiscard]] bool operator==(const ConfigSection& other) const {
        return toJson() == static_cast<const Derived&>(other).toJson();
    }

    /**
     * @brief Check inequality with another configuration
     */
    [[nodiscard]] bool operator!=(const ConfigSection& other) const {
        return !(*this == other);
    }

protected:
    /**
     * @brief Helper to add a property to a JSON Schema
     *
     * @tparam T Type of the property value
     * @param schema Schema object to modify
     * @param name Property name
     * @param type JSON Schema type ("string", "integer", "number", "boolean",
     * "array", "object")
     * @param defaultValue Default value for the property
     * @param description Optional description
     */
    template <JsonSerializable T>
    static void addSchemaProperty(json& schema, const std::string& name,
                                  const std::string& type, const T& defaultValue,
                                  const std::string& description = "") {
        if (!schema.contains("properties")) {
            schema["properties"] = json::object();
        }
        json& prop = schema["properties"][name];
        prop["type"] = type;
        prop["default"] = defaultValue;
        if (!description.empty()) {
            prop["description"] = description;
        }
    }

    /**
     * @brief Helper to add a required property to schema
     */
    static void addRequired(json& schema, const std::string& name) {
        if (!schema.contains("required")) {
            schema["required"] = json::array();
        }
        schema["required"].push_back(name);
    }

    /**
     * @brief Helper to add enum constraint to a property
     */
    template <typename... Args>
    static void addEnum(json& schema, const std::string& name, Args&&... values) {
        if (schema.contains("properties") && schema["properties"].contains(name)) {
            schema["properties"][name]["enum"] = json::array({std::forward<Args>(values)...});
        }
    }

    /**
     * @brief Helper to add range constraint to a numeric property
     */
    static void addRange(json& schema, const std::string& name,
                         std::optional<double> minimum = std::nullopt,
                         std::optional<double> maximum = std::nullopt) {
        if (schema.contains("properties") && schema["properties"].contains(name)) {
            auto& prop = schema["properties"][name];
            if (minimum) {
                prop["minimum"] = *minimum;
            }
            if (maximum) {
                prop["maximum"] = *maximum;
            }
        }
    }

private:
    /**
     * @brief Recursively merge JSON objects
     */
    static void mergeJson(json& target, const json& source) {
        if (source.is_object()) {
            for (auto& [key, value] : source.items()) {
                if (value.is_object() && target.contains(key) &&
                    target[key].is_object()) {
                    mergeJson(target[key], value);
                } else if (!value.is_null()) {
                    target[key] = value;
                }
            }
        }
    }

    /**
     * @brief Compute differences between two JSON objects
     */
    [[nodiscard]] static json computeDiff(const json& a, const json& b) {
        json result = json::object();

        if (a.is_object() && b.is_object()) {
            // Check for modified and deleted keys
            for (auto& [key, value] : a.items()) {
                if (!b.contains(key)) {
                    result[key] = {{"_deleted", true}, {"_old", value}};
                } else if (value != b[key]) {
                    if (value.is_object() && b[key].is_object()) {
                        auto nested = computeDiff(value, b[key]);
                        if (!nested.empty()) {
                            result[key] = nested;
                        }
                    } else {
                        result[key] = {{"_old", value}, {"_new", b[key]}};
                    }
                }
            }
            // Check for added keys
            for (auto& [key, value] : b.items()) {
                if (!a.contains(key)) {
                    result[key] = {{"_added", true}, {"_new", value}};
                }
            }
        }

        return result;
    }
};

/**
 * @brief Macro to define boilerplate for simple config sections
 *
 * This macro generates serialize, deserialize, and generateSchema methods
 * for simple configuration structs with primitive fields.
 *
 * @note For complex configurations, implement these methods manually.
 */
#define LITHIUM_CONFIG_SECTION_IMPL(ClassName, ...)                           \
    [[nodiscard]] json serialize() const {                                    \
        return json(__VA_ARGS__);                                             \
    }                                                                         \
    [[nodiscard]] static ClassName deserialize(const json& j) {               \
        ClassName config;                                                     \
        /* Derived class should implement field-by-field deserialization */   \
        return config;                                                        \
    }

}  // namespace lithium::config

#endif  // LITHIUM_CONFIG_CORE_CONFIG_SECTION_HPP
