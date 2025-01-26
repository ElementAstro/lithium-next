#include "config_task.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "config/configor.hpp"
#include "constant/constant.hpp"
#include "matchit/matchit.h"

namespace lithium::sequencer::task {

TaskConfigManagement::TaskConfigManagement(const std::string& name)
    : Task(name, [this](const json& params) { execute(params); }) {
    LOG_F(INFO, "TaskConfigManagement created with name: {}", name);
    // 添加参数定义
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

    // 设置任务优先级
    setPriority(8);

    // 设置错误处理回调
    setExceptionCallback([this](const std::exception& e) {
        setErrorType(TaskErrorType::SystemError);
        addHistoryEntry("Error occurred: " + std::string(e.what()));
        LOG_F(ERROR, "Exception caught in TaskConfigManagement: {}", e.what());
    });
}

void TaskConfigManagement::execute(const json& params) {
    LOG_F(INFO, "Executing ConfigManagement task: {}", params.dump());

    if (!validateParams(params)) {
        LOG_F(WARNING, "Parameter validation failed for: {}", params.dump());
        return;
    }

    try {
        using namespace matchit;
        using namespace std::string_literals;
        const std::string operation = params["operation"];

        LOG_F(DEBUG, "Operation to execute: {}", operation);

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
        LOG_F(INFO, "Operation {} completed successfully", operation);

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::SystemError);
        LOG_F(ERROR, "Failed to execute config operation: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to execute config operation: {}", e.what());
    }
}

