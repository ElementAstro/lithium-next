#ifndef LITHIUM_TASK_GUIDE_CAMERA_TASKS_HPP
#define LITHIUM_TASK_GUIDE_CAMERA_TASKS_HPP

#include "task/task.hpp"

namespace lithium::task::guide {

using json = nlohmann::json;

/**
 * @brief Set camera exposure task.
 * Sets the guide camera exposure time.
 */
class SetCameraExposureTask : public Task {
public:
    SetCameraExposureTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void setCameraExposure(const json& params);
};

/**
 * @brief Get camera exposure task.
 * Gets current guide camera exposure time.
 */
class GetCameraExposureTask : public Task {
public:
    GetCameraExposureTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getCameraExposure(const json& params);
};

/**
 * @brief Capture single frame task.
 * Captures a single frame with the guide camera.
 */
class CaptureSingleFrameTask : public Task {
public:
    CaptureSingleFrameTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void captureSingleFrame(const json& params);
};

/**
 * @brief Start loop task.
 * Starts continuous exposure looping.
 */
class StartLoopTask : public Task {
public:
    StartLoopTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void startLoop(const json& params);
};

/**
 * @brief Get subframe status task.
 * Gets whether subframes are being used.
 */
class GetSubframeStatusTask : public Task {
public:
    GetSubframeStatusTask();

    static auto taskName() -> std::string;
    void execute(const json& params) override;
    static auto createEnhancedTask() -> std::unique_ptr<Task>;

private:
    void getSubframeStatus(const json& params);
};

}  // namespace lithium::task::guide

#endif  // LITHIUM_TASK_GUIDE_CAMERA_TASKS_HPP
