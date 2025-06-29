#ifndef LITHIUM_TASK_FILTER_CALIBRATION_TASK_HPP
#define LITHIUM_TASK_FILTER_CALIBRATION_TASK_HPP

#include <atomic>
#include <chrono>

#include "base.hpp"

namespace lithium::task::filter {

/**
 * @enum CalibrationType
 * @brief Types of calibration frames.
 */
enum class CalibrationType {
    Dark,  ///< Dark calibration frames
    Flat,  ///< Flat field calibration frames
    Bias,  ///< Bias calibration frames
    All    ///< All calibration types
};

/**
 * @struct CalibrationSettings
 * @brief Settings for filter calibration.
 */
struct CalibrationSettings {
    CalibrationType type{
        CalibrationType::All};         ///< Type of calibration to perform
    std::vector<std::string> filters;  ///< Filters to calibrate

    // Dark frame settings
    std::vector<double> darkExposures{1.0, 60.0,
                                      300.0};  ///< Dark exposure times
    int darkCount{10};  ///< Number of dark frames per exposure

    // Flat frame settings
    double flatExposure{1.0};     ///< Flat field exposure time
    int flatCount{10};            ///< Number of flat frames per filter
    bool autoFlatExposure{true};  ///< Automatically determine flat exposure
    double targetADU{25000.0};    ///< Target ADU for flat frames

    // Bias frame settings
    int biasCount{50};  ///< Number of bias frames

    int gain{100};              ///< Camera gain setting
    int offset{10};             ///< Camera offset setting
    double temperature{-10.0};  ///< Target camera temperature
};

/**
 * @class FilterCalibrationTask
 * @brief Task for performing filter wheel calibration sequences.
 *
 * This task handles the creation of calibration frames (darks, flats, bias)
 * for specific filters. It supports automated flat field exposure
 * determination, temperature-controlled dark frames, and comprehensive
 * calibration workflows.
 */
class FilterCalibrationTask : public BaseFilterTask {
public:
    /**
     * @brief Constructs a FilterCalibrationTask.
     * @param name Optional custom name for the task (defaults to
     * "FilterCalibration").
     */
    FilterCalibrationTask(const std::string& name = "FilterCalibration");

    /**
     * @brief Executes the filter calibration with the provided parameters.
     * @param params JSON object containing calibration configuration.
     *
     * Parameters:
     * - calibration_type (string): Type of calibration ("dark", "flat", "bias",
     * "all")
     * - filters (array): List of filters to calibrate
     * - dark_exposures (array): Dark frame exposure times (default: [1.0, 60.0,
     * 300.0])
     * - dark_count (number): Number of dark frames per exposure (default: 10)
     * - flat_exposure (number): Flat field exposure time (default: 1.0)
     * - flat_count (number): Number of flat frames per filter (default: 10)
     * - auto_flat_exposure (boolean): Auto-determine flat exposure (default:
     * true)
     * - target_adu (number): Target ADU for flat frames (default: 25000.0)
     * - bias_count (number): Number of bias frames (default: 50)
     * - gain (number): Camera gain (default: 100)
     * - offset (number): Camera offset (default: 10)
     * - temperature (number): Target camera temperature (default: -10.0)
     */
    void execute(const json& params) override;

    /**
     * @brief Executes calibration with specific settings.
     * @param settings CalibrationSettings with configuration.
     * @return True if calibration completed successfully, false otherwise.
     */
    bool executeCalibration(const CalibrationSettings& settings);

    /**
     * @brief Captures dark calibration frames.
     * @param exposures List of exposure times for dark frames.
     * @param count Number of frames per exposure time.
     * @param gain Camera gain setting.
     * @param offset Camera offset setting.
     * @param temperature Target camera temperature.
     * @return True if successful, false otherwise.
     */
    bool captureDarkFrames(const std::vector<double>& exposures, int count,
                           int gain, int offset, double temperature);

    /**
     * @brief Captures flat field calibration frames for specified filters.
     * @param filters List of filters to capture flats for.
     * @param exposure Exposure time for flat frames.
     * @param count Number of frames per filter.
     * @param gain Camera gain setting.
     * @param offset Camera offset setting.
     * @param autoExposure Whether to automatically determine exposure.
     * @param targetADU Target ADU level for flat frames.
     * @return True if successful, false otherwise.
     */
    bool captureFlatFrames(const std::vector<std::string>& filters,
                           double exposure, int count, int gain, int offset,
                           bool autoExposure = true,
                           double targetADU = 25000.0);

    /**
     * @brief Captures bias calibration frames.
     * @param count Number of bias frames to capture.
     * @param gain Camera gain setting.
     * @param offset Camera offset setting.
     * @param temperature Target camera temperature.
     * @return True if successful, false otherwise.
     */
    bool captureBiasFrames(int count, int gain, int offset, double temperature);

    /**
     * @brief Automatically determines optimal flat field exposure time.
     * @param filterName The filter to determine exposure for.
     * @param targetADU Target ADU level.
     * @param gain Camera gain setting.
     * @param offset Camera offset setting.
     * @return Optimal exposure time in seconds.
     */
    double determineOptimalFlatExposure(const std::string& filterName,
                                        double targetADU, int gain, int offset);

    /**
     * @brief Gets the progress of the current calibration.
     * @return Progress as a percentage (0.0 to 100.0).
     */
    double getCalibrationProgress() const;

    /**
     * @brief Gets the estimated remaining time for calibration.
     * @return Estimated remaining time in seconds.
     */
    std::chrono::seconds getEstimatedRemainingTime() const;

private:
    /**
     * @brief Sets up parameter definitions specific to filter calibration.
     */
    void setupCalibrationDefaults();

    /**
     * @brief Converts calibration type string to enum.
     * @param typeStr String representation of calibration type.
     * @return CalibrationType enum value.
     */
    CalibrationType stringToCalibationType(const std::string& typeStr) const;

    /**
     * @brief Updates the calibration progress.
     * @param completedFrames Number of completed frames.
     * @param totalFrames Total number of frames in calibration.
     */
    void updateProgress(int completedFrames, int totalFrames);

    /**
     * @brief Waits for camera to reach target temperature.
     * @param targetTemperature Target temperature in Celsius.
     * @param timeoutMinutes Maximum wait time in minutes.
     * @return True if temperature reached, false if timeout.
     */
    bool waitForTemperature(double targetTemperature, int timeoutMinutes = 30);

    CalibrationSettings currentSettings_;  ///< Current calibration settings
    std::atomic<double> calibrationProgress_{
        0.0};  ///< Current calibration progress
    std::chrono::steady_clock::time_point
        calibrationStartTime_;  ///< Start time
    int completedFrames_{0};    ///< Number of completed frames
    int totalFrames_{0};        ///< Total frames in calibration
};

}  // namespace lithium::task::filter

#endif  // LITHIUM_TASK_FILTER_CALIBRATION_TASK_HPP
