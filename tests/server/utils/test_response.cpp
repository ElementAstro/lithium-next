/*
 * test_response.cpp - Tests for Response Builder Utility
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "server/utils/response.hpp"

#include <string>

using namespace lithium::server::utils;
using json = nlohmann::json;

// ============================================================================
// Success Response Tests
// ============================================================================

class ResponseBuilderSuccessTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderSuccessTest, BasicSuccess) {
    json data = {{"result", "ok"}};
    auto response = ResponseBuilder::success(data);

    EXPECT_EQ(response.code, 200);
}

TEST_F(ResponseBuilderSuccessTest, SuccessWithMessage) {
    json data = {{"count", 10}};
    auto response = ResponseBuilder::success(data, "Operation completed");

    EXPECT_EQ(response.code, 200);
}

TEST_F(ResponseBuilderSuccessTest, SuccessWithMessageMethod) {
    auto response = ResponseBuilder::successWithMessage("Task completed",
                                                        json{{"id", 123}});

    EXPECT_EQ(response.code, 200);
}

TEST_F(ResponseBuilderSuccessTest, EmptyData) {
    auto response = ResponseBuilder::success(json::object());

    EXPECT_EQ(response.code, 200);
}

// ============================================================================
// Created Response Tests
// ============================================================================

class ResponseBuilderCreatedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderCreatedTest, BasicCreated) {
    json data = {{"id", "new-resource-123"}};
    auto response = ResponseBuilder::created(data);

    EXPECT_EQ(response.code, 201);
}

TEST_F(ResponseBuilderCreatedTest, CreatedWithMessage) {
    json data = {{"id", "task-456"}};
    auto response =
        ResponseBuilder::created(data, "Resource created successfully");

    EXPECT_EQ(response.code, 201);
}

// ============================================================================
// Accepted Response Tests
// ============================================================================

class ResponseBuilderAcceptedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderAcceptedTest, BasicAccepted) {
    auto response = ResponseBuilder::accepted("Task queued for processing");

    EXPECT_EQ(response.code, 202);
}

TEST_F(ResponseBuilderAcceptedTest, AcceptedWithData) {
    json data = {{"task_id", "async-task-789"}, {"estimated_time", 30}};
    auto response = ResponseBuilder::accepted("Processing started", data);

    EXPECT_EQ(response.code, 202);
}

// ============================================================================
// No Content Response Tests
// ============================================================================

class ResponseBuilderNoContentTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderNoContentTest, BasicNoContent) {
    auto response = ResponseBuilder::noContent();

    EXPECT_EQ(response.code, 204);
}

// ============================================================================
// Error Response Tests
// ============================================================================

class ResponseBuilderErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderErrorTest, GenericError) {
    auto response =
        ResponseBuilder::error(500, "server_error", "Something went wrong");

    EXPECT_EQ(response.code, 500);
}

TEST_F(ResponseBuilderErrorTest, ErrorWithDetails) {
    json details = {{"field", "email"}, {"reason", "invalid"}};
    auto response = ResponseBuilder::error(400, "validation_error",
                                           "Validation failed", details);

    EXPECT_EQ(response.code, 400);
}

// ============================================================================
// Bad Request Tests
// ============================================================================

class ResponseBuilderBadRequestTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderBadRequestTest, BasicBadRequest) {
    auto response = ResponseBuilder::badRequest("Invalid input");

    EXPECT_EQ(response.code, 400);
}

TEST_F(ResponseBuilderBadRequestTest, BadRequestWithDetails) {
    json details = {{"missing_fields", {"name", "email"}}};
    auto response =
        ResponseBuilder::badRequest("Missing required fields", details);

    EXPECT_EQ(response.code, 400);
}

// ============================================================================
// Unauthorized Tests
// ============================================================================

class ResponseBuilderUnauthorizedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderUnauthorizedTest, DefaultUnauthorized) {
    auto response = ResponseBuilder::unauthorized();

    EXPECT_EQ(response.code, 401);
}

TEST_F(ResponseBuilderUnauthorizedTest, CustomUnauthorized) {
    auto response = ResponseBuilder::unauthorized("Invalid API key");

    EXPECT_EQ(response.code, 401);
}

// ============================================================================
// Forbidden Tests
// ============================================================================

class ResponseBuilderForbiddenTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderForbiddenTest, DefaultForbidden) {
    auto response = ResponseBuilder::forbidden();

    EXPECT_EQ(response.code, 403);
}

TEST_F(ResponseBuilderForbiddenTest, CustomForbidden) {
    auto response = ResponseBuilder::forbidden("Insufficient permissions");

    EXPECT_EQ(response.code, 403);
}

// ============================================================================
// Not Found Tests
// ============================================================================

class ResponseBuilderNotFoundTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderNotFoundTest, SimpleNotFound) {
    auto response = ResponseBuilder::notFound("Resource");

    EXPECT_EQ(response.code, 404);
}

TEST_F(ResponseBuilderNotFoundTest, NotFoundWithTypeAndName) {
    auto response = ResponseBuilder::notFound("Camera", "ZWO ASI294MC");

    EXPECT_EQ(response.code, 404);
}

TEST_F(ResponseBuilderNotFoundTest, DeviceNotFound) {
    auto response = ResponseBuilder::deviceNotFound("camera_1", "Camera");

    EXPECT_EQ(response.code, 404);
}

// ============================================================================
// Conflict Tests
// ============================================================================

class ResponseBuilderConflictTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderConflictTest, BasicConflict) {
    auto response = ResponseBuilder::conflict("Resource already exists");

    EXPECT_EQ(response.code, 409);
}

TEST_F(ResponseBuilderConflictTest, ConflictWithDetails) {
    json details = {{"existing_id", "resource-123"}};
    auto response = ResponseBuilder::conflict("Duplicate resource", details);

    EXPECT_EQ(response.code, 409);
}

// ============================================================================
// Unprocessable Entity Tests
// ============================================================================

class ResponseBuilderUnprocessableTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderUnprocessableTest, BasicUnprocessable) {
    auto response = ResponseBuilder::unprocessable("Cannot process request");

    EXPECT_EQ(response.code, 422);
}

TEST_F(ResponseBuilderUnprocessableTest, UnprocessableWithDetails) {
    json details = {{"reason", "Invalid state transition"}};
    auto response =
        ResponseBuilder::unprocessable("Operation not allowed", details);

    EXPECT_EQ(response.code, 422);
}

// ============================================================================
// Rate Limited Tests
// ============================================================================

class ResponseBuilderRateLimitedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderRateLimitedTest, DefaultRateLimited) {
    auto response = ResponseBuilder::rateLimited();

    EXPECT_EQ(response.code, 429);
}

TEST_F(ResponseBuilderRateLimitedTest, CustomRetryAfter) {
    auto response = ResponseBuilder::rateLimited(120);

    EXPECT_EQ(response.code, 429);
}

// ============================================================================
// Internal Error Tests
// ============================================================================

class ResponseBuilderInternalErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderInternalErrorTest, DefaultInternalError) {
    auto response = ResponseBuilder::internalError();

    EXPECT_EQ(response.code, 500);
}

TEST_F(ResponseBuilderInternalErrorTest, CustomInternalError) {
    auto response =
        ResponseBuilder::internalError("Database connection failed");

    EXPECT_EQ(response.code, 500);
}

// ============================================================================
// Service Unavailable Tests
// ============================================================================

class ResponseBuilderServiceUnavailableTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderServiceUnavailableTest, DefaultServiceUnavailable) {
    auto response = ResponseBuilder::serviceUnavailable();

    EXPECT_EQ(response.code, 503);
}

TEST_F(ResponseBuilderServiceUnavailableTest, CustomServiceUnavailable) {
    auto response =
        ResponseBuilder::serviceUnavailable("System under maintenance");

    EXPECT_EQ(response.code, 503);
}

// ============================================================================
// Missing Field Tests
// ============================================================================

class ResponseBuilderMissingFieldTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderMissingFieldTest, MissingField) {
    auto response = ResponseBuilder::missingField("device_id");

    EXPECT_EQ(response.code, 400);
}

TEST_F(ResponseBuilderMissingFieldTest, MultipleMissingFields) {
    // Test multiple calls
    auto response1 = ResponseBuilder::missingField("name");
    auto response2 = ResponseBuilder::missingField("email");

    EXPECT_EQ(response1.code, 400);
    EXPECT_EQ(response2.code, 400);
}

// ============================================================================
// Invalid JSON Tests
// ============================================================================

class ResponseBuilderInvalidJsonTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderInvalidJsonTest, InvalidJson) {
    auto response =
        ResponseBuilder::invalidJson("Unexpected token at position 10");

    EXPECT_EQ(response.code, 400);
}

// ============================================================================
// HTTP Status Code Coverage Tests
// ============================================================================

class ResponseBuilderStatusCodeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderStatusCodeTest, AllSuccessCodes) {
    EXPECT_EQ(ResponseBuilder::success(json{}).code, 200);
    EXPECT_EQ(ResponseBuilder::created(json{}).code, 201);
    EXPECT_EQ(ResponseBuilder::accepted("msg").code, 202);
    EXPECT_EQ(ResponseBuilder::noContent().code, 204);
}

TEST_F(ResponseBuilderStatusCodeTest, AllClientErrorCodes) {
    EXPECT_EQ(ResponseBuilder::badRequest("msg").code, 400);
    EXPECT_EQ(ResponseBuilder::unauthorized().code, 401);
    EXPECT_EQ(ResponseBuilder::forbidden().code, 403);
    EXPECT_EQ(ResponseBuilder::notFound("resource").code, 404);
    EXPECT_EQ(ResponseBuilder::conflict("msg").code, 409);
    EXPECT_EQ(ResponseBuilder::unprocessable("msg").code, 422);
    EXPECT_EQ(ResponseBuilder::rateLimited().code, 429);
}

TEST_F(ResponseBuilderStatusCodeTest, AllServerErrorCodes) {
    EXPECT_EQ(ResponseBuilder::internalError().code, 500);
    EXPECT_EQ(ResponseBuilder::serviceUnavailable().code, 503);
}

// ============================================================================
// Response Body Format Tests
// ============================================================================

class ResponseBuilderBodyFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    json parseResponseBody(const crow::response& response) {
        return json::parse(response.body);
    }
};

TEST_F(ResponseBuilderBodyFormatTest, SuccessBodyFormat) {
    auto response = ResponseBuilder::success(json{{"key", "value"}});
    auto body = parseResponseBody(response);

    EXPECT_TRUE(body.contains("success"));
    EXPECT_TRUE(body.contains("request_id"));
    EXPECT_TRUE(body.contains("data"));
    EXPECT_TRUE(body["success"].get<bool>());
}

TEST_F(ResponseBuilderBodyFormatTest, ErrorBodyFormat) {
    auto response = ResponseBuilder::badRequest("Error message");
    auto body = parseResponseBody(response);

    EXPECT_TRUE(body.contains("success"));
    EXPECT_TRUE(body.contains("request_id"));
    EXPECT_TRUE(body.contains("error"));
    EXPECT_FALSE(body["success"].get<bool>());
    EXPECT_TRUE(body["error"].contains("code"));
    EXPECT_TRUE(body["error"].contains("message"));
}

// ============================================================================
// Edge Cases
// ============================================================================

class ResponseBuilderEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseBuilderEdgeCasesTest, EmptyMessage) {
    auto response = ResponseBuilder::success(json{}, "");
    EXPECT_EQ(response.code, 200);
}

TEST_F(ResponseBuilderEdgeCasesTest, VeryLongMessage) {
    std::string longMessage(10000, 'x');
    auto response = ResponseBuilder::badRequest(longMessage);

    EXPECT_EQ(response.code, 400);
}

TEST_F(ResponseBuilderEdgeCasesTest, SpecialCharactersInMessage) {
    auto response = ResponseBuilder::badRequest("Error: \"quotes\" & <tags>");

    EXPECT_EQ(response.code, 400);
}

TEST_F(ResponseBuilderEdgeCasesTest, UnicodeMessage) {
    auto response = ResponseBuilder::badRequest("错误：无效的请求");

    EXPECT_EQ(response.code, 400);
}

TEST_F(ResponseBuilderEdgeCasesTest, LargeJsonData) {
    json largeData;
    for (int i = 0; i < 1000; ++i) {
        largeData["key_" + std::to_string(i)] = "value_" + std::to_string(i);
    }

    auto response = ResponseBuilder::success(largeData);
    EXPECT_EQ(response.code, 200);
}

TEST_F(ResponseBuilderEdgeCasesTest, NestedJsonData) {
    json nestedData;
    json* current = &nestedData;
    for (int i = 0; i < 10; ++i) {
        (*current)["level"] = i;
        (*current)["nested"] = json::object();
        current = &(*current)["nested"];
    }

    auto response = ResponseBuilder::success(nestedData);
    EXPECT_EQ(response.code, 200);
}

TEST_F(ResponseBuilderEdgeCasesTest, ArrayData) {
    json arrayData = json::array();
    for (int i = 0; i < 100; ++i) {
        arrayData.push_back({{"id", i}, {"name", "item_" + std::to_string(i)}});
    }

    auto response = ResponseBuilder::success(arrayData);
    EXPECT_EQ(response.code, 200);
}
