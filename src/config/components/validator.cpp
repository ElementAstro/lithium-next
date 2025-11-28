/*
 * validator.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-4

Description: Configuration Validator Implementation

**************************************************/

#include "validator.hpp"

#include <spdlog/spdlog.h>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace lithium::config {

std::string ValidationResult::getErrorMessage() const {
    if (errors.empty())
        return "";

    std::ostringstream oss;
    oss << "Validation errors";
    if (!path.empty()) {
        oss << " at '" << path << "'";
    }
    oss << ":\n";

    for (size_t i = 0; i < errors.size(); ++i) {
        oss << "  " << (i + 1) << ". " << errors[i] << "\n";
    }

    return oss.str();
}

std::string ValidationResult::getWarningMessage() const {
    if (warnings.empty())
        return "";

    std::ostringstream oss;
    oss << "Validation warnings";
    if (!path.empty()) {
        oss << " at '" << path << "'";
    }
    oss << ":\n";

    for (size_t i = 0; i < warnings.size(); ++i) {
        oss << "  " << (i + 1) << ". " << warnings[i] << "\n";
    }

    return oss.str();
}

struct ConfigValidator::Impl {
    Config config_;
    json schema_;
    bool hasSchema_ = false;
    std::unordered_map<std::string, ValidationRule> customRules_;
    std::shared_ptr<spdlog::logger> logger_;

    explicit Impl(const Config& config) : config_(config) {
        logger_ = spdlog::get("config_validator");
        if (!logger_) {
            logger_ = spdlog::default_logger();
        }

        logger_->info("ConfigValidator initialized with strict mode: {}",
                      config_.strictMode);
    }

    ValidationResult validateRecursive(const json& data, const json& schema,
                                       std::string_view path) const {
        ValidationResult result;
        result.path = path;

        if (schema.contains("type")) {
            const auto& expectedType = schema["type"];
            if (!validateType(data, expectedType, result, path)) {
                return result;
            }
        }

        if (data.is_object()) {
            validateObject(data, schema, result, path);
        } else if (data.is_array()) {
            validateArray(data, schema, result, path);
        } else if (data.is_number()) {
            validateNumber(data, schema, result);
        } else if (data.is_string()) {
            validateString(data, schema, result);
        }

        if (schema.contains("enum")) {
            validateEnumConstraint(data, schema["enum"], result);
        }

        return result;
    }

    void validateObject(const json& data, const json& schema,
                        ValidationResult& result, std::string_view path) const {
        if (schema.contains("required")) {
            const auto& required = schema["required"];
            if (required.is_array()) {
                for (const auto& field : required) {
                    if (field.is_string() &&
                        !data.contains(field.get<std::string>())) {
                        result.addError("Missing required field: " +
                                        field.get<std::string>());
                    }
                }
            }
        }

        if (schema.contains("properties")) {
            const auto& properties = schema["properties"];
            for (const auto& [key, value] : data.items()) {
                std::string currentPath = std::string(path);
                if (!currentPath.empty())
                    currentPath += ".";
                currentPath += key;

                if (properties.contains(key)) {
                    auto subResult =
                        validateRecursive(value, properties[key], currentPath);
                    mergeResults(result, subResult);
                } else if (!config_.allowAdditionalProperties) {
                    result.addError("Additional property not allowed: " + key);
                }
            }
        }
    }

    void validateArray(const json& data, const json& schema,
                       ValidationResult& result, std::string_view path) const {
        if (schema.contains("items")) {
            const auto& itemsSchema = schema["items"];
            for (size_t i = 0; i < data.size(); ++i) {
                std::string currentPath =
                    std::string(path) + "[" + std::to_string(i) + "]";
                auto subResult =
                    validateRecursive(data[i], itemsSchema, currentPath);
                mergeResults(result, subResult);
            }
        }

        if (schema.contains("minItems")) {
            size_t minItems = schema["minItems"].get<size_t>();
            if (data.size() < minItems) {
                result.addError("Array has " + std::to_string(data.size()) +
                                " items, minimum is " +
                                std::to_string(minItems));
            }
        }

        if (schema.contains("maxItems")) {
            size_t maxItems = schema["maxItems"].get<size_t>();
            if (data.size() > maxItems) {
                result.addError("Array has " + std::to_string(data.size()) +
                                " items, maximum is " +
                                std::to_string(maxItems));
            }
        }
    }

