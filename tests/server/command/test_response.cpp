/*
 * test_response.cpp - Tests for Command Response Builder
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "server/command/response.hpp"

#include <string>

using namespace lithium::app::command;
using json = nlohmann::json;

// ============================================================================
// Success Response Tests
// ============================================================================

class CommandResponseSuccessTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseSuccessTest, BasicSuccess) {
    auto response = CommandResponse::success();

    EXPECT_EQ(response["status"], "success");
    EXPECT_TRUE(response.contains("data"));
    EXPECT_TRUE(response["data"].empty());
}

TEST_F(CommandResponseSuccessTest, SuccessWithData) {
    json data = {{"key", "value"}, {"count", 42}};
    auto response = CommandResponse::success(data);

    EXPECT_EQ(response["status"], "success");
    EXPECT_EQ(response["data"]["key"], "value");
    EXPECT_EQ(response["data"]["count"], 42);
}

TEST_F(CommandResponseSuccessTest, SuccessWithComplexData) {
    json data = {{"nested", {{"level1", {{"level2", "deep"}}}}},
                 {"array", {1, 2, 3, 4, 5}},
                 {"boolean", true},
                 {"null_value", nullptr}};

    auto response = CommandResponse::success(data);

    EXPECT_EQ(response["status"], "success");
    EXPECT_EQ(response["data"]["nested"]["level1"]["level2"], "deep");
    EXPECT_EQ(response["data"]["array"].size(), 5);
}

TEST_F(CommandResponseSuccessTest, SuccessWithEmptyObject) {
    auto response = CommandResponse::success(json::object());

    EXPECT_EQ(response["status"], "success");
    EXPECT_TRUE(response["data"].is_object());
    EXPECT_TRUE(response["data"].empty());
}

// ============================================================================
// Error Response Tests
// ============================================================================

class CommandResponseErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseErrorTest, BasicError) {
    auto response = CommandResponse::error("error_code", "Error message");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "error_code");
    EXPECT_EQ(response["error"]["message"], "Error message");
    EXPECT_FALSE(response["error"].contains("details"));
}

TEST_F(CommandResponseErrorTest, ErrorWithDetails) {
    json details = {{"field", "email"}, {"reason", "invalid format"}};
    auto response = CommandResponse::error("validation_error",
                                           "Validation failed", details);

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "validation_error");
    EXPECT_EQ(response["error"]["details"]["field"], "email");
}

TEST_F(CommandResponseErrorTest, ErrorWithEmptyDetails) {
    auto response = CommandResponse::error("code", "message", json::object());

    EXPECT_EQ(response["status"], "error");
    EXPECT_FALSE(response["error"].contains("details"));
}

TEST_F(CommandResponseErrorTest, CommonErrorCodes) {
    std::vector<std::pair<std::string, std::string>> errors = {
        {"device_not_found", "Device not found"},
        {"missing_parameter", "Parameter missing"},
        {"invalid_parameter", "Invalid parameter"},
        {"service_unavailable", "Service unavailable"},
        {"operation_failed", "Operation failed"},
        {"timeout", "Operation timed out"},
        {"device_busy", "Device is busy"},
        {"not_connected", "Not connected"}};

    for (const auto& [code, message] : errors) {
        auto response = CommandResponse::error(code, message);
        EXPECT_EQ(response["error"]["code"], code);
    }
}

// ============================================================================
// Device Not Found Tests
// ============================================================================

class CommandResponseDeviceNotFoundTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseDeviceNotFoundTest, CameraNotFound) {
    auto response = CommandResponse::deviceNotFound("camera_1", "Camera");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "device_not_found");
    EXPECT_EQ(response["error"]["details"]["deviceId"], "camera_1");
    EXPECT_EQ(response["error"]["details"]["deviceType"], "Camera");
    EXPECT_NE(response["error"]["message"].get<std::string>().find("Camera"),
              std::string::npos);
}

TEST_F(CommandResponseDeviceNotFoundTest, MountNotFound) {
    auto response = CommandResponse::deviceNotFound("mount_eq6", "Mount");

    EXPECT_EQ(response["error"]["details"]["deviceId"], "mount_eq6");
    EXPECT_EQ(response["error"]["details"]["deviceType"], "Mount");
}

TEST_F(CommandResponseDeviceNotFoundTest, FocuserNotFound) {
    auto response = CommandResponse::deviceNotFound("focuser_zwo", "Focuser");

    EXPECT_EQ(response["error"]["details"]["deviceId"], "focuser_zwo");
    EXPECT_EQ(response["error"]["details"]["deviceType"], "Focuser");
}

TEST_F(CommandResponseDeviceNotFoundTest, FilterWheelNotFound) {
    auto response = CommandResponse::deviceNotFound("fw_manual", "FilterWheel");

    EXPECT_EQ(response["error"]["details"]["deviceId"], "fw_manual");
    EXPECT_EQ(response["error"]["details"]["deviceType"], "FilterWheel");
}

// ============================================================================
// Missing Parameter Tests
// ============================================================================

class CommandResponseMissingParameterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseMissingParameterTest, BasicMissingParameter) {
    auto response = CommandResponse::missingParameter("device_id");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "missing_parameter");
    EXPECT_EQ(response["error"]["details"]["param"], "device_id");
    EXPECT_NE(response["error"]["message"].get<std::string>().find("device_id"),
              std::string::npos);
}

TEST_F(CommandResponseMissingParameterTest, MultipleMissingParameters) {
    auto response1 = CommandResponse::missingParameter("exposure");
    auto response2 = CommandResponse::missingParameter("gain");
    auto response3 = CommandResponse::missingParameter("binning");

    EXPECT_EQ(response1["error"]["details"]["param"], "exposure");
    EXPECT_EQ(response2["error"]["details"]["param"], "gain");
    EXPECT_EQ(response3["error"]["details"]["param"], "binning");
}

// ============================================================================
// Invalid Parameter Tests
// ============================================================================

class CommandResponseInvalidParameterTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseInvalidParameterTest, BasicInvalidParameter) {
    auto response =
        CommandResponse::invalidParameter("exposure", "must be positive");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "invalid_parameter");
    EXPECT_EQ(response["error"]["details"]["param"], "exposure");
    EXPECT_EQ(response["error"]["details"]["reason"], "must be positive");
}

TEST_F(CommandResponseInvalidParameterTest, InvalidParameterTypes) {
    auto response1 =
        CommandResponse::invalidParameter("gain", "must be between 0 and 100");
    auto response2 =
        CommandResponse::invalidParameter("binning", "must be 1, 2, or 4");
    auto response3 =
        CommandResponse::invalidParameter("filter", "unknown filter name");

    EXPECT_EQ(response1["error"]["details"]["param"], "gain");
    EXPECT_EQ(response2["error"]["details"]["param"], "binning");
    EXPECT_EQ(response3["error"]["details"]["param"], "filter");
}

// ============================================================================
// Service Unavailable Tests
// ============================================================================

class CommandResponseServiceUnavailableTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseServiceUnavailableTest, BasicServiceUnavailable) {
    auto response = CommandResponse::serviceUnavailable("DeviceManager");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "service_unavailable");
    EXPECT_EQ(response["error"]["details"]["service"], "DeviceManager");
}

TEST_F(CommandResponseServiceUnavailableTest, MultipleServices) {
    auto response1 = CommandResponse::serviceUnavailable("TaskManager");
    auto response2 = CommandResponse::serviceUnavailable("ConfigManager");
    auto response3 = CommandResponse::serviceUnavailable("EventLoop");

    EXPECT_EQ(response1["error"]["details"]["service"], "TaskManager");
    EXPECT_EQ(response2["error"]["details"]["service"], "ConfigManager");
    EXPECT_EQ(response3["error"]["details"]["service"], "EventLoop");
}

// ============================================================================
// Operation Failed Tests
// ============================================================================

class CommandResponseOperationFailedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseOperationFailedTest, BasicOperationFailed) {
    auto response =
        CommandResponse::operationFailed("exposure", "sensor error");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "operation_failed");
    EXPECT_EQ(response["error"]["details"]["operation"], "exposure");
    EXPECT_EQ(response["error"]["details"]["reason"], "sensor error");
}

TEST_F(CommandResponseOperationFailedTest, MultipleOperations) {
    auto response1 =
        CommandResponse::operationFailed("connect", "device offline");
    auto response2 = CommandResponse::operationFailed("slew", "mount parked");
    auto response3 = CommandResponse::operationFailed("focus", "focuser stuck");

    EXPECT_EQ(response1["error"]["details"]["operation"], "connect");
    EXPECT_EQ(response2["error"]["details"]["operation"], "slew");
    EXPECT_EQ(response3["error"]["details"]["operation"], "focus");
}

// ============================================================================
// Timeout Tests
// ============================================================================

class CommandResponseTimeoutTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseTimeoutTest, BasicTimeout) {
    auto response = CommandResponse::timeout("exposure");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "timeout");
    EXPECT_EQ(response["error"]["details"]["operation"], "exposure");
}

TEST_F(CommandResponseTimeoutTest, MultipleTimeouts) {
    auto response1 = CommandResponse::timeout("connect");
    auto response2 = CommandResponse::timeout("slew");
    auto response3 = CommandResponse::timeout("plate_solve");

    EXPECT_EQ(response1["error"]["details"]["operation"], "connect");
    EXPECT_EQ(response2["error"]["details"]["operation"], "slew");
    EXPECT_EQ(response3["error"]["details"]["operation"], "plate_solve");
}

// ============================================================================
// Device Busy Tests
// ============================================================================

class CommandResponseDeviceBusyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseDeviceBusyTest, BasicDeviceBusy) {
    auto response = CommandResponse::deviceBusy("camera_1");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "device_busy");
    EXPECT_EQ(response["error"]["details"]["deviceId"], "camera_1");
    EXPECT_FALSE(response["error"]["details"].contains("currentOperation"));
}

TEST_F(CommandResponseDeviceBusyTest, DeviceBusyWithOperation) {
    auto response = CommandResponse::deviceBusy("camera_1", "exposing");

    EXPECT_EQ(response["error"]["code"], "device_busy");
    EXPECT_EQ(response["error"]["details"]["deviceId"], "camera_1");
    EXPECT_EQ(response["error"]["details"]["currentOperation"], "exposing");
}

TEST_F(CommandResponseDeviceBusyTest, DeviceBusyEmptyOperation) {
    auto response = CommandResponse::deviceBusy("mount_1", "");

    EXPECT_EQ(response["error"]["details"]["deviceId"], "mount_1");
    EXPECT_FALSE(response["error"]["details"].contains("currentOperation"));
}

// ============================================================================
// Not Connected Tests
// ============================================================================

class CommandResponseNotConnectedTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseNotConnectedTest, BasicNotConnected) {
    auto response = CommandResponse::notConnected("camera_1");

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], "not_connected");
    EXPECT_EQ(response["error"]["details"]["deviceId"], "camera_1");
}

TEST_F(CommandResponseNotConnectedTest, MultipleDevices) {
    auto response1 = CommandResponse::notConnected("camera_zwo");
    auto response2 = CommandResponse::notConnected("mount_eq6");
    auto response3 = CommandResponse::notConnected("focuser_moonlite");

    EXPECT_EQ(response1["error"]["details"]["deviceId"], "camera_zwo");
    EXPECT_EQ(response2["error"]["details"]["deviceId"], "mount_eq6");
    EXPECT_EQ(response3["error"]["details"]["deviceId"], "focuser_moonlite");
}

// ============================================================================
// Response Structure Tests
// ============================================================================

class CommandResponseStructureTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseStructureTest, SuccessHasRequiredFields) {
    auto response = CommandResponse::success(json{{"key", "value"}});

    EXPECT_TRUE(response.contains("status"));
    EXPECT_TRUE(response.contains("data"));
    EXPECT_EQ(response["status"], "success");
}

TEST_F(CommandResponseStructureTest, ErrorHasRequiredFields) {
    auto response = CommandResponse::error("code", "message");

    EXPECT_TRUE(response.contains("status"));
    EXPECT_TRUE(response.contains("error"));
    EXPECT_TRUE(response["error"].contains("code"));
    EXPECT_TRUE(response["error"].contains("message"));
    EXPECT_EQ(response["status"], "error");
}

TEST_F(CommandResponseStructureTest, StatusIsString) {
    auto success = CommandResponse::success();
    auto error = CommandResponse::error("code", "msg");

    EXPECT_TRUE(success["status"].is_string());
    EXPECT_TRUE(error["status"].is_string());
}

// ============================================================================
// JSON Serialization Tests
// ============================================================================

class CommandResponseSerializationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseSerializationTest, SuccessSerializes) {
    auto response = CommandResponse::success(json{{"key", "value"}});

    std::string serialized = response.dump();
    EXPECT_FALSE(serialized.empty());

    auto parsed = json::parse(serialized);
    EXPECT_EQ(parsed["data"]["key"], "value");
}

TEST_F(CommandResponseSerializationTest, ErrorSerializes) {
    auto response =
        CommandResponse::error("code", "message", json{{"detail", "info"}});

    std::string serialized = response.dump();
    EXPECT_FALSE(serialized.empty());

    auto parsed = json::parse(serialized);
    EXPECT_EQ(parsed["error"]["code"], "code");
}

// ============================================================================
// Edge Cases
// ============================================================================

class CommandResponseEdgeCasesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(CommandResponseEdgeCasesTest, EmptyStrings) {
    auto response1 = CommandResponse::error("", "");
    auto response2 = CommandResponse::deviceNotFound("", "");
    auto response3 = CommandResponse::missingParameter("");

    EXPECT_EQ(response1["error"]["code"], "");
    EXPECT_EQ(response2["error"]["details"]["deviceId"], "");
    EXPECT_EQ(response3["error"]["details"]["param"], "");
}

TEST_F(CommandResponseEdgeCasesTest, VeryLongStrings) {
    std::string longString(10000, 'x');

    auto response = CommandResponse::error(longString, longString);

    EXPECT_EQ(response["error"]["code"].get<std::string>().length(), 10000);
    EXPECT_EQ(response["error"]["message"].get<std::string>().length(), 10000);
}

TEST_F(CommandResponseEdgeCasesTest, SpecialCharacters) {
    std::string special = "Error: \"quotes\" & <tags> \n\t";

    auto response = CommandResponse::error("code", special);

    EXPECT_EQ(response["error"]["message"], special);
}

TEST_F(CommandResponseEdgeCasesTest, UnicodeStrings) {
    auto response = CommandResponse::error("错误代码", "错误消息");

    EXPECT_EQ(response["error"]["code"], "错误代码");
    EXPECT_EQ(response["error"]["message"], "错误消息");
}

TEST_F(CommandResponseEdgeCasesTest, LargeNestedData) {
    json largeData;
    for (int i = 0; i < 100; ++i) {
        largeData["key_" + std::to_string(i)] = {
            {"nested", {{"value", i}, {"array", {1, 2, 3}}}}};
    }

    auto response = CommandResponse::success(largeData);

    EXPECT_EQ(response["data"].size(), 100);
}
