/**
 * @file target.hpp
 * @brief Defines the Target class for managing task execution with
 * dependencies.
 */

#ifndef LITHIUM_TARGET_HPP
#define LITHIUM_TARGET_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/async/safetype.hpp"
#include "task.hpp"
#include "tools/astronomy/types.hpp"

namespace lithium::task {

// Import astronomy types into task namespace for convenience
using tools::astronomy::AltitudeConstraints;
using tools::astronomy::Coordinates;
using tools::astronomy::ExposurePlan;
using tools::astronomy::HorizontalCoordinates;
using tools::astronomy::MeridianFlipInfo;
using tools::astronomy::MeridianState;
using tools::astronomy::ObservabilityWindow;
using tools::astronomy::ObserverLocation;
using tools::astronomy::TargetConfig;

/**
 * @enum TargetStatus
 * @brief Represents the status of a target.
 */
enum class TargetStatus {
    Pending,     ///< Target is pending and has not started yet.
    InProgress,  ///< Target is currently in progress.
    Completed,   ///< Target has completed successfully.
    Failed,      ///< Target has failed.
    Skipped      ///< Target has been skipped.
};

/**
 * @brief Callback function type definitions.
 */
using TargetStartCallback = std::function<void(const std::string&)>;
using TargetEndCallback = std::function<void(const std::string&, TargetStatus)>;
using TargetErrorCallback =
    std::function<void(const std::string&, const std::exception&)>;

class Target;

/**
 * @brief Target modifier function type definition.
 */
using TargetModifier = std::function<void(Target&)>;

/**
 * @class Target
 * @brief Represents a target that can execute a series of tasks with optional
 * retries and cooldown periods.
 */
class Target {
public:
    /**
     * @brief Constructs a Target with a given name, cooldown period, and
     * maximum retries.
     * @param name The name of the target.
     * @param cooldown The cooldown period between task executions.
     * @param maxRetries The maximum number of retries for each task.
     */
    Target(std::string name,
           std::chrono::seconds cooldown = std::chrono::seconds{0},
           int maxRetries = 0);

    // Disable copy constructor and assignment operator
    Target(const Target&) = delete;
    Target& operator=(const Target&) = delete;

    /**
     * @brief Adds a task to the target.
     * @param task The task to be added.
     */
    void addTask(std::unique_ptr<Task> task);

    /**
     * @brief Sets the cooldown period for the target.
     * @param cooldown The cooldown period in seconds.
     */
    void setCooldown(std::chrono::seconds cooldown);

    /**
     * @brief Enables or disables the target.
     * @param enabled True to enable, false to disable.
     */
    void setEnabled(bool enabled);

    /**
     * @brief Sets the maximum number of retries for each task.
     * @param retries The maximum number of retries.
     */
    void setMaxRetries(int retries);

    /**
     * @brief Sets the status of the target.
     * @param status The status to be set.
     */
    void setStatus(TargetStatus status);

    /**
     * @brief Sets the callback function to be called when the target starts.
     * @param callback The callback function.
     */
    void setOnStart(TargetStartCallback callback);

    /**
     * @brief Sets the callback function to be called when the target ends.
     * @param callback The callback function.
     */
    void setOnEnd(TargetEndCallback callback);

    /**
     * @brief Sets the callback function to be called when an error occurs.
     * @param callback The callback function.
     */
    void setOnError(TargetErrorCallback callback);

    /**
     * @brief Gets the name of the target.
     * @return The name of the target.
     */
    [[nodiscard]] const std::string& getName() const;

    /**
     * @brief Gets the UUID of the target.
     * @return The UUID of the target.
     */
    [[nodiscard]] const std::string& getUUID() const;

    /**
     * @brief Gets the current status of the target.
     * @return The current status of the target.
     */
    [[nodiscard]] TargetStatus getStatus() const;

    /**
     * @brief Checks if the target is enabled.
     * @return True if the target is enabled, false otherwise.
     */
    [[nodiscard]] bool isEnabled() const;

    /**
     * @brief Gets the progress of the target as a percentage.
     * @return The progress percentage.
     */
    [[nodiscard]] double getProgress() const;

    /**
     * @brief Executes the target.
     */
    void execute();

    /**
     * @brief Loads tasks from a JSON array.
     * @param tasksJson The JSON array containing task definitions.
     */
    void loadTasksFromJson(const json& tasksJson);

    /**
     * @brief Gets the dependencies for this target.
     * @return Vector of dependency strings.
     */
    auto getDependencies() -> std::vector<std::string>;

    /**
     * @brief Gets the tasks in this target.
     * @return Vector of tasks.
     */
    auto getTasks() -> const std::vector<std::unique_ptr<Task>>&;

    /**
     * @brief Sets parameters for a specific task
     * @param taskUUID The UUID of the task
     * @param params The parameters in JSON format
     */
    void setTaskParams(const std::string& taskUUID, const json& params);

    /**
     * @brief Gets parameters for a specific task
     * @param taskUUID The UUID of the task
     * @return The task parameters
     */
    [[nodiscard]] auto getTaskParams(const std::string& taskUUID) const
        -> std::optional<json>;

