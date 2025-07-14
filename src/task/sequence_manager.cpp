/**
 * @file sequence_manager.cpp
 * @brief Implementation of the central manager for the task sequence system
 *
 * @date 2025-07-11
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2025 Max Qian
 */

#include "sequence_manager.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <unordered_map>

#include "spdlog/spdlog.h"

namespace lithium::task {

namespace fs = std::filesystem;

// Helper functions
namespace {
/**
 * @brief Converts a SequenceException to a detailed log message
 * @param e The exception to convert
 * @return Formatted error message with code and message
 */
std::string formatSequenceException(const SequenceException& e) {
    std::string codeStr;
    switch (e.code()) {
        case SequenceException::ErrorCode::FILE_ERROR:
            codeStr = "FILE_ERROR";
            break;
        case SequenceException::ErrorCode::VALIDATION_ERROR:
            codeStr = "VALIDATION_ERROR";
            break;
        case SequenceException::ErrorCode::GENERATION_ERROR:
            codeStr = "GENERATION_ERROR";
            break;
        case SequenceException::ErrorCode::EXECUTION_ERROR:
            codeStr = "EXECUTION_ERROR";
            break;
        case SequenceException::ErrorCode::DEPENDENCY_ERROR:
            codeStr = "DEPENDENCY_ERROR";
            break;
        case SequenceException::ErrorCode::TEMPLATE_ERROR:
            codeStr = "TEMPLATE_ERROR";
            break;
        case SequenceException::ErrorCode::DATABASE_ERROR:
            codeStr = "DATABASE_ERROR";
            break;
        case SequenceException::ErrorCode::CONFIGURATION_ERROR:
            codeStr = "CONFIGURATION_ERROR";
            break;
        default:
            codeStr = "UNKNOWN_ERROR";
    }
    return "SequenceException [" + codeStr + "]: " + e.what();
}

/**
 * @brief Detects the format of a sequence file based on content
 * @param content The file content to analyze
 * @return The detected format
 */
ExposureSequence::SerializationFormat detectFormatFromContent(
    const std::string& content) {
    // Simple heuristic-based detection
    // Look for format clues in the first 100 characters
    std::string sample =
        content.substr(0, std::min(content.size(), static_cast<size_t>(100)));

    // Check for binary marker (hypothetical)
    if (sample.find("\x1BLITH") != std::string::npos) {
        return ExposureSequence::SerializationFormat::BINARY;
    }

    // Check for JSON5 (comments)
    if (sample.find("//") != std::string::npos ||
        sample.find("/*") != std::string::npos) {
        return ExposureSequence::SerializationFormat::JSON5;
    }

    // Check whitespace for format
    size_t newlines = std::count(sample.begin(), sample.end(), '\n');
    if (newlines > 5) {
        return ExposureSequence::SerializationFormat::PRETTY_JSON;
    }

    // Default to standard JSON
    return ExposureSequence::SerializationFormat::JSON;
}

/**
 * @brief Detects format from file extension
 * @param filename The filename to analyze
 * @return The detected format
 */
ExposureSequence::SerializationFormat detectFormatFromExtension(
    const std::string& filename) {
    std::string extension = fs::path(filename).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (extension == ".json5") {
        return ExposureSequence::SerializationFormat::JSON5;
    } else if (extension == ".min.json") {
        return ExposureSequence::SerializationFormat::COMPACT_JSON;
    } else if (extension == ".bin") {
        return ExposureSequence::SerializationFormat::BINARY;
    } else {
        // Default to pretty JSON
        return ExposureSequence::SerializationFormat::PRETTY_JSON;
    }
}

/**
 * @brief Reads content from a file with proper error handling
 * @param filename The file to read
 * @return The file content
 * @throws SequenceException if file cannot be read
 */
std::string readFileContent(const std::string& filename) {
    try {
        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            throw SequenceException(SequenceException::ErrorCode::FILE_ERROR,
                                    "Failed to open file: " + filename);
        }

        return std::string((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
    } catch (const std::exception& e) {
        throw SequenceException(
            SequenceException::ErrorCode::FILE_ERROR,
            "Error reading file: " + filename + " - " + e.what());
    }
}
}  // namespace

class SequenceManager::Impl {
public:
    Impl(const SequenceOptions& options)
        : options_(options), taskGenerator_(TaskGenerator::createShared()) {
        // Initialize task generator
        initializeTaskGenerator();

        // Load templates if directory is provided
        if (!options.templateDirectory.empty() &&
            fs::exists(options.templateDirectory)) {
            try {
                size_t loaded = taskGenerator_->loadTemplatesFromDirectory(
                    options.templateDirectory);
                spdlog::info("Loaded {} sequence templates from directory: {}",
                             loaded, options.templateDirectory);
            } catch (const std::exception& e) {
                spdlog::warn("Failed to load templates from directory: {} - {}",
                             options.templateDirectory, e.what());
            }
        }

        // Register built-in task templates
        registerBuiltInTaskTemplates();
    }

