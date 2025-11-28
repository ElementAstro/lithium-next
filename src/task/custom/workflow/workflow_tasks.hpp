/**
 * @file workflow_tasks.hpp
 * @brief Astronomical workflow tasks for complete observation sessions.
 *
 * Provides high-level workflow tasks that orchestrate the complete
 * astronomical imaging process from target acquisition to exposure completion.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_WORKFLOW_TASKS_HPP
#define LITHIUM_TASK_WORKFLOW_TASKS_HPP

#include "../common/task_base.hpp"
#include "tools/astronomy/types.hpp"

namespace lithium::task::workflow {

// Import astronomy types
using tools::astronomy::Coordinates;
using tools::astronomy::ExposurePlan;

// ============================================================================
// Target Acquisition Task
// ============================================================================

/**
 * @brief Complete target acquisition workflow task.
 *
 * Executes the full target acquisition sequence:
 * 1. Slew to target coordinates
 * 2. Wait for mount to settle
 * 3. Perform plate solving
 * 4. Center target (if centering required)
 * 5. Start guiding (if guiding required)
 * 6. Perform autofocus (if focus required)
 *
 * Parameters:
 * - target_name: Target identifier
 * - coordinates: {ra, dec, epoch} - Target coordinates
 * - settle_time: Seconds to wait after slew (default: 5)
 * - plate_solve_exposure: Exposure time for plate solving (default: 5s)
 * - centering_tolerance: Centering accuracy in arcseconds (default: 30)
 * - max_centering_attempts: Maximum centering attempts (default: 3)
 * - start_guiding: Whether to start guiding (default: true)
 * - perform_autofocus: Whether to run autofocus (default: true)
 */
class TargetAcquisitionTask : public TaskBase {
public:
    TargetAcquisitionTask() : TaskBase("TargetAcquisition") { setupParameters(); }
    TargetAcquisitionTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "TargetAcquisition"; }
    static std::string getStaticTaskTypeName() { return "TargetAcquisition"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool performSlew(const Coordinates& coords, const json& params);
    bool performPlateSolve(const json& params);
    bool performCentering(const Coordinates& targetCoords, const json& params);
    bool startGuiding(const json& params);
    bool performAutofocus(const json& params);
};

// ============================================================================
// Exposure Sequence Task
// ============================================================================

/**
 * @brief Single target exposure sequence task.
 *
 * Executes a complete exposure sequence for one target:
 * 1. Execute each exposure plan in sequence
 * 2. Handle filter changes
 * 3. Perform dithering between exposures
 * 4. Monitor guiding and refocus as needed
 * 5. Handle meridian flip if required
 *
 * Parameters:
 * - target_name: Target identifier
 * - exposure_plans: Array of {filter, exposure_time, count, binning, dither}
 * - dither_enabled: Enable dithering (default: true)
 * - dither_pixels: Dither amount in pixels (default: 5)
 * - focus_check_interval: Minutes between focus checks (default: 60)
 * - temp_focus_threshold: Temperature change for refocus (default: 1.0°C)
 * - handle_meridian_flip: Auto handle meridian flip (default: true)
 */
class ExposureSequenceTask : public TaskBase {
public:
    ExposureSequenceTask() : TaskBase("ExposureSequence") { setupParameters(); }
    ExposureSequenceTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "ExposureSequence"; }
    static std::string getStaticTaskTypeName() { return "ExposureSequence"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool executeExposurePlan(const ExposurePlan& plan, const json& params);
    bool changeFilter(const std::string& filterName);
    bool takeSingleExposure(double exposureTime, int binning, int gain, int offset);
    bool performDither(double pixels);
    bool checkAndRefocus(const json& params);
    bool handleMeridianFlip(const json& params);
    
    double lastFocusTemp_{0.0};
    std::chrono::steady_clock::time_point lastFocusTime_;
};

// ============================================================================
// Session Task
// ============================================================================

/**
 * @brief Complete observation session task.
 *
 * Manages an entire observation session:
 * 1. Initialize all equipment
 * 2. Perform safety checks
 * 3. Cool camera to target temperature
 * 4. Execute targets in sequence based on priority/observability
 * 5. Handle target transitions
 * 6. End session (warm up, park, disconnect)
 *
 * Parameters:
 * - session_name: Session identifier
 * - targets: Array of target configurations
 * - camera_cooling_temp: Target camera temperature (default: -10)
 * - camera_cooling_duration: Max cooling time in minutes (default: 10)
 * - park_on_completion: Park mount when done (default: true)
 * - warm_camera_on_completion: Warm camera when done (default: true)
 * - shutdown_on_completion: Full shutdown when done (default: false)
 */
class SessionTask : public TaskBase {
public:
    SessionTask() : TaskBase("Session") { setupParameters(); }
    SessionTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "Session"; }
    static std::string getStaticTaskTypeName() { return "Session"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool initializeEquipment(const json& params);
    bool performSafetyChecks();
    bool coolCamera(double targetTemp, int maxDuration);
    bool executeTargets(const json& targets, const json& params);
    bool transitionToTarget(const json& currentTarget, const json& nextTarget);
    bool endSession(const json& params);
};

