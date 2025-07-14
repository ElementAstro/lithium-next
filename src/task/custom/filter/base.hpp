#ifndef LITHIUM_TASK_FILTER_BASE_FILTER_TASK_HPP
#define LITHIUM_TASK_FILTER_BASE_FILTER_TASK_HPP

#include <string>
#include <vector>
#include "../../task.hpp"

namespace lithium::task::filter {

/**
 * @enum FilterType
 * @brief Represents different types of filters.
 */
enum class FilterType {
    LRGB,        ///< Luminance, Red, Green, Blue filters
    Narrowband,  ///< Narrowband filters (Ha, OIII, SII, etc.)
    Broadband,   ///< Broadband filters (Clear, UV, IR)
    Custom       ///< Custom or user-defined filters
};

/**
 * @struct FilterInfo
 * @brief Contains information about a specific filter.
 */
struct FilterInfo {
    std::string name;            ///< Name of the filter
    FilterType type;             ///< Type category of the filter
    int position;                ///< Physical position in filter wheel
    double recommendedExposure;  ///< Recommended exposure time in seconds
    std::string description;     ///< Description of the filter
};

/**
 * @struct FilterSequenceStep
 * @brief Represents a single step in a filter sequence.
 */
struct FilterSequenceStep {
    std::string filterName;  ///< Name of the filter to use
    double exposure;         ///< Exposure time in seconds
    int frameCount;          ///< Number of frames to capture
    int gain;                ///< Camera gain setting
    int offset;              ///< Camera offset setting
    bool skipIfUnavailable;  ///< Skip this step if filter is not available
};

/**
 * @class BaseFilterTask
 * @brief Abstract base class for all filter-related tasks.
 *
 * This class provides common functionality for filter wheel operations,
 * including filter validation, wheel communication, and error handling.
 * Derived classes implement specific filter operations like sequences,
 * calibration, and maintenance.
 */
class BaseFilterTask : public Task {
public:
    /**
     * @brief Constructs a BaseFilterTask with the given name.
     * @param name The name of the filter task.
     */
    BaseFilterTask(const std::string& name);

    /**
     * @brief Virtual destructor for proper cleanup.
     */
    virtual ~BaseFilterTask() = default;

    /**
     * @brief Gets the list of available filters.
     * @return Vector of FilterInfo structures.
     */
    virtual std::vector<FilterInfo> getAvailableFilters() const;

    /**
     * @brief Checks if a specific filter is available.
     * @param filterName The name of the filter to check.
     * @return True if the filter is available, false otherwise.
     */
    virtual bool isFilterAvailable(const std::string& filterName) const;

    /**
     * @brief Gets the current filter position.
     * @return The current filter name, or empty string if unknown.
     */
    virtual std::string getCurrentFilter() const;

    /**
     * @brief Checks if the filter wheel is currently moving.
     * @return True if the wheel is moving, false otherwise.
     */
    virtual bool isFilterWheelMoving() const;

protected:
    /**
     * @brief Changes to the specified filter.
     * @param filterName The name of the filter to change to.
     * @return True if the change was successful, false otherwise.
     */
    virtual bool changeFilter(const std::string& filterName);

    /**
     * @brief Waits for the filter wheel to stop moving.
     * @param timeoutSeconds Maximum time to wait in seconds.
     * @return True if the wheel stopped, false if timeout occurred.
     */
    virtual bool waitForFilterWheel(int timeoutSeconds = 30);

    /**
     * @brief Validates filter sequence parameters.
     * @param params JSON parameters to validate.
     * @throws std::invalid_argument if validation fails.
     */
    void validateFilterParams(const json& params);

    /**
     * @brief Sets up default parameter definitions for filter tasks.
     */
    void setupFilterDefaults();

    /**
     * @brief Handles filter-related errors and updates task state.
     * @param filterName The name of the filter involved in the error.
     * @param error The error message.
     */
    void handleFilterError(const std::string& filterName,
                           const std::string& error);

private:
    std::vector<FilterInfo> availableFilters_;  ///< List of available filters
    std::string currentFilter_;                 ///< Currently selected filter
    bool isConnected_;  ///< Connection status to filter wheel
};

}  // namespace lithium::task::filter

#endif  // LITHIUM_TASK_FILTER_BASE_FILTER_TASK_HPP
