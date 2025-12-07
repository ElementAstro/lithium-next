/*
 * test_script_controller.cpp - Tests for Script Controllers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gtest/gtest.h>

#include "atom/type/json.hpp"

#include <filesystem>
#include <string>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ============================================================================
// Script Request Format Tests
// ============================================================================

class ScriptRequestFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ScriptRequestFormatTest, ExecuteScriptRequest) {
    json request = {{"script_path", "/scripts/capture.py"},
                    {"args", {"--target", "M31", "--exposure", "30"}},
                    {"async", true}};

    EXPECT_EQ(request["script_path"], "/scripts/capture.py");
    EXPECT_TRUE(request["async"].get<bool>());
}

TEST_F(ScriptRequestFormatTest, ExecuteInlineScriptRequest) {
    json request = {{"code", "print('Hello, World!')"},
                    {"language", "python"},
                    {"timeout", 30}};

    EXPECT_EQ(request["language"], "python");
    EXPECT_EQ(request["timeout"], 30);
}

TEST_F(ScriptRequestFormatTest, StopScriptRequest) {
    json request = {{"script_id", "script-123"}};

    EXPECT_EQ(request["script_id"], "script-123");
}

TEST_F(ScriptRequestFormatTest, GetScriptStatusRequest) {
    json request = {{"script_id", "script-123"}};

    EXPECT_EQ(request["script_id"], "script-123");
}

// ============================================================================
// Script Response Format Tests
// ============================================================================

class ScriptResponseFormatTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ScriptResponseFormatTest, ExecuteScriptResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"script_id", "script-123"},
                       {"status", "running"},
                       {"started_at", "2024-01-01T12:00:00Z"}}}};

    EXPECT_TRUE(response["success"].get<bool>());
    EXPECT_EQ(response["data"]["status"], "running");
}

TEST_F(ScriptResponseFormatTest, ScriptCompletedResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"script_id", "script-123"},
                       {"status", "completed"},
                       {"exit_code", 0},
                       {"output", "Script executed successfully"},
                       {"duration_ms", 1500}}}};

    EXPECT_EQ(response["data"]["status"], "completed");
    EXPECT_EQ(response["data"]["exit_code"], 0);
}

TEST_F(ScriptResponseFormatTest, ScriptFailedResponse) {
    json response = {
        {"success", false},
        {"error",
         {{"code", "script_error"},
          {"message", "Script execution failed"},
          {"details",
           {{"exit_code", 1}, {"stderr", "Error: File not found"}}}}}};

    EXPECT_FALSE(response["success"].get<bool>());
    EXPECT_EQ(response["error"]["code"], "script_error");
}

// ============================================================================
// Python Script Controller Tests
// ============================================================================

class PythonScriptControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PythonScriptControllerTest, ExecutePythonScript) {
    json request = {{"script_path", "/scripts/automation.py"},
                    {"args", {"--config", "config.json"}},
                    {"env", {{"PYTHONPATH", "/lib/python"}}}};

    EXPECT_EQ(request["script_path"], "/scripts/automation.py");
    EXPECT_TRUE(request.contains("env"));
}

TEST_F(PythonScriptControllerTest, PythonScriptWithVirtualEnv) {
    json request = {{"script_path", "/scripts/capture.py"},
                    {"venv", "/venvs/astro"},
                    {"args", {}}};

    EXPECT_EQ(request["venv"], "/venvs/astro");
}

TEST_F(PythonScriptControllerTest, InstallPackageRequest) {
    json request = {
        {"package", "numpy"}, {"version", "1.24.0"}, {"venv", "/venvs/astro"}};

    EXPECT_EQ(request["package"], "numpy");
}

TEST_F(PythonScriptControllerTest, ListPackagesResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"packages",
                        {{{"name", "numpy"}, {"version", "1.24.0"}},
                         {{"name", "astropy"}, {"version", "5.3.0"}},
                         {{"name", "scipy"}, {"version", "1.11.0"}}}}}}};

    EXPECT_EQ(response["data"]["packages"].size(), 3);
}

// ============================================================================
// Shell Script Controller Tests
// ============================================================================

class ShellScriptControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ShellScriptControllerTest, ExecuteShellCommand) {
    json request = {{"command", "ls -la /images"},
                    {"timeout", 30},
                    {"working_dir", "/home/user"}};

    EXPECT_EQ(request["command"], "ls -la /images");
    EXPECT_EQ(request["timeout"], 30);
}

TEST_F(ShellScriptControllerTest, ExecuteShellScript) {
    json request = {{"script_path", "/scripts/backup.sh"},
                    {"args", {"/images", "/backup"}},
                    {"shell", "/bin/bash"}};

    EXPECT_EQ(request["shell"], "/bin/bash");
}

TEST_F(ShellScriptControllerTest, ShellCommandResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"stdout", "file1.fits\nfile2.fits\n"},
                       {"stderr", ""},
                       {"exit_code", 0},
                       {"duration_ms", 50}}}};

    EXPECT_EQ(response["data"]["exit_code"], 0);
}

// ============================================================================
// Isolated Script Controller Tests
// ============================================================================

class IsolatedScriptControllerTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IsolatedScriptControllerTest, ExecuteIsolatedScript) {
    json request = {{"script_path", "/scripts/untrusted.py"},
                    {"isolated", true},
                    {"resource_limits",
                     {{"max_memory_mb", 512},
                      {"max_cpu_time_s", 60},
                      {"max_file_size_mb", 100}}}};

    EXPECT_TRUE(request["isolated"].get<bool>());
    EXPECT_EQ(request["resource_limits"]["max_memory_mb"], 512);
}

TEST_F(IsolatedScriptControllerTest, SandboxedExecution) {
    json request = {{"code", "import os; os.system('rm -rf /')"},
                    {"sandbox", true},
                    {"allowed_modules", {"math", "json"}}};

    EXPECT_TRUE(request["sandbox"].get<bool>());
    EXPECT_EQ(request["allowed_modules"].size(), 2);
}

// ============================================================================
// Virtual Environment Tests
// ============================================================================

class VirtualEnvironmentTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(VirtualEnvironmentTest, CreateVenvRequest) {
    json request = {{"name", "astro-env"},
                    {"python_version", "3.11"},
                    {"packages", {"numpy", "astropy", "scipy"}}};

    EXPECT_EQ(request["name"], "astro-env");
    EXPECT_EQ(request["packages"].size(), 3);
}

TEST_F(VirtualEnvironmentTest, DeleteVenvRequest) {
    json request = {{"name", "old-env"}};

    EXPECT_EQ(request["name"], "old-env");
}

TEST_F(VirtualEnvironmentTest, ListVenvsResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"environments",
                        {{{"name", "astro-env"},
                          {"python_version", "3.11"},
                          {"packages_count", 15}},
                         {{"name", "dev-env"},
                          {"python_version", "3.10"},
                          {"packages_count", 8}}}}}}};

    EXPECT_EQ(response["data"]["environments"].size(), 2);
}

TEST_F(VirtualEnvironmentTest, ActivateVenvRequest) {
    json request = {{"name", "astro-env"}};

    EXPECT_EQ(request["name"], "astro-env");
}

// ============================================================================
// Tool Registry Tests
// ============================================================================

class ToolRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ToolRegistryTest, RegisterToolRequest) {
    json request = {{"name", "plate_solver"},
                    {"path", "/usr/bin/solve-field"},
                    {"description", "Astrometry.net plate solver"},
                    {"args_template", "--ra {ra} --dec {dec} {image}"}};

    EXPECT_EQ(request["name"], "plate_solver");
}

TEST_F(ToolRegistryTest, UnregisterToolRequest) {
    json request = {{"name", "old_tool"}};

    EXPECT_EQ(request["name"], "old_tool");
}

TEST_F(ToolRegistryTest, ListToolsResponse) {
    json response = {{"success", true},
                     {"data",
                      {{"tools",
                        {{{"name", "plate_solver"},
                          {"path", "/usr/bin/solve-field"},
                          {"available", true}},
                         {{"name", "stacker"},
                          {"path", "/usr/bin/siril"},
                          {"available", true}}}}}}};

    EXPECT_EQ(response["data"]["tools"].size(), 2);
}

TEST_F(ToolRegistryTest, ExecuteToolRequest) {
    json request = {
        {"tool", "plate_solver"},
        {"args",
         {{"ra", "12.5"}, {"dec", "45.0"}, {"image", "/images/test.fits"}}}};

    EXPECT_EQ(request["tool"], "plate_solver");
}

// ============================================================================
// Script Status Tests
// ============================================================================

class ScriptStatusTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ScriptStatusTest, PendingStatus) {
    json status = {{"script_id", "script-123"},
                   {"status", "pending"},
                   {"queued_at", "2024-01-01T12:00:00Z"}};

    EXPECT_EQ(status["status"], "pending");
}

TEST_F(ScriptStatusTest, RunningStatus) {
    json status = {{"script_id", "script-123"},
                   {"status", "running"},
                   {"started_at", "2024-01-01T12:00:00Z"},
                   {"progress", 50},
                   {"current_step", "Processing images"}};

    EXPECT_EQ(status["status"], "running");
    EXPECT_EQ(status["progress"], 50);
}

TEST_F(ScriptStatusTest, CompletedStatus) {
    json status = {{"script_id", "script-123"},
                   {"status", "completed"},
                   {"started_at", "2024-01-01T12:00:00Z"},
                   {"completed_at", "2024-01-01T12:05:00Z"},
                   {"exit_code", 0},
                   {"result", {{"images_processed", 10}}}};

    EXPECT_EQ(status["status"], "completed");
    EXPECT_EQ(status["exit_code"], 0);
}

TEST_F(ScriptStatusTest, FailedStatus) {
    json status = {{"script_id", "script-123"},
                   {"status", "failed"},
                   {"started_at", "2024-01-01T12:00:00Z"},
                   {"failed_at", "2024-01-01T12:01:00Z"},
                   {"exit_code", 1},
                   {"error", "FileNotFoundError: config.json not found"}};

    EXPECT_EQ(status["status"], "failed");
    EXPECT_EQ(status["exit_code"], 1);
}

TEST_F(ScriptStatusTest, CancelledStatus) {
    json status = {{"script_id", "script-123"},
                   {"status", "cancelled"},
                   {"started_at", "2024-01-01T12:00:00Z"},
                   {"cancelled_at", "2024-01-01T12:02:00Z"},
                   {"cancelled_by", "user"}};

    EXPECT_EQ(status["status"], "cancelled");
}

// ============================================================================
// Script Output Streaming Tests
// ============================================================================

class ScriptOutputStreamingTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ScriptOutputStreamingTest, OutputChunk) {
    json chunk = {{"script_id", "script-123"},
                  {"stream", "stdout"},
                  {"data", "Processing file 1 of 10...\n"},
                  {"timestamp", "2024-01-01T12:00:01Z"}};

    EXPECT_EQ(chunk["stream"], "stdout");
}

TEST_F(ScriptOutputStreamingTest, ErrorChunk) {
    json chunk = {{"script_id", "script-123"},
                  {"stream", "stderr"},
                  {"data", "Warning: Low memory\n"},
                  {"timestamp", "2024-01-01T12:00:02Z"}};

    EXPECT_EQ(chunk["stream"], "stderr");
}

TEST_F(ScriptOutputStreamingTest, ProgressUpdate) {
    json update = {{"script_id", "script-123"},
                   {"type", "progress"},
                   {"progress", 75},
                   {"message", "Processing image 8 of 10"}};

    EXPECT_EQ(update["type"], "progress");
    EXPECT_EQ(update["progress"], 75);
}

// ============================================================================
// Script Error Tests
// ============================================================================

class ScriptErrorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ScriptErrorTest, ScriptNotFound) {
    json error = {{"success", false},
                  {"error",
                   {{"code", "script_not_found"},
                    {"message", "Script not found: /scripts/missing.py"}}}};

    EXPECT_EQ(error["error"]["code"], "script_not_found");
}

TEST_F(ScriptErrorTest, SyntaxError) {
    json error = {
        {"success", false},
        {"error",
         {{"code", "syntax_error"},
          {"message", "Python syntax error"},
          {"details", {{"line", 10}, {"column", 5}, {"text", "def foo("}}}}}};

    EXPECT_EQ(error["error"]["code"], "syntax_error");
}

TEST_F(ScriptErrorTest, TimeoutError) {
    json error = {
        {"success", false},
        {"error",
         {{"code", "timeout"},
          {"message", "Script execution timed out after 60 seconds"}}}};

    EXPECT_EQ(error["error"]["code"], "timeout");
}

TEST_F(ScriptErrorTest, PermissionDenied) {
    json error = {{"success", false},
                  {"error",
                   {{"code", "permission_denied"},
                    {"message", "Permission denied: /scripts/restricted.py"}}}};

    EXPECT_EQ(error["error"]["code"], "permission_denied");
}
