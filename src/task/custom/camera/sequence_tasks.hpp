#ifndef LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP

#include "../../task.hpp"
#include "custom/factory.hpp"

namespace lithium::sequencer::task {

// ==================== 智能曝光和序列任务 ====================

/**
 * @brief Smart exposure task for automatic exposure optimization.
 */
class SmartExposureTask : public Task {
public:
    SmartExposureTask(const std::string& name, const json& config);

    void execute(const json& params) override;
    static std::string getTaskType() { return "SmartExposure"; }

private:
    void executeImpl(const json& params);
    void validateSmartExposureParameters(const json& params);
};

// ==================== 自动化拍摄序列任务 ====================

/**
 * @brief Deep sky sequence task.
 * Performs automated deep sky imaging sequence.
 */
class DeepSkySequenceTask : public Task {
public:
    DeepSkySequenceTask(const std::string& name, const json& config);

    void execute(const json& params) override;
    static std::string getTaskType() { return "DeepSkySequence"; }

private:
    void executeImpl(const json& params);
    void validateDeepSkyParameters(const json& params);
};

/**
 * @brief Planetary imaging task.
 * Performs high-speed planetary imaging with lucky imaging.
 */
class PlanetaryImagingTask : public Task {
public:
    PlanetaryImagingTask(const std::string& name, const json& config);

    void execute(const json& params) override;
    static std::string getTaskType() { return "PlanetaryImaging"; }

private:
    void executeImpl(const json& params);
    void validatePlanetaryParameters(const json& params);
};

/**
 * @brief Timelapse task.
 * Performs timelapse imaging with specified intervals.
 */
class TimelapseTask : public Task {
public:
    TimelapseTask(const std::string& name, const json& config);

    void execute(const json& params) override;
    static std::string getTaskType() { return "Timelapse"; }

private:
    void executeImpl(const json& params);
    void validateTimelapseParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_SEQUENCE_TASKS_HPP