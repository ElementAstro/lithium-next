/*
 * validator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration Validator Component

**************************************************/

#ifndef LITHIUM_CONFIG_COMPONENTS_VALIDATOR_HPP
#define LITHIUM_CONFIG_COMPONENTS_VALIDATOR_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace lithium::config {

/**
 * @brief Schema validation configuration
 */
struct ValidatorConfig {
    bool strictMode = false;                ///< Strict validation mode
    bool allowAdditionalProperties = true;  ///< Allow extra properties
    bool validateFormats = true;            ///< Validate string formats
    bool coerceTypes = false;               ///< Attempt type coercion
};

/**
 * @brief Validation result with detailed error information
 */
struct ValidationResult {
    bool isValid = true;                ///< Whether validation passed
    std::vector<std::string> errors;    ///< List of validation errors
    std::vector<std::string> warnings;  ///< List of validation warnings
    std::string path;                   ///< Path where validation was performed

    void addError(std::string error) {
        isValid = false;
        errors.emplace_back(std::move(error));
    }

    void addWarning(std::string warning) {
        warnings.emplace_back(std::move(warning));
    }

    [[nodiscard]] bool hasErrors() const noexcept { return !errors.empty(); }
    [[nodiscard]] bool hasWarnings() const noexcept {
        return !warnings.empty();
    }
    [[nodiscard]] std::string getErrorMessage() const;
    [[nodiscard]] std::string getWarningMessage() const;
};

/**
 * @brief JSON Schema-based configuration validator
 *
 * This class provides comprehensive validation capabilities for JSON
 * configurations:
 * - JSON Schema validation (draft-07 compatible)
 * - Custom validation rules
 * - Type checking and constraints
 * - Range validation for numeric values
 * - Pattern matching for strings
 * - Required field validation
 */
class ConfigValidator {
public:
    using Config = ValidatorConfig;

    /**
     * @brief Constructor
     * @param config Validator configuration
     */
    explicit ConfigValidator(const Config& config = {});

    /**
     * @brief Destructor
     */
    ~ConfigValidator();

    ConfigValidator(const ConfigValidator&) = delete;
    ConfigValidator& operator=(const ConfigValidator&) = delete;
    ConfigValidator(ConfigValidator&&) noexcept;
    ConfigValidator& operator=(ConfigValidator&&) noexcept;

    /**
     * @brief Load schema from file
     * @param schemaPath Path to schema file
     * @return True if schema was loaded successfully
     */
    bool loadSchema(const std::string& schemaPath);

    /**
     * @brief Set schema from JSON string
     * @param schemaJson JSON schema string
     * @return True if schema was set successfully
     */
    bool setSchema(const std::string& schemaJson);

    /**
     * @brief Set schema from JSON object
     * @param schema JSON schema object
     * @return True if schema was set successfully
     */
    bool setSchema(const json& schema);

    /**
     * @brief Validate data against schema
     * @param data JSON data to validate
     * @param path Optional path for error reporting
     * @return Validation result
     */
    [[nodiscard]] ValidationResult validate(const json& data,
                                            std::string_view path = "") const;

    /**
     * @brief Validate a specific value path
     * @param data JSON data containing the value
     * @param valuePath Path to the value to validate
     * @return Validation result
     */
    [[nodiscard]] ValidationResult validateValue(
        const json& data, std::string_view valuePath) const;

    /**
     * @brief Custom validation rule function type
     */
    using ValidationRule =
        std::function<ValidationResult(const json&, std::string_view)>;

    /**
     * @brief Add custom validation rule
     * @param name Rule name
     * @param rule Validation function
     */
    void addRule(std::string name, ValidationRule rule);

    /**
     * @brief Remove custom validation rule
     * @param name Rule name to remove
     * @return True if rule was removed
     */
    bool removeRule(const std::string& name);

    /**
     * @brief Clear all custom validation rules
     */
    void clearRules();

    // Static validation helpers

    /**
     * @brief Validate required fields
     * @param data JSON data to validate
     * @param requiredFields List of required field names
     * @param path Optional path for error reporting
     * @return Validation result
     */
    [[nodiscard]] static ValidationResult validateRequired(
        const json& data, const std::vector<std::string>& requiredFields,
        std::string_view path = "");

    /**
     * @brief Validate numeric range
     * @param value JSON value to validate
     * @param min Optional minimum value
     * @param max Optional maximum value
     * @param path Optional path for error reporting
     * @return Validation result
     */
    [[nodiscard]] static ValidationResult validateRange(
        const json& value, std::optional<double> min = std::nullopt,
        std::optional<double> max = std::nullopt, std::string_view path = "");

