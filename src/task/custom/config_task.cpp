#include "config_task.hpp"
#include <spdlog/spdlog.h>
#include "atom/function/global_ptr.hpp"
#include "config/configor.hpp"
#include "constant/constant.hpp"
#include "factory.hpp"
#include "matchit/matchit.h"

namespace lithium::sequencer::task {

TaskConfigManagement::TaskConfigManagement(const std::string& name)
    : Task(name, [this](const json& params) { execute(params); }) {
    spdlog::info("TaskConfigManagement created with name: {}", name);
    // Add parameter definitions
    addParamDefinition("operation", "string", true, nullptr,
                       "Operation type: set/get/delete/load/save/merge/list");
    addParamDefinition("key_path", "string", false, nullptr,
                       "Configuration key path");
    addParamDefinition("value", "object", false, nullptr,
                       "Configuration value to set");
    addParamDefinition("file_path", "string", false, nullptr,
                       "File path for load/save operations");
    addParamDefinition("merge_data", "object", false, nullptr,
                       "Configuration data to merge");

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
    spdlog::info("Executing ConfigManagement task: {}", params.dump());

    if (!validateParams(params)) {
        spdlog::warn("Parameter validation failed for: {}", params.dump());
        return;
    }

    try {
        using namespace matchit;
        using namespace std::string_literals;
        const std::string operation = params["operation"];

        spdlog::debug("Operation to execute: {}", operation);

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
                    THROW_INVALID_CONFIG_EXCEPTION("Unknown operation: " +
                                                   operation);
                });

        addHistoryEntry("Operation " + operation + " completed successfully");
        spdlog::info("Operation {} completed successfully", operation);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::SystemError);
        spdlog::error("Failed to execute config operation: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to execute config operation: {}", e.what());
    }
}

