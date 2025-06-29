#ifndef LITHIUM_TASK_CONFIG_MANAGEMENT_HPP
#define LITHIUM_TASK_CONFIG_MANAGEMENT_HPP

#include "task.hpp"

namespace lithium::task::task {

class TaskConfigManagement : public Task {
public:
    explicit TaskConfigManagement(const std::string& name);

    void execute(const json& params) override;

private:
    // Original methods
    void handleSetConfig(const json& params);
    void handleGetConfig(const json& params);
    void handleDeleteConfig(const json& params);
    void handleLoadConfig(const json& params);
    void handleSaveConfig(const json& params);
    void handleMergeConfig(const json& params);
    void handleListConfig(const json& params);

    // Extended methods for new ConfigManager features
    void handleValidateConfig(const json& params);
    void handleSchemaConfig(const json& params);
    void handleWatchConfig(const json& params);
    void handleMetricsConfig(const json& params);
    void handleTidyConfig(const json& params);
    void handleAppendConfig(const json& params);
    void handleBatchLoadConfig(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CONFIG_MANAGEMENT_HPP