    /**
     * @brief Validate string pattern
     * @param value JSON value to validate
     * @param pattern Regex pattern
     * @param path Optional path for error reporting
     * @return Validation result
     */
    [[nodiscard]] static ValidationResult validatePattern(
        const json& value, const std::string& pattern,
        std::string_view path = "");

    /**
     * @brief Validate string length
     * @param value JSON value to validate
     * @param minLength Optional minimum length
     * @param maxLength Optional maximum length
     * @param path Optional path for error reporting
     * @return Validation result
     */
    [[nodiscard]] static ValidationResult validateLength(
        const json& value, std::optional<size_t> minLength = std::nullopt,
        std::optional<size_t> maxLength = std::nullopt,
        std::string_view path = "");

    /**
     * @brief Validate array size
     * @param value JSON value to validate
     * @param minItems Optional minimum items
     * @param maxItems Optional maximum items
     * @param path Optional path for error reporting
     * @return Validation result
     */
    [[nodiscard]] static ValidationResult validateArraySize(
        const json& value, std::optional<size_t> minItems = std::nullopt,
        std::optional<size_t> maxItems = std::nullopt,
        std::string_view path = "");

    /**
     * @brief Validate enum values
     * @param value JSON value to validate
     * @param allowedValues List of allowed values
     * @param path Optional path for error reporting
     * @return Validation result
     */
    [[nodiscard]] static ValidationResult validateEnum(
        const json& value, const std::vector<json>& allowedValues,
        std::string_view path = "");

    /**
     * @brief Check if schema is loaded
     * @return True if schema is loaded
     */
    [[nodiscard]] bool hasSchema() const;

    /**
     * @brief Get current schema
     * @return Current schema JSON object (empty if not set)
     */
    [[nodiscard]] json getSchema() const;

    /**
     * @brief Get current configuration
     * @return Current validator configuration
     */
    [[nodiscard]] const Config& getConfig() const;

    /**
     * @brief Set new configuration
     * @param newConfig New configuration
     */
    void setConfig(const Config& newConfig);

    /**
     * @brief Get all registered custom rule names
     * @return Vector of rule names
     */
    [[nodiscard]] std::vector<std::string> getRuleNames() const;

    /**
     * @brief Check if a custom rule exists
     * @param name Rule name to check
     * @return True if rule exists
     */
    [[nodiscard]] bool hasRule(const std::string& name) const;

    // ========================================================================
    // Hook Support
    // ========================================================================

    /**
     * @brief Validation event types for hooks
     */
    enum class ValidationEvent {
        BEFORE_VALIDATE,   ///< Before validation starts
        AFTER_VALIDATE,    ///< After validation completes
        VALIDATION_ERROR,  ///< Validation error occurred
        SCHEMA_LOADED,     ///< Schema was loaded
        RULE_ADDED,        ///< Custom rule was added
        RULE_REMOVED       ///< Custom rule was removed
    };

    /**
     * @brief Validation hook callback signature
     * @param event Type of validation event
     * @param path Configuration path being validated
     * @param result Validation result (for AFTER_VALIDATE/VALIDATION_ERROR)
     */
    using ValidationHook = std::function<void(ValidationEvent event,
                                              std::string_view path,
                                              const ValidationResult& result)>;

    /**
     * @brief Register a validation event hook
     * @param hook Callback function for validation events
     * @return Hook ID for later removal
     */
    size_t addHook(ValidationHook hook);

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
     * @brief Validate with detailed options
     * @param data JSON data to validate
     * @param options Validation options to override defaults
     * @param path Optional path for error reporting
     * @return Validation result
     */
    [[nodiscard]] ValidationResult validateWithOptions(
        const json& data, const Config& options,
        std::string_view path = "") const;

    /**
     * @brief Validate multiple data objects in batch
     * @param dataList Vector of JSON data to validate
     * @param path Optional path prefix for error reporting
     * @return Vector of validation results
     */
    [[nodiscard]] std::vector<ValidationResult> validateBatch(
        const std::vector<json>& dataList,
        std::string_view path = "") const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium::config

// Backward compatibility aliases
namespace lithium {
using ConfigValidator = lithium::config::ConfigValidator;
using ConfigValidatorConfig = lithium::config::ValidatorConfig;
using ValidationResult = lithium::config::ValidationResult;
}  // namespace lithium

#endif  // LITHIUM_CONFIG_COMPONENTS_VALIDATOR_HPP
