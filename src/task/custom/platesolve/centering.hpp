#ifndef LITHIUM_TASK_PLATESOLVE_CENTERING_TASK_HPP
#define LITHIUM_TASK_PLATESOLVE_CENTERING_TASK_HPP

#include "common.hpp"
#include "exposure.hpp"

namespace lithium::task::platesolve {

/**
 * @brief Task for automatic telescope centering using plate solving
 *
 * This task iteratively takes exposures and performs plate solving to
 * precisely center a target object in the field of view.
 */
class CenteringTask : public PlateSolveTaskBase {
public:
    CenteringTask();
    ~CenteringTask() override = default;

    /**
     * @brief Execute the centering task
     * @param params JSON parameters for the task
     */
    void execute(const json& params) override;

    /**
     * @brief Get the task name
     * @return Task name string
     */
    static auto taskName() -> std::string;

    /**
     * @brief Create an enhanced task instance with full configuration
     * @return Unique pointer to configured task
     */
    static auto createEnhancedTask() -> std::unique_ptr<lithium::task::Task>;

    /**
     * @brief Execute the centering implementation
     * @param params Task parameters
     * @return Centering result
     */
    auto executeImpl(const json& params) -> CenteringResult;

private:
    /**
     * @brief Perform one centering iteration
     * @param config Centering configuration
     * @param iteration Current iteration number
     * @return Plate solve result for this iteration
     */
    auto performCenteringIteration(const CenteringConfig& config, int iteration)
        -> PlatesolveResult;

    /**
     * @brief Calculate telescope correction required
     * @param currentPos Current telescope position
     * @param targetPos Target position
     * @return Correction coordinates
     */
    auto calculateCorrection(const Coordinates& currentPos,
                             const Coordinates& targetPos) -> Coordinates;

    /**
     * @brief Apply telescope correction by slewing
     * @param correction Correction coordinates
     */
    void applyTelecopeCorrection(const Coordinates& correction);

    /**
     * @brief Parse configuration from JSON parameters
     * @param params JSON parameters
     * @return Centering configuration
     */
    auto parseConfig(const json& params) -> CenteringConfig;

    /**
     * @brief Validate centering parameters
     * @param config Configuration to validate
     */
    void validateConfig(const CenteringConfig& config);

    /**
     * @brief Define parameter definitions for the task
     * @param task Task instance to configure
     */
    static void defineParameters(lithium::task::Task& task);

    std::unique_ptr<PlateSolveExposureTask> plateSolveTask_;
};

}  // namespace lithium::task::platesolve

#endif  // LITHIUM_TASK_PLATESOLVE_CENTERING_TASK_HPP
