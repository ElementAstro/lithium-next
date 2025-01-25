#include "task_combined_script_celestial.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "script/sheller.hpp"
#include "client/astrometry/astrometry.hpp"

namespace lithium::sequencer::task {

TaskCombinedScriptCelestial::TaskCombinedScriptCelestial(const std::string& scriptPath, const std::string& searchParams)
    : Task("TaskCombinedScriptCelestial", [this](const json& params) { execute(params); }),
      scriptPath_(scriptPath), searchParams_(searchParams) {}

void TaskCombinedScriptCelestial::execute(const json& params) {
    LOG_F(INFO, "Executing combined script and celestial search task with params: {}", params.dump(4));

    // Execute script part
    std::string scriptName = params.at("scriptName").get<std::string>();
    std::unordered_map<std::string, std::string> scriptArgs;
    if (params.contains("scriptArgs")) {
        scriptArgs = params.at("scriptArgs")
                         .get<std::unordered_map<std::string, std::string>>();
    }

    try {
        auto scriptManager = std::make_shared<lithium::ScriptManager>();
        scriptManager->registerScript(scriptName, scriptPath_);
        auto scriptResult = scriptManager->runScript(scriptName, scriptArgs);

        if (scriptResult) {
            LOG_F(INFO, "Script executed successfully: {}", scriptResult->first);
        } else {
            LOG_F(ERROR, "Script execution failed");
            THROW_RUNTIME_ERROR("Script execution failed");
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Script execution error: {}", e.what());
        THROW_RUNTIME_ERROR("Script execution error: " + std::string(e.what()));
    }

    // Execute celestial search part
    std::string targetName = params.at("targetName").get<std::string>();
    std::unordered_map<std::string, std::string> searchArgs;
    if (params.contains("searchArgs")) {
        searchArgs = params.at("searchArgs")
                         .get<std::unordered_map<std::string, std::string>>();
    }

    try {
        auto astrometryClient = std::make_shared<lithium::AstrometryClient>();
        astrometryClient->setSearchParams(searchParams_);
        auto searchResult = astrometryClient->search(targetName, searchArgs);

        if (searchResult) {
            LOG_F(INFO, "Celestial search executed successfully: {}", searchResult->first);
        } else {
            LOG_F(ERROR, "Celestial search execution failed");
            THROW_RUNTIME_ERROR("Celestial search execution failed");
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Celestial search execution error: {}", e.what());
        THROW_RUNTIME_ERROR("Celestial search execution error: " + std::string(e.what()));
    }
}

}  // namespace lithium::sequencer::task
