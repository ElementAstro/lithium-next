/**
 * @file astrometry_tasks.hpp
 * @brief Plate solving and astrometric calibration tasks
 */

#ifndef LITHIUM_TASK_CAMERA_ASTROMETRY_TASKS_HPP
#define LITHIUM_TASK_CAMERA_ASTROMETRY_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

/**
 * @brief Plate solving exposure task
 */
class PlateSolveExposureTask : public CameraTaskBase {
public:
    PlateSolveExposureTask() : CameraTaskBase("PlateSolveExposure") { setupParameters(); }
    PlateSolveExposureTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "PlateSolveExposure"; }
    static std::string getTaskTypeName() { return "PlateSolveExposure"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Target centering using plate solve
 */
class CenteringTask : public CameraTaskBase {
public:
    CenteringTask() : CameraTaskBase("Centering") { setupParameters(); }
    CenteringTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "Centering"; }
    static std::string getTaskTypeName() { return "Centering"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_ASTROMETRY_TASKS_HPP
