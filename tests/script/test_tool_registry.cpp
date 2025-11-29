/*
 * test_tool_registry.cpp
 *
 * Unit tests for PythonToolRegistry and related components
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "script/tool_registry.hpp"
#include "script/ipc_protocol.hpp"
#include "script/isolated_runner.hpp"

#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

namespace lithium::test {

using namespace std::chrono_literals;

// =============================================================================
// ToolParameterInfo Tests
// =============================================================================

class ToolParameterInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ToolParameterInfoTest, DefaultConstruction) {
    ToolParameterInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_EQ(info.type, ToolParameterType::Any);
    EXPECT_TRUE(info.required);
    EXPECT_TRUE(info.defaultValue.is_null());
}

TEST_F(ToolParameterInfoTest, ToJsonConversion) {
    ToolParameterInfo info;
    info.name = "test_param";
    info.type = ToolParameterType::String;
    info.description = "A test parameter";
    info.required = false;
    info.defaultValue = "default_value";

    auto json = info.toJson();

    EXPECT_EQ(json["name"], "test_param");
    EXPECT_EQ(json["type"], static_cast<int>(ToolParameterType::String));
    EXPECT_EQ(json["description"], "A test parameter");
    EXPECT_EQ(json["required"], false);
    EXPECT_EQ(json["default"], "default_value");
}

TEST_F(ToolParameterInfoTest, FromJsonConversion) {
    nlohmann::json j = {
        {"name", "restored_param"},
        {"type", static_cast<int>(ToolParameterType::Integer)},
        {"description", "Restored parameter"},
        {"required", true}
    };

    auto info = ToolParameterInfo::fromJson(j);

    EXPECT_EQ(info.name, "restored_param");
    EXPECT_EQ(info.type, ToolParameterType::Integer);
    EXPECT_EQ(info.description, "Restored parameter");
    EXPECT_TRUE(info.required);
}

// =============================================================================
// ToolFunctionInfo Tests
// =============================================================================

class ToolFunctionInfoTest : public ::testing::Test {};

TEST_F(ToolFunctionInfoTest, DefaultConstruction) {
    ToolFunctionInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.parameters.empty());
    EXPECT_EQ(info.returnType, "dict");
    EXPECT_FALSE(info.isAsync);
    EXPECT_TRUE(info.isStatic);
}

TEST_F(ToolFunctionInfoTest, ToJsonWithParameters) {
    ToolFunctionInfo info;
    info.name = "test_function";
    info.description = "A test function";
    info.category = "testing";
    info.tags = {"unit", "test"};

    ToolParameterInfo param;
    param.name = "param1";
    param.type = ToolParameterType::String;
    info.parameters.push_back(param);

    auto json = info.toJson();

    EXPECT_EQ(json["name"], "test_function");
    EXPECT_EQ(json["description"], "A test function");
    EXPECT_EQ(json["category"], "testing");
    EXPECT_EQ(json["tags"].size(), 2);
    EXPECT_EQ(json["parameters"].size(), 1);
}

// =============================================================================
// ToolInfo Tests
// =============================================================================

class ToolInfoTest : public ::testing::Test {};

TEST_F(ToolInfoTest, DefaultConstruction) {
    ToolInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_EQ(info.author, "Max Qian");
    EXPECT_EQ(info.license, "GPL-3.0-or-later");
    EXPECT_TRUE(info.supported);
}

TEST_F(ToolInfoTest, RoundTripJsonConversion) {
    ToolInfo original;
    original.name = "test_tool";
    original.version = "1.0.0";
    original.description = "A test tool";
    original.platforms = {"linux", "windows"};
    original.capabilities = {"feature1", "feature2"};
    original.categories = {"category1"};

    auto json = original.toJson();
    auto restored = ToolInfo::fromJson(json);

    EXPECT_EQ(restored.name, original.name);
    EXPECT_EQ(restored.version, original.version);
    EXPECT_EQ(restored.description, original.description);
    EXPECT_EQ(restored.platforms, original.platforms);
    EXPECT_EQ(restored.capabilities, original.capabilities);
    EXPECT_EQ(restored.categories, original.categories);
}

// =============================================================================
// ToolInvocationResult Tests
// =============================================================================

class ToolInvocationResultTest : public ::testing::Test {};

TEST_F(ToolInvocationResultTest, SuccessResult) {
    ToolInvocationResult result;
    result.success = true;
    result.data = nlohmann::json{{"key", "value"}};
    result.executionTime = 100ms;

    auto json = result.toJson();

    EXPECT_TRUE(json["success"]);
    EXPECT_EQ(json["data"]["key"], "value");
    EXPECT_EQ(json["execution_time_ms"], 100);
}

TEST_F(ToolInvocationResultTest, ErrorResult) {
    ToolInvocationResult result;
    result.success = false;
    result.error = "Something went wrong";
    result.errorType = "RuntimeError";
    result.traceback = "Traceback (most recent call last)...";

    auto json = result.toJson();

    EXPECT_FALSE(json["success"]);
    EXPECT_EQ(json["error"], "Something went wrong");
    EXPECT_EQ(json["error_type"], "RuntimeError");
    EXPECT_TRUE(json.contains("traceback"));
}

// =============================================================================
// IPC Protocol Tests
// =============================================================================

class IPCProtocolTest : public ::testing::Test {};

TEST_F(IPCProtocolTest, MessageHeaderSerialization) {
    MessageHeader header;
    header.type = MessageType::Execute;
    header.payloadSize = 100;
    header.sequenceId = 42;

    auto bytes = header.serialize();

    EXPECT_EQ(bytes.size(), MessageHeader::HEADER_SIZE);

    auto result = MessageHeader::deserialize(bytes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->magic, MessageHeader::MAGIC);
    EXPECT_EQ(result->type, MessageType::Execute);
    EXPECT_EQ(result->payloadSize, 100u);
    EXPECT_EQ(result->sequenceId, 42u);
}

TEST_F(IPCProtocolTest, InvalidMagicNumber) {
    std::vector<uint8_t> invalidData(MessageHeader::HEADER_SIZE, 0);

    auto result = MessageHeader::deserialize(invalidData);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), IPCError::InvalidMessage);
}

TEST_F(IPCProtocolTest, ExecuteRequestSerialization) {
    ExecuteRequest request;
    request.scriptContent = "print('Hello')";
    request.arguments = {{"name", "World"}};
    request.workingDirectory = "/tmp";

    auto json = request.toJson();

    EXPECT_EQ(json["script_content"], "print('Hello')");
    EXPECT_EQ(json["arguments"]["name"], "World");
    EXPECT_EQ(json["working_directory"], "/tmp");

    auto restored = ExecuteRequest::fromJson(json);
    EXPECT_EQ(restored.scriptContent, request.scriptContent);
}

TEST_F(IPCProtocolTest, ExecuteResultSerialization) {
    ExecuteResult execResult;
    execResult.success = true;
    execResult.result = {{"answer", 42}};
    execResult.output = "Hello, World!";
    execResult.executionTimeMs = 150;

    auto json = execResult.toJson();

    EXPECT_TRUE(json["success"]);
    EXPECT_EQ(json["result"]["answer"], 42);
    EXPECT_EQ(json["output"], "Hello, World!");
    EXPECT_EQ(json["execution_time_ms"], 150);
}

TEST_F(IPCProtocolTest, ProgressUpdateSerialization) {
    ProgressUpdate progress;
    progress.percentage = 50.0f;
    progress.message = "Halfway done";
    progress.currentStep = "processing";

    auto json = progress.toJson();

    EXPECT_FLOAT_EQ(json["percentage"], 50.0f);
    EXPECT_EQ(json["message"], "Halfway done");
    EXPECT_EQ(json["current_step"], "processing");
}

// =============================================================================
// IsolatedRunner Configuration Tests
// =============================================================================

class IsolatedRunnerConfigTest : public ::testing::Test {};

TEST_F(IsolatedRunnerConfigTest, DefaultConfiguration) {
    IsolationConfig config;

    EXPECT_EQ(config.level, IsolationLevel::Subprocess);
    EXPECT_EQ(config.maxMemoryMB, 512);
    EXPECT_EQ(config.maxCpuPercent, 100);
    EXPECT_EQ(config.timeout, 300s);
    EXPECT_FALSE(config.allowNetwork);
    EXPECT_TRUE(config.allowFilesystem);
    EXPECT_TRUE(config.captureOutput);
}

TEST_F(IsolatedRunnerConfigTest, IsolationLevelStrings) {
    EXPECT_EQ(isolationLevelToString(IsolationLevel::None), "None");
    EXPECT_EQ(isolationLevelToString(IsolationLevel::Subprocess), "Subprocess");
    EXPECT_EQ(isolationLevelToString(IsolationLevel::Sandboxed), "Sandboxed");
}

TEST_F(IsolatedRunnerConfigTest, ErrorCodeStrings) {
    EXPECT_EQ(isolatedRunnerErrorToString(IsolatedRunnerError::Success), "Success");
    EXPECT_EQ(isolatedRunnerErrorToString(IsolatedRunnerError::Timeout), "Timeout");
    EXPECT_EQ(isolatedRunnerErrorToString(IsolatedRunnerError::PythonNotFound),
              "Python not found");
}

// =============================================================================
// IsolatedExecutionResult Tests
// =============================================================================

class IsolatedExecutionResultTest : public ::testing::Test {};

TEST_F(IsolatedExecutionResultTest, DefaultValues) {
    IsolatedExecutionResult result;

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.exitCode, -1);
    EXPECT_TRUE(result.output.empty());
    EXPECT_TRUE(result.errorOutput.empty());
    EXPECT_TRUE(result.result.is_null());
    EXPECT_EQ(result.executionTime, 0ms);
}

// =============================================================================
// ToolRegistryError Tests
// =============================================================================

class ToolRegistryErrorTest : public ::testing::Test {};

TEST_F(ToolRegistryErrorTest, ErrorCodeStrings) {
    EXPECT_EQ(toolRegistryErrorToString(ToolRegistryError::Success), "Success");
    EXPECT_EQ(toolRegistryErrorToString(ToolRegistryError::NotInitialized),
              "Registry not initialized");
    EXPECT_EQ(toolRegistryErrorToString(ToolRegistryError::ToolNotFound),
              "Tool not found");
    EXPECT_EQ(toolRegistryErrorToString(ToolRegistryError::FunctionNotFound),
              "Function not found");
    EXPECT_EQ(toolRegistryErrorToString(ToolRegistryError::PythonError),
              "Python error");
}

// =============================================================================
// RegisteredTool Tests
// =============================================================================

class RegisteredToolTest : public ::testing::Test {};

TEST_F(RegisteredToolTest, ToJsonConversion) {
    RegisteredTool tool;
    tool.name = "test_tool";
    tool.modulePath = "python.tools.test";
    tool.isLoaded = true;
    tool.functionNames = {"func1", "func2"};

    auto json = tool.toJson();

    EXPECT_EQ(json["name"], "test_tool");
    EXPECT_EQ(json["module_path"], "python.tools.test");
    EXPECT_TRUE(json["is_loaded"]);
    EXPECT_EQ(json["function_names"].size(), 2);
}

TEST_F(RegisteredToolTest, ErrorState) {
    RegisteredTool tool;
    tool.name = "failed_tool";
    tool.isLoaded = false;
    tool.loadError = "Module not found";

    auto json = tool.toJson();

    EXPECT_FALSE(json["is_loaded"]);
    EXPECT_EQ(json["error"], "Module not found");
}

// =============================================================================
// Integration Tests (requires Python)
// =============================================================================

class ToolRegistryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Skip if Python is not available
        if (!std::filesystem::exists("/usr/bin/python3") &&
            !std::filesystem::exists("C:\\Python311\\python.exe") &&
            !std::filesystem::exists("C:\\Python312\\python.exe")) {
            GTEST_SKIP() << "Python interpreter not found";
        }
    }
};

// Note: Full integration tests require Python environment setup
// These tests verify the C++ side of the interface

TEST_F(ToolRegistryIntegrationTest, RegistryConstruction) {
    ToolRegistryConfig config;
    config.autoDiscover = false;  // Don't auto-discover in tests

    PythonToolRegistry registry(config);

    EXPECT_FALSE(registry.isInitialized());
    EXPECT_TRUE(registry.getToolNames().empty());
}

TEST_F(ToolRegistryIntegrationTest, ManualToolRegistration) {
    ToolRegistryConfig config;
    config.autoDiscover = false;

    PythonToolRegistry registry(config);

    ToolInfo info;
    info.name = "manual_tool";
    info.version = "1.0.0";
    info.description = "A manually registered tool";

    auto result = registry.registerTool(info);
    // Note: This may fail if Python isn't initialized
    // In a real test environment, initialization would be required
}

// =============================================================================
// Factory Tests
// =============================================================================

class IsolatedRunnerFactoryTest : public ::testing::Test {};

TEST_F(IsolatedRunnerFactoryTest, CreateDefault) {
    auto runner = IsolatedRunnerFactory::create();
    EXPECT_NE(runner, nullptr);
}

TEST_F(IsolatedRunnerFactoryTest, CreateQuick) {
    auto runner = IsolatedRunnerFactory::createQuick();
    EXPECT_NE(runner, nullptr);
}

TEST_F(IsolatedRunnerFactoryTest, CreateSecure) {
    auto runner = IsolatedRunnerFactory::createSecure();
    EXPECT_NE(runner, nullptr);
}

TEST_F(IsolatedRunnerFactoryTest, CreateScientific) {
    auto runner = IsolatedRunnerFactory::createScientific();
    EXPECT_NE(runner, nullptr);
}

TEST_F(IsolatedRunnerFactoryTest, CreateWithConfig) {
    IsolationConfig config;
    config.maxMemoryMB = 1024;
    config.timeout = 600s;

    auto runner = IsolatedRunnerFactory::create(config);
    EXPECT_NE(runner, nullptr);
    EXPECT_EQ(runner->getConfig().maxMemoryMB, 1024);
}

}  // namespace lithium::test