    /**
     * @brief Sets parameters for all tasks in this target
     * @param params The parameters in JSON format
     */
    void setParams(const json& params);

    /**
     * @brief Gets the parameters for this target's tasks
     * @return The current parameters
     */
    [[nodiscard]] auto getParams() const -> const json&;

    /**
     * @brief Converts the target to a JSON object.
     * @return The JSON object representing the target.
     */
    [[nodiscard]] auto toJson() const -> json;

    /**
     * @brief Converts a JSON object to a target.
     * @param data The JSON object to convert.
     */
    auto fromJson(const json& data) -> void;

    /**
     * @brief Creates a new task group.
     * @param groupName The name of the group.
     */
    void createTaskGroup(const std::string& groupName);

    /**
     * @brief Adds a task to a group.
     * @param groupName The name of the group.
     * @param taskUUID The UUID of the task to add.
     */
    void addTaskToGroup(const std::string& groupName,
                        const std::string& taskUUID);

    /**
     * @brief Removes a task from a group.
     * @param groupName The name of the group.
     * @param taskUUID The UUID of the task to remove.
     */
    void removeTaskFromGroup(const std::string& groupName,
                             const std::string& taskUUID);

    /**
     * @brief Gets the tasks in a group.
     * @param groupName The name of the group.
     * @return Vector of task UUIDs in the group.
     */
    std::vector<std::string> getTaskGroup(const std::string& groupName) const;

    /**
     * @brief Adds a dependency between tasks.
     * @param taskUUID The UUID of the task.
     * @param dependsOnUUID The UUID of the task it depends on.
     */
    void addTaskDependency(const std::string& taskUUID,
                           const std::string& dependsOnUUID);

    /**
     * @brief Removes a dependency between tasks.
     * @param taskUUID The UUID of the task.
     * @param dependsOnUUID The UUID of the task it depends on.
     */
    void removeTaskDependency(const std::string& taskUUID,
                              const std::string& dependsOnUUID);

    /**
     * @brief Checks if all dependencies for a task are satisfied.
     * @param taskUUID The UUID of the task.
     * @return True if all dependencies are satisfied.
     */
    bool checkDependencies(const std::string& taskUUID) const;

    /**
     * @brief Executes all tasks in a group.
     * @param groupName The name of the group.
     */
    void executeGroup(const std::string& groupName);

    /**
     * @brief Aborts execution of a task group.
     * @param groupName The name of the group.
     */
    void abortGroup(const std::string& groupName);

    // ========================================================================
    // Astronomical Observation Methods
    // ========================================================================

    /**
     * @brief Sets the astronomical configuration for this target.
     * @param config The target configuration.
     */
    void setAstroConfig(const TargetConfig& config);

    /**
     * @brief Gets the astronomical configuration.
     * @return The current target configuration.
     */
    [[nodiscard]] const TargetConfig& getAstroConfig() const;

    /**
     * @brief Gets a mutable reference to the astronomical configuration.
     * @return Mutable reference to configuration.
     */
    TargetConfig& getAstroConfigMutable();

    /**
     * @brief Sets the celestial coordinates for this target.
     * @param coords The coordinates to set.
     */
    void setCoordinates(const Coordinates& coords);

    /**
     * @brief Gets the celestial coordinates.
     * @return The target coordinates.
     */
    [[nodiscard]] const Coordinates& getCoordinates() const;

    /**
     * @brief Adds an exposure plan to this target.
     * @param plan The exposure plan to add.
     */
    void addExposurePlan(const ExposurePlan& plan);

    /**
     * @brief Removes an exposure plan by filter name.
     * @param filterName The filter name to remove.
     */
    void removeExposurePlan(const std::string& filterName);

    /**
     * @brief Gets all exposure plans.
     * @return Vector of exposure plans.
     */
    [[nodiscard]] const std::vector<ExposurePlan>& getExposurePlans() const;

    /**
     * @brief Gets the current active exposure plan.
     * @return Pointer to current plan, or nullptr if none active.
     */
    [[nodiscard]] ExposurePlan* getCurrentExposurePlan();

    /**
     * @brief Advances to the next exposure plan.
     * @return True if advanced, false if no more plans.
     */
    bool advanceExposurePlan();

    /**
     * @brief Records a completed exposure for the current plan.
     */
    void recordCompletedExposure();

    /**
     * @brief Sets the observability window for this target.
     * @param window The observability window.
     */
    void setObservabilityWindow(const ObservabilityWindow& window);

    /**
     * @brief Gets the observability window.
     * @return The current observability window.
     */
    [[nodiscard]] const ObservabilityWindow& getObservabilityWindow() const;

    /**
     * @brief Updates the current horizontal coordinates (alt/az).
     * @param coords The current alt/az coordinates.
     */
    void updateHorizontalCoordinates(const HorizontalCoordinates& coords);

    /**
     * @brief Gets the current horizontal coordinates.
     * @return Current alt/az coordinates.
     */
    [[nodiscard]] const HorizontalCoordinates& getHorizontalCoordinates() const;

    /**
     * @brief Updates meridian flip information.
     * @param info The meridian flip info.
     */
    void updateMeridianFlipInfo(const MeridianFlipInfo& info);

