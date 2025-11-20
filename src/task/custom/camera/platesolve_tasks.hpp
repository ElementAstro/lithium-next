#ifndef LITHIUM_TASK_CAMERA_PLATESOLVE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_PLATESOLVE_TASKS_HPP

#include "../../task.hpp"

namespace lithium::task::task {

// ==================== 板面解析集成任务 ====================

/**
 * @brief Plate solving exposure task.
 * Takes an exposure and performs plate solving for astrometry.
 */
class PlateSolveExposureTask : public Task {
public:
    PlateSolveExposureTask()
        : Task("PlateSolveExposure",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validatePlateSolveParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Automatic centering task.
 * Centers the target object in the field of view using plate solving.
 */
class CenteringTask : public Task {
public:
    CenteringTask()
        : Task("Centering",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateCenteringParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Mosaic imaging task.
 * Performs automated mosaic imaging with plate solving and positioning.
 */
class MosaicTask : public Task {
public:
    MosaicTask()
        : Task("Mosaic",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateMosaicParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_PLATESOLVE_TASKS_HPP