    ~Impl() {
        // Stop and clean up any running sequences
        for (auto& [id, future] : runningSequenceFutures_) {
            try {
                if (future.valid() && future.wait_for(std::chrono::milliseconds(
                                          100)) != std::future_status::ready) {
                    spdlog::debug("Abandoning sequence execution: {}", id);
                }
            } catch (const std::exception& e) {
                spdlog::error("Error while cleaning up sequence: {} - {}", id,
                              e.what());
            }
        }
    }

    std::shared_ptr<ExposureSequence> createSequence(const std::string& name) {
        auto sequence = std::make_shared<ExposureSequence>();

        // Apply options to the new sequence
        applyOptionsToSequence(sequence);

        // Set the task generator
        sequence->setTaskGenerator(taskGenerator_);

        return sequence;
    }

    std::shared_ptr<ExposureSequence> loadSequenceFromFile(
        const std::string& filename, bool validate) {
        try {
            // Check if file exists
            if (!fs::exists(filename)) {
                throw SequenceException(
                    SequenceException::ErrorCode::FILE_ERROR,
                    "Sequence file not found: " + filename);
            }

            // Read file content
            std::string content = readFileContent(filename);

            // Detect format
            ExposureSequence::SerializationFormat format =
                detectFormatFromExtension(filename);

            // Create sequence
            auto sequence = std::make_shared<ExposureSequence>();

            // Apply options to the new sequence
            applyOptionsToSequence(sequence);

            // Set the task generator
            sequence->setTaskGenerator(taskGenerator_);

            // Load sequence from file
            sequence->loadSequence(filename, true);

            // Validate if required
            if (validate) {
                std::string errorMessage;
                if (!sequence->validateSequenceFile(filename)) {
                    throw SequenceException(
                        SequenceException::ErrorCode::VALIDATION_ERROR,
                        "Sequence validation failed: " + errorMessage);
                }
            }

            return sequence;
        } catch (const SequenceException& e) {
            spdlog::error(formatSequenceException(e));
            throw;
        } catch (const std::exception& e) {
            std::string errorMsg =
                "Failed to load sequence from file: " + filename + " - " +
                e.what();
            spdlog::error(errorMsg);
            throw SequenceException(SequenceException::ErrorCode::FILE_ERROR,
                                    errorMsg);
        }
    }

    std::shared_ptr<ExposureSequence> createSequenceFromJson(const json& data,
                                                             bool validate) {
        try {
            // Create sequence
            auto sequence = std::make_shared<ExposureSequence>();

            // Apply options to the new sequence
            applyOptionsToSequence(sequence);

            // Set the task generator
            sequence->setTaskGenerator(taskGenerator_);

            // First validate if required
            if (validate) {
                std::string errorMessage;
                if (!sequence->validateSequenceJson(data, errorMessage)) {
                    throw SequenceException(
                        SequenceException::ErrorCode::VALIDATION_ERROR,
                        "Sequence validation failed: " + errorMessage);
                }
            }

            // Load from the validated JSON by first serializing to file and
            // then loading This avoids needing direct access to the private
            // deserializeFromJson method
            std::string tempFilePath;
            std::ofstream tempFileStream;
            {
                // Use the system temp directory and a random filename
                auto tempDir = fs::temp_directory_path();
                std::string randomName =
                    "lithium_seq_" + std::to_string(std::rand()) + ".json";
                tempFilePath = (tempDir / randomName).string();

                tempFileStream.open(tempFilePath,
                                    std::ios::out | std::ios::trunc);
                if (!tempFileStream.is_open()) {
                    throw SequenceException(
                        SequenceException::ErrorCode::FILE_ERROR,
                        "Failed to create temporary file for sequence JSON: " +
                            tempFilePath);
                }
            }

            try {
                // Write JSON to temporary file
                tempFileStream << data.dump(2);  // Pretty format
                tempFileStream.close();

                // Load from the file
                sequence->loadSequence(tempFilePath, false);

                // Clean up temporary file
                std::filesystem::remove(tempFilePath);
            } catch (...) {
                // Clean up on any exception
                std::filesystem::remove(tempFilePath);
                throw;
            }

            return sequence;
        } catch (const SequenceException& e) {
            spdlog::error(formatSequenceException(e));
            throw;
        } catch (const std::exception& e) {
            std::string errorMsg =
                "Failed to create sequence from JSON: " + std::string(e.what());
            spdlog::error(errorMsg);
            throw SequenceException(
                SequenceException::ErrorCode::GENERATION_ERROR, errorMsg);
        }
    }

