/**
 * @file test_validator.cpp
 * @brief Comprehensive unit tests for ConfigValidator component
 */

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

#include "config/components/validator.hpp"
#include "atom/type/json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace lithium::config;

namespace lithium::config::test {

class ConfigValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_validator_test";
        fs::create_directories(testDir_);
        createTestSchemas();
        validator_ = std::make_unique<ConfigValidator>();
    }

    void TearDown() override {
        validator_.reset();
        fs::remove_all(testDir_);
    }

    void createTestSchemas() {
        // Basic schema
        std::ofstream basic(testDir_ / "basic_schema.json");
        basic << R"({
            "type": "object",
            "properties": {
                "name": {"type": "string"},
                "age": {"type": "integer", "minimum": 0, "maximum": 150}
            },
            "required": ["name"]
        })";
        basic.close();

        // Complex schema
        std::ofstream complex(testDir_ / "complex_schema.json");
        complex << R"({
            "type": "object",
            "properties": {
                "user": {
                    "type": "object",
                    "properties": {
                        "email": {"type": "string", "pattern": "^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$"},
                        "roles": {"type": "array", "items": {"type": "string"}}
                    }
                }
            }
        })";
        complex.close();
    }

    fs::path testDir_;
    std::unique_ptr<ConfigValidator> validator_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ConfigValidatorTest, DefaultConstruction) {
    ConfigValidator validator;
    EXPECT_FALSE(validator.hasSchema());
}

TEST_F(ConfigValidatorTest, ConstructionWithConfig) {
    ValidatorConfig config;
    config.strictMode = true;
    config.allowAdditionalProperties = false;

    ConfigValidator validator(config);
    EXPECT_EQ(validator.getConfig().strictMode, true);
    EXPECT_EQ(validator.getConfig().allowAdditionalProperties, false);
}

TEST_F(ConfigValidatorTest, MoveConstruction) {
    json schema = {{"type", "object"}};
    validator_->setSchema(schema);
    ConfigValidator moved(std::move(*validator_));
    EXPECT_TRUE(moved.hasSchema());
}

TEST_F(ConfigValidatorTest, MoveAssignment) {
    json schema = {{"type", "object"}};
    validator_->setSchema(schema);
    ConfigValidator other;
    other = std::move(*validator_);
    EXPECT_TRUE(other.hasSchema());
}

// ============================================================================
// Schema Loading Tests
// ============================================================================

TEST_F(ConfigValidatorTest, LoadSchemaFromFile) {
    EXPECT_TRUE(validator_->loadSchema((testDir_ / "basic_schema.json").string()));
    EXPECT_TRUE(validator_->hasSchema());
}

TEST_F(ConfigValidatorTest, LoadSchemaFromNonExistentFile) {
    EXPECT_FALSE(validator_->loadSchema((testDir_ / "nonexistent.json").string()));
    EXPECT_FALSE(validator_->hasSchema());
}

TEST_F(ConfigValidatorTest, SetSchemaFromString) {
    std::string schemaStr = R"({"type": "object", "properties": {"name": {"type": "string"}}})";
    EXPECT_TRUE(validator_->setSchema(schemaStr));
    EXPECT_TRUE(validator_->hasSchema());
}

TEST_F(ConfigValidatorTest, SetSchemaFromJson) {
    json schema = {
        {"type", "object"},
        {"properties", {{"name", {{"type", "string"}}}}}
    };
    EXPECT_TRUE(validator_->setSchema(schema));
    EXPECT_TRUE(validator_->hasSchema());
}

TEST_F(ConfigValidatorTest, SetSchemaInvalidJson) {
    EXPECT_FALSE(validator_->setSchema("not valid json {{{"));
    EXPECT_FALSE(validator_->hasSchema());
}

TEST_F(ConfigValidatorTest, GetSchema) {
    json schema = {{"type", "object"}};
    validator_->setSchema(schema);
    auto retrieved = validator_->getSchema();
    EXPECT_EQ(retrieved["type"], "object");
}

TEST_F(ConfigValidatorTest, HasSchema) {
    EXPECT_FALSE(validator_->hasSchema());
    validator_->setSchema(json{{"type", "object"}});
    EXPECT_TRUE(validator_->hasSchema());
}

// ============================================================================
// Basic Validation Tests
// ============================================================================

