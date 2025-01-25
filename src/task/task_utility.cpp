#include "task_utility.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "tools/utility.hpp"

namespace lithium::sequencer::task {

TaskUtility::TaskUtility(const json& utilityParams)
    : Task("TaskUtility", [this](const json& params) { execute(params); }),
      utilityParams_(utilityParams) {}

void TaskUtility::execute(const json& params) {
    LOG_F(INFO, "Executing utility task with params: {}", params.dump(4));

    std::string functionName = params.at("functionName").get<std::string>();
    std::unordered_map<std::string, std::string> functionArgs;
    if (params.contains("functionArgs")) {
        functionArgs = params.at("functionArgs")
                           .get<std::unordered_map<std::string, std::string>>();
    }

    try {
        auto utilityManager = std::make_shared<lithium::UtilityManager>();
        utilityManager->registerFunction(functionName, utilityParams_);
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
}

}  // namespace lithium::sequencer::task