void TaskConfigManagement::handleSetConfig(const json& params) {
    spdlog::info("Handling set config with params: {}", params.dump());
    if (!validateSetParams(params)) {
        spdlog::warn("Set config parameter validation failed for: {}",
                     params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];
    const json& value = params["value"];

    spdlog::debug("Setting config at path: {} with value: {}", keyPath,
                  value.dump());

    if (!configManager->set(keyPath, value)) {
        spdlog::error("Failed to set config at path: {}", keyPath);
        THROW_RUNTIME_ERROR("Failed to set config at path: {}", keyPath);
    }

    addHistoryEntry("Set config at path: " + keyPath);
    spdlog::info("Set config at path: {}", keyPath);
}

void TaskConfigManagement::handleGetConfig(const json& params) {
    spdlog::info("Handling get config with params: {}", params.dump());
    if (!validateGetParams(params)) {
        spdlog::warn("Get config parameter validation failed for: {}",
                     params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];

    spdlog::debug("Getting config at path: {}", keyPath);

    auto value = configManager->get(keyPath);
    if (!value) {
        spdlog::error("Failed to get config at path: {}", keyPath);
        THROW_RUNTIME_ERROR("Failed to get config at path: {}", keyPath);
    }

    addHistoryEntry("Retrieved config from path: " + keyPath);
    spdlog::info("Retrieved config from path: {}", keyPath);
}

void TaskConfigManagement::handleDeleteConfig(const json& params) {
    spdlog::info("Handling delete config with params: {}", params.dump());
    if (!validateDeleteParams(params)) {
        spdlog::warn("Delete config parameter validation failed for: {}",
                     params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];

    spdlog::debug("Deleting config at path: {}", keyPath);

    if (!configManager->remove(keyPath)) {
        spdlog::error("Failed to delete config at path: {}", keyPath);
        THROW_RUNTIME_ERROR("Failed to delete config at path: {}", keyPath);
    }

    addHistoryEntry("Deleted config at path: " + keyPath);
    spdlog::info("Deleted config at path: {}", keyPath);
}

void TaskConfigManagement::handleLoadConfig(const json& params) {
    spdlog::info("Handling load config with params: {}", params.dump());
    if (!validateLoadParams(params)) {
        spdlog::warn("Load config parameter validation failed for: {}",
                     params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& filePath = params["file_path"];

    bool recursive = params.value("recursive", false);

    spdlog::debug("Loading config from file: {} with recursive: {}", filePath,
                  recursive);

    bool success;
    if (params.value("is_directory", false)) {
        success = configManager->loadFromDir(filePath, recursive);
    } else {
        success = configManager->loadFromFile(filePath);
    }

    if (!success) {
        spdlog::error("Failed to load config from: {}", filePath);
        THROW_RUNTIME_ERROR("Failed to load config from: {}", filePath);
    }

    addHistoryEntry("Loaded config from: " + filePath);
    spdlog::info("Loaded config from: {}", filePath);
}

void TaskConfigManagement::handleSaveConfig(const json& params) {
    spdlog::info("Handling save config with params: {}", params.dump());
    if (!validateSaveParams(params)) {
        spdlog::warn("Save config parameter validation failed for: {}",
                     params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& filePath = params["file_path"];

    spdlog::debug("Saving config to file: {}", filePath);

    bool success;
    if (params.value("save_all", false)) {
        success = configManager->saveAll(filePath);
    } else {
        success = configManager->save(filePath);
    }

    if (!success) {
        spdlog::error("Failed to save config to: {}", filePath);
        THROW_RUNTIME_ERROR("Failed to save config to: {}", filePath);
    }

    addHistoryEntry("Saved config to: " + filePath);
    spdlog::info("Saved config to: {}", filePath);
}

void TaskConfigManagement::handleMergeConfig(const json& params) {
    spdlog::info("Handling merge config with params: {}", params.dump());
    if (!validateMergeParams(params)) {
        spdlog::warn("Merge config parameter validation failed for: {}",
                     params.dump());
        return;
    }

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

    if (params.value("list_files", false)) {
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

bool TaskConfigManagement::validateSetParams(const json& params) {
    spdlog::debug("Validating set params: {}", params.dump());
    if (!params.contains("key_path") || !params.contains("value")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing required parameters for set operation");
        spdlog::warn("Missing required parameters for set operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateGetParams(const json& params) {
    spdlog::debug("Validating get params: {}", params.dump());
    if (!params.contains("key_path")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing key_path parameter for get operation");
        spdlog::warn("Missing key_path parameter for get operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateDeleteParams(const json& params) {
    spdlog::debug("Validating delete params: {}", params.dump());
    if (!params.contains("key_path")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing key_path parameter for delete operation");
        spdlog::warn("Missing key_path parameter for delete operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateLoadParams(const json& params) {
    spdlog::debug("Validating load params: {}", params.dump());
    if (!params.contains("file_path")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing file_path parameter for load operation");
        spdlog::warn("Missing file_path parameter for load operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateSaveParams(const json& params) {
    spdlog::debug("Validating save params: {}", params.dump());
    if (!params.contains("file_path")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing file_path parameter for save operation");
        spdlog::warn("Missing file_path parameter for save operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateMergeParams(const json& params) {
    spdlog::debug("Validating merge params: {}", params.dump());
    if (!params.contains("merge_data") || !params["merge_data"].is_object()) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry(
            "Missing or invalid merge_data parameter for merge operation");
        spdlog::warn(
            "Missing or invalid merge_data parameter for merge operation");
        return false;
    }
    return true;
}

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
                  {{"type", "object"},
                   {"description", "Configuration value to set"}}},
                 {"file_path",
                  {{"type", "string"},
                   {"description", "File path for load/save operations"}}},
                 {"merge_data",
                  {{"type", "object"},
                   {"description",
                    "Configuration data to merge with existing settings"}}},
                 {"backup",
                  {{"type", "boolean"},
                   {"description",
                    "Create backup before modifying configuration"},
                   {"default", true}}},
                 {"validate",
                  {{"type", "boolean"},
                   {"description", "Validate configuration after changes"},
                   {"default", true}}}},
        .version = "1.0.0",
        .dependencies = {},
        .isEnabled = true},
    [](const std::string& name,
       const json& config) -> std::unique_ptr<TaskConfigManagement> {
        return std::make_unique<TaskConfigManagement>(name);
    });
}  // namespace

}  // namespace lithium::sequencer::task
