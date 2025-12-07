/*
 * test_api.cpp - Tests for API Models
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "server/models/api.hpp"

#include <regex>
#include <set>
#include <thread>
#include <vector>

using namespace lithium::models::api;
using json = nlohmann::json;

// ============================================================================
// generateRequestId Tests
// ============================================================================

class GenerateRequestIdTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GenerateRequestIdTest, ReturnsNonEmptyString) {
    auto id = generateRequestId();
    EXPECT_FALSE(id.empty());
}

TEST_F(GenerateRequestIdTest, ContainsHyphen) {
    auto id = generateRequestId();
    EXPECT_NE(id.find('-'), std::string::npos);
}

TEST_F(GenerateRequestIdTest, UniqueIds) {
    std::set<std::string> ids;
    for (int i = 0; i < 1000; ++i) {
        ids.insert(generateRequestId());
    }
    EXPECT_EQ(ids.size(), 1000);
}

TEST_F(GenerateRequestIdTest, FormatValidation) {
    auto id = generateRequestId();

    // Format should be {timestamp_hex}-{counter_hex}
    std::regex pattern("^[0-9a-f]+-[0-9a-f]{4}$");
    EXPECT_TRUE(std::regex_match(id, pattern));
}

TEST_F(GenerateRequestIdTest, ConcurrentGeneration) {
    std::set<std::string> ids;
    std::mutex mutex;
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&ids, &mutex]() {
            for (int j = 0; j < 100; ++j) {
                auto id = generateRequestId();
                std::lock_guard<std::mutex> lock(mutex);
                ids.insert(id);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All IDs should be unique
    EXPECT_EQ(ids.size(), 1000);
}

TEST_F(GenerateRequestIdTest, CounterWraparound) {
    // Generate many IDs to test counter behavior
    for (int i = 0; i < 100000; ++i) {
        auto id = generateRequestId();
        EXPECT_FALSE(id.empty());
    }
}

// ============================================================================
// makeSuccess Tests
// ============================================================================

class MakeSuccessTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MakeSuccessTest, BasicSuccess) {
    json data = {{"key", "value"}};
    std::string requestId = "test-request-id";

    auto result = makeSuccess(data, requestId);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["request_id"], requestId);
    EXPECT_EQ(result["data"]["key"], "value");
}

TEST_F(MakeSuccessTest, SuccessWithMessage) {
    json data = {{"count", 42}};
    std::string requestId = "req-123";
    std::string message = "Operation completed successfully";

    auto result = makeSuccess(data, requestId, message);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["message"], message);
}

TEST_F(MakeSuccessTest, SuccessWithoutMessage) {
    json data = {{"status", "ok"}};
    std::string requestId = "req-456";

    auto result = makeSuccess(data, requestId);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(result.contains("message"));
}

TEST_F(MakeSuccessTest, EmptyData) {
    json data = json::object();
    std::string requestId = "req-789";

    auto result = makeSuccess(data, requestId);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_TRUE(result["data"].empty());
}

TEST_F(MakeSuccessTest, ComplexData) {
    json data = {{"nested", {{"level1", {{"level2", "deep"}}}}},
                 {"array", {1, 2, 3, 4, 5}},
                 {"number", 3.14159},
                 {"boolean", true},
                 {"null_value", nullptr}};
    std::string requestId = "complex-req";

    auto result = makeSuccess(data, requestId);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["data"]["nested"]["level1"]["level2"], "deep");
    EXPECT_EQ(result["data"]["array"].size(), 5);
}

// ============================================================================
// makeAccepted Tests
// ============================================================================

class MakeAcceptedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MakeAcceptedTest, BasicAccepted) {
    json data = {{"task_id", "task-123"}};
    std::string requestId = "req-accepted";
    std::string message = "Task queued for processing";

    auto result = makeAccepted(data, requestId, message);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_EQ(result["request_id"], requestId);
    EXPECT_EQ(result["data"]["task_id"], "task-123");
}

TEST_F(MakeAcceptedTest, AcceptedWithoutMessage) {
    json data = {{"job_id", "job-456"}};
    std::string requestId = "req-job";

    auto result = makeAccepted(data, requestId);

    EXPECT_TRUE(result["success"].get<bool>());
    EXPECT_FALSE(result.contains("message"));
}

// ============================================================================
// makeError Tests
// ============================================================================

class MakeErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MakeErrorTest, BasicError) {
    std::string code = "invalid_request";
    std::string message = "The request was invalid";
    std::string requestId = "err-req-1";

    auto result = makeError(code, message, requestId);

    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["request_id"], requestId);
    EXPECT_EQ(result["error"]["code"], code);
    EXPECT_EQ(result["error"]["message"], message);
}

TEST_F(MakeErrorTest, ErrorWithDetails) {
    std::string code = "validation_error";
    std::string message = "Validation failed";
    std::string requestId = "err-req-2";
    json details = {{"field", "email"}, {"reason", "invalid format"}};

    auto result = makeError(code, message, requestId, details);

    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["error"]["details"]["field"], "email");
    EXPECT_EQ(result["error"]["details"]["reason"], "invalid format");
}

TEST_F(MakeErrorTest, ErrorWithoutDetails) {
    std::string code = "not_found";
    std::string message = "Resource not found";
    std::string requestId = "err-req-3";

    auto result = makeError(code, message, requestId);

    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_FALSE(result["error"].contains("details"));
}

TEST_F(MakeErrorTest, ErrorWithEmptyDetails) {
    std::string code = "server_error";
    std::string message = "Internal server error";
    std::string requestId = "err-req-4";
    json details = json::object();

    auto result = makeError(code, message, requestId, details);

    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_FALSE(result["error"].contains("details"));
}

TEST_F(MakeErrorTest, CommonErrorCodes) {
    std::vector<std::string> errorCodes = {
        "bad_request",      "unauthorized",   "forbidden",
        "not_found",        "conflict",       "unprocessable_entity",
        "rate_limited",     "internal_error", "service_unavailable",
        "device_not_found", "invalid_json",   "missing_field"};

    for (const auto& code : errorCodes) {
        auto result = makeError(code, "Test message", "test-req");
        EXPECT_EQ(result["error"]["code"], code);
    }
}

// ============================================================================
// makeDeviceNotFound Tests
// ============================================================================

class MakeDeviceNotFoundTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MakeDeviceNotFoundTest, CameraNotFound) {
    std::string deviceId = "camera_1";
    std::string deviceKind = "Camera";
    std::string requestId = "dev-req-1";

    auto result = makeDeviceNotFound(deviceId, deviceKind, requestId);

    EXPECT_FALSE(result["success"].get<bool>());
    EXPECT_EQ(result["error"]["code"], "device_not_found");
    EXPECT_EQ(result["error"]["details"]["deviceId"], deviceId);
    EXPECT_EQ(result["error"]["details"]["deviceType"], deviceKind);
    EXPECT_NE(result["error"]["message"].get<std::string>().find("Camera"),
              std::string::npos);
}

TEST_F(MakeDeviceNotFoundTest, MountNotFound) {
    std::string deviceId = "mount_eq6";
    std::string deviceKind = "Mount";
    std::string requestId = "dev-req-2";

    auto result = makeDeviceNotFound(deviceId, deviceKind, requestId);

    EXPECT_EQ(result["error"]["details"]["deviceId"], deviceId);
    EXPECT_EQ(result["error"]["details"]["deviceType"], deviceKind);
}

TEST_F(MakeDeviceNotFoundTest, FocuserNotFound) {
    std::string deviceId = "focuser_zwo";
    std::string deviceKind = "Focuser";
    std::string requestId = "dev-req-3";

    auto result = makeDeviceNotFound(deviceId, deviceKind, requestId);

    EXPECT_EQ(result["error"]["details"]["deviceId"], deviceId);
    EXPECT_EQ(result["error"]["details"]["deviceType"], deviceKind);
}

TEST_F(MakeDeviceNotFoundTest, FilterWheelNotFound) {
    std::string deviceId = "fw_manual";
    std::string deviceKind = "FilterWheel";
    std::string requestId = "dev-req-4";

    auto result = makeDeviceNotFound(deviceId, deviceKind, requestId);

    EXPECT_EQ(result["error"]["details"]["deviceId"], deviceId);
    EXPECT_EQ(result["error"]["details"]["deviceType"], deviceKind);
}

// ============================================================================
// Response Structure Tests
// ============================================================================

class ResponseStructureTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ResponseStructureTest, SuccessResponseHasRequiredFields) {
    auto result = makeSuccess(json{{"data", "test"}}, "req-id");

    EXPECT_TRUE(result.contains("success"));
    EXPECT_TRUE(result.contains("request_id"));
    EXPECT_TRUE(result.contains("data"));
}

TEST_F(ResponseStructureTest, ErrorResponseHasRequiredFields) {
    auto result = makeError("error_code", "Error message", "req-id");

    EXPECT_TRUE(result.contains("success"));
    EXPECT_TRUE(result.contains("request_id"));
    EXPECT_TRUE(result.contains("error"));
    EXPECT_TRUE(result["error"].contains("code"));
    EXPECT_TRUE(result["error"].contains("message"));
}

TEST_F(ResponseStructureTest, SuccessIsBoolean) {
    auto success = makeSuccess(json{}, "req-1");
    auto error = makeError("code", "msg", "req-2");

    EXPECT_TRUE(success["success"].is_boolean());
    EXPECT_TRUE(error["success"].is_boolean());
}

TEST_F(ResponseStructureTest, RequestIdIsString) {
    auto result = makeSuccess(json{}, "test-request-id");

    EXPECT_TRUE(result["request_id"].is_string());
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

class JsonSerializationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(JsonSerializationTest, SuccessResponseSerializes) {
    auto result = makeSuccess(json{{"key", "value"}}, "req-id", "message");

    std::string serialized = result.dump();
    EXPECT_FALSE(serialized.empty());

    auto parsed = json::parse(serialized);
    EXPECT_EQ(parsed["data"]["key"], "value");
}

TEST_F(JsonSerializationTest, ErrorResponseSerializes) {
    auto result =
        makeError("code", "message", "req-id", json{{"detail", "info"}});

    std::string serialized = result.dump();
    EXPECT_FALSE(serialized.empty());

    auto parsed = json::parse(serialized);
    EXPECT_EQ(parsed["error"]["code"], "code");
}

TEST_F(JsonSerializationTest, PrettyPrint) {
    auto result = makeSuccess(json{{"nested", {{"key", "value"}}}}, "req-id");

    std::string pretty = result.dump(2);
    EXPECT_NE(pretty.find('\n'), std::string::npos);
}

// ============================================================================
// Edge Cases
// ============================================================================

class ApiModelsEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ApiModelsEdgeCasesTest, EmptyRequestId) {
    auto result = makeSuccess(json{}, "");
    EXPECT_EQ(result["request_id"], "");
}

TEST_F(ApiModelsEdgeCasesTest, VeryLongMessage) {
    std::string longMessage(10000, 'x');
    auto result = makeSuccess(json{}, "req-id", longMessage);

    EXPECT_EQ(result["message"].get<std::string>().length(), 10000);
}

TEST_F(ApiModelsEdgeCasesTest, SpecialCharactersInMessage) {
    std::string message =
        "Error: \"quotes\" and 'apostrophes' and \\ backslash";
    auto result = makeError("code", message, "req-id");

    EXPECT_EQ(result["error"]["message"], message);
}

TEST_F(ApiModelsEdgeCasesTest, UnicodeInData) {
    json data = {{"message", "こんにちは世界"},
                 {"emoji", "🔭🌟"},
                 {"chinese", "天文摄影"}};

    auto result = makeSuccess(data, "req-id");

    EXPECT_EQ(result["data"]["message"], "こんにちは世界");
}

TEST_F(ApiModelsEdgeCasesTest, NullValuesInData) {
    json data = {{"null_field", nullptr}, {"valid_field", "value"}};

    auto result = makeSuccess(data, "req-id");

    EXPECT_TRUE(result["data"]["null_field"].is_null());
    EXPECT_EQ(result["data"]["valid_field"], "value");
}

TEST_F(ApiModelsEdgeCasesTest, ArrayData) {
    json data = json::array({1, 2, 3, "four", 5.0});

    auto result = makeSuccess(data, "req-id");

    EXPECT_TRUE(result["data"].is_array());
    EXPECT_EQ(result["data"].size(), 5);
}
