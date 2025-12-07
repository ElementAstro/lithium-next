/*
 * test_base_service.cpp - Tests for BaseDeviceService
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "device/service/base_service.hpp"
#include "device/template/device.hpp"

using namespace lithium::device;
using namespace testing;
using json = nlohmann::json;

// Mock device for testing
class MockDevice : public AtomDriver {
public:
    explicit MockDevice(const std::string& name) : AtomDriver(name) {
        type_ = "mock";
    }

    MOCK_METHOD(bool, initialize, (), (override));
    MOCK_METHOD(bool, destroy, (), (override));
    MOCK_METHOD(bool, connect, (const std::string&, int, int), (override));
    MOCK_METHOD(bool, disconnect, (), (override));
    MOCK_METHOD(bool, isConnected, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, scan, (), (override));
};

// Concrete service for testing
class TestDeviceService : public TypedDeviceService<TestDeviceService, MockDevice> {
public:
    TestDeviceService()
        : TypedDeviceService("TestService", "TestDevice", "test_device") {}

    // Expose protected methods for testing
    using TypedDeviceService::makeSuccessResponse;
    using TypedDeviceService::makeErrorResponse;
    using TypedDeviceService::executeWithErrorHandling;
    using TypedDeviceService::logOperationStart;
    using TypedDeviceService::logOperationEnd;
    using TypedDeviceService::publishDeviceStateChange;
};

class BaseDeviceServiceTest : public Test {
protected:
    void SetUp() override {
        service_ = std::make_unique<TestDeviceService>();
    }

    void TearDown() override {
        service_.reset();
    }

    std::unique_ptr<TestDeviceService> service_;
};

// ========== Response Construction Tests ==========

TEST_F(BaseDeviceServiceTest, MakeSuccessResponse_Empty) {
    auto response = service_->makeSuccessResponse();

    EXPECT_TRUE(response.contains("status"));
    EXPECT_EQ(response["status"], "success");
}

TEST_F(BaseDeviceServiceTest, MakeSuccessResponse_WithData) {
    json data = {{"key", "value"}};
    auto response = service_->makeSuccessResponse(data);

    EXPECT_EQ(response["status"], "success");
    EXPECT_TRUE(response.contains("data"));
    EXPECT_EQ(response["data"]["key"], "value");
}

TEST_F(BaseDeviceServiceTest, MakeSuccessResponse_WithMessage) {
    auto response = service_->makeSuccessResponse("Operation completed");

    EXPECT_EQ(response["status"], "success");
    EXPECT_TRUE(response.contains("message"));
    EXPECT_EQ(response["message"], "Operation completed");
}

TEST_F(BaseDeviceServiceTest, MakeSuccessResponse_WithDataAndMessage) {
    json data = {{"result", 42}};
    auto response = service_->makeSuccessResponse(data, "Done");

    EXPECT_EQ(response["status"], "success");
    EXPECT_EQ(response["data"]["result"], 42);
    EXPECT_EQ(response["message"], "Done");
}

TEST_F(BaseDeviceServiceTest, MakeErrorResponse_WithCodeAndMessage) {
    auto response = service_->makeErrorResponse(
        ErrorCode::DEVICE_NOT_FOUND, "Camera not found");

    EXPECT_EQ(response["status"], "error");
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["error"]["code"], ErrorCode::DEVICE_NOT_FOUND);
    EXPECT_EQ(response["error"]["message"], "Camera not found");
}

TEST_F(BaseDeviceServiceTest, MakeErrorResponse_FromException) {
    std::runtime_error ex("Something went wrong");
    auto response = service_->makeErrorResponse(ex);

    EXPECT_EQ(response["status"], "error");
    EXPECT_EQ(response["error"]["code"], ErrorCode::INTERNAL_ERROR);
    EXPECT_EQ(response["error"]["message"], "Something went wrong");
}

// ========== Error Handling Tests ==========

TEST_F(BaseDeviceServiceTest, ExecuteWithErrorHandling_Success) {
    auto result = service_->executeWithErrorHandling("testOp", []() -> json {
        return {{"status", "success"}, {"value", 123}};
    });

    EXPECT_EQ(result["status"], "success");
    EXPECT_EQ(result["value"], 123);
}

TEST_F(BaseDeviceServiceTest, ExecuteWithErrorHandling_Exception) {
    auto result = service_->executeWithErrorHandling("testOp", []() -> json {
        throw std::runtime_error("Test error");
    });

    EXPECT_EQ(result["status"], "error");
    EXPECT_EQ(result["error"]["message"], "Test error");
}

// ========== Service Name Tests ==========

TEST_F(BaseDeviceServiceTest, GetServiceName_ReturnsCorrectName) {
    EXPECT_EQ(service_->getServiceName(), "TestService");
}

TEST_F(BaseDeviceServiceTest, GetDeviceTypeName_ReturnsCorrectName) {
    EXPECT_EQ(service_->getDeviceTypeName(), "TestDevice");
}

// ========== Error Code Constants Tests ==========

TEST(ErrorCodeTest, ConstantsAreDefined) {
    EXPECT_STREQ(ErrorCode::INTERNAL_ERROR, "internal_error");
    EXPECT_STREQ(ErrorCode::DEVICE_NOT_FOUND, "device_not_found");
    EXPECT_STREQ(ErrorCode::DEVICE_NOT_CONNECTED, "device_not_connected");
    EXPECT_STREQ(ErrorCode::DEVICE_BUSY, "device_busy");
    EXPECT_STREQ(ErrorCode::CONNECTION_FAILED, "connection_failed");
    EXPECT_STREQ(ErrorCode::INVALID_FIELD_VALUE, "invalid_field_value");
    EXPECT_STREQ(ErrorCode::FEATURE_NOT_SUPPORTED, "feature_not_supported");
    EXPECT_STREQ(ErrorCode::OPERATION_FAILED, "operation_failed");
    EXPECT_STREQ(ErrorCode::INVALID_COORDINATES, "invalid_coordinates");
    EXPECT_STREQ(ErrorCode::TIMEOUT, "timeout");
}

// ========== Device Check Tests ==========

TEST_F(BaseDeviceServiceTest, CheckDeviceConnected_NullDevice) {
    std::shared_ptr<MockDevice> nullDevice = nullptr;
    auto error = service_->checkDeviceConnected(nullDevice, "TestDevice");

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ((*error)["status"], "error");
    EXPECT_EQ((*error)["error"]["code"], ErrorCode::DEVICE_NOT_FOUND);
}

TEST_F(BaseDeviceServiceTest, CheckDeviceConnected_Disconnected) {
    auto device = std::make_shared<MockDevice>("TestDevice");
    EXPECT_CALL(*device, isConnected()).WillOnce(Return(false));

    auto error = service_->checkDeviceConnected(device, "TestDevice");

    ASSERT_TRUE(error.has_value());
    EXPECT_EQ((*error)["status"], "error");
    EXPECT_EQ((*error)["error"]["code"], ErrorCode::DEVICE_NOT_CONNECTED);
}

TEST_F(BaseDeviceServiceTest, CheckDeviceConnected_Connected) {
    auto device = std::make_shared<MockDevice>("TestDevice");
    EXPECT_CALL(*device, isConnected()).WillOnce(Return(true));

    auto error = service_->checkDeviceConnected(device, "TestDevice");

    EXPECT_FALSE(error.has_value());
}

// ========== Logging Tests ==========

TEST_F(BaseDeviceServiceTest, LogOperationStart_NoThrow) {
    EXPECT_NO_THROW(service_->logOperationStart("testOperation"));
}

TEST_F(BaseDeviceServiceTest, LogOperationEnd_NoThrow) {
    EXPECT_NO_THROW(service_->logOperationEnd("testOperation"));
}

// ========== State Change Tests ==========

TEST_F(BaseDeviceServiceTest, PublishDeviceStateChange_NoThrow) {
    // Should not throw even if message bus is not available
    EXPECT_NO_THROW(service_->publishDeviceStateChange("camera", "cam-001", "connected"));
}

// ========== Response Format Tests ==========

TEST_F(BaseDeviceServiceTest, SuccessResponse_HasCorrectStructure) {
    auto response = service_->makeSuccessResponse();

    EXPECT_TRUE(response.is_object());
    EXPECT_TRUE(response.contains("status"));
    EXPECT_EQ(response["status"], "success");
}

TEST_F(BaseDeviceServiceTest, ErrorResponse_HasCorrectStructure) {
    auto response = service_->makeErrorResponse(
        ErrorCode::DEVICE_BUSY, "Device is busy");

    EXPECT_TRUE(response.is_object());
    EXPECT_TRUE(response.contains("status"));
    EXPECT_TRUE(response.contains("error"));
    EXPECT_EQ(response["status"], "error");
    EXPECT_TRUE(response["error"].contains("code"));
    EXPECT_TRUE(response["error"].contains("message"));
}

TEST_F(BaseDeviceServiceTest, ExecuteWithErrorHandling_StdException) {
    auto result = service_->executeWithErrorHandling("testOp", []() -> json {
        throw std::invalid_argument("Invalid argument");
    });

    EXPECT_EQ(result["status"], "error");
    EXPECT_EQ(result["error"]["message"], "Invalid argument");
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
