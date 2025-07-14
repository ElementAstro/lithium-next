/**
 * @file sequence_manager.hpp
 * @brief Central manager for the task sequence system
 *
 * This file provides a comprehensive integration point for the task sequence system,
 * integrating the task generator, exposure sequencer, and exception handling.
 *
 * @date 2025-07-11
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2025 Max Qian
 */

#ifndef LITHIUM_TASK_SEQUENCE_MANAGER_HPP
#define LITHIUM_TASK_SEQUENCE_MANAGER_HPP

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <exception>

#include "generator.hpp"
#include "sequencer.hpp"
#include "task.hpp"
#include "target.hpp"
#include "atom/type/json.hpp"

namespace lithium::task {

using json = nlohmann::json;

/**
 * @class SequenceException
 * @brief Exception class for sequence management errors.
 */
class SequenceException : public std::exception {
public:
    /**
     * @brief Error codes for SequenceException.
     */
    enum class ErrorCode {
        FILE_ERROR,              ///< File read/write error
        VALIDATION_ERROR,        ///< Sequence validation error
        GENERATION_ERROR,        ///< Task generation error
        EXECUTION_ERROR,         ///< Sequence execution error
        DEPENDENCY_ERROR,        ///< Dependency resolution error
        TEMPLATE_ERROR,          ///< Template processing error
        DATABASE_ERROR,          ///< Database operation error
        CONFIGURATION_ERROR      ///< Configuration error
    };

    /**
     * @brief Constructor for SequenceException.
     * @param code The error code.
     * @param message The error message.
     */
    SequenceException(ErrorCode code, const std::string& message)
        : code_(code), msg_(message) {}

    /**
     * @brief Get the error message.
     * @return The error message.
     */
    const char* what() const noexcept override { return msg_.c_str(); }

    /**
     * @brief Get the error code.
     * @return The error code.
     */
    ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;   ///< The error code.
    std::string msg_;  ///< The error message.
};

/**
 * @brief Structure for sequence creation options.
 */
struct SequenceOptions {
    bool validateOnLoad = true;                  ///< Validate sequences when loading
    bool autoGenerateMissingTargets = false;     ///< Generate targets that are referenced but missing
    ExposureSequence::SerializationFormat defaultFormat = 
        ExposureSequence::SerializationFormat::PRETTY_JSON;  ///< Default serialization format
    std::string templateDirectory;               ///< Directory for sequence templates
    ExposureSequence::SchedulingStrategy schedulingStrategy = 
        ExposureSequence::SchedulingStrategy::Dependencies;  ///< Default scheduling strategy
    ExposureSequence::RecoveryStrategy recoveryStrategy = 
        ExposureSequence::RecoveryStrategy::Retry;           ///< Default recovery strategy
    size_t maxConcurrentTargets = 1;             ///< Maximum concurrent targets
    std::chrono::seconds defaultTaskTimeout{30}; ///< Default timeout for tasks
    std::chrono::seconds globalTimeout{0};       ///< Global sequence timeout (0 = no timeout)
    bool persistToDatabase = true;               ///< Whether to persist sequences to database
    bool logProgress = true;                     ///< Whether to log progress
    bool enablePerformanceMetrics = true;        ///< Whether to collect performance metrics
};

/**
 * @brief Structure for sequence execution results.
 */
struct SequenceResult {
    bool success;                                 ///< Whether the sequence was successful
    std::vector<std::string> completedTargets;    ///< Names of completed targets
    std::vector<std::string> failedTargets;       ///< Names of failed targets
    std::vector<std::string> skippedTargets;      ///< Names of skipped targets
    double totalProgress;                         ///< Overall progress percentage
    std::chrono::milliseconds totalExecutionTime; ///< Total execution time
    json executionStats;                          ///< Detailed execution statistics
    std::vector<std::string> warnings;            ///< Warnings during execution
    std::vector<std::string> errors;              ///< Errors during execution
};

/**
 * @class SequenceManager
 * @brief Central manager for task sequences.
 * 
 * This class provides a unified interface for creating, loading, validating,
 * and executing task sequences. It integrates the TaskGenerator and ExposureSequence
 * components to provide a seamless workflow.
 */
class SequenceManager {
public:
    /**
     * @brief Constructor for SequenceManager.
     * @param options Configuration options for sequence management.
     */
    explicit SequenceManager(const SequenceOptions& options = SequenceOptions{});

    /**
     * @brief Destructor for SequenceManager.
     */
    ~SequenceManager();

    /**
     * @brief Create a shared pointer to a SequenceManager instance.
     * @param options Configuration options for sequence management.
     * @return A shared pointer to a SequenceManager instance.
     */
    static std::shared_ptr<SequenceManager> createShared(
        const SequenceOptions& options = SequenceOptions{});

    // Sequence creation and loading

    /**
     * @brief Creates a new empty sequence.
     * @param name The name of the sequence.
     * @return The created sequence.
     */
    std::shared_ptr<ExposureSequence> createSequence(const std::string& name);

    /**
     * @brief Loads a sequence from a file.
     * @param filename The path to the sequence file.
     * @param validate Whether to validate the sequence (default: true).
     * @return The loaded sequence.
     * @throws SequenceException If the file cannot be read or the sequence is invalid.
     */
    std::shared_ptr<ExposureSequence> loadSequenceFromFile(
        const std::string& filename, bool validate = true);

    /**
     * @brief Creates a sequence from a JSON object.
     * @param data The JSON object containing the sequence data.
     * @param validate Whether to validate the sequence (default: true).
     * @return The created sequence.
     * @throws SequenceException If the JSON is invalid.
     */
    std::shared_ptr<ExposureSequence> createSequenceFromJson(
        const json& data, bool validate = true);

