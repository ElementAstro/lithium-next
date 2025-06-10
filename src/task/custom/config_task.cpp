#include "config_task.hpp"
#include <spdlog/spdlog.h>
#include "atom/function/global_ptr.hpp"
#include "config/configor.hpp"
#include "constant/constant.hpp"
#include "factory.hpp"
#include "matchit/matchit.h"

namespace lithium::sequencer::task {

TaskConfigManagement::TaskConfigManagement(const std::string& name)
    : Task(name, [this](const json& params) { this->execute(params); }) {
    spdlog::info("TaskConfigManagement created with name: {}", name);

    // Add parameter definitions using the new interface
    addParamDefinition("operation", "string", true, nullptr,
                       "Operation type: set/get/delete/load/save/merge/list");
    addParamDefinition("key_path", "string", false, nullptr,
                       "Configuration key path");
    addParamDefinition("value", "any", false, nullptr,
                       "Configuration value to set");
    addParamDefinition("file_path", "string", false, nullptr,
                       "File path for load/save operations");
    addParamDefinition("merge_data", "object", false, nullptr,
                       "Configuration data to merge");
    addParamDefinition("recursive", "boolean", false, json(false),
                       "Recursive loading for directories");
    addParamDefinition("is_directory", "boolean", false, json(false),
                       "Whether the file_path is a directory");
    addParamDefinition("save_all", "boolean", false, json(false),
                       "Save all configuration data");
    addParamDefinition("list_files", "boolean", false, json(false),
                       "List config files instead of keys");

    // Set task priority
    setPriority(8);

    // Set exception callback
    setExceptionCallback([this](const std::exception& e) {
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Error occurred: " + std::string(e.what()));
        spdlog::error("Exception caught in TaskConfigManagement: {}", e.what());
    });
}

void TaskConfigManagement::execute(const json& params) {
    auto startTime = std::chrono::steady_clock::now();

    spdlog::info("Executing ConfigManagement task: {}", params.dump());
    addHistoryEntry("Started execution with params: " + params.dump());

    // Validate parameters using the built-in validation
    if (!validateParams(params)) {
        setErrorType(TaskErrorType::InvalidParameter);
        auto errors = getParamErrors();
        for (const auto& error : errors) {
            spdlog::warn("Parameter validation error: {}", error);
            addHistoryEntry("Parameter validation error: " + error);
        }
        return;
    }

    try {
        using namespace matchit;
        using namespace std::string_literals;
        const std::string operation = params["operation"];

        spdlog::debug("Operation to execute: {}", operation);
        addHistoryEntry("Executing operation: " + operation);

        match(operation)(
            pattern | "set" = [&] { handleSetConfig(params); },
            pattern | "get" = [&] { handleGetConfig(params); },
            pattern | "delete" = [&] { handleDeleteConfig(params); },
            pattern | "load" = [&] { handleLoadConfig(params); },
            pattern | "save" = [&] { handleSaveConfig(params); },
            pattern | "merge" = [&] { handleMergeConfig(params); },
            pattern | "list" = [&] { handleListConfig(params); },
            pattern | _ =
                [&] {
                    setErrorType(TaskErrorType::InvalidParameter);
                    std::string errorMsg = "Unknown operation: " + operation;
                    addHistoryEntry(errorMsg);
                    throw std::invalid_argument(errorMsg);
                });

        addHistoryEntry("Operation " + operation + " completed successfully");
        spdlog::info("Operation {} completed successfully", operation);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::SystemError);
        std::string errorMsg =
            "Failed to execute config operation: " + std::string(e.what());
        addHistoryEntry(errorMsg);
        spdlog::error("{}", errorMsg);
        throw;
    }

    // Record execution time
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    // Note: executionTime_ is private, so we rely on the base class to track
    // this

    addHistoryEntry("Task execution completed in " +
                    std::to_string(duration.count()) + "ms");
}