    std::shared_ptr<ExposureSequence> createSequenceFromTemplate(
        const std::string& templateName, const json& params) {
        try {
            // Check if template exists
            auto templateInfo = taskGenerator_->getTemplateInfo(templateName);
            if (!templateInfo) {
                throw SequenceException(
                    SequenceException::ErrorCode::TEMPLATE_ERROR,
                    "Template not found: " + templateName);
            }

            // Generate sequence script from template
            auto result = taskGenerator_->generateScript(templateName, params);
            if (!result.success) {
                std::string errorMsg =
                    "Failed to generate sequence from template: ";
                if (!result.errors.empty()) {
                    errorMsg += result.errors[0];
                } else {
                    errorMsg += "unknown error";
                }
                throw SequenceException(
                    SequenceException::ErrorCode::TEMPLATE_ERROR, errorMsg);
            }

            // Parse the generated script
            json sequenceJson;
            try {
                sequenceJson = json::parse(result.generatedScript);
            } catch (const json::exception& e) {
                throw SequenceException(
                    SequenceException::ErrorCode::TEMPLATE_ERROR,
                    "Failed to parse generated sequence: " +
                        std::string(e.what()));
            }

            // Create sequence from the generated JSON
            return createSequenceFromJson(sequenceJson, true);

        } catch (const SequenceException& e) {
            spdlog::error(formatSequenceException(e));
            throw;
        } catch (const std::exception& e) {
            std::string errorMsg =
                "Failed to create sequence from template: " + templateName +
                " - " + e.what();
            spdlog::error(errorMsg);
            throw SequenceException(
                SequenceException::ErrorCode::TEMPLATE_ERROR, errorMsg);
        }
    }

    std::vector<std::string> listAvailableTemplates() const {
        return taskGenerator_->getAvailableTemplates();
    }

    std::optional<TaskGenerator::ScriptTemplate> getTemplateInfo(
        const std::string& templateName) const {
        return taskGenerator_->getTemplateInfo(templateName);
    }

    bool validateSequenceFile(const std::string& filename,
                              std::string& errorMessage) const {
        try {
            // Create a temporary sequence for validation
            ExposureSequence sequence;
            return sequence.validateSequenceFile(filename);
        } catch (const std::exception& e) {
            errorMessage = e.what();
            return false;
        }
    }

    bool validateSequenceJson(const json& data,
                              std::string& errorMessage) const {
        try {
            // Create a temporary sequence for validation
            ExposureSequence sequence;
            return sequence.validateSequenceJson(data, errorMessage);
        } catch (const std::exception& e) {
            errorMessage = e.what();
            return false;
        }
    }

    std::optional<SequenceResult> executeSequence(
        std::shared_ptr<ExposureSequence> sequence, bool async) {
        // Generate a unique ID for this execution
        std::string executionId = std::to_string(nextExecutionId_++);

        // Set up execution callbacks
        setupExecutionCallbacks(sequence, executionId);

        if (async) {
            // Start async execution
            auto future =
                std::async(std::launch::async, [this, sequence, executionId]() {
                    return executeSequenceInternal(sequence, executionId);
                });

            // Store future
            std::unique_lock<std::mutex> lock(futuresMutex_);
            runningSequenceFutures_[executionId] = std::move(future);

            // Return empty result for async execution
            return std::nullopt;
        } else {
            // Execute synchronously and return result
            return executeSequenceInternal(sequence, executionId);
        }
    }