    /**
     * @brief Creates a sequence from a template.
     * @param templateName The name of the template.
     * @param params Parameters to customize the template.
     * @return The created sequence.
     * @throws SequenceException If the template cannot be found or is invalid.
     */
    std::shared_ptr<ExposureSequence> createSequenceFromTemplate(
        const std::string& templateName, const json& params);

    /**
     * @brief Lists available sequence templates.
     * @return A vector of template names.
     */
    std::vector<std::string> listAvailableTemplates() const;

    /**
     * @brief Gets template information.
     * @param templateName The name of the template.
     * @return The template information.
     */
    std::optional<TaskGenerator::ScriptTemplate> getTemplateInfo(
        const std::string& templateName) const;

    // Sequence validation

    /**
     * @brief Validates a sequence file.
     * @param filename The path to the sequence file.
     * @param errorMessage Output parameter for error message.
     * @return True if valid, false otherwise with error message.
     */
    bool validateSequenceFile(const std::string& filename, 
                              std::string& errorMessage) const;

    /**
     * @brief Validates a sequence JSON.
     * @param data The JSON data to validate.
     * @param errorMessage Output parameter for error message.
     * @return True if valid, false otherwise with error message.
     */
    bool validateSequenceJson(const json& data, 
                              std::string& errorMessage) const;

    // Execution and control

    /**
     * @brief Executes a sequence.
     * @param sequence The sequence to execute.
     * @param async Whether to execute asynchronously (default: true).
     * @return The execution result (immediate if sync, empty if async).
     */
    std::optional<SequenceResult> executeSequence(
        std::shared_ptr<ExposureSequence> sequence, bool async = true);

    /**
     * @brief Waits for an asynchronous sequence execution to complete.
     * @param sequence The sequence being executed.
     * @param timeout Maximum wait time (0 = wait indefinitely).
     * @return The execution result, or std::nullopt if timeout.
     */
    std::optional<SequenceResult> waitForCompletion(
        std::shared_ptr<ExposureSequence> sequence, 
        std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

    /**
     * @brief Stops execution of a sequence.
     * @param sequence The sequence to stop.
     * @param graceful Whether to stop gracefully (default: true).
     */
    void stopExecution(std::shared_ptr<ExposureSequence> sequence, 
                      bool graceful = true);

    /**
     * @brief Pauses execution of a sequence.
     * @param sequence The sequence to pause.
     */
    void pauseExecution(std::shared_ptr<ExposureSequence> sequence);

    /**
     * @brief Resumes execution of a paused sequence.
     * @param sequence The sequence to resume.
     */
    void resumeExecution(std::shared_ptr<ExposureSequence> sequence);

    // Database operations

    /**
     * @brief Saves a sequence to the database.
     * @param sequence The sequence to save.
     * @return The UUID of the saved sequence.
     */
    std::string saveToDatabase(std::shared_ptr<ExposureSequence> sequence);

    /**
     * @brief Loads a sequence from the database.
     * @param uuid The UUID of the sequence.
     * @return The loaded sequence.
     */
    std::shared_ptr<ExposureSequence> loadFromDatabase(const std::string& uuid);

    /**
     * @brief Lists all sequences in the database.
     * @return A vector of sequence models.
     */
    std::vector<SequenceModel> listSequences() const;

    /**
     * @brief Deletes a sequence from the database.
     * @param uuid The UUID of the sequence.
     */
    void deleteFromDatabase(const std::string& uuid);

    // Configuration and settings

    /**
     * @brief Updates the manager's configuration.
     * @param options The new options.
     */
    void updateConfiguration(const SequenceOptions& options);

    /**
     * @brief Gets the current configuration.
     * @return The current options.
     */
    const SequenceOptions& getConfiguration() const;

    /**
     * @brief Registers a task template.
     * @param name The template name.
     * @param templateInfo The template information.
     */
    void registerTaskTemplate(const std::string& name,
                              const TaskGenerator::ScriptTemplate& templateInfo);

    /**
     * @brief Registers built-in task templates.
     */
    void registerBuiltInTaskTemplates();

    /**
     * @brief Loads task templates from a directory.
     * @param directory The directory path.
     * @return The number of templates loaded.
     */
    size_t loadTemplatesFromDirectory(const std::string& directory);

    // Macro management

    /**
     * @brief Adds a global macro.
     * @param name The macro name.
     * @param value The macro value.
     */
    void addGlobalMacro(const std::string& name, MacroValue value);

    /**
     * @brief Removes a global macro.
     * @param name The macro name.
     */
    void removeGlobalMacro(const std::string& name);

    /**
     * @brief Lists all global macros.
     * @return A vector of macro names.
     */
    std::vector<std::string> listGlobalMacros() const;

    // Event handling

    /**
     * @brief Sets a callback for sequence start events.
     * @param callback The callback function.
     */
    void setOnSequenceStart(std::function<void(const std::string&)> callback);

    /**
     * @brief Sets a callback for sequence end events.
     * @param callback The callback function.
     */
    void setOnSequenceEnd(std::function<void(const std::string&, bool)> callback);

    /**
     * @brief Sets a callback for target start events.
     * @param callback The callback function.
     */
    void setOnTargetStart(std::function<void(const std::string&, const std::string&)> callback);

    /**
     * @brief Sets a callback for target end events.
     * @param callback The callback function.
     */
    void setOnTargetEnd(
        std::function<void(const std::string&, const std::string&, TargetStatus)> callback);

    /**
     * @brief Sets a callback for error events.
     * @param callback The callback function.
     */
    void setOnError(
        std::function<void(const std::string&, const std::string&, const std::exception&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;  ///< Pimpl for implementation details
};

}  // namespace lithium::task

#endif  // LITHIUM_TASK_SEQUENCE_MANAGER_HPP