void TaskConfigManagement::handleSetConfig(const json& params) {
    spdlog::info("Handling set config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];
    const json& value = params["value"];

    spdlog::debug("Setting config at path: {} with value: {}", keyPath,
                  value.dump());

    if (!configManager->set(keyPath, value)) {
        setErrorType(TaskErrorType::SystemError);
        std::string errorMsg = "Failed to set config at path: " + keyPath;
        addHistoryEntry(errorMsg);
        spdlog::error("{}", errorMsg);
        throw std::runtime_error(errorMsg);
    }

    addHistoryEntry("Set config at path: " + keyPath);
    spdlog::info("Set config at path: {}", keyPath);
}

void TaskConfigManagement::handleGetConfig(const json& params) {
    spdlog::info("Handling get config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];

    spdlog::debug("Getting config at path: {}", keyPath);

    auto value = configManager->get(keyPath);
    if (!value) {
        setErrorType(TaskErrorType::SystemError);
        std::string errorMsg = "Failed to get config at path: " + keyPath;
        addHistoryEntry(errorMsg);
        spdlog::error("{}", errorMsg);
        throw std::runtime_error(errorMsg);
    }

    addHistoryEntry("Retrieved config from path: " + keyPath + " = " +
                    value->dump());
    spdlog::info("Retrieved config from path: {}", keyPath);
}

void TaskConfigManagement::handleDeleteConfig(const json& params) {
    spdlog::info("Handling delete config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];

    spdlog::debug("Deleting config at path: {}", keyPath);

    if (!configManager->remove(keyPath)) {
        setErrorType(TaskErrorType::SystemError);
        std::string errorMsg = "Failed to delete config at path: " + keyPath;
        addHistoryEntry(errorMsg);
        spdlog::error("{}", errorMsg);
        throw std::runtime_error(errorMsg);
    }

    addHistoryEntry("Deleted config at path: " + keyPath);
    spdlog::info("Deleted config at path: {}", keyPath);
}

void TaskConfigManagement::handleLoadConfig(const json& params) {
    spdlog::info("Handling load config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& filePath = params["file_path"];

    bool recursive = params.value("recursive", false);
    bool isDirectory = params.value("is_directory", false);

    spdlog::debug(
        "Loading config from file: {} with recursive: {}, isDirectory: {}",
        filePath, recursive, isDirectory);

    bool success;
    if (isDirectory) {
        success = configManager->loadFromDir(filePath, recursive);
    } else {
        success = configManager->loadFromFile(filePath);
    }

    if (!success) {
        setErrorType(TaskErrorType::SystemError);
        std::string errorMsg = "Failed to load config from: " + filePath;
        addHistoryEntry(errorMsg);
        spdlog::error("{}", errorMsg);
        throw std::runtime_error(errorMsg);
    }

    addHistoryEntry("Loaded config from: " + filePath);
    spdlog::info("Loaded config from: {}", filePath);
}

void TaskConfigManagement::handleSaveConfig(const json& params) {
    spdlog::info("Handling save config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& filePath = params["file_path"];
    bool saveAll = params.value("save_all", false);

    spdlog::debug("Saving config to file: {} with saveAll: {}", filePath,
                  saveAll);

    bool success;
    if (saveAll) {
        success = configManager->saveAll(filePath);
    } else {
        success = configManager->save(filePath);
    }

    if (!success) {
        setErrorType(TaskErrorType::SystemError);
        std::string errorMsg = "Failed to save config to: " + filePath;
        addHistoryEntry(errorMsg);
        spdlog::error("{}", errorMsg);
        throw std::runtime_error(errorMsg);
    }

    addHistoryEntry("Saved config to: " + filePath);
    spdlog::info("Saved config to: {}", filePath);
}

void TaskConfigManagement::handleMergeConfig(const json& params) {
    spdlog::info("Handling merge config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const json& mergeData = params["merge_data"];

    spdlog::debug("Merging config data: {}", mergeData.dump());

    configManager->merge(mergeData);

    addHistoryEntry("Merged config data successfully");
    spdlog::info("Merged config data successfully");
}

void TaskConfigManagement::handleListConfig(const json& params) {
    spdlog::info("Handling list config with params: {}", params.dump());
    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    bool listFiles = params.value("list_files", false);

    if (listFiles) {
        auto paths = configManager->listPaths();
        addHistoryEntry("Listed " + std::to_string(paths.size()) +
                        " config files");
        spdlog::info("Listed {} config files", paths.size());
    } else {
        auto keys = configManager->getKeys();
        addHistoryEntry("Listed " + std::to_string(keys.size()) +
                        " config keys");
        spdlog::info("Listed {} config keys", keys.size());
    }
}

// Remove individual validation methods since we now use the built-in parameter
// validation The parameter definitions with required flags handle validation
// automatically

// Register ConfigTask with factory
namespace {
static auto config_task_registrar = TaskRegistrar<TaskConfigManagement>(
    "config_task",
    TaskInfo{
        .name = "config_task",
        .description = "Manage configuration settings and parameters",
        .category = "configuration",
        .requiredParameters = {"operation"},
        .parameterSchema =
            json{{"operation",
                  {{"type", "string"},
                   {"description", "Configuration operation to perform"},
                   {"enum", json::array({"set", "get", "delete", "load", "save",
                                         "merge", "list"})}}},
                 {"key_path",
                  {{"type", "string"},
                   {"description",
                    "Configuration key path using dot notation"}}},
                 {"value",
                  {{"type", "any"},
                   {"description", "Configuration value to set"}}},
                 {"file_path",
                  {{"type", "string"},
                   {"description", "File path for load/save operations"}}},
                 {"merge_data",
                  {{"type", "object"},
                   {"description",
                    "Configuration data to merge with existing settings"}}},
                 {"recursive",
                  {{"type", "boolean"},
                   {"description", "Recursive loading for directories"},
                   {"default", false}}},
                 {"is_directory",
                  {{"type", "boolean"},
                   {"description", "Whether the file_path is a directory"},
                   {"default", false}}},
                 {"save_all",
                  {{"type", "boolean"},
                   {"description", "Save all configuration data"},
                   {"default", false}}},
                 {"list_files",
                  {{"type", "boolean"},
                   {"description", "List config files instead of keys"},
                   {"default", false}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<TaskConfigManagement> {
        return std::make_unique<TaskConfigManagement>(name);
    });
}  // namespace

}  // namespace lithium::sequencer::task