    void validateNumber(const json& data, const json& schema,
                        ValidationResult& result) const {
        const double value = data.get<double>();

        if (schema.contains("minimum")) {
            double minVal = schema["minimum"].get<double>();
            if (value < minVal) {
                result.addError("Value " + std::to_string(value) +
                                " is below minimum " + std::to_string(minVal));
            }
        }

        if (schema.contains("maximum")) {
            double maxVal = schema["maximum"].get<double>();
            if (value > maxVal) {
                result.addError("Value " + std::to_string(value) +
                                " is above maximum " + std::to_string(maxVal));
            }
        }
    }

    void validateString(const json& data, const json& schema,
                        ValidationResult& result) const {
        const std::string str = data.get<std::string>();
        const size_t length = str.length();

        if (schema.contains("minLength")) {
            size_t minLen = schema["minLength"].get<size_t>();
            if (length < minLen) {
                result.addError("String length " + std::to_string(length) +
                                " is below minimum " + std::to_string(minLen));
            }
        }

        if (schema.contains("maxLength")) {
            size_t maxLen = schema["maxLength"].get<size_t>();
            if (length > maxLen) {
                result.addError("String length " + std::to_string(length) +
                                " is above maximum " + std::to_string(maxLen));
            }
        }

        if (schema.contains("pattern")) {
            const std::string pattern = schema["pattern"].get<std::string>();
            try {
                static std::unordered_map<std::string, std::regex> regexCache;

                auto it = regexCache.find(pattern);
                if (it == regexCache.end()) {
                    it = regexCache.emplace(pattern, std::regex(pattern)).first;
                }

                if (!std::regex_match(str, it->second)) {
                    result.addError("String does not match pattern: " +
                                    pattern);
                }
            } catch (const std::regex_error& e) {
                result.addError("Invalid regex pattern: " + pattern);
            }
        }
    }

    void validateEnumConstraint(const json& data, const json& enumValues,
                                ValidationResult& result) const {
        bool found = false;
        for (const auto& enumValue : enumValues) {
            if (data == enumValue) {
                found = true;
                break;
            }
        }
        if (!found) {
            result.addError("Value is not one of the allowed enum values");
        }
    }

    bool validateType(const json& data, const json& expectedType,
                      ValidationResult& result, std::string_view path) const {
        if (expectedType.is_string()) {
            const std::string type = expectedType.get<std::string>();
            return validateSingleType(data, type, result, path);
        } else if (expectedType.is_array()) {
            for (const auto& type : expectedType) {
                if (type.is_string() &&
                    validateSingleType(data, type.get<std::string>(), result,
                                       path, false)) {
                    return true;
                }
            }
            result.addError("Value does not match any of the expected types");
            return false;
        }
        return true;
    }

    bool validateSingleType(const json& data, const std::string& expectedType,
                            ValidationResult& result, std::string_view path,
                            bool addError = true) const {
        bool isValid = false;

        if (expectedType == "null") {
            isValid = data.is_null();
        } else if (expectedType == "boolean") {
            isValid = data.is_boolean();
        } else if (expectedType == "integer") {
            isValid = data.is_number_integer();
        } else if (expectedType == "number") {
            isValid = data.is_number();
        } else if (expectedType == "string") {
            isValid = data.is_string();
        } else if (expectedType == "array") {
            isValid = data.is_array();
        } else if (expectedType == "object") {
            isValid = data.is_object();
        } else {
            if (addError) {
                result.addError("Unknown type: " + expectedType);
            }
            return false;
        }

        if (!isValid && addError) {
            result.addError("Expected type " + expectedType + " but got " +
                            (data.is_null() ? "null" : data.type_name()));
        }

        return isValid;
    }

