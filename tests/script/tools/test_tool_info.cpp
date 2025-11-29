/*
 * test_tool_info.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file test_tool_info.cpp
 * @brief Comprehensive tests for Tool Info structures
 */

#include <gtest/gtest.h>
#include "script/tools/tool_info.hpp"

using namespace lithium::tools;

// =============================================================================
// ToolParameterInfo Tests
// =============================================================================

class ToolParameterInfoTest : public ::testing::Test {};

TEST_F(ToolParameterInfoTest, DefaultConstruction) {
    ToolParameterInfo param;
    EXPECT_TRUE(param.name.empty());
    EXPECT_TRUE(param.type.empty());
    EXPECT_FALSE(param.required);
}

TEST_F(ToolParameterInfoTest, ToJson) {
    ToolParameterInfo param;
    param.name = "input";
    param.type = "string";
    param.description = "Input parameter";
    param.required = true;
    param.defaultValue = "default";

    auto j = param.toJson();
    EXPECT_EQ(j["name"], "input");
    EXPECT_EQ(j["type"], "string");
    EXPECT_EQ(j["description"], "Input parameter");
    EXPECT_EQ(j["required"], true);
    EXPECT_EQ(j["defaultValue"], "default");
}

TEST_F(ToolParameterInfoTest, FromJson) {
    nlohmann::json j = {
        {"name", "output"},
        {"type", "integer"},
        {"description", "Output value"},
        {"required", false},
        {"defaultValue", 42}
    };

    auto result = ToolParameterInfo::fromJson(j);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "output");
    EXPECT_EQ(result->type, "integer");
    EXPECT_FALSE(result->required);
}

// =============================================================================
// ToolFunctionInfo Tests
// =============================================================================

class ToolFunctionInfoTest : public ::testing::Test {};

TEST_F(ToolFunctionInfoTest, DefaultConstruction) {
    ToolFunctionInfo func;
    EXPECT_TRUE(func.name.empty());
    EXPECT_TRUE(func.parameters.empty());
}

TEST_F(ToolFunctionInfoTest, ToJson) {
    ToolFunctionInfo func;
    func.name = "process";
    func.description = "Process data";
    func.returnType = "bool";

    ToolParameterInfo param;
    param.name = "data";
    param.type = "string";
    func.parameters.push_back(param);

    auto j = func.toJson();
    EXPECT_EQ(j["name"], "process");
    EXPECT_EQ(j["description"], "Process data");
    EXPECT_EQ(j["returnType"], "bool");
    EXPECT_EQ(j["parameters"].size(), 1);
}

TEST_F(ToolFunctionInfoTest, FromJson) {
    nlohmann::json j = {
        {"name", "calculate"},
        {"description", "Calculate result"},
        {"returnType", "float"},
        {"parameters", nlohmann::json::array()}
    };

    auto result = ToolFunctionInfo::fromJson(j);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "calculate");
    EXPECT_EQ(result->returnType, "float");
}

// =============================================================================
// ToolInfo Tests
// =============================================================================

class ToolInfoTest : public ::testing::Test {};

TEST_F(ToolInfoTest, DefaultConstruction) {
    ToolInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.functions.empty());
    EXPECT_TRUE(info.enabled);
}

TEST_F(ToolInfoTest, ToJson) {
    ToolInfo info;
    info.name = "math_tool";
    info.description = "Mathematical operations";
    info.version = "1.0.0";
    info.author = "Test Author";
    info.category = "math";
    info.enabled = true;

    ToolFunctionInfo func;
    func.name = "add";
    info.functions.push_back(func);

    auto j = info.toJson();
    EXPECT_EQ(j["name"], "math_tool");
    EXPECT_EQ(j["description"], "Mathematical operations");
    EXPECT_EQ(j["version"], "1.0.0");
    EXPECT_EQ(j["author"], "Test Author");
    EXPECT_EQ(j["category"], "math");
    EXPECT_EQ(j["enabled"], true);
    EXPECT_EQ(j["functions"].size(), 1);
}

TEST_F(ToolInfoTest, FromJson) {
    nlohmann::json j = {
        {"name", "string_tool"},
        {"description", "String operations"},
        {"version", "2.0.0"},
        {"author", "Author"},
        {"category", "string"},
        {"enabled", false},
        {"functions", nlohmann::json::array()}
    };

    auto result = ToolInfo::fromJson(j);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->name, "string_tool");
    EXPECT_EQ(result->version, "2.0.0");
    EXPECT_FALSE(result->enabled);
}

TEST_F(ToolInfoTest, HasFunction) {
    ToolInfo info;
    ToolFunctionInfo func;
    func.name = "test_func";
    info.functions.push_back(func);

    EXPECT_TRUE(info.hasFunction("test_func"));
    EXPECT_FALSE(info.hasFunction("nonexistent"));
}

TEST_F(ToolInfoTest, GetFunction) {
    ToolInfo info;
    ToolFunctionInfo func;
    func.name = "get_func";
    func.description = "Test function";
    info.functions.push_back(func);

    auto result = info.getFunction("get_func");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->description, "Test function");
}

TEST_F(ToolInfoTest, GetFunctionNonexistent) {
    ToolInfo info;
    auto result = info.getFunction("nonexistent");
    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// ToolRegistryConfig Tests
// =============================================================================

class ToolRegistryConfigTest : public ::testing::Test {};

TEST_F(ToolRegistryConfigTest, DefaultValues) {
    ToolRegistryConfig config;
    EXPECT_FALSE(config.toolsDirectory.empty() || config.toolsDirectory == "");
    EXPECT_TRUE(config.autoDiscover);
    EXPECT_TRUE(config.enableHotReload);
}

TEST_F(ToolRegistryConfigTest, ToJson) {
    ToolRegistryConfig config;
    config.toolsDirectory = "/path/to/tools";
    config.autoDiscover = false;
    config.enableHotReload = false;

    auto j = config.toJson();
    EXPECT_EQ(j["toolsDirectory"], "/path/to/tools");
    EXPECT_EQ(j["autoDiscover"], false);
    EXPECT_EQ(j["enableHotReload"], false);
}

TEST_F(ToolRegistryConfigTest, FromJson) {
    nlohmann::json j = {
        {"toolsDirectory", "/custom/path"},
        {"autoDiscover", true},
        {"enableHotReload", true}
    };

    auto result = ToolRegistryConfig::fromJson(j);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->toolsDirectory, "/custom/path");
    EXPECT_TRUE(result->autoDiscover);
}
