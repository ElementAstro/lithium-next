/**
 * @file guider_tasks.hpp
 * @brief Guider-related tasks for autoguiding and dithering
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_GUIDER_TASKS_HPP
#define LITHIUM_TASK_GUIDER_TASKS_HPP

#include "../common/task_base.hpp"
#include "../common/types.hpp"
#include "../common/validation.hpp"

namespace lithium::task::guider {

using namespace lithium::task;

/**
 * @brief Start autoguiding task
 *
 * Initializes and starts the autoguiding system, including
 * calibration if necessary.
 */
class StartGuidingTask : public TaskBase {
public:
    StartGuidingTask() : TaskBase("StartGuiding") { setupParameters(); }
    StartGuidingTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "StartGuiding"; }
    static std::string getStaticTaskTypeName() { return "StartGuiding"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool calibrateGuider(const json& params);
    bool startGuiding(const json& params);
};

/**
 * @brief Stop autoguiding task
 */
class StopGuidingTask : public TaskBase {
public:
    StopGuidingTask() : TaskBase("StopGuiding") { setupParameters(); }
    StopGuidingTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "StopGuiding"; }
    static std::string getStaticTaskTypeName() { return "StopGuiding"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Pause autoguiding task
 */
class PauseGuidingTask : public TaskBase {
public:
    PauseGuidingTask() : TaskBase("PauseGuiding") { setupParameters(); }
    PauseGuidingTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "PauseGuiding"; }
    static std::string getStaticTaskTypeName() { return "PauseGuiding"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Resume autoguiding task
 */
class ResumeGuidingTask : public TaskBase {
public:
    ResumeGuidingTask() : TaskBase("ResumeGuiding") { setupParameters(); }
    ResumeGuidingTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "ResumeGuiding"; }
    static std::string getStaticTaskTypeName() { return "ResumeGuiding"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Dither the guiding position
 *
 * Performs a small offset in the guiding position and waits
 * for the system to settle before continuing imaging.
 */
class DitherTask : public TaskBase {
public:
    DitherTask() : TaskBase("Dither") { setupParameters(); }
    DitherTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "Dither"; }
    static std::string getStaticTaskTypeName() { return "Dither"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    void performDither(double amount);
    bool waitForSettle(double timeout, double threshold);
};

/**
 * @brief Guided exposure sequence with dithering
 *
 * Takes a series of guided exposures with optional dithering
 * between frames.
 */
class GuidedExposureSequenceTask : public TaskBase {
public:
    GuidedExposureSequenceTask() : TaskBase("GuidedExposureSequence") { setupParameters(); }
    GuidedExposureSequenceTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "GuidedExposureSequence"; }
    static std::string getStaticTaskTypeName() { return "GuidedExposureSequence"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool waitForGuiding(double timeout);
};

/**
 * @brief Calibrate the guider
 */
class CalibrateGuiderTask : public TaskBase {
public:
    CalibrateGuiderTask() : TaskBase("CalibrateGuider") { setupParameters(); }
    CalibrateGuiderTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "CalibrateGuider"; }
    static std::string getStaticTaskTypeName() { return "CalibrateGuider"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
};

}  // namespace lithium::task::guider

#endif  // LITHIUM_TASK_GUIDER_TASKS_HPP
