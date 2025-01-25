#include "task_celestial_search.hpp"
#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "client/astrometry/astrometry.hpp"

namespace lithium::sequencer::task {

TaskCelestialSearch::TaskCelestialSearch(const std::string& searchParams)
    : Task("TaskCelestialSearch", [this](const json& params) { execute(params); }),
      searchParams_(searchParams) {}

void TaskCelestialSearch::execute(const json& params) {
    LOG_F(INFO, "Executing celestial search task with params: {}", params.dump(4));

    std::string targetName = params.at("targetName").get<std::string>();
    std::unordered_map<std::string, std::string> searchArgs;
    if (params.contains("searchArgs")) {
        searchArgs = params.at("searchArgs")
                         .get<std::unordered_map<std::string, std::string>>();
    }

    try {
        auto astrometryClient = std::make_shared<lithium::AstrometryClient>();
        astrometryClient->setSearchParams(searchParams_);
        auto result = astrometryClient->search(targetName, searchArgs);

        if (result) {
            LOG_F(INFO, "Celestial search executed successfully: {}", result->first);
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
