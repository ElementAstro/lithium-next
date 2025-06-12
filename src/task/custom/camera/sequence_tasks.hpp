#ifndef LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP

#include "../../task.hpp"
#include "custom/factory.hpp"

namespace lithium::task::task {

// ==================== 智能曝光和序列任务 ====================

/**
 * @brief Smart exposure task for automatic exposure optimization.
 */
class SmartExposureTask : public Task {
public:
    SmartExposureTask()
        : Task("SmartExposure",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "SmartExposure"; }

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateSmartExposureParameters(const json& params);

private:
    void executeImpl(const json& params);
};

// ==================== 自动化拍摄序列任务 ====================

/**
 * @brief Deep sky sequence task.
 * Performs automated deep sky imaging sequence.
 */
class DeepSkySequenceTask : public Task {
public:
    DeepSkySequenceTask()
        : Task("DeepSkySequence",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "DeepSkySequence"; }

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateDeepSkyParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Planetary imaging task.
 * Performs high-speed planetary imaging with lucky imaging.
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

/**
 * @brief Timelapse task.
 * Performs timelapse imaging with specified intervals.
 */
class TimelapseTask : public Task {
public:
    TimelapseTask()
        : Task("Timelapse",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static std::string getTaskType() { return "Timelapse"; }

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateTimelapseParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP