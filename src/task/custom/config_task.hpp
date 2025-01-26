#ifndef LITHIUM_TASK_CONFIG_MANAGEMENT_HPP
#define LITHIUM_TASK_CONFIG_MANAGEMENT_HPP

#include "task.hpp"

namespace lithium::sequencer::task {

class TaskConfigManagement : public Task {
public:
    explicit TaskConfigManagement(const std::string& name);

    void execute(const json& params) override;

private:
    // 处理不同的配置管理操作
    void handleSetConfig(const json& params);
    void handleGetConfig(const json& params);
    void handleDeleteConfig(const json& params);
    void handleLoadConfig(const json& params);
    void handleSaveConfig(const json& params);
    void handleMergeConfig(const json& params);
    void handleListConfig(const json& params);

    // 参数验证
    bool validateSetParams(const json& params);
    bool validateGetParams(const json& params);
    bool validateDeleteParams(const json& params);
    bool validateLoadParams(const json& params);
    bool validateSaveParams(const json& params);
    bool validateMergeParams(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CONFIG_MANAGEMENT_HPP
