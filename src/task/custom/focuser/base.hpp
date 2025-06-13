#ifndef LITHIUM_TASK_FOCUSER_BASE_FOCUSER_TASK_HPP
#define LITHIUM_TASK_FOCUSER_BASE_FOCUSER_TASK_HPP

#include <string>
#include <vector>
#include <optional>
#include "../../task.hpp"

namespace lithium::task::focuser {

/**
 * @enum FocuserDirection
 * @brief Represents the direction of focuser movement.
 */
enum class FocuserDirection {
    In,   ///< Move focuser inward (closer to camera)
    Out   ///< Move focuser outward (away from camera)
};

/**
 * @enum FocusQuality
 * @brief Represents the quality assessment of focus.
 */
enum class FocusQuality {
    Excellent,  ///< HFR < 2.0, high star count
    Good,       ///< HFR 2.0-3.0, adequate star count
    Fair,       ///< HFR 3.0-4.0, moderate star count
    Poor,       ///< HFR 4.0-5.0, low star count
    Bad         ///< HFR > 5.0 or insufficient stars
};

/**
 * @struct FocusMetrics
 * @brief Contains metrics for focus quality assessment.
 */
struct FocusMetrics {
    double hfr;              ///< Half Flux Radius
    double fwhm;             ///< Full Width Half Maximum
    int starCount;           ///< Number of detected stars
    double peakIntensity;    ///< Peak intensity of brightest star
    double backgroundLevel;  ///< Background noise level
    FocusQuality quality;    ///< Overall quality assessment
};

/**
 * @struct FocusPosition
 * @brief Represents a focuser position with associated data.
 */
struct FocusPosition {
    int position;            ///< Absolute focuser position
    FocusMetrics metrics;    ///< Focus quality metrics at this position
    double temperature;      ///< Temperature when measurement was taken
    std::string timestamp;   ///< Time when measurement was taken
};

/**
 * @struct FocusCurve
 * @brief Represents a focus curve with multiple position measurements.
 */
struct FocusCurve {
    std::vector<FocusPosition> positions;  ///< All measured positions
    int bestPosition;                      ///< Position with best focus
    double confidence;                     ///< Confidence level (0.0-1.0)
    std::string algorithm;                 ///< Algorithm used for analysis
};

/**
 * @class BaseFocuserTask
 * @brief Abstract base class for all focuser-related tasks.
 *
 * This class provides common functionality for focuser operations,
 * including position management, temperature compensation, focus
 * quality assessment, and error handling. Derived classes implement
 * specific focuser operations like autofocus, calibration, and monitoring.
 */
class BaseFocuserTask : public Task {
public:
    /**
     * @brief Constructs a BaseFocuserTask with the given name.
     * @param name The name of the focuser task.
     */
    BaseFocuserTask(const std::string& name);

    /**
     * @brief Virtual destructor.
     */
    virtual ~BaseFocuserTask() = default;

    /**
     * @brief Gets the current focuser position.
     * @return Current absolute position, or nullopt if unavailable.
     */
    std::optional<int> getCurrentPosition() const;

    /**
     * @brief Moves the focuser to an absolute position.
     * @param position Target absolute position.
     * @param timeout Maximum wait time in seconds.
     * @return True if movement was successful.
     */
    bool moveToPosition(int position, int timeout = 30);

    /**
     * @brief Moves the focuser by a relative number of steps.
     * @param steps Number of steps to move (positive = out, negative = in).
     * @param timeout Maximum wait time in seconds.
     * @return True if movement was successful.
     */
    bool moveRelative(int steps, int timeout = 30);

    /**
     * @brief Checks if the focuser is currently moving.
     * @return True if focuser is in motion.
     */
    bool isMoving() const;

    /**
     * @brief Aborts any current focuser movement.
     * @return True if abort was successful.
     */
    bool abortMovement();

    /**
     * @brief Gets the current temperature from the focuser.
     * @return Temperature in Celsius, or nullopt if unavailable.
     */
    std::optional<double> getTemperature() const;

    /**
     * @brief Takes an exposure and analyzes focus quality.
     * @param exposureTime Exposure duration in seconds.
     * @param binning Camera binning factor.
     * @return Focus metrics for the current position.
     */
    FocusMetrics analyzeFocusQuality(double exposureTime = 2.0, int binning = 1);

    /**
     * @brief Calculates temperature compensation offset.
     * @param currentTemp Current temperature in Celsius.
     * @param referenceTemp Reference temperature in Celsius.
     * @param compensationRate Steps per degree Celsius.
     * @return Number of steps to compensate.
     */
    int calculateTemperatureCompensation(double currentTemp, 
                                       double referenceTemp, 
                                       double compensationRate = 2.0);

    /**
     * @brief Validates focuser parameters.
     * @param params JSON parameters to validate.
     * @return True if parameters are valid.
     */
    bool validateFocuserParams(const json& params);

    /**
     * @brief Gets the focuser limits.
     * @return Pair of (minimum, maximum) positions.
     */
    std::pair<int, int> getFocuserLimits() const;

    /**
     * @brief Sets up focuser for operation.
     * @return True if setup was successful.
     */
    bool setupFocuser();

    /**
     * @brief Performs backlash compensation.
     * @param direction Direction of intended movement.
     * @param backlashSteps Number of backlash compensation steps.
     * @return True if compensation was successful.
     */
    bool performBacklashCompensation(FocuserDirection direction, int backlashSteps);

protected:
    /**
     * @brief Waits for focuser to complete movement.
     * @param timeout Maximum wait time in seconds.
     * @return True if focuser stopped moving within timeout.
     */
    bool waitForMovementComplete(int timeout = 30);

    /**
     * @brief Validates position is within focuser limits.
     * @param position Position to validate.
     * @return True if position is valid.
     */
    bool isValidPosition(int position) const;

    /**
     * @brief Updates task history with focuser operation.
     * @param operation Description of the operation.
     * @param success Whether the operation was successful.
     */
    void logFocuserOperation(const std::string& operation, bool success);

    /**
     * @brief Gets focus quality assessment from metrics.
     * @param metrics Focus metrics to assess.
     * @return Quality level assessment.
     */
    FocusQuality assessFocusQuality(const FocusMetrics& metrics);

private:
    mutable std::mutex focuserMutex_;  ///< Mutex for thread-safe operations
    std::pair<int, int> limits_;       ///< Focuser position limits
    double lastTemperature_;           ///< Last recorded temperature
    bool isSetup_;                     ///< Whether focuser is properly set up
};

}  // namespace lithium::task::focuser

#endif  // LITHIUM_TASK_FOCUSER_BASE_FOCUSER_TASK_HPP