// ============================================================================
// Safety Check Task
// ============================================================================

/**
 * @brief Safety and weather monitoring task.
 *
 * Performs continuous safety monitoring:
 * - Weather conditions (cloud, humidity, wind)
 * - Device status
 * - Mount limits
 * - Emergency stop conditions
 *
 * Parameters:
 * - check_weather: Enable weather monitoring (default: true)
 * - max_cloud_cover: Maximum cloud cover percentage (default: 50)
 * - max_humidity: Maximum humidity percentage (default: 85)
 * - max_wind_speed: Maximum wind speed in km/h (default: 40)
 * - check_interval: Check interval in seconds (default: 60)
 */
class SafetyCheckTask : public TaskBase {
public:
    SafetyCheckTask() : TaskBase("SafetyCheck") { setupParameters(); }
    SafetyCheckTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "SafetyCheck"; }
    static std::string getStaticTaskTypeName() { return "SafetyCheck"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool checkWeather(const json& params);
    bool checkDeviceStatus();
    bool checkMountLimits();
};

// ============================================================================
// Meridian Flip Task
// ============================================================================

/**
 * @brief Meridian flip handling task.
 *
 * Performs a complete meridian flip sequence:
 * 1. Stop guiding
 * 2. Perform flip
 * 3. Wait for settle
 * 4. Plate solve and re-center
 * 5. Restart guiding
 *
 * Parameters:
 * - target_coordinates: {ra, dec} for re-centering
 * - settle_time: Post-flip settle time (default: 10s)
 * - recenter: Perform plate solve centering after flip (default: true)
 * - restart_guiding: Restart guiding after flip (default: true)
 */
class MeridianFlipTask : public TaskBase {
public:
    MeridianFlipTask() : TaskBase("MeridianFlip") { setupParameters(); }
    MeridianFlipTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "MeridianFlip"; }
    static std::string getStaticTaskTypeName() { return "MeridianFlip"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool stopGuiding();
    bool performFlip();
    bool recenterTarget(const Coordinates& coords);
    bool restartGuiding();
};

// ============================================================================
// Dither Task
// ============================================================================

/**
 * @brief Dithering task for guided imaging.
 *
 * Performs dithering between exposures to reduce fixed pattern noise.
 *
 * Parameters:
 * - dither_pixels: Dither amount in pixels (default: 5.0)
 * - settle_pixels: Settle threshold in pixels (default: 0.5)
 * - settle_time: Minimum settle time in seconds (default: 10)
 * - settle_timeout: Maximum settle wait in seconds (default: 60)
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
    bool sendDitherCommand(double pixels);
    bool waitForSettle(double threshold, int minTime, int timeout);
};

// ============================================================================
// Wait Task
// ============================================================================

/**
 * @brief Configurable wait/delay task.
 *
 * Waits for specified conditions or duration.
 *
 * Parameters:
 * - wait_type: Type of wait (duration, time, altitude, sunset, astronomical_twilight)
 * - duration: Wait duration in seconds (for duration type)
 * - target_time: Target time as timestamp (for time type)
 * - target_altitude: Target altitude for object (for altitude type)
 * - min_altitude: Minimum altitude to wait for (default: 30)
 */
class WaitTask : public TaskBase {
public:
    WaitTask() : TaskBase("Wait") { setupParameters(); }
    WaitTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "Wait"; }
    static std::string getStaticTaskTypeName() { return "Wait"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool waitForDuration(int seconds);
    bool waitForTime(std::chrono::system_clock::time_point targetTime);
    bool waitForAltitude(const Coordinates& coords, double minAltitude);
    bool waitForTwilight(const std::string& twilightType);
};

// ============================================================================
// Calibration Frame Task
// ============================================================================

/**
 * @brief Calibration frame acquisition task.
 *
 * Acquires calibration frames (darks, flats, bias).
 *
 * Parameters:
 * - frame_type: Type of frame (dark, flat, bias)
 * - count: Number of frames to capture
 * - exposure_time: Exposure time (for darks, flats)
 * - binning: Binning mode (default: 1)
 * - filter: Filter name (for flats)
 * - target_adu: Target ADU level for flats (default: 30000)
 */
class CalibrationFrameTask : public TaskBase {
public:
    CalibrationFrameTask() : TaskBase("CalibrationFrame") { setupParameters(); }
    CalibrationFrameTask(const std::string& name, const json& config)
        : TaskBase(name, config) { setupParameters(); }

    static std::string taskName() { return "CalibrationFrame"; }
    static std::string getStaticTaskTypeName() { return "CalibrationFrame"; }

protected:
    void executeImpl(const json& params) override;

private:
    void setupParameters();
    bool captureDarks(int count, double exposure, int binning);
    bool captureFlats(int count, const std::string& filter, int binning, int targetADU);
    bool captureBias(int count, int binning);
    double calculateFlatExposure(const std::string& filter, int targetADU);
};

}  // namespace lithium::task::workflow

#endif  // LITHIUM_TASK_WORKFLOW_TASKS_HPP
