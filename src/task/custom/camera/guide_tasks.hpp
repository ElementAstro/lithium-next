#ifndef LITHIUM_TASK_CAMERA_GUIDE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_GUIDE_TASKS_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::task::task {

// ==================== 导星和抖动任务 ====================

/**
 * @brief Guided exposure task.
 * Performs guided exposure with autoguiding integration.
 */
class GuidedExposureTask : public Task {
public:
    GuidedExposureTask()
        : Task("GuidedExposure",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;

    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateGuidingParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Dithering sequence task.
 * Performs dithering sequence for improved image quality.
 */
class DitherSequenceTask : public Task {
public:
    DitherSequenceTask()
        : Task("DitherSequence",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateDitheringParameters(const json& params);

private:
    void executeImpl(const json& params);
};

/**
 * @brief Automatic guiding setup task.
 * Sets up and calibrates autoguiding system.
 */
class AutoGuidingTask : public Task {
public:
    AutoGuidingTask()
        : Task("AutoGuiding",
               [this](const json& params) { this->executeImpl(params); }) {}

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateAutoGuidingParameters(const json& params);

private:
    void executeImpl(const json& params);
};

}  // namespace lithium::task::task

#endif  // LITHIUM_TASK_CAMERA_GUIDE_TASKS_HPP
