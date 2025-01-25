#include "task_config_management.hpp"
#include "config/configor.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::sequencer::task {

TaskConfigManagement::TaskConfigManagement(const json& configParams)
    : Task("ConfigManagement", [this](const json& params) { execute(params); }),
      configParams_(configParams) {}

void TaskConfigManagement::execute(const json& params) {
    LOG_F(INFO, "Executing ConfigManagement task with params: {}", params.dump(4));

    std::shared_ptr<ConfigManager> configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    if (!configManager) {
        LOG_F(ERROR, "ConfigManager not set");
        THROW_RUNTIME_ERROR("ConfigManager not set");
    }

    for (const auto& [key, value] : configParams_.items()) {
        configManager->set(key, value);
        LOG_F(INFO, "Config parameter set: {} = {}", key, value.dump());
    }

    LOG_F(INFO, "ConfigManagement task completed");
}

}  // namespace lithium::sequencer::task
