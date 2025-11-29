/**
 * @file test_exception.cpp
 * @brief Unit tests for Configuration Exception Types
 */

#include <gtest/gtest.h>
#include "config/core/exception.hpp"

namespace lithium::config::test {

class ConfigExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// BadConfigException Tests
// ============================================================================

TEST_F(ConfigExceptionTest, BadConfigExceptionConstruction) {
    try {
        THROW_BAD_CONFIG_EXCEPTION("Test bad config error");
        FAIL() << "Expected BadConfigException to be thrown";
    } catch (const BadConfigException& e) {
        EXPECT_NE(std::string(e.what()).find("Test bad config error"),
                  std::string::npos);
    } catch (...) {
        FAIL() << "Expected BadConfigException, got different exception";
    }
}

TEST_F(ConfigExceptionTest, BadConfigExceptionInheritance) {
    try {
        THROW_BAD_CONFIG_EXCEPTION("Inheritance test");
        FAIL() << "Expected exception to be thrown";
    } catch (const atom::error::Exception& e) {
        // Should be catchable as base Exception
        EXPECT_NE(std::string(e.what()).find("Inheritance test"),
                  std::string::npos);
    }
}

// ============================================================================
// InvalidConfigException Tests
// ============================================================================

TEST_F(ConfigExceptionTest, InvalidConfigExceptionConstruction) {
    try {
        THROW_INVALID_CONFIG_EXCEPTION("Invalid value provided");
        FAIL() << "Expected InvalidConfigException to be thrown";
    } catch (const InvalidConfigException& e) {
        EXPECT_NE(std::string(e.what()).find("Invalid value provided"),
                  std::string::npos);
    } catch (...) {
        FAIL() << "Expected InvalidConfigException, got different exception";
    }
}

TEST_F(ConfigExceptionTest, InvalidConfigExceptionInheritance) {
    try {
        THROW_INVALID_CONFIG_EXCEPTION("Test inheritance");
        FAIL() << "Expected exception to be thrown";
    } catch (const BadConfigException& e) {
        // Should be catchable as BadConfigException (parent)
        EXPECT_NE(std::string(e.what()).find("Test inheritance"),
                  std::string::npos);
    }
}

// ============================================================================
// ConfigNotFoundException Tests
// ============================================================================

TEST_F(ConfigExceptionTest, ConfigNotFoundExceptionConstruction) {
    try {
        THROW_CONFIG_NOT_FOUND_EXCEPTION("Config key not found: test/path");
        FAIL() << "Expected ConfigNotFoundException to be thrown";
    } catch (const ConfigNotFoundException& e) {
        EXPECT_NE(std::string(e.what()).find("Config key not found"),
                  std::string::npos);
    } catch (...) {
        FAIL() << "Expected ConfigNotFoundException, got different exception";
    }
}

TEST_F(ConfigExceptionTest, ConfigNotFoundExceptionInheritance) {
    try {
        THROW_CONFIG_NOT_FOUND_EXCEPTION("Not found test");
        FAIL() << "Expected exception to be thrown";
    } catch (const BadConfigException& e) {
        // Should be catchable as BadConfigException (parent)
        EXPECT_NE(std::string(e.what()).find("Not found test"),
                  std::string::npos);
    }
}

// ============================================================================
// ConfigIOException Tests
// ============================================================================

TEST_F(ConfigExceptionTest, ConfigIOExceptionConstruction) {
    try {
        THROW_CONFIG_IO_EXCEPTION("Failed to read config file");
        FAIL() << "Expected ConfigIOException to be thrown";
    } catch (const ConfigIOException& e) {
        EXPECT_NE(std::string(e.what()).find("Failed to read config file"),
                  std::string::npos);
    } catch (...) {
        FAIL() << "Expected ConfigIOException, got different exception";
    }
}

TEST_F(ConfigExceptionTest, ConfigIOExceptionInheritance) {
    try {
        THROW_CONFIG_IO_EXCEPTION("IO test");
        FAIL() << "Expected exception to be thrown";
    } catch (const BadConfigException& e) {
        // Should be catchable as BadConfigException (parent)
        EXPECT_NE(std::string(e.what()).find("IO test"), std::string::npos);
    }
}

// ============================================================================
// Backward Compatibility Tests
// ============================================================================

TEST_F(ConfigExceptionTest, BackwardCompatibilityAliases) {
    // Test that global namespace aliases work
    try {
        throw ::BadConfigException("", 0, "", "Global alias test");
        FAIL() << "Expected exception to be thrown";
    } catch (const ::BadConfigException& e) {
        EXPECT_NE(std::string(e.what()).find("Global alias test"),
                  std::string::npos);
    }

    try {
        throw ::InvalidConfigException("", 0, "", "Invalid global alias");
        FAIL() << "Expected exception to be thrown";
    } catch (const ::InvalidConfigException& e) {
        EXPECT_NE(std::string(e.what()).find("Invalid global alias"),
                  std::string::npos);
    }
}

}  // namespace lithium::config::test
