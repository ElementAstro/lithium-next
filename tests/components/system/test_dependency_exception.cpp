/**
 * @file test_dependency_exception.cpp
 * @brief Unit tests for dependency exception types
 *
 * Tests DependencyException, DependencyError, and DependencyResult types
 */

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>

#include "components/system/dependency_exception.hpp"

using namespace lithium::system;

// ============================================================================
// DependencyErrorCode Tests
// ============================================================================

class DependencyErrorCodeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DependencyErrorCodeTest, ErrorCodeValues) {
    EXPECT_EQ(static_cast<uint32_t>(DependencyErrorCode::SUCCESS), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::PACKAGE_MANAGER_NOT_FOUND), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::INSTALL_FAILED), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::UNINSTALL_FAILED), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::DEPENDENCY_NOT_FOUND), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::CONFIG_LOAD_FAILED), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::INVALID_VERSION), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::NETWORK_ERROR), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::PERMISSION_DENIED), 0);
    EXPECT_NE(static_cast<uint32_t>(DependencyErrorCode::UNKNOWN_ERROR), 0);
}

TEST_F(DependencyErrorCodeTest, ErrorCodesAreDistinct) {
    std::set<uint32_t> codes;
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::SUCCESS));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::PACKAGE_MANAGER_NOT_FOUND));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::INSTALL_FAILED));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::UNINSTALL_FAILED));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::DEPENDENCY_NOT_FOUND));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::CONFIG_LOAD_FAILED));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::INVALID_VERSION));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::NETWORK_ERROR));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::PERMISSION_DENIED));
    codes.insert(static_cast<uint32_t>(DependencyErrorCode::UNKNOWN_ERROR));

    // All error codes should be unique
    EXPECT_EQ(codes.size(), 10);
}

// ============================================================================
// DependencyError Tests
// ============================================================================

class DependencyErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DependencyErrorTest, ConstructWithCodeAndMessage) {
    DependencyError error(DependencyErrorCode::INSTALL_FAILED, "Installation failed");

    EXPECT_EQ(error.code(), DependencyErrorCode::INSTALL_FAILED);
    EXPECT_EQ(error.message(), "Installation failed");
}

TEST_F(DependencyErrorTest, ConstructWithContext) {
    lithium::exception::ErrorContext context;
    context.addDetail("package", "openssl");
    context.addDetail("version", "1.1.1");

    DependencyError error(DependencyErrorCode::DEPENDENCY_NOT_FOUND,
                          "Dependency not found", context);

    EXPECT_EQ(error.code(), DependencyErrorCode::DEPENDENCY_NOT_FOUND);
    EXPECT_EQ(error.message(), "Dependency not found");
}

TEST_F(DependencyErrorTest, GetCode) {
    DependencyError error(DependencyErrorCode::NETWORK_ERROR, "Network error");
    EXPECT_EQ(error.code(), DependencyErrorCode::NETWORK_ERROR);
}

TEST_F(DependencyErrorTest, GetMessage) {
    DependencyError error(DependencyErrorCode::PERMISSION_DENIED, "Access denied");
    EXPECT_EQ(error.message(), "Access denied");
}

TEST_F(DependencyErrorTest, ToJson) {
    DependencyError error(DependencyErrorCode::CONFIG_LOAD_FAILED,
                          "Failed to load config");

    auto json = error.toJson();

    EXPECT_TRUE(json.contains("code"));
    EXPECT_TRUE(json.contains("message"));
    EXPECT_TRUE(json.contains("context"));
    EXPECT_EQ(json["message"], "Failed to load config");
}

TEST_F(DependencyErrorTest, DifferentErrorCodes) {
    DependencyError e1(DependencyErrorCode::SUCCESS, "Success");
    DependencyError e2(DependencyErrorCode::INSTALL_FAILED, "Install failed");
    DependencyError e3(DependencyErrorCode::UNKNOWN_ERROR, "Unknown error");

    EXPECT_EQ(e1.code(), DependencyErrorCode::SUCCESS);
    EXPECT_EQ(e2.code(), DependencyErrorCode::INSTALL_FAILED);
    EXPECT_EQ(e3.code(), DependencyErrorCode::UNKNOWN_ERROR);
}