    std::optional<SequenceResult> waitForCompletion(
        std::shared_ptr<ExposureSequence> sequence,
        std::chrono::milliseconds timeout) {
        // Find the future for this sequence
        std::string executionId;
        std::future<SequenceResult> future;

        {
            std::unique_lock<std::mutex> lock(futuresMutex_);
            for (auto& [id, fut] : runningSequenceFutures_) {
                // We identify by sequence pointer for now
                // In a more sophisticated system, we'd store the
                // sequence-to-execution mapping
                if (sequenceExecutions_[id] == sequence.get()) {
                    executionId = id;
                    future = std::move(fut);
                    break;
                }
            }
        }

        if (!future.valid()) {
            spdlog::error("No running execution found for sequence");
            return std::nullopt;
        }

        // Wait for completion with timeout
        if (timeout.count() == 0) {
            // Wait indefinitely
            try {
                SequenceResult result = future.get();

                // Clean up
                std::unique_lock<std::mutex> lock(futuresMutex_);
                runningSequenceFutures_.erase(executionId);
                sequenceExecutions_.erase(executionId);

                return result;
            } catch (const std::exception& e) {
                spdlog::error("Error waiting for sequence completion: {}",
                              e.what());

                // Clean up
                std::unique_lock<std::mutex> lock(futuresMutex_);
                runningSequenceFutures_.erase(executionId);
                sequenceExecutions_.erase(executionId);

                return std::nullopt;
            }
        } else {
            // Wait with timeout
            auto status = future.wait_for(timeout);
            if (status == std::future_status::ready) {
                try {
                    SequenceResult result = future.get();

                    // Clean up
                    std::unique_lock<std::mutex> lock(futuresMutex_);
                    runningSequenceFutures_.erase(executionId);
                    sequenceExecutions_.erase(executionId);

                    return result;
                } catch (const std::exception& e) {
                    spdlog::error("Error getting sequence result: {}",
                                  e.what());

                    // Clean up
                    std::unique_lock<std::mutex> lock(futuresMutex_);
                    runningSequenceFutures_.erase(executionId);
                    sequenceExecutions_.erase(executionId);

                    return std::nullopt;
                }
            } else {
                // Timeout occurred
                return std::nullopt;
            }
        }
    }

    void stopExecution(std::shared_ptr<ExposureSequence> sequence,
                       bool graceful) {
        try {
            sequence->stop();
            spdlog::info("Sequence execution stopped");
        } catch (const std::exception& e) {
            spdlog::error("Failed to stop sequence: {}", e.what());
        }
    }

    void pauseExecution(std::shared_ptr<ExposureSequence> sequence) {
        try {
            sequence->pause();
            spdlog::info("Sequence execution paused");
        } catch (const std::exception& e) {
            spdlog::error("Failed to pause sequence: {}", e.what());
        }
    }

    void resumeExecution(std::shared_ptr<ExposureSequence> sequence) {
        try {
            sequence->resume();
            spdlog::info("Sequence execution resumed");
        } catch (const std::exception& e) {
            spdlog::error("Failed to resume sequence: {}", e.what());
        }
    }

    std::string saveToDatabase(std::shared_ptr<ExposureSequence> sequence) {
        try {
            sequence->saveToDatabase();
            // Return the UUID of the saved sequence
            // This is a placeholder - in the actual implementation, we'd get
            // this from the sequence
            return "sequence-uuid";
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::DATABASE_ERROR,
                "Failed to save sequence to database: " +
                    std::string(e.what()));
        }
    }

    std::shared_ptr<ExposureSequence> loadFromDatabase(
        const std::string& uuid) {
        try {
            // Create a new sequence
            auto sequence = std::make_shared<ExposureSequence>();

            // Apply options
            applyOptionsToSequence(sequence);

            // Load from database
            sequence->loadFromDatabase(uuid);

            return sequence;
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::DATABASE_ERROR,
                "Failed to load sequence from database: " +
                    std::string(e.what()));
        }
    }

