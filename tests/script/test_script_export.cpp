/*
 * test_script_export.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "script/script_export.hpp"
#include "atom/type/json.hpp"

namespace lithium::test {

// ============================================================================
// HttpMethod Tests
// ============================================================================

TEST(HttpMethodTest, ToString) {
    EXPECT_EQ(httpMethodToString(HttpMethod::GET), "GET");
    EXPECT_EQ(httpMethodToString(HttpMethod::POST), "POST");
    EXPECT_EQ(httpMethodToString(HttpMethod::PUT), "PUT");
    EXPECT_EQ(httpMethodToString(HttpMethod::DELETE_), "DELETE");
    EXPECT_EQ(httpMethodToString(HttpMethod::PATCH), "PATCH");
}

TEST(HttpMethodTest, FromString) {
    EXPECT_EQ(stringToHttpMethod("GET"), HttpMethod::GET);
    EXPECT_EQ(stringToHttpMethod("POST"), HttpMethod::POST);
    EXPECT_EQ(stringToHttpMethod("PUT"), HttpMethod::PUT);
    EXPECT_EQ(stringToHttpMethod("DELETE"), HttpMethod::DELETE_);
    EXPECT_EQ(stringToHttpMethod("PATCH"), HttpMethod::PATCH);
    EXPECT_EQ(stringToHttpMethod("UNKNOWN"), HttpMethod::POST);
}

// ============================================================================
// ParamInfo Tests
// ============================================================================

TEST(ParamInfoTest, ToJsonBasic) {
    ParamInfo param;
    param.name = "test_param";
    param.type = "int";
    param.required = true;
    param.description = "A test parameter";

    auto json = param.toJson();

    EXPECT_EQ(json["name"], "test_param");
    EXPECT_EQ(json["type"], "int");
    EXPECT_EQ(json["required"], true);
    EXPECT_EQ(json["description"], "A test parameter");
    EXPECT_FALSE(json.contains("default"));
}

TEST(ParamInfoTest, ToJsonWithDefault) {
    ParamInfo param;
    param.name = "optional_param";
    param.type = "str";
    param.required = false;
    param.defaultValue = "default_value";
    param.description = "An optional parameter";

    auto json = param.toJson();

    EXPECT_EQ(json["name"], "optional_param");
    EXPECT_EQ(json["required"], false);
    EXPECT_EQ(json["default"], "default_value");
}

TEST(ParamInfoTest, FromJson) {
    nlohmann::json json = {
        {"name", "parsed_param"},
        {"type", "float"},
        {"required", false},
        {"default", "3.14"},
        {"description", "Parsed parameter"}
    };

    auto param = ParamInfo::fromJson(json);

    EXPECT_EQ(param.name, "parsed_param");
    EXPECT_EQ(param.type, "float");
    EXPECT_FALSE(param.required);
    EXPECT_TRUE(param.defaultValue.has_value());
    EXPECT_EQ(*param.defaultValue, "3.14");
    EXPECT_EQ(param.description, "Parsed parameter");
}

TEST(ParamInfoTest, RoundTrip) {
    ParamInfo original;
    original.name = "roundtrip";
    original.type = "dict";
    original.required = true;
    original.description = "Test roundtrip";

    auto json = original.toJson();
    auto parsed = ParamInfo::fromJson(json);

    EXPECT_EQ(parsed.name, original.name);
    EXPECT_EQ(parsed.type, original.type);
    EXPECT_EQ(parsed.required, original.required);
    EXPECT_EQ(parsed.description, original.description);
}

// ============================================================================
// ExportInfo Tests
// ============================================================================

TEST(ExportInfoTest, ControllerToJson) {
    ExportInfo info;
    info.name = "test_controller";
    info.type = ExportType::CONTROLLER;
    info.description = "Test controller endpoint";
    info.endpoint = "/api/test";
    info.method = HttpMethod::POST;
    info.returnType = "dict";
    info.version = "1.0.0";
    info.tags = {"test", "api"};

    ParamInfo param;
    param.name = "input";
    param.type = "str";
    param.required = true;
    info.params.push_back(param);

    auto json = info.toJson();

    EXPECT_EQ(json["name"], "test_controller");
    EXPECT_EQ(json["export_type"], "controller");
    EXPECT_EQ(json["endpoint"], "/api/test");
    EXPECT_EQ(json["method"], "POST");
    EXPECT_EQ(json["parameters"].size(), 1);
    EXPECT_EQ(json["tags"].size(), 2);
}

TEST(ExportInfoTest, CommandToJson) {
    ExportInfo info;
    info.name = "test_command";
    info.type = ExportType::COMMAND;
    info.description = "Test command";
    info.commandId = "test.command";
    info.priority = 10;
    info.timeoutMs = 5000;
    info.returnType = "dict";

    auto json = info.toJson();

    EXPECT_EQ(json["name"], "test_command");
    EXPECT_EQ(json["export_type"], "command");
    EXPECT_EQ(json["command_id"], "test.command");
    EXPECT_EQ(json["priority"], 10);
    EXPECT_EQ(json["timeout_ms"], 5000);
}

TEST(ExportInfoTest, FromJsonController) {
    nlohmann::json json = {
        {"name", "parsed_controller"},
        {"export_type", "controller"},
        {"description", "Parsed controller"},
        {"endpoint", "/api/parsed"},
        {"method", "GET"},
        {"return_type", "str"},
        {"parameters", nlohmann::json::array()},
        {"tags", {"parsed"}},
        {"version", "2.0.0"},
        {"deprecated", false}
    };

    auto info = ExportInfo::fromJson(json);

    EXPECT_EQ(info.name, "parsed_controller");
    EXPECT_EQ(info.type, ExportType::CONTROLLER);
    EXPECT_EQ(info.endpoint, "/api/parsed");
    EXPECT_EQ(info.method, HttpMethod::GET);
    EXPECT_EQ(info.version, "2.0.0");
}

TEST(ExportInfoTest, FromJsonCommand) {
    nlohmann::json json = {
        {"name", "parsed_command"},
        {"export_type", "command"},
        {"description", "Parsed command"},
        {"command_id", "parsed.cmd"},
        {"priority", 5},
        {"timeout_ms", 10000},
        {"return_type", "dict"},
        {"parameters", nlohmann::json::array()},
        {"tags", nlohmann::json::array()},
        {"version", "1.0.0"},
        {"deprecated", false}
    };

    auto info = ExportInfo::fromJson(json);

    EXPECT_EQ(info.name, "parsed_command");
    EXPECT_EQ(info.type, ExportType::COMMAND);
    EXPECT_EQ(info.commandId, "parsed.cmd");
    EXPECT_EQ(info.priority, 5);
    EXPECT_EQ(info.timeoutMs, 10000);
}

// ============================================================================
// ScriptExports Tests
// ============================================================================

TEST(ScriptExportsTest, HasExports) {
    ScriptExports exports;
    EXPECT_FALSE(exports.hasExports());

    ExportInfo ctrl;
    ctrl.type = ExportType::CONTROLLER;
    exports.controllers.push_back(ctrl);
    EXPECT_TRUE(exports.hasExports());
}

TEST(ScriptExportsTest, Count) {
    ScriptExports exports;
    EXPECT_EQ(exports.count(), 0);

    ExportInfo ctrl;
    ctrl.type = ExportType::CONTROLLER;
    exports.controllers.push_back(ctrl);
    exports.controllers.push_back(ctrl);

    ExportInfo cmd;
    cmd.type = ExportType::COMMAND;
    exports.commands.push_back(cmd);

    EXPECT_EQ(exports.count(), 3);
}

TEST(ScriptExportsTest, ToJsonAndBack) {
    ScriptExports original;
    original.moduleName = "test_module";
    original.moduleFile = "/path/to/module.py";
    original.version = "1.0.0";

    ExportInfo ctrl;
    ctrl.name = "test_ctrl";
    ctrl.type = ExportType::CONTROLLER;
    ctrl.endpoint = "/api/test";
    original.controllers.push_back(ctrl);

    ExportInfo cmd;
    cmd.name = "test_cmd";
    cmd.type = ExportType::COMMAND;
    cmd.commandId = "test.cmd";
    original.commands.push_back(cmd);

    auto json = original.toJson();
    auto parsed = ScriptExports::fromJson(json);

    EXPECT_EQ(parsed.moduleName, original.moduleName);
    EXPECT_EQ(parsed.moduleFile, original.moduleFile);
    EXPECT_EQ(parsed.version, original.version);
    EXPECT_EQ(parsed.controllers.size(), 1);
    EXPECT_EQ(parsed.commands.size(), 1);
    EXPECT_EQ(parsed.controllers[0].name, "test_ctrl");
    EXPECT_EQ(parsed.commands[0].commandId, "test.cmd");
}

TEST(ScriptExportsTest, EmptyExports) {
    ScriptExports exports;
    exports.moduleName = "empty";

    auto json = exports.toJson();
    auto parsed = ScriptExports::fromJson(json);

    EXPECT_EQ(parsed.moduleName, "empty");
    EXPECT_TRUE(parsed.controllers.empty());
    EXPECT_TRUE(parsed.commands.empty());
    EXPECT_FALSE(parsed.hasExports());
}

TEST(ScriptExportsTest, MultipleExports) {
    ScriptExports exports;
    exports.moduleName = "multi";

    for (int i = 0; i < 5; ++i) {
        ExportInfo ctrl;
        ctrl.name = "ctrl_" + std::to_string(i);
        ctrl.type = ExportType::CONTROLLER;
        ctrl.endpoint = "/api/" + std::to_string(i);
        exports.controllers.push_back(ctrl);
    }

    for (int i = 0; i < 3; ++i) {
        ExportInfo cmd;
        cmd.name = "cmd_" + std::to_string(i);
        cmd.type = ExportType::COMMAND;
        cmd.commandId = "cmd." + std::to_string(i);
        exports.commands.push_back(cmd);
    }

    EXPECT_EQ(exports.count(), 8);

    auto json = exports.toJson();
    auto parsed = ScriptExports::fromJson(json);

    EXPECT_EQ(parsed.controllers.size(), 5);
    EXPECT_EQ(parsed.commands.size(), 3);
}

// ============================================================================
// Integration Tests (require Python)
// ============================================================================

#ifdef LITHIUM_TEST_WITH_PYTHON

#include <filesystem>
#include <fstream>

#include "script/python_caller.hpp"

namespace fs = std::filesystem;

class PythonExportIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir_ = fs::temp_directory_path() / "lithium_export_test";
        fs::create_directories(testDir_);
        createTestScript();
        createManifestFile();

        wrapper_ = std::make_unique<lithium::PythonWrapper>();
        wrapper_->add_sys_path(testDir_.string());
    }

    void TearDown() override {
        wrapper_.reset();
        fs::remove_all(testDir_);
    }

    void createTestScript() {
        std::string content = R"(
__version__ = "1.0.0"

def add(a: int, b: int) -> dict:
    return {"result": a + b}

def greet(name: str = "World") -> str:
    return f"Hello, {name}!"
)";
        std::ofstream file(testDir_ / "test_exports.py");
        file << content;
    }

    void createManifestFile() {
        std::string manifest = R"({
    "module_name": "test_exports",
    "version": "1.0.0",
    "exports": {
        "controllers": [
            {
                "name": "add",
                "endpoint": "/api/add",
                "method": "POST",
                "description": "Add two numbers"
            },
            {
                "name": "greet",
                "endpoint": "/api/greet",
                "method": "GET",
                "description": "Greet someone"
            }
        ],
        "commands": []
    }
})";
        std::ofstream file(testDir_ / "lithium_manifest.json");
        file << manifest;
    }

    fs::path testDir_;
    std::unique_ptr<lithium::PythonWrapper> wrapper_;
};

TEST_F(PythonExportIntegrationTest, LoadScriptAndDiscoverExports) {
    wrapper_->load_script("test_exports", "test");

    auto exports = wrapper_->discover_exports("test");
    ASSERT_TRUE(exports.has_value());
    EXPECT_EQ(exports->moduleName, "test");
}

TEST_F(PythonExportIntegrationTest, HasExports) {
    wrapper_->load_script("test_exports", "test");

    EXPECT_TRUE(wrapper_->has_exports("test"));
    EXPECT_FALSE(wrapper_->has_exports("nonexistent"));
}

TEST_F(PythonExportIntegrationTest, ListScriptsWithExports) {
    wrapper_->load_script("test_exports", "test1");
    wrapper_->load_script("test_exports", "test2");

    auto scripts = wrapper_->list_scripts_with_exports();
    EXPECT_GE(scripts.size(), 2);
}

TEST_F(PythonExportIntegrationTest, GetAllExports) {
    wrapper_->load_script("test_exports", "test");

    auto allExports = wrapper_->get_all_exports();
    EXPECT_EQ(allExports.size(), 1);
    EXPECT_TRUE(allExports.count("test") > 0);
}

TEST_F(PythonExportIntegrationTest, InvokeExport) {
    wrapper_->load_script("test_exports", "test");

    py::dict kwargs;
    kwargs["a"] = py::int_(5);
    kwargs["b"] = py::int_(3);

    auto result = wrapper_->invoke_export("test", "add", kwargs);
    EXPECT_FALSE(result.is_none());
}

#endif  // LITHIUM_TEST_WITH_PYTHON

}  // namespace lithium::test
