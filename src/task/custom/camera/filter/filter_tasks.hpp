/**
 * @file filter_tasks.hpp
 * @brief Filter wheel sequence tasks
 */

#ifndef LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP
#define LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

/**
 * @brief Multi-filter imaging sequence task
 */
class FilterSequenceTask : public CameraTaskBase {
public:
    FilterSequenceTask() : CameraTaskBase("FilterSequence") {
        setupParameters();
    }
    FilterSequenceTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "FilterSequence"; }
    static std::string getTaskTypeName() { return "FilterSequence"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief RGB color imaging sequence
 */
class RGBSequenceTask : public CameraTaskBase {
public:
    RGBSequenceTask() : CameraTaskBase("RGBSequence") { setupParameters(); }
    RGBSequenceTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "RGBSequence"; }
    static std::string getTaskTypeName() { return "RGBSequence"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

/**
 * @brief Narrowband filter imaging sequence (Ha, OIII, SII)
 */
class NarrowbandSequenceTask : public CameraTaskBase {
public:
    NarrowbandSequenceTask() : CameraTaskBase("NarrowbandSequence") {
        setupParameters();
    }
    NarrowbandSequenceTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) {
        setupParameters();
    }

    static std::string taskName() { return "NarrowbandSequence"; }
    static std::string getTaskTypeName() { return "NarrowbandSequence"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_FILTER_TASKS_HPP