    void mergeResults(ValidationResult& target,
                      const ValidationResult& source) const {
        if (!source.isValid) {
            target.isValid = false;
        }
        target.errors.reserve(target.errors.size() + source.errors.size());
        target.warnings.reserve(target.warnings.size() +
                                source.warnings.size());
        target.errors.insert(target.errors.end(), source.errors.begin(),
                             source.errors.end());
        target.warnings.insert(target.warnings.end(), source.warnings.begin(),
                               source.warnings.end());
    }
};

ConfigValidator::ConfigValidator(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {}

ConfigValidator::~ConfigValidator() = default;

ConfigValidator::ConfigValidator(ConfigValidator&&) noexcept = default;
ConfigValidator& ConfigValidator::operator=(ConfigValidator&&) noexcept =
    default;

bool ConfigValidator::loadSchema(const std::string& schemaPath) {
    try {
        std::ifstream file(schemaPath);
        if (!file.is_open()) {
            impl_->logger_->error("Failed to open schema file: {}", schemaPath);
            return false;
        }

        json schema;
        file >> schema;
        return setSchema(schema);
    } catch (const std::exception& e) {
        impl_->logger_->error("Error loading schema from {}: {}", schemaPath,
                              e.what());
        return false;
    }
}

bool ConfigValidator::setSchema(const std::string& schemaJson) {
    try {
        json schema = json::parse(schemaJson);
        return setSchema(schema);
    } catch (const std::exception& e) {
        impl_->logger_->error("Error parsing schema JSON: {}", e.what());
        return false;
    }
}

bool ConfigValidator::setSchema(const json& schema) {
    try {
        impl_->schema_ = schema;
        impl_->hasSchema_ = true;
        impl_->logger_->info("JSON schema loaded successfully");
        return true;
    } catch (const std::exception& e) {
        impl_->logger_->error("Error setting schema: {}", e.what());
        return false;
    }
}

ValidationResult ConfigValidator::validate(const json& data,
                                           std::string_view path) const {
    ValidationResult result;
    result.path = path;

    if (!impl_->hasSchema_) {
        result.addWarning("No schema loaded for validation");
        return result;
    }

    try {
        result = impl_->validateRecursive(data, impl_->schema_, path);

        for (const auto& [name, rule] : impl_->customRules_) {
            try {
                auto customResult = rule(data, path);
                impl_->mergeResults(result, customResult);
            } catch (const std::exception& e) {
                result.addError("Custom rule '" + name +
                                "' failed: " + e.what());
            }
        }

        if (result.isValid) {
            impl_->logger_->debug("Validation passed for path: {}", path);
        } else {
            impl_->logger_->warn(
                "Validation failed for path: {} with {} errors", path,
                result.errors.size());
        }

    } catch (const std::exception& e) {
        result.addError("Validation error: " + std::string(e.what()));
        impl_->logger_->error("Exception during validation: {}", e.what());
    }

    return result;
}

ValidationResult ConfigValidator::validateValue(
    const json& data, std::string_view valuePath) const {
    try {
        const json* current = &data;
        std::string pathStr(valuePath);

        if (!pathStr.empty() && pathStr != "/") {
            std::istringstream ss(pathStr);
            std::string segment;

            while (std::getline(ss, segment, '.')) {
                if (segment.empty())
                    continue;

                if (current->is_object() && current->contains(segment)) {
                    current = &(*current)[segment];
                } else {
                    ValidationResult result;
                    result.addError("Path not found: " + segment);
                    return result;
                }
            }
        }

        return validate(*current, valuePath);
    } catch (const std::exception& e) {
        ValidationResult result;
        result.addError("Error navigating to path: " + std::string(e.what()));
        return result;
    }
}

void ConfigValidator::addRule(std::string name, ValidationRule rule) {
    impl_->customRules_[std::move(name)] = std::move(rule);
    impl_->logger_->debug("Added custom validation rule: {}", name);
}

bool ConfigValidator::removeRule(const std::string& name) {
    const bool removed = impl_->customRules_.erase(name) > 0;
    if (removed) {
        impl_->logger_->debug("Removed custom validation rule: {}", name);
    }
    return removed;
}

void ConfigValidator::clearRules() {
    const auto count = impl_->customRules_.size();
    impl_->customRules_.clear();
    impl_->logger_->debug("Cleared {} custom validation rules", count);
}

ValidationResult ConfigValidator::validateRequired(
    const json& data, const std::vector<std::string>& requiredFields,
    std::string_view path) {
    ValidationResult result;
    result.path = path;

    if (!data.is_object()) {
        result.addError("Expected object for required field validation");
        return result;
    }

    for (const auto& field : requiredFields) {
        if (!data.contains(field)) {
            result.addError("Missing required field: " + field);
        }
    }

    return result;
}

ValidationResult ConfigValidator::validateRange(const json& value,
                                                std::optional<double> min,
                                                std::optional<double> max,
                                                std::string_view path) {
    ValidationResult result;
    result.path = path;

    if (!value.is_number()) {
        result.addError("Expected numeric value for range validation");
        return result;
    }

    const double num = value.get<double>();

    if (min.has_value() && num < min.value()) {
        result.addError("Value " + std::to_string(num) + " is below minimum " +
                        std::to_string(min.value()));
    }

    if (max.has_value() && num > max.value()) {
        result.addError("Value " + std::to_string(num) + " is above maximum " +
                        std::to_string(max.value()));
    }

    return result;
}

ValidationResult ConfigValidator::validatePattern(const json& value,
                                                  const std::string& pattern,
                                                  std::string_view path) {
    ValidationResult result;
    result.path = path;

    if (!value.is_string()) {
        result.addError("Expected string value for pattern validation");
        return result;
    }

    try {
        std::regex regex(pattern);
        const std::string str = value.get<std::string>();
        if (!std::regex_match(str, regex)) {
            result.addError("String does not match pattern: " + pattern);
        }
    } catch (const std::regex_error& e) {
        result.addError("Invalid regex pattern: " + pattern);
    }

    return result;
}

ValidationResult ConfigValidator::validateLength(
    const json& value, std::optional<size_t> minLength,
    std::optional<size_t> maxLength, std::string_view path) {
    ValidationResult result;
    result.path = path;

    if (!value.is_string()) {
        result.addError("Expected string value for length validation");
        return result;
    }

    const size_t length = value.get<std::string>().length();

    if (minLength.has_value() && length < minLength.value()) {
        result.addError("String length " + std::to_string(length) +
                        " is below minimum " +
                        std::to_string(minLength.value()));
    }

    if (maxLength.has_value() && length > maxLength.value()) {
        result.addError("String length " + std::to_string(length) +
                        " is above maximum " +
                        std::to_string(maxLength.value()));
    }

    return result;
}

ValidationResult ConfigValidator::validateArraySize(
    const json& value, std::optional<size_t> minItems,
    std::optional<size_t> maxItems, std::string_view path) {
    ValidationResult result;
    result.path = path;

    if (!value.is_array()) {
        result.addError("Expected array value for size validation");
        return result;
    }

    const size_t size = value.size();

    if (minItems.has_value() && size < minItems.value()) {
        result.addError("Array has " + std::to_string(size) +
                        " items, minimum is " +
                        std::to_string(minItems.value()));
    }

    if (maxItems.has_value() && size > maxItems.value()) {
        result.addError("Array has " + std::to_string(size) +
                        " items, maximum is " +
                        std::to_string(maxItems.value()));
    }

    return result;
}

ValidationResult ConfigValidator::validateEnum(
    const json& value, const std::vector<json>& allowedValues,
    std::string_view path) {
    ValidationResult result;
    result.path = path;

    bool found = false;
    for (const auto& allowedValue : allowedValues) {
        if (value == allowedValue) {
            found = true;
            break;
        }
    }

    if (!found) {
        result.addError("Value is not one of the allowed enum values");
    }

    return result;
}

bool ConfigValidator::hasSchema() const { return impl_->hasSchema_; }

const ConfigValidator::Config& ConfigValidator::getConfig() const {
    return impl_->config_;
}

void ConfigValidator::setConfig(const Config& newConfig) {
    impl_->config_ = newConfig;
    impl_->logger_->info("Validator configuration updated");
}

}  // namespace lithium::config
