/**
 * @file test_types.cpp
 * @brief Unit tests for core configuration types
 */

#include <gtest/gtest.h>
#include "config/core/types.hpp"

namespace lithium::config::test {

class ConfigTypesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Version Tests
// ============================================================================

TEST_F(ConfigTypesTest, CoreVersionDefined) {
    EXPECT_NE(CORE_VERSION, nullptr);
    EXPECT_STRNE(CORE_VERSION, "");
}

TEST_F(ConfigTypesTest, CoreVersionFormat) {
    std::string version(CORE_VERSION);
    // Version should be in format X.Y.Z
    EXPECT_FALSE(version.empty());
    // Check for at least one dot (semantic versioning)
    EXPECT_NE(version.find('.'), std::string::npos);
}

// ============================================================================
// Header Aggregation Tests
// ============================================================================

TEST_F(ConfigTypesTest, IncludesException) {
    // Verify that exception types are accessible through types.hpp
    try {
        THROW_BAD_CONFIG_EXCEPTION("Test");
        FAIL() << "Expected exception";
    } catch (const BadConfigException&) {
        // Expected
    }
}

TEST_F(ConfigTypesTest, IncludesManager) {
    // Verify that ConfigManager is accessible through types.hpp
    ConfigManager manager;
    EXPECT_TRUE(manager.getKeys().empty());
}

}  // namespace lithium::config::test