void TaskConfigManagement::handleSetConfig(const json& params) {
    LOG_F(INFO, "Handling set config with params: {}", params.dump());
    if (!validateSetParams(params)) {
        LOG_F(WARNING, "Set config parameter validation failed for: {}",
              params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];
    const json& value = params["value"];

    LOG_F(DEBUG, "Setting config at path: {} with value: {}", keyPath,
          value.dump());

    if (!configManager->set(keyPath, value)) {
        LOG_F(ERROR, "Failed to set config at path: {}", keyPath);
        THROW_RUNTIME_ERROR("Failed to set config at path: {}", keyPath);
    }

    addHistoryEntry("Set config at path: " + keyPath);
    LOG_F(INFO, "Set config at path: {}", keyPath);
}

void TaskConfigManagement::handleGetConfig(const json& params) {
    LOG_F(INFO, "Handling get config with params: {}", params.dump());
    if (!validateGetParams(params)) {
        LOG_F(WARNING, "Get config parameter validation failed for: {}",
              params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];

    LOG_F(DEBUG, "Getting config at path: {}", keyPath);

    auto value = configManager->get(keyPath);
    if (!value) {
        LOG_F(ERROR, "Failed to get config at path: {}", keyPath);
        THROW_RUNTIME_ERROR("Failed to get config at path: {}", keyPath);
    }

    addHistoryEntry("Retrieved config from path: " + keyPath);
    LOG_F(INFO, "Retrieved config from path: {}", keyPath);
}

void TaskConfigManagement::handleDeleteConfig(const json& params) {
    LOG_F(INFO, "Handling delete config with params: {}", params.dump());
    if (!validateDeleteParams(params)) {
        LOG_F(WARNING, "Delete config parameter validation failed for: {}",
              params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& keyPath = params["key_path"];

    LOG_F(DEBUG, "Deleting config at path: {}", keyPath);

    if (!configManager->remove(keyPath)) {
        LOG_F(ERROR, "Failed to delete config at path: {}", keyPath);
        THROW_RUNTIME_ERROR("Failed to delete config at path: {}", keyPath);
    }

    addHistoryEntry("Deleted config at path: " + keyPath);
    LOG_F(INFO, "Deleted config at path: {}", keyPath);
}

void TaskConfigManagement::handleLoadConfig(const json& params) {
    LOG_F(INFO, "Handling load config with params: {}", params.dump());
    if (!validateLoadParams(params)) {
        LOG_F(WARNING, "Load config parameter validation failed for: {}",
              params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& filePath = params["file_path"];

    bool recursive = params.value("recursive", false);

    LOG_F(DEBUG, "Loading config from file: {} with recursive: {}", filePath,
          recursive);

    bool success;
    if (params.value("is_directory", false)) {
        success = configManager->loadFromDir(filePath, recursive);
    } else {
        success = configManager->loadFromFile(filePath);
    }

    if (!success) {
        LOG_F(ERROR, "Failed to load config from: {}", filePath);
        THROW_RUNTIME_ERROR("Failed to load config from: {}", filePath);
    }

    addHistoryEntry("Loaded config from: " + filePath);
    LOG_F(INFO, "Loaded config from: {}", filePath);
}

void TaskConfigManagement::handleSaveConfig(const json& params) {
    LOG_F(INFO, "Handling save config with params: {}", params.dump());
    if (!validateSaveParams(params)) {
        LOG_F(WARNING, "Save config parameter validation failed for: {}",
              params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const std::string& filePath = params["file_path"];

    LOG_F(DEBUG, "Saving config to file: {}", filePath);

    bool success;
    if (params.value("save_all", false)) {
        success = configManager->saveAll(filePath);
    } else {
        success = configManager->save(filePath);
    }

    if (!success) {
        LOG_F(ERROR, "Failed to save config to: {}", filePath);
        THROW_RUNTIME_ERROR("Failed to save config to: {}", filePath);
    }

    addHistoryEntry("Saved config to: " + filePath);
    LOG_F(INFO, "Saved config to: {}", filePath);
}

void TaskConfigManagement::handleMergeConfig(const json& params) {
    LOG_F(INFO, "Handling merge config with params: {}", params.dump());
    if (!validateMergeParams(params)) {
        LOG_F(WARNING, "Merge config parameter validation failed for: {}",
              params.dump());
        return;
    }

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();
    const json& mergeData = params["merge_data"];

    LOG_F(DEBUG, "Merging config data: {}", mergeData.dump());

    configManager->merge(mergeData);

    addHistoryEntry("Merged config data successfully");
    LOG_F(INFO, "Merged config data successfully");
}

void TaskConfigManagement::handleListConfig(const json& params) {
    LOG_F(INFO, "Handling list config with params: {}", params.dump());
    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    if (params.value("list_files", false)) {
        auto paths = configManager->listPaths();
        addHistoryEntry("Listed " + std::to_string(paths.size()) +
                        " config files");
        LOG_F(INFO, "Listed {} config files", paths.size());
    } else {
        auto keys = configManager->getKeys();
        addHistoryEntry("Listed " + std::to_string(keys.size()) +
                        " config keys");
        LOG_F(INFO, "Listed {} config keys", keys.size());
    }
}

bool TaskConfigManagement::validateSetParams(const json& params) {
    LOG_F(DEBUG, "Validating set params: {}", params.dump());
    if (!params.contains("key_path") || !params.contains("value")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing required parameters for set operation");
        LOG_F(WARNING, "Missing required parameters for set operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateGetParams(const json& params) {
    LOG_F(DEBUG, "Validating get params: {}", params.dump());
    if (!params.contains("key_path")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing key_path parameter for get operation");
        LOG_F(WARNING, "Missing key_path parameter for get operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateDeleteParams(const json& params) {
    LOG_F(DEBUG, "Validating delete params: {}", params.dump());
    if (!params.contains("key_path")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing key_path parameter for delete operation");
        LOG_F(WARNING, "Missing key_path parameter for delete operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateLoadParams(const json& params) {
    LOG_F(DEBUG, "Validating load params: {}", params.dump());
    if (!params.contains("file_path")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing file_path parameter for load operation");
        LOG_F(WARNING, "Missing file_path parameter for load operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateSaveParams(const json& params) {
    LOG_F(DEBUG, "Validating save params: {}", params.dump());
    if (!params.contains("file_path")) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry("Missing file_path parameter for save operation");
        LOG_F(WARNING, "Missing file_path parameter for save operation");
        return false;
    }
    return true;
}

bool TaskConfigManagement::validateMergeParams(const json& params) {
    LOG_F(DEBUG, "Validating merge params: {}", params.dump());
    if (!params.contains("merge_data") || !params["merge_data"].is_object()) {
        setErrorType(TaskErrorType::InvalidParameter);
        addHistoryEntry(
            "Missing or invalid merge_data parameter for merge operation");
        LOG_F(WARNING,
              "Missing or invalid merge_data parameter for merge operation");
        return false;
    }
    return true;
}

}  // namespace lithium::sequencer::task