    std::vector<SequenceModel> listSequences() const {
        try {
            // Create a temporary sequence to access the database
            ExposureSequence sequence;
            return sequence.listSequences();
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::DATABASE_ERROR,
                "Failed to list sequences: " + std::string(e.what()));
        }
    }

    void deleteFromDatabase(const std::string& uuid) {
        try {
            // Create a temporary sequence to access the database
            ExposureSequence sequence;
            sequence.deleteFromDatabase(uuid);
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::DATABASE_ERROR,
                "Failed to delete sequence: " + std::string(e.what()));
        }
    }

    void updateConfiguration(const SequenceOptions& options) {
        options_ = options;

        // Update task generator configuration
        updateTaskGeneratorConfig();
    }

    const SequenceOptions& getConfiguration() const { return options_; }

    void registerTaskTemplate(
        const std::string& name,
        const TaskGenerator::ScriptTemplate& templateInfo) {
        try {
            taskGenerator_->registerScriptTemplate(name, templateInfo);
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::CONFIGURATION_ERROR,
                "Failed to register task template: " + std::string(e.what()));
        }
    }

    void registerBuiltInTaskTemplates() {
        // Register basic exposure template
        TaskGenerator::ScriptTemplate basicExposureTemplate{
            .name = "BasicExposure",
            .description = "Basic exposure sequence template",
            .content = R"({
                "targets": [
                    {
                        "name": "{{targetName}}",
                        "enabled": true,
                        "maxRetries": 3,
                        "cooldown": 5,
                        "tasks": [
                            {
                                "name": "Exposure",
                                "type": "TakeExposure",
                                "params": {
                                    "exposure": {{exposureTime}},
                                    "type": "{{frameType}}",
                                    "binning": {{binning}},
                                    "gain": {{gain}},
                                    "offset": {{offset}}
                                }
                            }
                        ]
                    }
                ],
                "state": 0,
                "maxConcurrentTargets": 1
            })",
            .requiredParams = {"targetName", "exposureTime", "frameType",
                               "binning", "gain", "offset"},
            .parameterSchema = json::parse(R"({
                "targetName": {"type": "string", "description": "Name of the target"},
                "exposureTime": {"type": "number", "minimum": 0.001, "description": "Exposure time in seconds"},
                "frameType": {"type": "string", "enum": ["light", "dark", "bias", "flat"], "description": "Type of frame to capture"},
                "binning": {"type": "integer", "minimum": 1, "default": 1, "description": "Binning factor"},
                "gain": {"type": "integer", "minimum": 0, "default": 0, "description": "Camera gain"},
                "offset": {"type": "integer", "minimum": 0, "default": 10, "description": "Camera offset"}
            })"),
            .category = "Exposure",
            .version = "1.0.0"};

        // Register multiple exposure template
        TaskGenerator::ScriptTemplate multipleExposureTemplate{
            .name = "MultipleExposure",
            .description = "Multiple exposure sequence template",
            .content = R"({
                "targets": [
                    {
                        "name": "{{targetName}}",
                        "enabled": true,
                        "maxRetries": 3,
                        "cooldown": 5,
                        "tasks": [
                            {
                                "name": "MultipleExposure",
                                "type": "TakeManyExposure",
                                "params": {
                                    "count": {{count}},
                                    "exposure": {{exposureTime}},
                                    "type": "{{frameType}}",
                                    "binning": {{binning}},
                                    "gain": {{gain}},
                                    "offset": {{offset}}
                                }
                            }
                        ]
                    }
                ],
                "state": 0,
                "maxConcurrentTargets": 1
            })",
            .requiredParams = {"targetName", "count", "exposureTime",
                               "frameType", "binning", "gain", "offset"},
            .parameterSchema = json::parse(R"({
                "targetName": {"type": "string", "description": "Name of the target"},
                "count": {"type": "integer", "minimum": 1, "description": "Number of exposures to take"},
                "exposureTime": {"type": "number", "minimum": 0.001, "description": "Exposure time in seconds"},
                "frameType": {"type": "string", "enum": ["light", "dark", "bias", "flat"], "description": "Type of frame to capture"},
                "binning": {"type": "integer", "minimum": 1, "default": 1, "description": "Binning factor"},
                "gain": {"type": "integer", "minimum": 0, "default": 0, "description": "Camera gain"},
                "offset": {"type": "integer", "minimum": 0, "default": 10, "description": "Camera offset"}
            })"),
            .category = "Exposure",
            .version = "1.0.0"};

        // Register the templates
        registerTaskTemplate("BasicExposure", basicExposureTemplate);
        registerTaskTemplate("MultipleExposure", multipleExposureTemplate);

        spdlog::info("Registered built-in task templates");
    }

    size_t loadTemplatesFromDirectory(const std::string& directory) {
        try {
            return taskGenerator_->loadTemplatesFromDirectory(directory);
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::CONFIGURATION_ERROR,
                "Failed to load templates: " + std::string(e.what()));
        }
    }

    void addGlobalMacro(const std::string& name, MacroValue value) {
        try {
            taskGenerator_->addMacro(name, std::move(value));
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::CONFIGURATION_ERROR,
                "Failed to add global macro: " + std::string(e.what()));
        }
    }

    void removeGlobalMacro(const std::string& name) {
        try {
            taskGenerator_->removeMacro(name);
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::CONFIGURATION_ERROR,
                "Failed to remove global macro: " + std::string(e.what()));
        }
    }

    std::vector<std::string> listGlobalMacros() const {
        try {
            return taskGenerator_->listMacros();
        } catch (const std::exception& e) {
            throw SequenceException(
                SequenceException::ErrorCode::CONFIGURATION_ERROR,
                "Failed to list global macros: " + std::string(e.what()));
        }
    }

    void setOnSequenceStart(std::function<void(const std::string&)> callback) {
        onSequenceStartCallback_ = std::move(callback);
    }

    void setOnSequenceEnd(
        std::function<void(const std::string&, bool)> callback) {
        onSequenceEndCallback_ = std::move(callback);
    }

    void setOnTargetStart(
        std::function<void(const std::string&, const std::string&)> callback) {
        onTargetStartCallback_ = std::move(callback);
    }

    void setOnTargetEnd(std::function<void(const std::string&,
                                           const std::string&, TargetStatus)>
                            callback) {
        onTargetEndCallback_ = std::move(callback);
    }

    void setOnError(std::function<void(const std::string&, const std::string&,
                                       const std::exception&)>
                        callback) {
        onErrorCallback_ = std::move(callback);
    }

