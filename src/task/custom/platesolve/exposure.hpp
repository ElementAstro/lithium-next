#ifndef LITHIUM_TASK_PLATESOLVE_PLATESOLVE_EXPOSURE_HPP
#define LITHIUM_TASK_PLATESOLVE_PLATESOLVE_EXPOSURE_HPP

#include "common.hpp"

namespace lithium::task::platesolve {

/**
 * @brief Task for taking exposures and performing plate solving
 *
 * This task combines camera exposure functionality with plate solving
 * to determine the exact coordinates and orientation of the captured image.
 */
class PlateSolveExposureTask : public PlateSolveTaskBase {
public:
    PlateSolveExposureTask();
    ~PlateSolveExposureTask() override = default;

    /**
     * @brief Execute the plate solve exposure task
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
     * @brief Execute the plate solve exposure implementation
     * @param params Task parameters
     * @return Plate solve result
     */
    auto executeImpl(const json& params) -> PlatesolveResult;

private:
    /**
     * @brief Take exposure for plate solving
     * @param config Plate solve configuration
     * @return Path to captured image
     */
    auto takeExposure(const PlateSolveConfig& config) -> std::string;

    /**
     * @brief Parse configuration from JSON parameters
     * @param params JSON parameters
     * @return Plate solve configuration
     */
    auto parseConfig(const json& params) -> PlateSolveConfig;

    /**
     * @brief Validate plate solve parameters
     * @param config Configuration to validate
     */
    void validateConfig(const PlateSolveConfig& config);

    /**
     * @brief Define parameter definitions for the task
     * @param task Task instance to configure
     */
    static void defineParameters(lithium::task::Task& task);
};

}  // namespace lithium::task::platesolve

#endif  // LITHIUM_TASK_PLATESOLVE_PLATESOLVE_EXPOSURE_HPP
