#ifndef LITHIUM_TASK_CELESTIAL_SEARCH_HPP
#define LITHIUM_TASK_CELESTIAL_SEARCH_HPP

#include <memory>
#include <string>
#include <vector>
#include "target/engine.hpp"
#include "task.hpp"

namespace lithium::sequencer::task {

class TaskCelestialSearch : public Task {
public:
    TaskCelestialSearch();
    void execute(const json& params) override;

private:
    void searchByName(const json& params);
    void searchByType(const json& params);
    void searchByMagnitude(const json& params);
    void getRecommendations(const json& params);
    void updateSearchHistory(const std::string& user, const std::string& query);

    std::shared_ptr<target::SearchEngine> searchEngine_;
    static constexpr int DEFAULT_FUZZY_TOLERANCE = 2;
    static constexpr int DEFAULT_TOP_N = 10;
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CELESTIAL_SEARCH_HPP
