/**
 * @file settings_tasks.hpp
 * @brief Camera settings and preview tasks
 */

#ifndef LITHIUM_TASK_CAMERA_SETTINGS_TASKS_HPP
#define LITHIUM_TASK_CAMERA_SETTINGS_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

/**
 * @brief Camera settings configuration task
 */
class CameraSettingsTask : public CameraTaskBase {
public:
    CameraSettingsTask() : CameraTaskBase("CameraSettings") { setupParameters(); }
    CameraSettingsTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "CameraSettings"; }
    static std::string getTaskTypeName() { return "CameraSettings"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Camera preview task for quick live view
 */
class CameraPreviewTask : public CameraTaskBase {
public:
    CameraPreviewTask() : CameraTaskBase("CameraPreview") { setupParameters(); }
    CameraPreviewTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "CameraPreview"; }
    static std::string getTaskTypeName() { return "CameraPreview"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_SETTINGS_TASKS_HPP