private:
    SequenceOptions options_;
    std::shared_ptr<TaskGenerator> taskGenerator_;

    // Execution management
    std::mutex futuresMutex_;
    std::atomic<int> nextExecutionId_{0};
    std::unordered_map<std::string, std::future<SequenceResult>>
        runningSequenceFutures_;
    std::unordered_map<std::string, ExposureSequence*> sequenceExecutions_;

    // Callback functions
    std::function<void(const std::string&)> onSequenceStartCallback_;
    std::function<void(const std::string&, bool)> onSequenceEndCallback_;
    std::function<void(const std::string&, const std::string&)>
        onTargetStartCallback_;
    std::function<void(const std::string&, const std::string&, TargetStatus)>
        onTargetEndCallback_;
    std::function<void(const std::string&, const std::string&,
                       const std::exception&)>
        onErrorCallback_;

    // Initialize task generator
    void initializeTaskGenerator() {
        // Configure the task generator
        TaskGenerator::ScriptConfig scriptConfig;
        scriptConfig.templatePath = options_.templateDirectory;
        scriptConfig.enableValidation = options_.validateOnLoad;
        scriptConfig.outputFormat = "json";  // Default to JSON

        taskGenerator_->setScriptConfig(scriptConfig);
    }

    // Update task generator configuration
    void updateTaskGeneratorConfig() {
        TaskGenerator::ScriptConfig scriptConfig =
            taskGenerator_->getScriptConfig();
        scriptConfig.templatePath = options_.templateDirectory;
        scriptConfig.enableValidation = options_.validateOnLoad;

        taskGenerator_->setScriptConfig(scriptConfig);
    }

    // Apply options to sequence
    void applyOptionsToSequence(std::shared_ptr<ExposureSequence> sequence) {
        // Apply scheduling and recovery strategies
        sequence->setSchedulingStrategy(options_.schedulingStrategy);
        sequence->setRecoveryStrategy(options_.recoveryStrategy);

        // Apply concurrency settings
        sequence->setMaxConcurrentTargets(options_.maxConcurrentTargets);

        // Apply timeout
        if (options_.globalTimeout.count() > 0) {
            sequence->setGlobalTimeout(options_.globalTimeout);
        }
    }

    // Set up execution callbacks for a sequence
    void setupExecutionCallbacks(std::shared_ptr<ExposureSequence> sequence,
                                 const std::string& executionId) {
        // Store the sequence execution mapping
        {
            std::unique_lock<std::mutex> lock(futuresMutex_);
            sequenceExecutions_[executionId] = sequence.get();
        }

        // Set sequence callbacks
        sequence->setOnSequenceStart([this, executionId]() {
            if (onSequenceStartCallback_) {
                onSequenceStartCallback_(executionId);
            }
        });

        sequence->setOnSequenceEnd([this, executionId]() {
            if (onSequenceEndCallback_) {
                // Check success status based on failed targets
                bool success = false;
                {
                    std::unique_lock<std::mutex> lock(futuresMutex_);
                    ExposureSequence* seq = sequenceExecutions_[executionId];
                    if (seq) {
                        success = seq->getFailedTargets().empty();
                    }
                }

                onSequenceEndCallback_(executionId, success);
            }
        });

        // The ExposureSequence expects TargetCallback to have signature (name,
        // status) but our start callback doesn't have status, so provide a
        // dummy status
        sequence->setOnTargetStart(
            [this, executionId](const std::string& targetName, TargetStatus) {
                if (onTargetStartCallback_) {
                    onTargetStartCallback_(executionId, targetName);
                }
            });

        sequence->setOnTargetEnd(
            [this, executionId](const std::string& targetName,
                                TargetStatus status) {
                if (onTargetEndCallback_) {
                    onTargetEndCallback_(executionId, targetName, status);
                }
            });

        sequence->setOnError([this, executionId](const std::string& targetName,
                                                 const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(executionId, targetName, e);
            }
        });
    }

    // Internal execution function that runs the sequence and collects results
    SequenceResult executeSequenceInternal(
        std::shared_ptr<ExposureSequence> sequence,
        const std::string& executionId) {
        SequenceResult result;
        result.success = false;

        auto startTime = std::chrono::steady_clock::now();

        try {
            // Start execution
            sequence->executeAll();

            // Wait for completion (sequence.executeAll() is blocking in sync
            // mode)
            auto endTime = std::chrono::steady_clock::now();
            result.totalExecutionTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    endTime - startTime);

            // Collect results
            result.success = sequence->getFailedTargets().empty();
            result.totalProgress = sequence->getProgress();

            // Get target statuses
            for (const auto& targetName : sequence->getTargetNames()) {
                TargetStatus status = sequence->getTargetStatus(targetName);

                switch (status) {
                    case TargetStatus::Completed:
                        result.completedTargets.push_back(targetName);
                        break;
                    case TargetStatus::Failed:
                        result.failedTargets.push_back(targetName);
                        break;
                    case TargetStatus::Skipped:
                        result.skippedTargets.push_back(targetName);
                        break;
                    default:
                        // Other statuses are not relevant for the final result
                        break;
                }
            }

            // Get execution statistics
            result.executionStats = sequence->getExecutionStats();

        } catch (const std::exception& e) {
            result.success = false;
            result.errors.push_back(e.what());

            spdlog::error("Error executing sequence {}: {}", executionId,
                          e.what());

            // Calculate execution time even for failed sequences
            auto endTime = std::chrono::steady_clock::now();
            result.totalExecutionTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    endTime - startTime);
        }

        // Clean up
        {
            std::unique_lock<std::mutex> lock(futuresMutex_);
            sequenceExecutions_.erase(executionId);
        }

        return result;
    }
};