TEST_F(ConfigValidatorTest, ValidateValidData) {
    json schema = {
        {"type", "object"},
        {"properties", {{"name", {{"type", "string"}}}}}
    };
    validator_->setSchema(schema);

    json data = {{"name", "John"}};
    auto result = validator_->validate(data);
    EXPECT_TRUE(result.isValid);
    EXPECT_FALSE(result.hasErrors());
}

TEST_F(ConfigValidatorTest, ValidateInvalidType) {
    json schema = {
        {"type", "object"},
        {"properties", {{"age", {{"type", "integer"}}}}}
    };
    validator_->setSchema(schema);

    json data = {{"age", "not_an_integer"}};
    auto result = validator_->validate(data);
    EXPECT_FALSE(result.isValid);
    EXPECT_TRUE(result.hasErrors());
}

TEST_F(ConfigValidatorTest, ValidateWithPath) {
    json schema = {{"type", "string"}};
    validator_->setSchema(schema);

    json data = "valid_string";
    auto result = validator_->validate(data, "config/path");
    EXPECT_TRUE(result.isValid);
    EXPECT_EQ(result.path, "config/path");
}

TEST_F(ConfigValidatorTest, ValidateValue) {
    json schema = {
        {"type", "object"},
        {"properties", {
            {"nested", {
                {"type", "object"},
                {"properties", {{"value", {{"type", "integer"}}}}}
            }}
        }}
    };
    validator_->setSchema(schema);

    json data = {{"nested", {{"value", 42}}}};
    auto result = validator_->validateValue(data, "nested/value");
    EXPECT_TRUE(result.isValid);
}

// ============================================================================
// ValidationResult Tests
// ============================================================================

TEST_F(ConfigValidatorTest, ValidationResultAddError) {
    ValidationResult result;
    EXPECT_TRUE(result.isValid);

    result.addError("Test error");
    EXPECT_FALSE(result.isValid);
    EXPECT_TRUE(result.hasErrors());
    EXPECT_EQ(result.errors.size(), 1);
}

TEST_F(ConfigValidatorTest, ValidationResultAddWarning) {
    ValidationResult result;
    result.addWarning("Test warning");

    EXPECT_TRUE(result.isValid);  // Warnings don't affect validity
    EXPECT_TRUE(result.hasWarnings());
    EXPECT_EQ(result.warnings.size(), 1);
}

TEST_F(ConfigValidatorTest, ValidationResultGetErrorMessage) {
    ValidationResult result;
    result.addError("Error 1");
    result.addError("Error 2");

    std::string msg = result.getErrorMessage();
    EXPECT_NE(msg.find("Error 1"), std::string::npos);
    EXPECT_NE(msg.find("Error 2"), std::string::npos);
}

TEST_F(ConfigValidatorTest, ValidationResultGetWarningMessage) {
    ValidationResult result;
    result.addWarning("Warning 1");
    result.addWarning("Warning 2");

    std::string msg = result.getWarningMessage();
    EXPECT_NE(msg.find("Warning 1"), std::string::npos);
    EXPECT_NE(msg.find("Warning 2"), std::string::npos);
}

// ============================================================================
// Custom Rules Tests
// ============================================================================

TEST_F(ConfigValidatorTest, AddCustomRule) {
    validator_->addRule("positive_check", [](const json& data, std::string_view path) {
        ValidationResult result;
        if (data.is_number() && data.get<int>() <= 0) {
            result.addError("Value must be positive");
        }
        return result;
    });

    EXPECT_TRUE(validator_->hasRule("positive_check"));
}

TEST_F(ConfigValidatorTest, RemoveCustomRule) {
    validator_->addRule("test_rule", [](const json&, std::string_view) {
        return ValidationResult{};
    });

    EXPECT_TRUE(validator_->removeRule("test_rule"));
    EXPECT_FALSE(validator_->hasRule("test_rule"));
}

TEST_F(ConfigValidatorTest, RemoveNonExistentRule) {
    EXPECT_FALSE(validator_->removeRule("nonexistent"));
}

TEST_F(ConfigValidatorTest, ClearRules) {
    validator_->addRule("rule1", [](const json&, std::string_view) {
        return ValidationResult{};
    });
    validator_->addRule("rule2", [](const json&, std::string_view) {
        return ValidationResult{};
    });

    validator_->clearRules();
    EXPECT_FALSE(validator_->hasRule("rule1"));
    EXPECT_FALSE(validator_->hasRule("rule2"));
}

