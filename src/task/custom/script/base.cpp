#include "base.hpp"

#include "spdlog/spdlog.h"

namespace lithium::task::script {

BaseScriptTask::BaseScriptTask(const std::string& name,
                               const std::string& scriptConfigPath)
    : Task(name, [this](const json& params) { execute(params); }),
      scriptConfigPath_(scriptConfigPath) {
    scriptManager_ = std::make_shared<ScriptManager>();

    if (!scriptConfigPath.empty()) {
        try {
            scriptAnalyzer_ =
                std::make_unique<ScriptAnalyzer>(scriptConfigPath);
        } catch (const std::exception& e) {
            spdlog::warn("Failed to initialize script analyzer: {}", e.what());
        }
    }

    setupScriptDefaults();
}

void BaseScriptTask::setupScriptDefaults() {
    // Define common script parameters
    addParamDefinition("scriptName", "string", true, nullptr,
                       "Name of the script to execute");
    addParamDefinition("scriptContent", "string", false, nullptr,
                       "Inline script content");
    addParamDefinition("timeout", "number", false, 30,
                       "Execution timeout in seconds");
    addParamDefinition("args", "object", false, json::object(),
                       "Script arguments");
    addParamDefinition("workingDirectory", "string", false, "",
                       "Working directory for script execution");

    // Set task defaults
    setTimeout(std::chrono::seconds(300));
    setPriority(5);
    setLogLevel(2);

    // Set exception callback
    setExceptionCallback([this](const std::exception& e) {
        spdlog::error("Script task exception: {}", e.what());
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Exception: " + std::string(e.what()));
    });
}

void BaseScriptTask::execute(const json& params) {
    addHistoryEntry("Starting script task execution");

    try {
        validateScriptParams(params);

        std::string scriptName = params["scriptName"].get<std::string>();
        auto args = params.value("args", json::object())
                        .get<std::unordered_map<std::string, std::string>>();

        // Register script if content provided
        if (params.contains("scriptContent")) {
            std::string content = params["scriptContent"].get<std::string>();
            if (!content.empty()) {
                scriptManager_->registerScript(scriptName, content);
            }
        }

        // Execute the script
        auto result = executeScript(scriptName, args);

        if (!result.success) {
            setErrorType(TaskErrorType::SystemError);
            throw std::runtime_error("Script execution failed: " +
                                     result.error);
        }

        addHistoryEntry("Script executed successfully: " + scriptName);

    } catch (const std::exception& e) {
        handleScriptError(params.value("scriptName", "unknown"), e.what());
        throw;
    }
}

void BaseScriptTask::validateScriptParams(const json& params) {
    if (!validateParams(params)) {
        auto errors = getParamErrors();
        std::string errorMsg = "Parameter validation failed: ";
        for (const auto& error : errors) {
            errorMsg += error + "; ";
        }
        setErrorType(TaskErrorType::InvalidParameter);
        throw std::invalid_argument(errorMsg);
    }
}

ScriptType BaseScriptTask::detectScriptType(const std::string& content) {
    if (content.find("#!/usr/bin/env python") != std::string::npos ||
        content.find("import ") != std::string::npos ||
        content.find("def ") != std::string::npos) {
        return ScriptType::Python;
    }

    return ScriptType::Shell;  // Default to shell
}

void BaseScriptTask::handleScriptError(const std::string& scriptName,
                                       const std::string& error) {
    spdlog::error("Script error [{}]: {}", scriptName, error);
    setErrorType(TaskErrorType::SystemError);
    addHistoryEntry("Script error (" + scriptName + "): " + error);
}

}  // namespace lithium::task::script
