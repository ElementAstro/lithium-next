/**
 * @file generator.hpp
 * @brief Task Generator
 *
 * This file contains the definition of a task generator.
 *
 * @date 2023-07-21
 * @modified 2024-04-27
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_GENERATOR_HPP
#define LITHIUM_TASK_GENERATOR_HPP

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "atom/type/json.hpp"
using json = nlohmann::json;

namespace lithium {

/**
 * @brief Exception class for TaskGenerator errors.
 */
class TaskGeneratorException : public std::exception {
public:
    /**
     * @brief Error codes for TaskGeneratorException.
     */
    enum class ErrorCode {
        UNDEFINED_MACRO,         ///< Undefined macro error.
        INVALID_MACRO_ARGS,      ///< Invalid macro arguments error.
        MACRO_EVALUATION_ERROR,  ///< Macro evaluation error.
        JSON_PROCESSING_ERROR,   ///< JSON processing error.
        INVALID_MACRO_TYPE,      ///< Invalid macro type error.
        CACHE_ERROR              ///< Cache error.
    };

    /**
     * @brief Constructor for TaskGeneratorException.
     * @param code The error code.
     * @param message The error message.
     */
    TaskGeneratorException(ErrorCode code, const std::string& message)
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

using MacroValue =
    std::variant<std::string,
                 std::function<std::string(const std::vector<std::string>&)>>;

/**
 * @brief TaskGenerator class for generating tasks based on macros.
 */
class TaskGenerator {
public:
    /**
     * @brief Constructor for TaskGenerator.
     */
    TaskGenerator();

    /**
     * @brief Destructor for TaskGenerator.
     */
    ~TaskGenerator();

    /**
     * @brief Create a shared pointer to a TaskGenerator instance.
     * @return A shared pointer to a TaskGenerator instance.
     */
    static auto createShared() -> std::shared_ptr<TaskGenerator>;

    /**
     * @brief Add a macro to the generator.
     * @param name The name of the macro.
     * @param value The value of the macro.
     */
    void addMacro(const std::string& name, MacroValue value);

    /**
     * @brief Remove a macro from the generator.
     * @param name The name of the macro to remove.
     */
    void removeMacro(const std::string& name);

    /**
     * @brief List all macros in the generator.
     * @return A vector of macro names.
     */
    std::vector<std::string> listMacros() const;

    /**
     * @brief Process a JSON object with the current macros.
     * @param j The JSON object to process.
     */
    void processJson(json& j) const;

    /**
     * @brief Process a JSON object with JSON-defined macros.
     * @param j The JSON object to process.
     */
    void processJsonWithJsonMacros(json& j);

    /**
     * @brief Clear the macro cache.
     */
    void clearMacroCache();

    /**
     * @brief Check if a macro exists.
     * @param name The name of the macro.
     * @return True if the macro exists, false otherwise.
     */
    bool hasMacro(const std::string& name) const;

    /**
     * @brief Get the size of the macro cache.
     * @return The size of the macro cache.
     */
    size_t getCacheSize() const;

    /**
     * @brief Set the maximum size of the macro cache.
     * @param size The maximum size of the macro cache.
     */
    void setMaxCacheSize(size_t size);

    /**
     * @brief Structure for performance statistics.
     */
    struct Statistics {
        size_t cache_hits = 0;                 ///< Number of cache hits.
        size_t cache_misses = 0;               ///< Number of cache misses.
        size_t macro_evaluations = 0;          ///< Number of macro evaluations.
        double average_evaluation_time = 0.0;  ///< Average evaluation time.
    };

    /**
     * @brief Get the performance statistics.
     * @return The performance statistics.
     */
    Statistics getStatistics() const;

    /**
     * @brief Reset the performance statistics.
     */
    void resetStatistics();

    // ==================== Script Generation Extensions ====================

    /**
     * @brief Script generation configuration
     */
    struct ScriptConfig {
        std::string templatePath;  ///< Path to script templates
        std::string outputFormat =
            "json";                      ///< Output format (json, yaml, etc.)
        bool enableValidation = true;    ///< Enable script validation
        bool enableOptimization = true;  ///< Enable script optimization
        std::vector<std::string> includePaths;  ///< Include paths for scripts
        std::unordered_map<std::string, std::string>
            globalVariables;  ///< Global variables
    };