TEST_F(DependencyErrorTest, EmptyMessage) {
    DependencyError error(DependencyErrorCode::UNKNOWN_ERROR, "");
    EXPECT_TRUE(error.message().empty());
}

TEST_F(DependencyErrorTest, LongMessage) {
    std::string longMessage(10000, 'x');
    DependencyError error(DependencyErrorCode::UNKNOWN_ERROR, longMessage);
    EXPECT_EQ(error.message().length(), 10000);
}

// ============================================================================
// DependencyException Tests
// ============================================================================

class DependencyExceptionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DependencyExceptionTest, ConstructWithCodeAndMessage) {
    DependencyException ex(DependencyErrorCode::INSTALL_FAILED,
                           "Installation failed");

    EXPECT_EQ(ex.errorCode(), DependencyErrorCode::INSTALL_FAILED);
}

TEST_F(DependencyExceptionTest, ThrowAndCatch) {
    bool caught = false;

    try {
        throw DependencyException(DependencyErrorCode::DEPENDENCY_NOT_FOUND,
                                  "Package not found");
    } catch (const DependencyException& ex) {
        caught = true;
        EXPECT_EQ(ex.errorCode(), DependencyErrorCode::DEPENDENCY_NOT_FOUND);
    }

    EXPECT_TRUE(caught);
}

TEST_F(DependencyExceptionTest, CatchAsStdException) {
    bool caught = false;

    try {
        throw DependencyException(DependencyErrorCode::NETWORK_ERROR,
                                  "Network failure");
    } catch (const std::exception& ex) {
        caught = true;
        // what() should return something
        EXPECT_NE(std::string(ex.what()).length(), 0);
    }

    EXPECT_TRUE(caught);
}

TEST_F(DependencyExceptionTest, ConstructWithInnerException) {
    std::runtime_error inner("Inner error");

    DependencyException ex(DependencyErrorCode::UNKNOWN_ERROR,
                           "Outer error", inner);

    EXPECT_EQ(ex.errorCode(), DependencyErrorCode::UNKNOWN_ERROR);
}

TEST_F(DependencyExceptionTest, ConstructWithContext) {
    lithium::exception::ErrorContext context;
    context.addDetail("operation", "install");

    DependencyException ex(DependencyErrorCode::PERMISSION_DENIED,
                           "Permission denied", context);

    EXPECT_EQ(ex.errorCode(), DependencyErrorCode::PERMISSION_DENIED);
}

TEST_F(DependencyExceptionTest, ConstructWithTags) {
    std::vector<std::string> tags = {"critical", "network"};

    DependencyException ex(DependencyErrorCode::NETWORK_ERROR,
                           "Network error", {}, tags);

    EXPECT_EQ(ex.errorCode(), DependencyErrorCode::NETWORK_ERROR);
}

TEST_F(DependencyExceptionTest, ErrorCodeAccessor) {
    DependencyException ex(DependencyErrorCode::INVALID_VERSION,
                           "Invalid version");

    EXPECT_EQ(ex.errorCode(), DependencyErrorCode::INVALID_VERSION);
}

// ============================================================================
// DependencyResult<T> Tests
// ============================================================================

class DependencyResultTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DependencyResultTest, SuccessWithValue) {
    DependencyResult<std::string> result;
    result.value = "success_value";

    EXPECT_TRUE(result.value.has_value());
    EXPECT_EQ(*result.value, "success_value");
    EXPECT_FALSE(result.error.has_value());
}