TEST_F(ConfigValidatorTest, GetRuleNames) {
    validator_->addRule("rule1", [](const json&, std::string_view) {
        return ValidationResult{};
    });
    validator_->addRule("rule2", [](const json&, std::string_view) {
        return ValidationResult{};
    });

    auto names = validator_->getRuleNames();
    EXPECT_EQ(names.size(), 2);
    EXPECT_TRUE(std::find(names.begin(), names.end(), "rule1") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "rule2") != names.end());
}

TEST_F(ConfigValidatorTest, HasRule) {
    EXPECT_FALSE(validator_->hasRule("test_rule"));
    validator_->addRule("test_rule", [](const json&, std::string_view) {
        return ValidationResult{};
    });
    EXPECT_TRUE(validator_->hasRule("test_rule"));
}

// ============================================================================
// Static Validation Helpers Tests
// ============================================================================

TEST_F(ConfigValidatorTest, ValidateRequired) {
    json data = {{"name", "John"}, {"email", "john@example.com"}};
    std::vector<std::string> required = {"name", "email"};

    auto result = ConfigValidator::validateRequired(data, required);
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateRequiredMissing) {
    json data = {{"name", "John"}};
    std::vector<std::string> required = {"name", "email"};

    auto result = ConfigValidator::validateRequired(data, required);
    EXPECT_FALSE(result.isValid);
    EXPECT_TRUE(result.hasErrors());
}

TEST_F(ConfigValidatorTest, ValidateRangeValid) {
    json value = 50;
    auto result = ConfigValidator::validateRange(value, 0, 100);
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateRangeBelowMin) {
    json value = -10;
    auto result = ConfigValidator::validateRange(value, 0, 100);
    EXPECT_FALSE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateRangeAboveMax) {
    json value = 150;
    auto result = ConfigValidator::validateRange(value, 0, 100);
    EXPECT_FALSE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateRangeMinOnly) {
    json value = 50;
    auto result = ConfigValidator::validateRange(value, 0, std::nullopt);
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateRangeMaxOnly) {
    json value = 50;
    auto result = ConfigValidator::validateRange(value, std::nullopt, 100);
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidatePatternValid) {
    json value = "test@example.com";
    auto result = ConfigValidator::validatePattern(value, "^[a-z]+@[a-z]+\\.[a-z]+$");
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidatePatternInvalid) {
    json value = "invalid-email";
    auto result = ConfigValidator::validatePattern(value, "^[a-z]+@[a-z]+\\.[a-z]+$");
    EXPECT_FALSE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateLengthValid) {
    json value = "hello";
    auto result = ConfigValidator::validateLength(value, 1, 10);
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateLengthTooShort) {
    json value = "hi";
    auto result = ConfigValidator::validateLength(value, 5, 10);
    EXPECT_FALSE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateLengthTooLong) {
    json value = "this is a very long string";
    auto result = ConfigValidator::validateLength(value, 1, 10);
    EXPECT_FALSE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateArraySizeValid) {
    json value = {1, 2, 3, 4, 5};
    auto result = ConfigValidator::validateArraySize(value, 1, 10);
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateArraySizeTooFew) {
    json value = {1};
    auto result = ConfigValidator::validateArraySize(value, 3, 10);
    EXPECT_FALSE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateArraySizeTooMany) {
    json value = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto result = ConfigValidator::validateArraySize(value, 1, 5);
    EXPECT_FALSE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateEnumValid) {
    json value = "option2";
    std::vector<json> allowed = {"option1", "option2", "option3"};
    auto result = ConfigValidator::validateEnum(value, allowed);
    EXPECT_TRUE(result.isValid);
}

