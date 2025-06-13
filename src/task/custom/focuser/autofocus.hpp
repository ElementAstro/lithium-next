#ifndef LITHIUM_TASK_FOCUSER_AUTOFOCUS_TASK_HPP
#define LITHIUM_TASK_FOCUSER_AUTOFOCUS_TASK_HPP

#include <vector>
#include "base.hpp"

namespace lithium::task::focuser {

/**
 * @enum AutofocusAlgorithm
 * @brief Different autofocus algorithms available.
 */
enum class AutofocusAlgorithm {
    VCurve,         ///< V-curve fitting algorithm
    HyperbolicFit,  ///< Hyperbolic curve fitting
    Polynomial,     ///< Polynomial curve fitting
    SimpleSweep     ///< Simple linear sweep
};

/**
 * @enum AutofocusMode
 * @brief Different autofocus operation modes.
 */
enum class AutofocusMode {
    Full,           ///< Full autofocus with coarse and fine sweeps
    Quick,          ///< Quick autofocus with reduced steps
    Fine,           ///< Fine tuning around current position
    Starless,       ///< Optimized for starless conditions (planetary)
    HighPrecision   ///< High precision with multiple iterations
};

/**
 * @class AutofocusTask
 * @brief Task for automatic focusing using star analysis.
 *
 * This task performs automatic focusing by moving the focuser through
 * a range of positions, analyzing star quality at each position, and
 * determining the optimal focus position using curve fitting algorithms.
 */
class AutofocusTask : public BaseFocuserTask {
public:
    /**
     * @brief Constructs an AutofocusTask.
     * @param name Optional custom name for the task.
     */
    AutofocusTask(const std::string& name = "Autofocus");

    /**
     * @brief Executes the autofocus with the provided parameters.
     * @param params JSON object containing autofocus configuration.
     *
     * Parameters:
     * - mode (string): "full", "quick", "fine", "starless", "high_precision"
     * (default: "full")
     * - algorithm (string): "vcurve", "hyperbolic", "polynomial", "simple"
     * (default: "vcurve")
     * - exposure_time (double): Exposure time for focus frames in seconds
     * (default: auto-selected based on mode)
     * - step_size (int): Step size between focus positions (default: auto)
     * - max_steps (int): Maximum number of steps from center (default: auto)
     * - tolerance (double): Focus tolerance for convergence (default: 0.1)
     * - binning (int): Camera binning factor (default: 1)
     * - backlash_compensation (bool): Enable backlash compensation (default: true)
     * - temperature_compensation (bool): Enable temperature compensation
     * (default: false)
     * - min_stars (int): Minimum stars required for analysis (default: 5)
     * - max_iterations (int): Max iterations for high precision mode (default: 3)
     */
    void execute(const json& params) override;

    /**
     * @brief Performs autofocus with specified algorithm.
     * @param algorithm Algorithm to use for focus curve analysis.
     * @param exposureTime Exposure time for each focus frame.
     * @param stepSize Step size between positions.
     * @param maxSteps Maximum steps from starting position.
     * @return Focus curve with results.
     */
    FocusCurve performAutofocus(
        AutofocusAlgorithm algorithm = AutofocusAlgorithm::VCurve,
        double exposureTime = 2.0, int stepSize = 100, int maxSteps = 25);

    /**
     * @brief Performs a coarse focus sweep.
     * @param startPos Starting position for sweep.
     * @param stepSize Step size between measurements.
     * @param numSteps Number of steps to measure.
     * @param exposureTime Exposure time for each measurement.
     * @return Vector of focus positions with metrics.
     */
    std::vector<FocusPosition> performCoarseSweep(int startPos, int stepSize,
                                                  int numSteps,
                                                  double exposureTime);

    /**
     * @brief Performs fine focus around best position.
     * @param centerPos Center position for fine focus.
     * @param stepSize Fine step size.
     * @param numSteps Number of fine steps each direction.
     * @param exposureTime Exposure time for measurements.
     * @return Vector of fine focus positions.
     */
    std::vector<FocusPosition> performFineFocus(int centerPos, int stepSize,
                                                int numSteps,
                                                double exposureTime);

    /**
     * @brief Analyzes focus curve using specified algorithm.
     * @param positions Vector of focus positions with metrics.
     * @param algorithm Algorithm to use for analysis.
     * @return Focus curve with best position and confidence.
     */
    FocusCurve analyzeFocusCurve(const std::vector<FocusPosition>& positions,
                                 AutofocusAlgorithm algorithm);

    /**
     * @brief Creates an enhanced autofocus task.
     * @return Unique pointer to configured task.
     */
    static std::unique_ptr<Task> createEnhancedTask();

    /**
     * @brief Defines task parameters.
     * @param task Task instance to configure.
     */
    static void defineParameters(Task& task);

private:
    /**
     * @brief Validates autofocus parameters.
     * @param params Parameters to validate.
     */
    void validateAutofocusParams(const json& params);

    /**
     * @brief Converts string to autofocus algorithm enum.
     * @param algorithmStr Algorithm name as string.
     * @return Corresponding algorithm enum.
     */
    AutofocusAlgorithm parseAlgorithm(const std::string& algorithmStr);

    /**
     * @brief Finds best focus position using V-curve fitting.
     * @param positions Vector of focus positions.
     * @return Best position and confidence.
     */
    std::pair<int, double> findBestPositionVCurve(
        const std::vector<FocusPosition>& positions);

    /**
     * @brief Finds best focus position using hyperbolic fitting.
     * @param positions Vector of focus positions.
     * @return Best position and confidence.
     */
    std::pair<int, double> findBestPositionHyperbolic(
        const std::vector<FocusPosition>& positions);

    /**
     * @brief Validates focus curve quality.
     * @param curve Focus curve to validate.
     * @return True if curve quality is acceptable.
     */
    bool validateFocusCurve(const FocusCurve& curve);

    /**
     * @brief Applies temperature compensation if enabled.
     * @param basePosition Base focus position.
     * @param currentTemp Current temperature.
     * @param referenceTemp Reference temperature.
     * @return Compensated position.
     */
    int applyTemperatureCompensation(int basePosition, double currentTemp,
                                     double referenceTemp);
    /**
     * @brief Converts string to autofocus mode enum.
     * @param modeStr Mode name as string.
     * @return Corresponding mode enum.
     */
    AutofocusMode parseMode(const std::string& modeStr);
    /**
     * @brief 获取指定模式的默认参数（曝光、步长、步数）
     * @param mode 对焦模式
     * @return (曝光时间, 步长, 步数)
     */
    std::tuple<double, int, int> getModeDefaults(AutofocusMode mode);
};

}  // namespace lithium::task::focuser

#endif  // LITHIUM_TASK_FOCUSER_AUTOFOCUS_TASK_HPP