    /**
     * @brief Gets meridian flip information.
     * @return Current meridian flip info.
     */
    [[nodiscard]] const MeridianFlipInfo& getMeridianFlipInfo() const;

    /**
     * @brief Checks if the target is currently observable.
     * @return True if observable based on constraints.
     */
    [[nodiscard]] bool isObservable() const;

    /**
     * @brief Checks if the target needs a meridian flip.
     * @return True if meridian flip is needed.
     */
    [[nodiscard]] bool needsMeridianFlip() const;

    /**
     * @brief Marks meridian flip as completed.
     */
    void markMeridianFlipCompleted();

    /**
     * @brief Gets the target priority.
     * @return Priority value (1-10).
     */
    [[nodiscard]] int getPriority() const;

    /**
     * @brief Sets the target priority.
     * @param priority Priority value (1-10).
     */
    void setPriority(int priority);

    /**
     * @brief Gets total remaining exposure time across all plans.
     * @return Remaining time in seconds.
     */
    [[nodiscard]] double getRemainingExposureTime() const;

    /**
     * @brief Gets overall exposure plan progress.
     * @return Progress percentage (0-100).
     */
    [[nodiscard]] double getExposureProgress() const;

    /**
     * @brief Checks if all exposure plans are complete.
     * @return True if all plans completed.
     */
    [[nodiscard]] bool areExposurePlansComplete() const;

    /**
     * @brief Pauses target execution.
     */
    void pause();

    /**
     * @brief Resumes target execution.
     */
    void resume();

    /**
     * @brief Checks if target is paused.
     * @return True if paused.
     */
    [[nodiscard]] bool isPaused() const;

    /**
     * @brief Aborts target execution.
     */
    void abort();

    /**
     * @brief Checks if target has been aborted.
     * @return True if aborted.
     */
    [[nodiscard]] bool isAborted() const;

private:
    std::string name_;  ///< The name of the target.
    std::string uuid_;  ///< The unique identifier of the target.
    std::vector<std::unique_ptr<Task>>
        tasks_;  ///< The list of tasks to be executed by the target.
    std::vector<std::string> dependencies_;  ///< The list of task dependencies.
    std::chrono::seconds
        cooldown_;        ///< The cooldown period between task executions.
    bool enabled_{true};  ///< Indicates whether the target is enabled.
    std::atomic<TargetStatus> status_{
        TargetStatus::Pending};  ///< The current status of the target.
    std::shared_mutex
        mutex_;  ///< Mutex for thread-safe access to target properties.

    // Progress tracking
    std::atomic<size_t> completedTasks_{0};  ///< The number of completed tasks.
    size_t totalTasks_{0};                   ///< The total number of tasks.

    // Callback functions
    TargetStartCallback
        onStart_;  ///< Callback function to be called when the target starts.
    TargetEndCallback
        onEnd_;  ///< Callback function to be called when the target ends.
    TargetErrorCallback
        onError_;  ///< Callback function to be called when an error occurs.

    // Retry mechanism
    int maxRetries_;  ///< The maximum number of retries for each task.
    mutable std::shared_mutex callbackMutex_;  ///< Mutex for callback access

    std::shared_ptr<atom::async::LockFreeHashTable<std::string, json>>
        queue_;  ///< The task queue.

    // Task parameters storage
    std::unordered_map<std::string, json>
        taskParams_;                         ///< Individual task parameters
    mutable std::shared_mutex paramsMutex_;  ///< Mutex for parameter access

    json params_;  ///< Parameters for all tasks in this target

    // Task group management
    std::unordered_map<std::string, std::vector<std::string>>
        taskGroups_;                ///< Task groups
    std::shared_mutex groupMutex_;  ///< Mutex for group access

    // Task dependency relationships
    std::unordered_map<std::string, std::vector<std::string>>
        taskDependencies_;                ///< Task dependencies
    mutable std::shared_mutex depMutex_;  ///< Mutex for dependency access

    // ========================================================================
    // Astronomical Observation Data
    // ========================================================================

    TargetConfig astroConfig_;              ///< Astronomical configuration
    ObservabilityWindow observability_;     ///< Current observability window
    HorizontalCoordinates currentAltAz_;    ///< Current altitude/azimuth
    MeridianFlipInfo meridianInfo_;         ///< Meridian flip information
    mutable std::shared_mutex astroMutex_;  ///< Mutex for astro data access

    size_t currentExposurePlanIndex_{0};  ///< Index of current exposure plan

    // Execution state
    std::atomic<bool> paused_{false};   ///< Whether target is paused
    std::atomic<bool> aborted_{false};  ///< Whether target is aborted

    /**
     * @brief Notifies that the target has started.
     */
    void notifyStart();

    /**
     * @brief Notifies that the target has ended.
     * @param status The final status of the target.
     */
    void notifyEnd(TargetStatus status);

    /**
     * @brief Notifies that an error occurred in the target.
     * @param e The exception that occurred.
     */
    void notifyError(const std::exception& e);
};

}  // namespace lithium::task

#endif  // LITHIUM_TARGET_HPP