TEST_F(DependencyResultTest, FailureWithError) {
    DependencyResult<std::string> result;
    result.error = DependencyError(DependencyErrorCode::INSTALL_FAILED,
                                   "Installation failed");

    EXPECT_FALSE(result.value.has_value());
    EXPECT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code(), DependencyErrorCode::INSTALL_FAILED);
}

TEST_F(DependencyResultTest, IntResult) {
    DependencyResult<int> result;
    result.value = 42;

    EXPECT_TRUE(result.value.has_value());
    EXPECT_EQ(*result.value, 42);
}

TEST_F(DependencyResultTest, BoolResult) {
    DependencyResult<bool> result;
    result.value = true;

    EXPECT_TRUE(result.value.has_value());
    EXPECT_TRUE(*result.value);
}

TEST_F(DependencyResultTest, VectorResult) {
    DependencyResult<std::vector<std::string>> result;
    result.value = std::vector<std::string>{"a", "b", "c"};

    EXPECT_TRUE(result.value.has_value());
    EXPECT_EQ(result.value->size(), 3);
}

// ============================================================================
// DependencyResult<void> Tests
// ============================================================================

class DependencyResultVoidTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DependencyResultVoidTest, DefaultValues) {
    DependencyResult<void> result;
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.has_value());
}

TEST_F(DependencyResultVoidTest, SuccessResult) {
    DependencyResult<void> result;
    result.success = true;

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.error.has_value());
}

TEST_F(DependencyResultVoidTest, FailureResult) {
    DependencyResult<void> result;
    result.success = false;
    result.error = DependencyError(DependencyErrorCode::UNINSTALL_FAILED,
                                   "Uninstall failed");

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code(), DependencyErrorCode::UNINSTALL_FAILED);
}

TEST_F(DependencyResultVoidTest, CheckSuccessAndError) {
    DependencyResult<void> successResult;
    successResult.success = true;

    DependencyResult<void> failureResult;
    failureResult.success = false;
    failureResult.error = DependencyError(DependencyErrorCode::UNKNOWN_ERROR,
                                          "Unknown error");

    EXPECT_TRUE(successResult.success);
    EXPECT_FALSE(successResult.error.has_value());

    EXPECT_FALSE(failureResult.success);
    EXPECT_TRUE(failureResult.error.has_value());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(DependencyErrorTest, SpecialCharactersInMessage) {
    DependencyError error(DependencyErrorCode::UNKNOWN_ERROR,
                          "Error with special chars: <>&\"'");
    EXPECT_FALSE(error.message().empty());
}

TEST_F(DependencyErrorTest, UnicodeMessage) {
    DependencyError error(DependencyErrorCode::UNKNOWN_ERROR,
                          "错误消息 with unicode: 日本語");
    EXPECT_FALSE(error.message().empty());
}

TEST_F(DependencyExceptionTest, NestedExceptions) {
    bool caught = false;

    try {
        try {
            throw std::runtime_error("Inner error");
        } catch (const std::exception& inner) {
            throw DependencyException(DependencyErrorCode::UNKNOWN_ERROR,
                                      "Outer error", inner);
        }
    } catch (const DependencyException& ex) {
        caught = true;
        EXPECT_EQ(ex.errorCode(), DependencyErrorCode::UNKNOWN_ERROR);
    }

    EXPECT_TRUE(caught);
}

TEST_F(DependencyResultTest, EmptyOptionals) {
    DependencyResult<std::string> result;

    EXPECT_FALSE(result.value.has_value());
    EXPECT_FALSE(result.error.has_value());
}

TEST_F(DependencyResultTest, BothValueAndError) {
    // This is an edge case - normally shouldn't happen but test behavior
    DependencyResult<std::string> result;
    result.value = "some value";
    result.error = DependencyError(DependencyErrorCode::UNKNOWN_ERROR, "error");

    // Both can be set (though semantically incorrect)
    EXPECT_TRUE(result.value.has_value());
    EXPECT_TRUE(result.error.has_value());
}

}  // namespace lithium::test