// Implementation of SequenceManager methods

SequenceManager::SequenceManager(const SequenceOptions& options)
    : impl_(std::make_unique<Impl>(options)) {}

SequenceManager::~SequenceManager() = default;

std::shared_ptr<SequenceManager> SequenceManager::createShared(
    const SequenceOptions& options) {
    return std::make_shared<SequenceManager>(options);
}

std::shared_ptr<ExposureSequence> SequenceManager::createSequence(
    const std::string& name) {
    return impl_->createSequence(name);
}

std::shared_ptr<ExposureSequence> SequenceManager::loadSequenceFromFile(
    const std::string& filename, bool validate) {
    return impl_->loadSequenceFromFile(filename, validate);
}

std::shared_ptr<ExposureSequence> SequenceManager::createSequenceFromJson(
    const json& data, bool validate) {
    return impl_->createSequenceFromJson(data, validate);
}

std::shared_ptr<ExposureSequence> SequenceManager::createSequenceFromTemplate(
    const std::string& templateName, const json& params) {
    return impl_->createSequenceFromTemplate(templateName, params);
}

std::vector<std::string> SequenceManager::listAvailableTemplates() const {
    return impl_->listAvailableTemplates();
}

std::optional<TaskGenerator::ScriptTemplate> SequenceManager::getTemplateInfo(
    const std::string& templateName) const {
    return impl_->getTemplateInfo(templateName);
}