TEST_F(ConfigValidatorTest, ValidateEnumInvalid) {
    json value = "invalid_option";
    std::vector<json> allowed = {"option1", "option2", "option3"};
    auto result = ConfigValidator::validateEnum(value, allowed);
    EXPECT_FALSE(result.isValid);
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(ConfigValidatorTest, GetConfig) {
    auto config = validator_->getConfig();
    EXPECT_FALSE(config.strictMode);
    EXPECT_TRUE(config.allowAdditionalProperties);
}

TEST_F(ConfigValidatorTest, SetConfig) {
    ValidatorConfig newConfig;
    newConfig.strictMode = true;
    newConfig.coerceTypes = true;

    validator_->setConfig(newConfig);
    auto config = validator_->getConfig();
    EXPECT_TRUE(config.strictMode);
    EXPECT_TRUE(config.coerceTypes);
}

// ============================================================================
// ValidateWithOptions Tests
// ============================================================================

TEST_F(ConfigValidatorTest, ValidateWithOptions) {
    json schema = {{"type", "object"}};
    validator_->setSchema(schema);

    ValidatorConfig options;
    options.strictMode = true;

    json data = {{"key", "value"}};
    auto result = validator_->validateWithOptions(data, options);
    EXPECT_TRUE(result.isValid);
}

// ============================================================================
// Batch Validation Tests
// ============================================================================

TEST_F(ConfigValidatorTest, ValidateBatch) {
    json schema = {
        {"type", "object"},
        {"properties", {{"value", {{"type", "integer"}}}}}
    };
    validator_->setSchema(schema);

    std::vector<json> dataList = {
        {{"value", 1}},
        {{"value", 2}},
        {{"value", "invalid"}}
    };

    auto results = validator_->validateBatch(dataList);
    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results[0].isValid);
    EXPECT_TRUE(results[1].isValid);
    EXPECT_FALSE(results[2].isValid);
}

// ============================================================================
// Hook Tests
// ============================================================================

TEST_F(ConfigValidatorTest, AddHook) {
    bool hookCalled = false;
    ConfigValidator::ValidationEvent receivedEvent;

    size_t hookId = validator_->addHook(
        [&](ConfigValidator::ValidationEvent event, std::string_view path,
            const ValidationResult& result) {
            hookCalled = true;
            receivedEvent = event;
        });

    json schema = {{"type", "object"}};
    validator_->setSchema(schema);
    validator_->validate(json::object());

    EXPECT_TRUE(hookCalled);
    EXPECT_TRUE(validator_->removeHook(hookId));
}

TEST_F(ConfigValidatorTest, HookOnSchemaLoaded) {
    ConfigValidator::ValidationEvent receivedEvent;

    size_t hookId = validator_->addHook(
        [&](ConfigValidator::ValidationEvent event, std::string_view path,
            const ValidationResult& result) {
            receivedEvent = event;
        });

    validator_->setSchema(json{{"type", "object"}});
    EXPECT_EQ(receivedEvent, ConfigValidator::ValidationEvent::SCHEMA_LOADED);
    validator_->removeHook(hookId);
}

TEST_F(ConfigValidatorTest, RemoveHook) {
    size_t hookId = validator_->addHook(
        [](ConfigValidator::ValidationEvent, std::string_view, const ValidationResult&) {});
    EXPECT_TRUE(validator_->removeHook(hookId));
    EXPECT_FALSE(validator_->removeHook(hookId));
}

TEST_F(ConfigValidatorTest, ClearHooks) {
    validator_->addHook(
        [](ConfigValidator::ValidationEvent, std::string_view, const ValidationResult&) {});
    validator_->addHook(
        [](ConfigValidator::ValidationEvent, std::string_view, const ValidationResult&) {});

    validator_->clearHooks();

    bool hookCalled = false;
    validator_->addHook(
        [&](ConfigValidator::ValidationEvent, std::string_view, const ValidationResult&) {
            hookCalled = true;
        });
    validator_->clearHooks();
    validator_->validate(json::object());
    EXPECT_FALSE(hookCalled);
}

// ============================================================================
// ValidatorConfig Tests
// ============================================================================

TEST_F(ConfigValidatorTest, ValidatorConfigDefaults) {
    ValidatorConfig config;
    EXPECT_FALSE(config.strictMode);
    EXPECT_TRUE(config.allowAdditionalProperties);
    EXPECT_TRUE(config.validateFormats);
    EXPECT_FALSE(config.coerceTypes);
}

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(ConfigValidatorTest, BackwardCompatibilityAliases) {
    // Test that lithium namespace aliases work
    lithium::ConfigValidator validator;
    EXPECT_FALSE(validator.hasSchema());

    lithium::ValidationResult result;
    EXPECT_TRUE(result.isValid);
}

}  // namespace lithium::config::test
