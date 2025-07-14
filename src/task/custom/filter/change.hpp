#ifndef LITHIUM_TASK_FILTER_CHANGE_TASK_HPP
#define LITHIUM_TASK_FILTER_CHANGE_TASK_HPP

#include "base.hpp"

namespace lithium::task::filter {

/**
 * @class FilterChangeTask
 * @brief Task for changing individual filters on the filter wheel.
 *
 * This task handles single filter changes with proper validation,
 * error handling, and status reporting. It supports waiting for
 * the filter wheel to settle and provides detailed progress information.
 */
class FilterChangeTask : public BaseFilterTask {
public:
    /**
     * @brief Constructs a FilterChangeTask.
     * @param name Optional custom name for the task (defaults to
     * "FilterChange").
     */
    FilterChangeTask(const std::string& name = "FilterChange");

    /**
     * @brief Executes the filter change with the provided parameters.
     * @param params JSON object containing filter change configuration.
     *
     * Required parameters:
     * - filterName (string): Name of the filter to change to
     *
     * Optional parameters:
     * - timeout (number): Maximum wait time in seconds (default: 30)
     * - verify (boolean): Verify filter position after change (default: true)
     * - retries (number): Number of retry attempts (default: 3)
     */
    void execute(const json& params) override;

    /**
     * @brief Changes to a specific filter by name.
     * @param filterName The name of the filter to change to.
     * @param timeout Maximum wait time in seconds.
     * @param verify Whether to verify the filter position after change.
     * @return True if the change was successful, false otherwise.
     */
    bool changeToFilter(const std::string& filterName, int timeout = 30,
                        bool verify = true);

    /**
     * @brief Changes to a specific filter by position.
     * @param position The position number of the filter.
     * @param timeout Maximum wait time in seconds.
     * @param verify Whether to verify the filter position after change.
     * @return True if the change was successful, false otherwise.
     */
    bool changeToPosition(int position, int timeout = 30, bool verify = true);

    /**
     * @brief Gets the time taken for the last filter change.
     * @return Duration of the last filter change in milliseconds.
     */
    std::chrono::milliseconds getLastChangeTime() const;

private:
    /**
     * @brief Sets up parameter definitions specific to filter changes.
     */
    void setupFilterChangeDefaults();

    /**
     * @brief Verifies that the filter wheel moved to the correct position.
     * @param expectedFilter The expected filter name.
     * @return True if verification succeeded, false otherwise.
     */
    bool verifyFilterPosition(const std::string& expectedFilter);

    std::chrono::milliseconds lastChangeTime_{
        0};              ///< Duration of last filter change
    int maxRetries_{3};  ///< Maximum number of retry attempts
};

}  // namespace lithium::task::filter

#endif  // LITHIUM_TASK_FILTER_CHANGE_TASK_HPP