bool SequenceManager::validateSequenceFile(const std::string& filename,
                                           std::string& errorMessage) const {
    return impl_->validateSequenceFile(filename, errorMessage);
}

bool SequenceManager::validateSequenceJson(const json& data,
                                           std::string& errorMessage) const {
    return impl_->validateSequenceJson(data, errorMessage);
}

std::optional<SequenceResult> SequenceManager::executeSequence(
    std::shared_ptr<ExposureSequence> sequence, bool async) {
    return impl_->executeSequence(sequence, async);
}

std::optional<SequenceResult> SequenceManager::waitForCompletion(
    std::shared_ptr<ExposureSequence> sequence,
    std::chrono::milliseconds timeout) {
    return impl_->waitForCompletion(sequence, timeout);
}

void SequenceManager::stopExecution(std::shared_ptr<ExposureSequence> sequence,
                                    bool graceful) {
    impl_->stopExecution(sequence, graceful);
}

void SequenceManager::pauseExecution(
    std::shared_ptr<ExposureSequence> sequence) {
    impl_->pauseExecution(sequence);
}

void SequenceManager::resumeExecution(
    std::shared_ptr<ExposureSequence> sequence) {
    impl_->resumeExecution(sequence);
}

std::string SequenceManager::saveToDatabase(
    std::shared_ptr<ExposureSequence> sequence) {
    return impl_->saveToDatabase(sequence);
}

std::shared_ptr<ExposureSequence> SequenceManager::loadFromDatabase(
    const std::string& uuid) {
    return impl_->loadFromDatabase(uuid);
}

std::vector<SequenceModel> SequenceManager::listSequences() const {
    return impl_->listSequences();
}

void SequenceManager::deleteFromDatabase(const std::string& uuid) {
    impl_->deleteFromDatabase(uuid);
}

void SequenceManager::updateConfiguration(const SequenceOptions& options) {
    impl_->updateConfiguration(options);
}

const SequenceOptions& SequenceManager::getConfiguration() const {
    return impl_->getConfiguration();
}

void SequenceManager::registerTaskTemplate(
    const std::string& name,
    const TaskGenerator::ScriptTemplate& templateInfo) {
    impl_->registerTaskTemplate(name, templateInfo);
}

void SequenceManager::registerBuiltInTaskTemplates() {
    impl_->registerBuiltInTaskTemplates();
}

size_t SequenceManager::loadTemplatesFromDirectory(
    const std::string& directory) {
    return impl_->loadTemplatesFromDirectory(directory);
}

void SequenceManager::addGlobalMacro(const std::string& name,
                                     MacroValue value) {
    impl_->addGlobalMacro(name, std::move(value));
}

void SequenceManager::removeGlobalMacro(const std::string& name) {
    impl_->removeGlobalMacro(name);
}

std::vector<std::string> SequenceManager::listGlobalMacros() const {
    return impl_->listGlobalMacros();
}

void SequenceManager::setOnSequenceStart(
    std::function<void(const std::string&)> callback) {
    impl_->setOnSequenceStart(std::move(callback));
}

void SequenceManager::setOnSequenceEnd(
    std::function<void(const std::string&, bool)> callback) {
    impl_->setOnSequenceEnd(std::move(callback));
}

void SequenceManager::setOnTargetStart(
    std::function<void(const std::string&, const std::string&)> callback) {
    impl_->setOnTargetStart(std::move(callback));
}

void SequenceManager::setOnTargetEnd(
    std::function<void(const std::string&, const std::string&, TargetStatus)>
        callback) {
    impl_->setOnTargetEnd(std::move(callback));
}

void SequenceManager::setOnError(
    std::function<void(const std::string&, const std::string&,
                       const std::exception&)>
        callback) {
    impl_->setOnError(std::move(callback));
}

}  // namespace lithium::task
