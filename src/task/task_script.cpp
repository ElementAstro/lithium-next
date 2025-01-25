#include "task_script.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "script/sheller.hpp"

namespace lithium::sequencer::task {

TaskScript::TaskScript(const std::string& scriptPath)
    : Task("TaskScript", [this](const json& params) { execute(params); }),
      scriptPath_(scriptPath) {}

void TaskScript::execute(const json& params) {
    LOG_F(INFO, "Executing script task with params: {}", params.dump(4));

    std::string scriptName = params.at("scriptName").get<std::string>();
    std::unordered_map<std::string, std::string> scriptArgs;
    if (params.contains("scriptArgs")) {
        scriptArgs = params.at("scriptArgs")
                         .get<std::unordered_map<std::string, std::string>>();
    }

    try {
        auto scriptManager = std::make_shared<lithium::ScriptManager>();
        scriptManager->registerScript(scriptName, scriptPath_);
        auto result = scriptManager->runScript(scriptName, scriptArgs);

        if (result) {
            LOG_F(INFO, "Script executed successfully: {}", result->first);
        } else {
            LOG_F(ERROR, "Script execution failed");
            THROW_RUNTIME_ERROR("Script execution failed");
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Script execution error: {}", e.what());
        THROW_RUNTIME_ERROR("Script execution error: " + std::string(e.what()));
    }
}

}  // namespace lithium::sequencer::task
