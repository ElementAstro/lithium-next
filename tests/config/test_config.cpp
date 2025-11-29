/**
 * @file test_config.cpp
 * @brief Unit tests for aggregated config.hpp header
 */

#include <gtest/gtest.h>
#include "config/config.hpp"

namespace lithium::config::test {

class ConfigHeaderTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Version Tests
// ============================================================================

TEST_F(ConfigHeaderTest, ConfigVersionDefined) {
    EXPECT_NE(CONFIG_VERSION, nullptr);
    EXPECT_STRNE(CONFIG_VERSION, "");
}

TEST_F(ConfigHeaderTest, GetConfigVersion) {
    const char* version = getConfigVersion();
    EXPECT_NE(version, nullptr);
    EXPECT_STRNE(version, "");
    EXPECT_STREQ(version, CONFIG_VERSION);
}

TEST_F(ConfigHeaderTest, VersionFormat) {
    std::string version(CONFIG_VERSION);
    // Version should be in format X.Y.Z
    EXPECT_FALSE(version.empty());
    EXPECT_NE(version.find('.'), std::string::npos);
}

// ============================================================================
// Core Module Inclusion Tests
// ============================================================================

TEST_F(ConfigHeaderTest, CoreTypesIncluded) {
    // Verify CORE_VERSION is accessible
    EXPECT_NE(CORE_VERSION, nullptr);
}

TEST_F(ConfigHeaderTest, ConfigManagerAccessible) {
    ConfigManager manager;
    EXPECT_TRUE(manager.getKeys().empty());
}

TEST_F(ConfigHeaderTest, ExceptionsAccessible) {
    try {
        THROW_BAD_CONFIG_EXCEPTION("Test");
        FAIL() << "Expected exception";
    } catch (const BadConfigException&) {
        // Expected
    }
}

// ============================================================================
// Components Module Inclusion Tests
// ============================================================================

TEST_F(ConfigHeaderTest, ComponentsVersionDefined) {
    EXPECT_NE(COMPONENTS_VERSION, nullptr);
}

TEST_F(ConfigHeaderTest, ConfigCacheAccessible) {
    ConfigCache cache;
    EXPECT_TRUE(cache.empty());
}

TEST_F(ConfigHeaderTest, ConfigValidatorAccessible) {
    ConfigValidator validator;
    EXPECT_FALSE(validator.hasSchema());
}

TEST_F(ConfigHeaderTest, ConfigSerializerAccessible) {
    ConfigSerializer serializer;
    auto config = serializer.getConfig();
    EXPECT_TRUE(config.enableMetrics);
}

TEST_F(ConfigHeaderTest, ConfigWatcherAccessible) {
    ConfigWatcher watcher;
    EXPECT_FALSE(watcher.isRunning());
}

// ============================================================================
// Utils Module Inclusion Tests
// ============================================================================

TEST_F(ConfigHeaderTest, UtilsVersionDefined) {
    EXPECT_NE(UTILS_VERSION, nullptr);
}

TEST_F(ConfigHeaderTest, JSON5FunctionsAccessible) {
    auto result = lithium::internal::removeComments("{}");
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(ConfigHeaderTest, LithiumNamespaceConfigManager) {
    lithium::ConfigManager manager;
    EXPECT_TRUE(manager.getKeys().empty());
}

TEST_F(ConfigHeaderTest, LithiumNamespaceConfigCache) {
    lithium::ConfigCache cache;
    EXPECT_TRUE(cache.empty());
}

TEST_F(ConfigHeaderTest, LithiumNamespaceConfigValidator) {
    lithium::ConfigValidator validator;
    EXPECT_FALSE(validator.hasSchema());
}

TEST_F(ConfigHeaderTest, LithiumNamespaceConfigSerializer) {
    lithium::ConfigSerializer serializer;
    auto metrics = serializer.getMetrics();
    EXPECT_EQ(metrics.totalSerializations, 0);
}

TEST_F(ConfigHeaderTest, LithiumNamespaceSerializationFormat) {
    lithium::SerializationFormat format = lithium::SerializationFormat::JSON;
    EXPECT_EQ(format, SerializationFormat::JSON);
}

TEST_F(ConfigHeaderTest, LithiumNamespaceValidationResult) {
    lithium::ValidationResult result;
    EXPECT_TRUE(result.isValid);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ConfigHeaderTest, FullWorkflow) {
    // Create manager
    auto manager = ConfigManager::createShared();
    ASSERT_NE(manager, nullptr);

    // Set values
    manager->set("app/name", "TestApp");
    manager->set("app/version", "1.0.0");
    manager->set("app/debug", true);

    // Get values
    auto name = manager->get_as<std::string>("app/name");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(*name, "TestApp");

    // Validate
    auto result = manager->validateAll();
    EXPECT_TRUE(result.isValid);

    // Export
    std::string exported = manager->exportAs(SerializationFormat::JSON);
    EXPECT_FALSE(exported.empty());
}

TEST_F(ConfigHeaderTest, ComponentsInteraction) {
    ConfigManager manager;

    // Use cache
    const auto& cache = manager.getCache();
    auto stats = cache.getStatistics();
    EXPECT_GE(stats.hits.load(), 0);

    // Use validator
    const auto& validator = manager.getValidator();
    EXPECT_FALSE(validator.hasSchema());

    // Use serializer
    const auto& serializer = manager.getSerializer();
    auto metrics = serializer.getMetrics();
    EXPECT_GE(metrics.totalSerializations, 0);

    // Use watcher
    const auto& watcher = manager.getWatcher();
    EXPECT_FALSE(watcher.isRunning());
}

}  // namespace lithium::config::test