    /**
     * @brief Script generation result
     */
    struct ScriptGenerationResult {
        std::string generatedScript;        ///< Generated script content
        std::vector<std::string> warnings;  ///< Generation warnings
        std::vector<std::string> errors;    ///< Generation errors
        json metadata;                      ///< Script metadata
        bool success = false;               ///< Generation success flag
    };

    /**
     * @brief Script template information
     */
    struct ScriptTemplate {
        std::string name;                         ///< Template name
        std::string description;                  ///< Template description
        std::string content;                      ///< Template content
        std::vector<std::string> requiredParams;  ///< Required parameters
        json parameterSchema;                     ///< Parameter schema
        std::string category;                     ///< Template category
        std::string version;                      ///< Template version
    };

    /**
     * @brief Set script generation configuration
     * @param config The script configuration
     */
    void setScriptConfig(const ScriptConfig& config);

    /**
     * @brief Get current script configuration
     * @return The current script configuration
     */
    ScriptConfig getScriptConfig() const;

    /**
     * @brief Register a script template
     * @param templateName The name of the template
     * @param templateInfo The template information
     */
    void registerScriptTemplate(const std::string& templateName,
                                const ScriptTemplate& templateInfo);

    /**
     * @brief Unregister a script template
     * @param templateName The name of the template to unregister
     */
    void unregisterScriptTemplate(const std::string& templateName);

    /**
     * @brief Generate script from template
     * @param templateName The name of the template to use
     * @param parameters The parameters for script generation
     * @return Script generation result
     */
    ScriptGenerationResult generateScript(const std::string& templateName,
                                          const json& parameters);

    /**
     * @brief Generate sequence script from JSON configuration
     * @param sequenceConfig The sequence configuration
     * @return Generated script result
     */
    ScriptGenerationResult generateSequenceScript(const json& sequenceConfig);

    /**
     * @brief Parse and validate script
     * @param script The script content to parse
     * @param format The script format (json, yaml, etc.)
     * @return Parsing result with validation information
     */
    ScriptGenerationResult parseScript(const std::string& script,
                                       const std::string& format = "json");

    /**
     * @brief Get list of available script templates
     * @return Vector of template names
     */
    std::vector<std::string> getAvailableTemplates() const;

    /**
     * @brief Get script template information
     * @param templateName The name of the template
     * @return Template information if found
     */
    std::optional<ScriptTemplate> getTemplateInfo(
        const std::string& templateName) const;

    /**
     * @brief Generate custom task script
     * @param taskType The type of task to generate
     * @param taskConfig The task configuration
     * @return Generated task script
     */
    ScriptGenerationResult generateCustomTaskScript(const std::string& taskType,
                                                    const json& taskConfig);

    /**
     * @brief Optimize existing script
     * @param script The script to optimize
     * @return Optimized script result
     */
    ScriptGenerationResult optimizeScript(const std::string& script);

    /**
     * @brief Validate script syntax and structure
     * @param script The script to validate
     * @param templateName Optional template name for validation
     * @return Validation result
     */
    bool validateScript(const std::string& script,
                        const std::string& templateName = "");

    /**
     * @brief Load script templates from directory
     * @param templateDir The directory containing templates
     * @return Number of templates loaded
     */
    size_t loadTemplatesFromDirectory(const std::string& templateDir);

    /**
     * @brief Save script templates to directory
     * @param templateDir The directory to save templates
     * @return True if successful
     */
    bool saveTemplatesToDirectory(const std::string& templateDir) const;

    /**
     * @brief Convert script between formats
     * @param script The script content
     * @param fromFormat Source format
     * @param toFormat Target format
     * @return Converted script result
     */
    ScriptGenerationResult convertScriptFormat(const std::string& script,
                                               const std::string& fromFormat,
                                               const std::string& toFormat);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;  ///< Pimpl for encapsulation
};

}  // namespace lithium

#endif  // LITHIUM_TASK_GENERATOR_HPP