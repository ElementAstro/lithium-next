#ifndef LITHIUM_TASK_CELESTIAL_SEARCH_HPP
#define LITHIUM_TASK_CELESTIAL_SEARCH_HPP

#include "task.hpp"
#include <string>
#include <unordered_map>

namespace lithium::sequencer::task {

class TaskCelestialSearch : public Task {
public:
    TaskCelestialSearch(const std::string& searchParams);
    void execute(const json& params) override;

private:
    std::string searchParams_;
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CELESTIAL_SEARCH_HPP
