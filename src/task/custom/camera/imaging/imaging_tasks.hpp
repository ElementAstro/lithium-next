/**
 * @file imaging_tasks.hpp
 * @brief Advanced imaging tasks (deep sky, planetary, timelapse, mosaic)
 */

#ifndef LITHIUM_TASK_CAMERA_IMAGING_TASKS_HPP
#define LITHIUM_TASK_CAMERA_IMAGING_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

/**
 * @brief Deep sky imaging sequence task
 */
class DeepSkySequenceTask : public CameraTaskBase {
public:
    DeepSkySequenceTask() : CameraTaskBase("DeepSkySequence") {
        setupParameters();
    }
    DeepSkySequenceTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "DeepSkySequence"; }
    static std::string getTaskTypeName() { return "DeepSkySequence"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief High-speed planetary imaging task
 */
class PlanetaryImagingTask : public CameraTaskBase {
public:
    PlanetaryImagingTask() : CameraTaskBase("PlanetaryImaging") {
        setupParameters();
    }
    PlanetaryImagingTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "PlanetaryImaging"; }
    static std::string getTaskTypeName() { return "PlanetaryImaging"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Timelapse imaging task
 */
class TimelapseTask : public CameraTaskBase {
public:
    TimelapseTask() : CameraTaskBase("Timelapse") { setupParameters(); }
    TimelapseTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "Timelapse"; }
    static std::string getTaskTypeName() { return "Timelapse"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Mosaic imaging task with plate solving
 */
class MosaicTask : public CameraTaskBase {
public:
    MosaicTask() : CameraTaskBase("Mosaic") { setupParameters(); }
    MosaicTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "Mosaic"; }
    static std::string getTaskTypeName() { return "Mosaic"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    void calculatePanelPositions(int rows, int cols, double overlap,
                                 double fovWidth, double fovHeight);
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_IMAGING_TASKS_HPP
