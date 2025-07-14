#ifndef LITHIUM_TASK_ADVANCED_PLANETARY_IMAGING_TASK_HPP
#define LITHIUM_TASK_ADVANCED_PLANETARY_IMAGING_TASK_HPP

#include "../../task.hpp"
#include "../factory.hpp"

namespace lithium::task::task {

/**
 * @brief Planetary imaging task.
 *
 * Performs high-speed planetary imaging with lucky imaging support
 * for capturing planetary details through atmospheric turbulence.
 */
class PlanetaryImagingTask : public Task {
public:
    PlanetaryImagingTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "PlanetaryImaging"; }

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validatePlanetaryParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_ADVANCED_PLANETARY_IMAGING_TASK_HPP
