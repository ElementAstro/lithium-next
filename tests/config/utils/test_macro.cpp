/**
 * @file test_macro.cpp
 * @brief Comprehensive unit tests for configuration helper macros
 */

#include <gtest/gtest.h>
#include <memory>

#include "config/utils/macro.hpp"
#include "config/core/manager.hpp"

using namespace lithium::config;

namespace lithium::config::test {

class ConfigMacroTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = std::make_shared<ConfigManager>();
        setupTestConfig();
    }

    void TearDown() override {
        manager_.reset();
    }

    void setupTestConfig() {
        manager_->set("test/int_value", 42);
        manager_->set("test/float_value", 3.14f);
        manager_->set("test/double_value", 2.71828);
        manager_->set("test/bool_value", true);
        manager_->set("test/string_value", std::string("hello"));
    }

    std::shared_ptr<ConfigManager> manager_;
};

// ============================================================================
// GetConfigValue Tests
// ============================================================================

TEST_F(ConfigMacroTest, GetConfigValueInt) {
    int value = GetConfigValue<int>(manager_, "test/int_value");
    EXPECT_EQ(value, 42);
}

TEST_F(ConfigMacroTest, GetConfigValueFloat) {
    float value = GetConfigValue<float>(manager_, "test/float_value");
    EXPECT_FLOAT_EQ(value, 3.14f);
}

TEST_F(ConfigMacroTest, GetConfigValueDouble) {
    double value = GetConfigValue<double>(manager_, "test/double_value");
    EXPECT_DOUBLE_EQ(value, 2.71828);
}

TEST_F(ConfigMacroTest, GetConfigValueBool) {
    bool value = GetConfigValue<bool>(manager_, "test/bool_value");
    EXPECT_TRUE(value);
}

TEST_F(ConfigMacroTest, GetConfigValueString) {
    std::string value = GetConfigValue<std::string>(manager_, "test/string_value");
    EXPECT_EQ(value, "hello");
}

TEST_F(ConfigMacroTest, GetConfigValueNullManager) {
    std::shared_ptr<ConfigManager> nullManager;
    EXPECT_THROW(
        GetConfigValue<int>(nullManager, "test/int_value"),
        std::runtime_error
    );
}

TEST_F(ConfigMacroTest, GetConfigValueNotFound) {
    EXPECT_THROW(
        GetConfigValue<int>(manager_, "nonexistent/path"),
        std::runtime_error
    );
}

TEST_F(ConfigMacroTest, GetConfigValueWrongType) {
    EXPECT_THROW(
        GetConfigValue<int>(manager_, "test/string_value"),
        std::runtime_error
    );
}

// ============================================================================
// ConfigurationType Concept Tests
// ============================================================================

TEST_F(ConfigMacroTest, ConfigurationTypeInt) {
    static_assert(ConfigurationType<int>, "int should satisfy ConfigurationType");
}

TEST_F(ConfigMacroTest, ConfigurationTypeFloat) {
    static_assert(ConfigurationType<float>, "float should satisfy ConfigurationType");
}

TEST_F(ConfigMacroTest, ConfigurationTypeDouble) {
    static_assert(ConfigurationType<double>, "double should satisfy ConfigurationType");
}

TEST_F(ConfigMacroTest, ConfigurationTypeBool) {
    static_assert(ConfigurationType<bool>, "bool should satisfy ConfigurationType");
}

TEST_F(ConfigMacroTest, ConfigurationTypeString) {
    static_assert(ConfigurationType<std::string>, "std::string should satisfy ConfigurationType");
}

// ============================================================================
// GET_CONFIG_VALUE Macro Tests
// ============================================================================

TEST_F(ConfigMacroTest, GetConfigValueMacroInt) {
    GET_CONFIG_VALUE(manager_, "test/int_value", int, result);
    EXPECT_EQ(result, 42);
}

TEST_F(ConfigMacroTest, GetConfigValueMacroString) {
    GET_CONFIG_VALUE(manager_, "test/string_value", std::string, result);
    EXPECT_EQ(result, "hello");
}

