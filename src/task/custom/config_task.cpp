#include "config_task.hpp"
#include <spdlog/spdlog.h>
#include "atom/function/global_ptr.hpp"
#include "config/configor.hpp"
#include "constant/constant.hpp"
#include "factory.hpp"
#include "matchit/matchit.h"

namespace lithium::task::task {

TaskConfigManagement::TaskConfigManagement(const std::string& name)
    : Task(name, [this](const json& params) { this->execute(params); }) {
    spdlog::info("TaskConfigManagement created with name: {}", name);

    // Add parameter definitions using the new interface
    addParamDefinition("operation", "string", true, nullptr,
                       "Operation type: "
                       "set/get/delete/load/save/merge/list/validate/schema/"
                       "watch/metrics/tidy");
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

    // New parameters for extended functionality
    addParamDefinition("schema", "object", false, nullptr,
                       "JSON schema for validation");
    addParamDefinition("schema_path", "string", false, nullptr,
                       "Path to configuration section for schema operations");
    addParamDefinition("enable_watch", "boolean", false, json(false),
                       "Enable/disable file watching");
    addParamDefinition("file_paths", "array", false, nullptr,
                       "Array of file paths for batch operations");
    addParamDefinition("append_value", "boolean", false, json(false),
                       "Append value to array instead of setting");
    addParamDefinition("validate_all", "boolean", false, json(false),
                       "Validate entire configuration");
    addParamDefinition("reset_metrics", "boolean", false, json(false),
                       "Reset performance metrics");

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
            pattern | "validate" = [&] { handleValidateConfig(params); },
            pattern | "schema" = [&] { handleSchemaConfig(params); },
            pattern | "watch" = [&] { handleWatchConfig(params); },
            pattern | "metrics" = [&] { handleMetricsConfig(params); },
            pattern | "tidy" = [&] { handleTidyConfig(params); },
            pattern | "append" = [&] { handleAppendConfig(params); },
            pattern | "batch_load" = [&] { handleBatchLoadConfig(params); },
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

// New extended methods

void TaskConfigManagement::handleValidateConfig(const json& params) {
    spdlog::info("Handling validate config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    bool validateAll = params.value("validate_all", false);

    try {
        ValidationResult result;
        if (validateAll) {
            result = configManager->validateAll();
            addHistoryEntry("Validated entire configuration");
        } else {
            if (!params.contains("key_path")) {
                throw std::invalid_argument(
                    "key_path required for specific validation");
            }
            const std::string& keyPath = params["key_path"];
            result = configManager->validate(keyPath);
            addHistoryEntry("Validated config at path: " + keyPath);
        }

        if (!result.hasErrors()) {
            spdlog::info("Configuration validation passed");
        } else {
            spdlog::warn("Configuration validation failed: {}",
                         result.getErrorMessage());
            addHistoryEntry("Validation failed: " + result.getErrorMessage());
        }

    } catch (const std::exception& e) {
        setErrorType(TaskErrorType::SystemError);
        std::string errorMsg = "Validation error: " + std::string(e.what());
        addHistoryEntry(errorMsg);
        spdlog::error("{}", errorMsg);
        throw;
    }
}

void TaskConfigManagement::handleSchemaConfig(const json& params) {
    spdlog::info("Handling schema config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    if (!params.contains("schema_path")) {
        throw std::invalid_argument(
            "schema_path required for schema operations");
    }

    const std::string& schemaPath = params["schema_path"];

    if (params.contains("schema")) {
        // Set schema from JSON object
        const json& schema = params["schema"];
        if (!configManager->setSchema(schemaPath, schema)) {
            throw std::runtime_error("Failed to set schema for path: " +
                                     schemaPath);
        }
        addHistoryEntry("Set schema for path: " + schemaPath);
        spdlog::info("Set schema for path: {}", schemaPath);

    } else if (params.contains("file_path")) {
        // Load schema from file
        const std::string& filePath = params["file_path"];
        if (!configManager->loadSchema(schemaPath, filePath)) {
            throw std::runtime_error("Failed to load schema from: " + filePath);
        }
        addHistoryEntry("Loaded schema from: " + filePath +
                        " for path: " + schemaPath);
        spdlog::info("Loaded schema from: {} for path: {}", filePath,
                     schemaPath);

    } else {
        throw std::invalid_argument(
            "Either 'schema' or 'file_path' required for schema operations");
    }
}

void TaskConfigManagement::handleWatchConfig(const json& params) {
    spdlog::info("Handling watch config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    if (!params.contains("file_path")) {
        throw std::invalid_argument("file_path required for watch operations");
    }

    const std::string& filePath = params["file_path"];
    bool enableWatch = params.value("enable_watch", true);

    bool success;
    if (enableWatch) {
        success = configManager->enableAutoReload(filePath);
        addHistoryEntry("Enabled auto-reload for: " + filePath);
        spdlog::info("Enabled auto-reload for: {}", filePath);
    } else {
        success = configManager->disableAutoReload(filePath);
        addHistoryEntry("Disabled auto-reload for: " + filePath);
        spdlog::info("Disabled auto-reload for: {}", filePath);
    }

    if (!success) {
        throw std::runtime_error("Failed to modify watch status for: " +
                                 filePath);
    }
}

void TaskConfigManagement::handleMetricsConfig(const json& params) {
    spdlog::info("Handling metrics config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    bool resetMetrics = params.value("reset_metrics", false);

    if (resetMetrics) {
        configManager->resetMetrics();
        addHistoryEntry("Reset performance metrics");
        spdlog::info("Reset performance metrics");
    } else {
        auto metrics = configManager->getMetrics();

        std::string metricsStr = "Performance Metrics - ";
        metricsStr +=
            "Operations: " + std::to_string(metrics.total_operations) + ", ";
        metricsStr +=
            "Cache Hits: " + std::to_string(metrics.cache_hits) + ", ";
        metricsStr +=
            "Cache Misses: " + std::to_string(metrics.cache_misses) + ", ";
        metricsStr += "Avg Access Time: " +
                      std::to_string(metrics.average_access_time_ms) + "ms";

        addHistoryEntry("Retrieved metrics: " + metricsStr);
        spdlog::info("Retrieved metrics: {}", metricsStr);
    }
}

void TaskConfigManagement::handleTidyConfig(const json& params) {
    spdlog::info("Handling tidy config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    configManager->tidy();

    addHistoryEntry("Configuration cleanup completed");
    spdlog::info("Configuration cleanup completed");
}

void TaskConfigManagement::handleAppendConfig(const json& params) {
    spdlog::info("Handling append config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    if (!params.contains("key_path") || !params.contains("value")) {
        throw std::invalid_argument(
            "key_path and value required for append operation");
    }

    const std::string& keyPath = params["key_path"];
    const json& value = params["value"];

    if (!configManager->append(keyPath, value)) {
        throw std::runtime_error("Failed to append value to array at path: " +
                                 keyPath);
    }

    addHistoryEntry("Appended value to array at path: " + keyPath);
    spdlog::info("Appended value to array at path: {}", keyPath);
}

void TaskConfigManagement::handleBatchLoadConfig(const json& params) {
    spdlog::info("Handling batch load config with params: {}", params.dump());

    auto configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    if (!params.contains("file_paths") || !params["file_paths"].is_array()) {
        throw std::invalid_argument(
            "file_paths array required for batch load operation");
    }

    const json& filePathsJson = params["file_paths"];
    std::vector<std::filesystem::path> filePaths;

    for (const auto& pathJson : filePathsJson) {
        if (pathJson.is_string()) {
            filePaths.emplace_back(pathJson.get<std::string>());
        }
    }

    if (filePaths.empty()) {
        throw std::invalid_argument(
            "No valid file paths provided for batch load");
    }

    size_t loadedCount = configManager->loadFromFiles(filePaths);

    std::string resultMsg = "Batch loaded " + std::to_string(loadedCount) +
                            " out of " + std::to_string(filePaths.size()) +
                            " files";
    addHistoryEntry(resultMsg);
    spdlog::info("{}", resultMsg);

    if (loadedCount != filePaths.size()) {
        spdlog::warn("Some files failed to load in batch operation");
    }
}

// Register ConfigTask with factory
namespace {
static auto config_task_registrar =
    TaskRegistrar<TaskConfigManagement>(
        "config_task",
        TaskInfo{
            .name = "config_task",
            .description = "Advanced configuration management with validation, "
                           "caching, and monitoring",
            .category = "configuration",
            .requiredParameters = {"operation"},
            .parameterSchema =
                json{
                    {"operation",
                     {{"type", "string"},
                      {"description", "Configuration operation to perform"},
                      {"enum", json::array({"set", "get", "delete", "load",
                                            "save", "merge", "list", "validate",
                                            "schema", "watch", "metrics",
                                            "tidy", "append", "batch_load"})}}},
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
                    {"file_paths",
                     {{"type", "array"},
                      {"description",
                       "Array of file paths for batch operations"}}},
                    {"merge_data",
                     {{"type", "object"},
                      {"description",
                       "Configuration data to merge with existing settings"}}},
                    {"schema",
                     {{"type", "object"},
                      {"description", "JSON schema for validation"}}},
                    {"schema_path",
                     {{"type", "string"},
                      {"description",
                       "Path to configuration section for schema operations"}}},
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
                      {"default", false}}},
                    {"enable_watch",
                     {{"type", "boolean"},
                      {"description", "Enable/disable file watching"},
                      {"default", false}}},
                    {"validate_all",
                     {{"type", "boolean"},
                      {"description", "Validate entire configuration"},
                      {"default", false}}},
                    {"reset_metrics",
                     {{"type", "boolean"},
                      {"description", "Reset performance metrics"},
                      {"default", false}}}},
            .version = "2.0.0",
            .dependencies = {},
            .isEnabled = true},
        [](const std::string& name,
           const json& config) -> std::unique_ptr<TaskConfigManagement> {
            return std::make_unique<TaskConfigManagement>(name);
        });
}  // namespace

}  // namespace lithium::task::task
