#ifndef LITHIUM_TASK_CAMERA_SEQUENCE_ANALYSIS_TASKS_HPP
#define LITHIUM_TASK_CAMERA_SEQUENCE_ANALYSIS_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::task::task {

/**
 * @brief Advanced imaging sequence task.
 * Manages complex multi-target imaging sequences with automatic optimization.
 */
class AdvancedImagingSequenceTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateSequenceParameters(const json& params);
    static void handleSequenceError(Task& task, const std::exception& e);
};

/**
 * @brief Image quality analysis task.
 * Analyzes captured images for quality metrics and optimization feedback.
 */
class ImageQualityAnalysisTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAnalysisParameters(const json& params);
};

/**
 * @brief Adaptive exposure optimization task.
 * Automatically optimizes exposure parameters based on conditions.
 */
class AdaptiveExposureOptimizationTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateOptimizationParameters(const json& params);
};

/**
 * @brief Star analysis and tracking task.
 * Analyzes star field for tracking quality and guiding performance.
 */
class StarAnalysisTrackingTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateStarAnalysisParameters(const json& params);
};

/**
 * @brief Weather adaptive scheduling task.
 * Adapts imaging schedule based on weather conditions and forecasts.
 */
class WeatherAdaptiveSchedulingTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateWeatherParameters(const json& params);
};

/**
 * @brief Intelligent target selection task.
 * Automatically selects optimal targets based on conditions and equipment.
 */
class IntelligentTargetSelectionTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateTargetSelectionParameters(const json& params);
};

/**
 * @brief Data pipeline management task.
 * Manages the image processing and analysis pipeline.
 */
class DataPipelineManagementTask : public Task {
public:
    using Task::Task;

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validatePipelineParameters(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_SEQUENCE_ANALYSIS_TASKS_HPP
