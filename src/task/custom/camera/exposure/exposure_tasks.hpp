/**
 * @file exposure_tasks.hpp
 * @brief Camera exposure tasks - single frame and sequence exposures
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_CAMERA_EXPOSURE_TASKS_HPP
#define LITHIUM_TASK_CAMERA_EXPOSURE_TASKS_HPP

#include "../common/camera_task_base.hpp"

namespace lithium::task::camera {

// ============================================================================
// Single Exposure Task
// ============================================================================

/**
 * @brief Single exposure task with comprehensive parameter validation
 *
 * Takes a single exposure with configurable parameters including
 * exposure time, gain, offset, binning, and frame type.
 */
class TakeExposureTask : public CameraTaskBase {
public:
    TakeExposureTask() : CameraTaskBase("TakeExposure") { setupParameters(); }
    TakeExposureTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "TakeExposure"; }
    static std::string getTaskTypeName() { return "TakeExposure"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

// ============================================================================
// Multiple Exposure Task
// ============================================================================

/**
 * @brief Multiple exposure sequence task
 *
 * Takes a series of exposures with configurable count and delay between frames.
 */
class TakeManyExposureTask : public CameraTaskBase {
public:
    TakeManyExposureTask() : CameraTaskBase("TakeManyExposure") { setupParameters(); }
    TakeManyExposureTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "TakeManyExposure"; }
    static std::string getTaskTypeName() { return "TakeManyExposure"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

// ============================================================================
// Subframe Exposure Task
// ============================================================================

/**
 * @brief Subframe/ROI exposure task
 *
 * Takes exposures with a defined region of interest for faster readout
 * or targeted observations.
 */
class SubframeExposureTask : public CameraTaskBase {
public:
    SubframeExposureTask() : CameraTaskBase("SubframeExposure") { setupParameters(); }
    SubframeExposureTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "SubframeExposure"; }
    static std::string getTaskTypeName() { return "SubframeExposure"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
};

// ============================================================================
// Smart Exposure Task
// ============================================================================

/**
 * @brief Smart exposure task with automatic optimization
 *
 * Automatically adjusts exposure time to achieve target SNR through
 * iterative exposures and image analysis.
 */
class SmartExposureTask : public CameraTaskBase {
public:
    SmartExposureTask() : CameraTaskBase("SmartExposure") { setupParameters(); }
    SmartExposureTask(const std::string& name, const json& config)
        : CameraTaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "SmartExposure"; }
    static std::string getTaskTypeName() { return "SmartExposure"; }

protected:
    void executeImpl(const json& params) override;
    void validateParams(const json& params) override;

private:
    void setupParameters();
    double calculateOptimalExposure(double currentExposure, double achievedSNR, double targetSNR);
};

}  // namespace lithium::task::camera

#endif  // LITHIUM_TASK_CAMERA_EXPOSURE_TASKS_HPP
