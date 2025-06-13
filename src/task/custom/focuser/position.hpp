#ifndef LITHIUM_TASK_FOCUSER_POSITION_TASK_HPP
#define LITHIUM_TASK_FOCUSER_POSITION_TASK_HPP

#include "base.hpp"

namespace lithium::task::focuser {

/**
 * @class FocuserPositionTask
 * @brief Task for basic focuser position movements.
 *
 * This task handles single position changes, relative movements,
 * and position synchronization with proper validation and error handling.
 */
class FocuserPositionTask : public BaseFocuserTask {
public:
    /**
     * @brief Constructs a FocuserPositionTask.
     * @param name Optional custom name for the task.
     */
    FocuserPositionTask(const std::string& name = "FocuserPosition");

    /**
     * @brief Executes the position movement with the provided parameters.
     * @param params JSON object containing position movement configuration.
     *
     * Parameters:
     * - action (string): "move_absolute", "move_relative", "get_position", or "sync_position"
     * - position (int): Target position for absolute moves or sync (required for absolute/sync)
     * - steps (int): Number of steps for relative moves (required for relative)
     * - timeout (int): Movement timeout in seconds (default: 30)
     * - verify (bool): Verify position after movement (default: true)
     */
    void execute(const json& params) override;

    /**
     * @brief Moves focuser to an absolute position.
     * @param position Target absolute position.
     * @param timeout Maximum wait time in seconds.
     * @param verify Whether to verify final position.
     * @return True if movement was successful.
     */
    bool moveAbsolute(int position, int timeout = 30, bool verify = true);

    /**
     * @brief Moves focuser by relative steps.
     * @param steps Number of steps (positive = out, negative = in).
     * @param timeout Maximum wait time in seconds.
     * @return True if movement was successful.
     */
    bool moveRelativeSteps(int steps, int timeout = 30);

    /**
     * @brief Synchronizes focuser position (sets current position as reference).
     * @param position Position value to set as current.
     * @return True if synchronization was successful.
     */
    bool syncPosition(int position);

    /**
     * @brief Gets the current focuser position with error handling.
     * @return Current position or throws exception on error.
     */
    int getPositionSafe();

    /**
     * @brief Creates an enhanced position task with full parameter definitions.
     * @return Unique pointer to configured task.
     */
    static std::unique_ptr<Task> createEnhancedTask();

    /**
     * @brief Defines task parameters for the base Task class.
     * @param task Task instance to configure.
     */
    static void defineParameters(Task& task);

private:
    /**
     * @brief Validates position movement parameters.
     * @param params Parameters to validate.
     */
    void validatePositionParams(const json& params);

    /**
     * @brief Verifies focuser reached target position.
     * @param expectedPosition Expected final position.
     * @param tolerance Allowed position tolerance.
     * @return True if position is within tolerance.
     */
    bool verifyPosition(int expectedPosition, int tolerance = 5);
};

}  // namespace lithium::task::focuser

#endif  // LITHIUM_TASK_FOCUSER_POSITION_TASK_HPP
