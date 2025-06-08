/*
 * config_validator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration Validator Component

**************************************************/

#ifndef LITHIUM_CONFIG_VALIDATOR_HPP
#define LITHIUM_CONFIG_VALIDATOR_HPP

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace lithium {

/**
 * @brief Schema validation configuration
 */
struct ConfigValidatorConfig {
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
    using Config = ConfigValidatorConfig;

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

    bool loadSchema(const std::string& schemaPath);
    bool setSchema(const std::string& schemaJson);
    bool setSchema(const json& schema);

    [[nodiscard]] ValidationResult validate(const json& data,
                                            std::string_view path = "") const;
    [[nodiscard]] ValidationResult validateValue(
        const json& data, std::string_view valuePath) const;

    using ValidationRule =
        std::function<ValidationResult(const json&, std::string_view)>;

    void addRule(std::string name, ValidationRule rule);
    bool removeRule(const std::string& name);
    void clearRules();

    [[nodiscard]] static ValidationResult validateRequired(
        const json& data, const std::vector<std::string>& requiredFields,
        std::string_view path = "");

    [[nodiscard]] static ValidationResult validateRange(
        const json& value, std::optional<double> min = std::nullopt,
        std::optional<double> max = std::nullopt, std::string_view path = "");

    [[nodiscard]] static ValidationResult validatePattern(
        const json& value, const std::string& pattern,
        std::string_view path = "");

    [[nodiscard]] static ValidationResult validateLength(
        const json& value, std::optional<size_t> minLength = std::nullopt,
        std::optional<size_t> maxLength = std::nullopt,
        std::string_view path = "");

    [[nodiscard]] static ValidationResult validateArraySize(
        const json& value, std::optional<size_t> minItems = std::nullopt,
        std::optional<size_t> maxItems = std::nullopt,
        std::string_view path = "");

    [[nodiscard]] static ValidationResult validateEnum(
        const json& value, const std::vector<json>& allowedValues,
        std::string_view path = "");

    [[nodiscard]] bool hasSchema() const;
    [[nodiscard]] const Config& getConfig() const;
    void setConfig(const Config& newConfig);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lithium

#endif  // LITHIUM_CONFIG_VALIDATOR_HPP
