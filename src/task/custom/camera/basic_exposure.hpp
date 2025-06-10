#ifndef LITHIUM_TASK_CAMERA_BASIC_EXPOSURE_HPP
#define LITHIUM_TASK_CAMERA_BASIC_EXPOSURE_HPP

#include "../../task.hpp"
#include "common.hpp"

namespace lithium::sequencer::task {

/**
 * @brief Derived class for creating TakeExposure tasks.
 * Basic single exposure task with comprehensive parameter validation.
 */
class TakeExposureTask : public TaskCreator<TakeExposureTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateCameraParameters(const json& params);
    static void handleCameraError(Task& task, const std::exception& e);
};

/**
 * @brief Derived class for creating TakeManyExposure tasks.
 * Sequence of exposures with delay support and comprehensive error handling.
 */
class TakeManyExposureTask : public TaskCreator<TakeManyExposureTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    
private:
    static void defineParameters(Task& task);
    static void validateSequenceParameters(const json& params);
    static void handleSequenceError(Task& task, const std::exception& e);
};

/**
 * @brief Derived class for creating SubframeExposure tasks.
 * Subframe/ROI exposure with precise coordinate validation.
 */
class SubframeExposureTask : public TaskCreator<SubframeExposureTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateSubframeParameters(const json& params);
};

/**
 * @brief Derived class for creating CameraSettings tasks.
 * Camera configuration management with validation.
 */
class CameraSettingsTask : public TaskCreator<CameraSettingsTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validateSettingsParameters(const json& params);
};

/**
 * @brief Derived class for creating CameraPreview tasks.
 * Quick preview exposures with optimized settings.
 */
class CameraPreviewTask : public TaskCreator<CameraPreviewTask> {
public:
    static auto taskName() -> std::string;
    static void execute(const json& params);
    
    // Enhanced functionality using new Task base class features
    static auto createEnhancedTask() -> std::unique_ptr<Task>;
    static void defineParameters(Task& task);
    static void validatePreviewParameters(const json& params);
};

}  // namespace lithium::sequencer::task

#endif  // LITHIUM_TASK_CAMERA_BASIC_EXPOSURE_HPP