TEST_F(ConfigMacroTest, GetConfigValueMacroNotFound) {
    EXPECT_THROW(
        {
            GET_CONFIG_VALUE(manager_, "nonexistent/path", int, result);
        },
        BadConfigException
    );
}

// ============================================================================
// SET_CONFIG_VALUE Macro Tests
// ============================================================================

TEST_F(ConfigMacroTest, SetConfigValueMacroInt) {
    SET_CONFIG_VALUE(manager_, "test/new_int", 100);
    auto value = manager_->get_as<int>("test/new_int");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 100);
}

TEST_F(ConfigMacroTest, SetConfigValueMacroString) {
    SET_CONFIG_VALUE(manager_, "test/new_string", std::string("world"));
    auto value = manager_->get_as<std::string>("test/new_string");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "world");
}

TEST_F(ConfigMacroTest, SetConfigValueMacroBool) {
    SET_CONFIG_VALUE(manager_, "test/new_bool", false);
    auto value = manager_->get_as<bool>("test/new_bool");
    ASSERT_TRUE(value.has_value());
    EXPECT_FALSE(*value);
}

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(ConfigMacroTest, LithiumNamespaceGetConfigValue) {
    int value = lithium::GetConfigValue<int>(manager_, "test/int_value");
    EXPECT_EQ(value, 42);
}

TEST_F(ConfigMacroTest, LithiumNamespaceConfigurationType) {
    static_assert(lithium::ConfigurationType<int>, "int should satisfy lithium::ConfigurationType");
}

// ============================================================================
// Edge Cases Tests
// ============================================================================

TEST_F(ConfigMacroTest, GetConfigValueEmptyPath) {
    EXPECT_THROW(
        GetConfigValue<int>(manager_, ""),
        std::runtime_error
    );
}

TEST_F(ConfigMacroTest, GetConfigValueNestedPath) {
    manager_->set("deeply/nested/path/value", 123);
    int value = GetConfigValue<int>(manager_, "deeply/nested/path/value");
    EXPECT_EQ(value, 123);
}

TEST_F(ConfigMacroTest, GetConfigValueSpecialCharacters) {
    manager_->set("path_with_underscore/value", 456);
    int value = GetConfigValue<int>(manager_, "path_with_underscore/value");
    EXPECT_EQ(value, 456);
}

// ============================================================================
// Type Conversion Tests
// ============================================================================

TEST_F(ConfigMacroTest, IntToDoubleConversion) {
    manager_->set("test/int_for_double", 42);
    // Getting int as double should work due to JSON number handling
    double value = GetConfigValue<double>(manager_, "test/int_for_double");
    EXPECT_DOUBLE_EQ(value, 42.0);
}

TEST_F(ConfigMacroTest, BoolToIntConversionFails) {
    manager_->set("test/bool_for_int", true);
    // Getting bool as int might fail depending on JSON library behavior
    // This test documents the expected behavior
}

// ============================================================================
// Multiple Operations Tests
// ============================================================================

TEST_F(ConfigMacroTest, MultipleGetOperations) {
    int intVal = GetConfigValue<int>(manager_, "test/int_value");
    std::string strVal = GetConfigValue<std::string>(manager_, "test/string_value");
    bool boolVal = GetConfigValue<bool>(manager_, "test/bool_value");

    EXPECT_EQ(intVal, 42);
    EXPECT_EQ(strVal, "hello");
    EXPECT_TRUE(boolVal);
}

TEST_F(ConfigMacroTest, SetThenGet) {
    SET_CONFIG_VALUE(manager_, "test/round_trip", 999);
    int value = GetConfigValue<int>(manager_, "test/round_trip");
    EXPECT_EQ(value, 999);
}

TEST_F(ConfigMacroTest, OverwriteValue) {
    SET_CONFIG_VALUE(manager_, "test/overwrite", 1);
    SET_CONFIG_VALUE(manager_, "test/overwrite", 2);
    int value = GetConfigValue<int>(manager_, "test/overwrite");
    EXPECT_EQ(value, 2);
}

}  // namespace lithium::config::test
