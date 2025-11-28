/**
 * @file guiding_tasks.hpp
 * @brief Autoguiding and dithering tasks
 */

#ifndef LITHIUM_TASK_CAMERA_GUIDING_TASKS_HPP
#define LITHIUM_TASK_CAMERA_GUIDING_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

/**
 * @brief Autoguiding setup and control task
 */
class AutoGuidingTask : public CameraTaskBase {
public:
    AutoGuidingTask() : CameraTaskBase("AutoGuiding") { setupParameters(); }
    AutoGuidingTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "AutoGuiding"; }
    static std::string getTaskTypeName() { return "AutoGuiding"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    bool calibrateGuider(const json& params);
    bool startGuiding(const json& params);
};

/**
 * @brief Guided exposure with autoguiding integration
 */
class GuidedExposureTask : public CameraTaskBase {
public:
    GuidedExposureTask() : CameraTaskBase("GuidedExposure") { setupParameters(); }
    GuidedExposureTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "GuidedExposure"; }
    static std::string getTaskTypeName() { return "GuidedExposure"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    bool waitForGuiding(double timeout);
};

/**
 * @brief Dithering sequence task
 */
class DitherSequenceTask : public CameraTaskBase {
public:
    DitherSequenceTask() : CameraTaskBase("DitherSequence") { setupParameters(); }
    DitherSequenceTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "DitherSequence"; }
    static std::string getTaskTypeName() { return "DitherSequence"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    void performDither(double amount);
    bool waitForSettle(double timeout, double threshold);
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_GUIDING_TASKS_HPP
