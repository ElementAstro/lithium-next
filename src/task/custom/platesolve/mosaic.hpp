#ifndef LITHIUM_TASK_PLATESOLVE_MOSAIC_TASK_HPP
#define LITHIUM_TASK_PLATESOLVE_MOSAIC_TASK_HPP

#include "centering.hpp"
#include "common.hpp"

namespace lithium::task::platesolve {

/**
 * @brief Task for automated mosaic imaging with plate solving
 *
 * This task automatically creates a mosaic pattern by moving the telescope
 * to different positions, centering on each position, and taking exposures.
 */
class MosaicTask : public PlateSolveTaskBase {
public:
    MosaicTask();
    ~MosaicTask() override = default;

    /**
     * @brief Execute the mosaic task
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

private:
    /**
     * @brief Execute the mosaic implementation
     * @param params Task parameters
     * @return Mosaic result
     */
    auto executeImpl(const json& params) -> MosaicResult;

    /**
     * @brief Calculate grid positions for mosaic
     * @param config Mosaic configuration
     * @return Vector of grid positions
     */
    auto calculateGridPositions(const MosaicConfig& config)
        -> std::vector<Coordinates>;

    /**
     * @brief Process one mosaic position
     * @param position Position coordinates
     * @param config Mosaic configuration
     * @param positionIndex Current position index
     * @param totalPositions Total number of positions
     * @return Centering result for this position
     */
    auto processPosition(const Coordinates& position,
                         const MosaicConfig& config, int positionIndex,
                         int totalPositions) -> CenteringResult;

    /**
     * @brief Take exposures at current position
     * @param config Mosaic configuration
     * @param positionIndex Current position index
     * @return Number of successful exposures
     */
    auto takeExposuresAtPosition(const MosaicConfig& config, int positionIndex)
        -> int;

    /**
     * @brief Parse configuration from JSON parameters
     * @param params JSON parameters
     * @return Mosaic configuration
     */
    auto parseConfig(const json& params) -> MosaicConfig;

    /**
     * @brief Validate mosaic parameters
     * @param config Configuration to validate
     */
    void validateConfig(const MosaicConfig& config);

    /**
     * @brief Define parameter definitions for the task
     * @param task Task instance to configure
     */
    static void defineParameters(lithium::task::Task& task);

    std::unique_ptr<CenteringTask> centeringTask_;
};

}  // namespace lithium::task::platesolve

#endif  // LITHIUM_TASK_PLATESOLVE_MOSAIC_TASK_HPP
