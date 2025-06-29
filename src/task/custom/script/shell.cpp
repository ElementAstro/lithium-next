#include "shell.hpp"
#include "../factory.hpp"

#include "spdlog/spdlog.h"

namespace lithium::task::script {

ShellScriptTask::ShellScriptTask(const std::string& name,
                                 const std::string& scriptConfigPath)
    : BaseScriptTask(name, scriptConfigPath) {
    setupShellDefaults();
}

void ShellScriptTask::setupShellDefaults() {
    addParamDefinition("shellType", "string", false, "/bin/bash",
                       "Shell interpreter to use");
    addParamDefinition("environmentVars", "object", false, json::object(),
                       "Environment variables");

    setTaskType("shell_script");
}

void ShellScriptTask::setShellType(const std::string& shell) {
    shellType_ = shell;
    addHistoryEntry("Shell type set to: " + shell);
}

void ShellScriptTask::setWorkingDirectory(const std::string& directory) {
    workingDirectory_ = directory;
    addHistoryEntry("Working directory set to: " + directory);
}

void ShellScriptTask::setEnvironmentVariable(const std::string& key,
                                             const std::string& value) {
    environmentVars_[key] = value;
    addHistoryEntry("Environment variable set: " + key + "=" + value);
}

ScriptExecutionResult ShellScriptTask::executeScript(
    const std::string& scriptName,
    const std::unordered_map<std::string, std::string>& args) {
    spdlog::info("Executing shell script: {}", scriptName);
    addHistoryEntry("Executing shell script: " + scriptName);

    auto startTime = std::chrono::steady_clock::now();

    try {
        // Set environment variables
        for (const auto& [key, value] : environmentVars_) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        // Change working directory if specified
        if (!workingDirectory_.empty()) {
            if (chdir(workingDirectory_.c_str()) != 0) {
                throw std::runtime_error("Failed to change directory to: " +
                                         workingDirectory_);
            }
        }

        // Execute script using ScriptManager
        auto result = scriptManager_->runScript(scriptName, args, true);

        auto endTime = std::chrono::steady_clock::now();
        auto executionTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

        if (result) {
            return ScriptExecutionResult{
                .success = result->second == 0,
                .exitCode = result->second,
                .output = result->first,
                .error = result->second != 0 ? result->first : "",
                .executionTime = executionTime};
        } else {
            return ScriptExecutionResult{
                .success = false,
                .exitCode = -1,
                .output = "",
                .error = "Script execution returned no result",
                .executionTime = executionTime};
        }

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto executionTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
                                                                  startTime);

        return ScriptExecutionResult{.success = false,
                                     .exitCode = -1,
                                     .output = "",
                                     .error = e.what(),
                                     .executionTime = executionTime};
    }
}

std::string ShellScriptTask::buildCommand(
    const std::string& scriptName,
    const std::unordered_map<std::string, std::string>& args) {
    std::string command = shellType_ + " " + scriptName;

    for (const auto& [key, value] : args) {
        command += " --" + key + "=" + value;
    }

    return command;
}

// Register with factory
namespace {
static auto shell_script_task_registrar = TaskRegistrar<ShellScriptTask>(
    "shell_script",
    TaskInfo{
        .name = "shell_script",
        .description = "Execute shell/bash scripts with monitoring",
        .category = "automation",
        .requiredParameters = {"scriptName"},
        .parameterSchema =
            json{{"scriptName",
                  {{"type", "string"}, {"description", "Script name or path"}}},
                 {"scriptContent",
                  {{"type", "string"},
                   {"description", "Inline script content"}}},
                 {"shellType",
                  {{"type", "string"},
                   {"description", "Shell interpreter"},
                   {"default", "/bin/bash"}}},
                 {"timeout",
                  {{"type", "number"},
                   {"description", "Timeout in seconds"},
                   {"default", 30}}},
                 {"args",
                  {{"type", "object"},
                   {"description", "Script arguments"},
                   {"default", json::object()}}},
                 {"workingDirectory",
                  {{"type", "string"}, {"description", "Working directory"}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<ShellScriptTask> {
        return std::make_unique<ShellScriptTask>(
            name, config.value("scriptConfigPath", ""));
    });
}  // namespace

}  // namespace lithium::task::script
