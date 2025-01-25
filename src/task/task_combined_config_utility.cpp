#include "task_combined_config_utility.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "config/configor.hpp"
#include "tools/utility.hpp"

namespace lithium::sequencer::task {

TaskCombinedConfigUtility::TaskCombinedConfigUtility(const json& combinedParams)
    : Task("TaskCombinedConfigUtility", [this](const json& params) { execute(params); }),
      combinedParams_(combinedParams) {}

void TaskCombinedConfigUtility::execute(const json& params) {
    LOG_F(INFO, "Executing combined config and utility task with params: {}", params.dump(4));

    // Configuration Management
    std::shared_ptr<ConfigManager> configManager =
        GetPtr<ConfigManager>(Constants::CONFIG_MANAGER).value();

    if (!configManager) {
        LOG_F(ERROR, "ConfigManager not set");
        THROW_RUNTIME_ERROR("ConfigManager not set");
    }

    for (const auto& [key, value] : combinedParams_.items()) {
        configManager->set(key, value);
        LOG_F(INFO, "Config parameter set: {} = {}", key, value.dump());
    }

    // Utility Functions
    std::string functionName = params.at("functionName").get<std::string>();
    std::unordered_map<std::string, std::string> functionArgs;
    if (params.contains("functionArgs")) {
        functionArgs = params.at("functionArgs")
                           .get<std::unordered_map<std::string, std::string>>();
    }

    try {
        auto utilityManager = std::make_shared<lithium::UtilityManager>();
        utilityManager->registerFunction(functionName, combinedParams_);
        auto result = utilityManager->runFunction(functionName, functionArgs);

        if (result) {
            LOG_F(INFO, "Utility function executed successfully: {}", result->first);
        } else {
            LOG_F(ERROR, "Utility function execution failed");
            THROW_RUNTIME_ERROR("Utility function execution failed");
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Utility function execution error: {}", e.what());
        THROW_RUNTIME_ERROR("Utility function execution error: " + std::string(e.what()));
    }

    LOG_F(INFO, "Combined config and utility task completed");
}

}  // namespace lithium::sequencer::task
