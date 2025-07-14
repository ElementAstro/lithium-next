/**
 * @file sequencer.cpp
 * @brief Implementation of the task sequencer for managing target execution.
 */

#include "sequencer.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>
#include <stack>

#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

#include "config/config_serializer.hpp"
#include "constant/constant.hpp"
#include "registration.hpp"
#include "uuid.hpp"

namespace {
// Forward declarations for helper functions
json convertTargetToStandardFormat(const json& targetJson);
json convertBetweenSchemaVersions(const json& sourceJson,
                                  const std::string& sourceVersion,
                                  const std::string& targetVersion);

lithium::SerializationFormat convertFormat(
    lithium::task::ExposureSequence::SerializationFormat format) {
    switch (format) {
        case lithium::task::ExposureSequence::SerializationFormat::JSON:
            return lithium::SerializationFormat::JSON;
        case lithium::task::ExposureSequence::SerializationFormat::COMPACT_JSON:
            return lithium::SerializationFormat::COMPACT_JSON;
        case lithium::task::ExposureSequence::SerializationFormat::PRETTY_JSON:
            return lithium::SerializationFormat::PRETTY_JSON;
        case lithium::task::ExposureSequence::SerializationFormat::JSON5:
            return lithium::SerializationFormat::JSON5;
        case lithium::task::ExposureSequence::SerializationFormat::BINARY:
            return lithium::SerializationFormat::BINARY_JSON;
        default:
            return lithium::SerializationFormat::PRETTY_JSON;
    }
}

/**
 * @brief Convert a specific target format to a common JSON format
 * @param targetJson The target-specific JSON data
 * @return Standardized JSON format
 */
json convertTargetToStandardFormat(const json& targetJson) {
    // Create a standardized format
    json standardJson = targetJson;

    // Handle version differences
    if (!standardJson.contains("version")) {
        standardJson["version"] = "2.0.0";
    }

    // Ensure essential fields exist
    if (!standardJson.contains("uuid") || standardJson["uuid"].is_null()) {
        standardJson["uuid"] = atom::utils::UUID().toString();
    }

    // Ensure tasks array exists
    if (!standardJson.contains("tasks")) {
        standardJson["tasks"] = json::array();
    }

    // Standardize task format
    for (auto& taskJson : standardJson["tasks"]) {
        if (!taskJson.contains("version")) {
            taskJson["version"] = "2.0.0";
        }

        // Ensure task has a UUID
        if (!taskJson.contains("uuid")) {
            taskJson["uuid"] = atom::utils::UUID().toString();
        }
    }

    return standardJson;
}

/**
 * @brief Convert a JSON object from one schema to another
 * @param sourceJson Source JSON object
 * @param sourceVersion Source schema version
 * @param targetVersion Target schema version
 * @return Converted JSON object
 */
json convertBetweenSchemaVersions(const json& sourceJson,
                                  const std::string& sourceVersion,
                                  const std::string& targetVersion) {
    // If versions match, no conversion needed
    if (sourceVersion == targetVersion) {
        return sourceJson;
    }

    json result = sourceJson;

    // Handle specific version upgrades
    if (sourceVersion == "1.0.0" && targetVersion == "2.0.0") {
        // Upgrade from 1.0 to 2.0
        result["version"] = "2.0.0";

        // Add additional fields for 2.0.0 schema
        if (!result.contains("schedulingStrategy")) {
            result["schedulingStrategy"] = 0;  // Default strategy
        }

        if (!result.contains("recoveryStrategy")) {
            result["recoveryStrategy"] = 0;  // Default strategy
        }

        // Update task format if needed
        if (result.contains("targets") && result["targets"].is_array()) {
            for (auto& target : result["targets"]) {
                target["version"] = "2.0.0";

                // Update task format
                if (target.contains("tasks") && target["tasks"].is_array()) {
                    for (auto& task : target["tasks"]) {
                        task["version"] = "2.0.0";
                    }
                }
            }
        }
    }

    return result;
}
}  // namespace

namespace lithium::task {

using json = nlohmann::json;

ExposureSequence::ExposureSequence() {
    // Initialize database connection
    try {
        db_ = std::make_shared<database::Database>("sequences.db");
        sequenceTable_ = std::make_unique<database::Table<SequenceModel>>(*db_);
        sequenceTable_->createTable();
        spdlog::info("Database initialized successfully");
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to initialize database: " +
                            std::string(e.what()));
    }

    // Initialize config serializer with optimized settings
    lithium::ConfigSerializer::Config serializerConfig;
    serializerConfig.enableMetrics = true;
    serializerConfig.enableValidation = true;
    serializerConfig.bufferSize =
        128 * 1024;  // 128KB buffer for better performance
    configSerializer_ =
        std::make_unique<lithium::ConfigSerializer>(serializerConfig);
    spdlog::info("ConfigSerializer initialized with optimized settings");

    AddPtr(
        Constants::TASK_QUEUE,
        std::make_shared<atom::async::LockFreeHashTable<std::string, json>>());

    taskGenerator_ = TaskGenerator::createShared();

    // Register built-in tasks with the factory
    registerBuiltInTasks();
    spdlog::info("Built-in tasks registered with factory");

    initializeDefaultMacros();
}

ExposureSequence::~ExposureSequence() { stop(); }

void ExposureSequence::addTarget(std::unique_ptr<Target> target) {
    if (!target) {
        spdlog::error("Cannot add a null target");
        throw std::invalid_argument("Cannot add a null target");
    }

    std::unique_lock lock(mutex_);
    auto it = std::find_if(targets_.begin(), targets_.end(),
                           [&](const std::unique_ptr<Target>& t) {
                               return t->getUUID() == target->getUUID();
                           });
    if (it != targets_.end()) {
        spdlog::error("Target with UUID '{}' already exists",
                      target->getUUID());
        THROW_RUNTIME_ERROR("Target with UUID '" + target->getUUID() +
                            "' already exists");
    }

    spdlog::info("Adding target: {}", target->getName());
    targets_.push_back(std::move(target));
    totalTargets_ = targets_.size();
    spdlog::info("Total targets: {}", totalTargets_);
}

void ExposureSequence::removeTarget(const std::string& name) {
    std::unique_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });

    if (it == targets_.end()) {
        spdlog::error("Target with name '{}' not found", name);
        THROW_RUNTIME_ERROR("Target with name '" + name + "' not found");
    }

    spdlog::info("Removing target: {}", name);
    targets_.erase(it);
    totalTargets_ = targets_.size();
    spdlog::info("Total targets: {}", totalTargets_);
}

void ExposureSequence::modifyTarget(const std::string& name,
                                    const TargetModifier& modifier) {
    std::shared_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });

    if (it != targets_.end()) {
        try {
            spdlog::info("Modifying target: {}", name);
            modifier(**it);
            spdlog::info("Target '{}' modified successfully", name);
        } catch (const std::exception& e) {
            spdlog::error("Failed to modify target '{}': {}", name, e.what());
            THROW_RUNTIME_ERROR("Failed to modify target '" + name +
                                "': " + e.what());
        }
    } else {
        spdlog::error("Target with name '{}' not found", name);
        THROW_RUNTIME_ERROR("Target with name '" + name + "' not found");
    }
}

void ExposureSequence::executeAll() {
    SequenceState expected = SequenceState::Idle;
    if (!state_.compare_exchange_strong(expected, SequenceState::Running)) {
        spdlog::error("Cannot start sequence. Current state: {}",
                      static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Sequence is not in Idle state");
    }

    completedTargets_.store(0);
    spdlog::info("Starting sequence execution");
    notifySequenceStart();

    // Start sequence execution in a separate thread
    sequenceThread_ = std::jthread([this] { executeSequence(); });
}

void ExposureSequence::stop() {
    SequenceState current = state_.load();
    if (current == SequenceState::Idle || current == SequenceState::Stopped) {
        spdlog::info("Sequence is already in Idle or Stopped state");
        return;
    }

    spdlog::info("Stopping sequence execution");
    state_.store(SequenceState::Stopping);

    if (sequenceThread_.joinable()) {
        sequenceThread_.request_stop();
        sequenceThread_.join();
    }

    state_.store(SequenceState::Idle);
    notifySequenceEnd();
}

void ExposureSequence::pause() {
    SequenceState expected = SequenceState::Running;
    if (!state_.compare_exchange_strong(expected, SequenceState::Paused)) {
        spdlog::error("Cannot pause sequence. Current state: {}",
                      static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Cannot pause sequence. Current state: " +
                            std::to_string(static_cast<int>(state_.load())));
    }
    spdlog::info("Sequence paused");
}

void ExposureSequence::resume() {
    SequenceState expected = SequenceState::Paused;
    if (!state_.compare_exchange_strong(expected, SequenceState::Running)) {
        spdlog::error("Cannot resume sequence. Current state: {}",
                      static_cast<int>(state_.load()));
        THROW_RUNTIME_ERROR("Cannot resume sequence. Current state: " +
                            std::to_string(static_cast<int>(state_.load())));
    }
    spdlog::info("Sequence resumed");
}

/**
 * @brief Serializes the sequence to JSON with enhanced format.
 * @return JSON representation of the sequence.
 */
json ExposureSequence::serializeToJson() const {
    json j;
    std::shared_lock lock(mutex_);

    j["version"] = "2.0.0";  // Version information for schema compatibility
    j["uuid"] = uuid_;
    j["state"] = static_cast<int>(state_.load());
    j["maxConcurrentTargets"] = maxConcurrentTargets_;
    j["globalTimeout"] = globalTimeout_.count();
    j["schedulingStrategy"] = static_cast<int>(schedulingStrategy_);
    j["recoveryStrategy"] = static_cast<int>(recoveryStrategy_);

    // Serialize main targets
    j["targets"] = json::array();
    for (const auto& target : targets_) {
        j["targets"].push_back(target->toJson());
    }

    // Serialize alternative targets
    j["alternativeTargets"] = json::object();
    for (const auto& [name, target] : alternativeTargets_) {
        j["alternativeTargets"][name] = target->toJson();
    }

    // Serialize dependencies
    j["dependencies"] = targetDependencies_;

    // Serialize execution statistics
    j["executionStats"] = {
        {"totalExecutions", stats_.totalExecutions},
        {"successfulExecutions", stats_.successfulExecutions},
        {"failedExecutions", stats_.failedExecutions},
        {"averageExecutionTime", stats_.averageExecutionTime}};

    return j;
}

/**
 * @brief Initializes the sequence from JSON with enhanced format.
 * @param data The JSON data.
 * @throws std::runtime_error If the JSON is invalid or incompatible.
 */
void ExposureSequence::deserializeFromJson(const json& data) {
    std::unique_lock lock(mutex_);

    // Get the current version and the data version
    const std::string currentVersion = "2.0.0";
    std::string dataVersion =
        data.contains("version") ? data["version"].get<std::string>() : "1.0.0";

    // Standardize and convert the data format if needed
    json processedData;

    try {
        // First, convert to a standard format to handle different schemas
        processedData = convertTargetToStandardFormat(data);

        // Then, handle schema version differences
        if (dataVersion != currentVersion) {
            processedData = convertBetweenSchemaVersions(
                processedData, dataVersion, currentVersion);
            spdlog::info("Converted sequence from version {} to {}",
                         dataVersion, currentVersion);
        }
    } catch (const std::exception& e) {
        spdlog::warn(
            "Error converting sequence format: {}, proceeding with original "
            "data",
            e.what());
        processedData = data;
    }

    // Process JSON with macro replacements if a task generator is available
    if (taskGenerator_) {
        try {
            processJsonWithGenerator(processedData);
            spdlog::debug("Applied macro replacements to sequence data");
        } catch (const std::exception& e) {
            spdlog::warn("Failed to apply macro replacements: {}", e.what());
        }
    }

    // Load basic properties with validation
    try {
        // Core properties with defaults
        uuid_ = processedData.value("uuid", atom::utils::UUID().toString());
        state_ = static_cast<SequenceState>(processedData.value("state", 0));
        maxConcurrentTargets_ =
            processedData.value("maxConcurrentTargets", size_t(1));
        globalTimeout_ = std::chrono::seconds(
            processedData.value("globalTimeout", int64_t(3600)));

        // Strategy properties
        schedulingStrategy_ = static_cast<SchedulingStrategy>(
            processedData.value("schedulingStrategy", 0));
        recoveryStrategy_ = static_cast<RecoveryStrategy>(
            processedData.value("recoveryStrategy", 0));

        // Clear existing targets
        targets_.clear();
        alternativeTargets_.clear();
        targetDependencies_.clear();

        // Load main targets with error handling for each target
        if (processedData.contains("targets") &&
            processedData["targets"].is_array()) {
            for (const auto& targetJson : processedData["targets"]) {
                try {
                    auto target = Target::createFromJson(targetJson);
                    targets_.push_back(std::move(target));
                } catch (const std::exception& e) {
                    spdlog::error("Failed to create target: {}", e.what());
                }
            }
        }

        // Load alternative targets
        if (processedData.contains("alternativeTargets") &&
            processedData["alternativeTargets"].is_object()) {
            for (auto it = processedData["alternativeTargets"].begin();
                 it != processedData["alternativeTargets"].end(); ++it) {
                try {
                    auto target = Target::createFromJson(it.value());
                    alternativeTargets_[it.key()] = std::move(target);
                } catch (const std::exception& e) {
                    spdlog::error("Failed to create alternative target: {}",
                                  e.what());
                }
            }
        }

        // Load dependencies
        if (processedData.contains("dependencies") &&
            processedData["dependencies"].is_object()) {
            targetDependencies_ =
                processedData["dependencies"]
                    .get<std::unordered_map<std::string,
                                            std::vector<std::string>>>();
        }

        // Load execution statistics
        if (processedData.contains("executionStats")) {
            const auto& statsJson = processedData["executionStats"];
            stats_.totalExecutions =
                statsJson.value("totalExecutions", size_t(0));
            stats_.successfulExecutions =
                statsJson.value("successfulExecutions", size_t(0));
            stats_.failedExecutions =
                statsJson.value("failedExecutions", size_t(0));
            stats_.averageExecutionTime =
                statsJson.value("averageExecutionTime", 0.0);
        }

    } catch (const std::exception& e) {
        spdlog::error("Error deserializing sequence: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to deserialize sequence: " +
                            std::string(e.what()));
    }

    // Update target ready status
    updateTargetReadyStatus();

    // Reset counters
    totalTargets_ = targets_.size();
    completedTargets_ = 0;
    failedTargets_ = 0;
    failedTargetNames_.clear();

    spdlog::info("Loaded sequence with {} targets and {} alternative targets",
                 targets_.size(), alternativeTargets_.size());
}

/**
 * @brief Saves the sequence to a file with enhanced format.
 * @param filename The name of the file to save to.
 * @throws std::runtime_error If the file cannot be written.
 */
void ExposureSequence::saveSequence(const std::string& filename,
                                    SerializationFormat format) const {
    json j = serializeToJson();

    try {
        // Use ConfigSerializer for enhanced format support and performance
        lithium::SerializationOptions options;

        switch (format) {
            case SerializationFormat::COMPACT_JSON:
                options = lithium::SerializationOptions::compact();
                break;
            case SerializationFormat::PRETTY_JSON:
                options = lithium::SerializationOptions::pretty(4);
                break;
            case SerializationFormat::JSON5:
                options = lithium::SerializationOptions::json5();
                break;
            case SerializationFormat::BINARY:
                // Use binary format with defaults
                break;
            default:
                options = lithium::SerializationOptions::pretty(4);
                break;
        }

        bool success = configSerializer_->serializeToFile(j, filename, options);
        if (!success) {
            spdlog::error("Failed to save sequence to file: {}", filename);
            THROW_RUNTIME_ERROR("Failed to save sequence to file: " + filename);
        }

        spdlog::info("Sequence saved to file: {}", filename);
    } catch (const std::exception& e) {
        spdlog::error("Failed to save sequence: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to save sequence: " +
                            std::string(e.what()));
    }
}

/**
 * @brief Loads a sequence from a file with enhanced format.
 * @param filename The name of the file to load from.
 * @throws std::runtime_error If the file cannot be read or contains invalid
 * data.
 */
void ExposureSequence::loadSequence(const std::string& filename,
                                    bool detectFormat) {
    try {
        // Use ConfigSerializer for enhanced format support and automatic format
        // detection
        std::optional<lithium::SerializationFormat> format = std::nullopt;

        // Auto-detect format if requested
        if (detectFormat) {
            const std::filesystem::path filePath(filename);
            format = configSerializer_->detectFormat(filePath);
            if (!format) {
                spdlog::warn(
                    "Failed to auto-detect format, will try using file "
                    "extension");
            } else {
                spdlog::info("Auto-detected format: {}",
                             static_cast<int>(format.value()));
            }
        }

        // Load and deserialize the file
        auto result = configSerializer_->deserializeFromFile(filename, format);

        if (!result.isValid()) {
            spdlog::error("Failed to load sequence from file: {}",
                          result.errorMessage);
            THROW_RUNTIME_ERROR("Failed to load sequence from file: " +
                                result.errorMessage);
        }

        deserializeFromJson(result.data);

        spdlog::info("Sequence loaded from file: {} ({}KB, {}ms)", filename,
                     result.bytesProcessed / 1024, result.duration.count());
    } catch (const std::exception& e) {
        spdlog::error("Failed to load sequence: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to load sequence: " +
                            std::string(e.what()));
    }
}

/**
 * @brief Processes JSON with the task generator.
 * @param data The JSON data to process.
 */
void ExposureSequence::processJsonWithGenerator(json& data) {
    if (!taskGenerator_) {
        spdlog::warn("Task generator not available, skipping macro processing");
        return;
    }

    try {
        // Process the JSON with full macro replacement
        taskGenerator_->processJsonWithJsonMacros(data);

        spdlog::debug("Successfully processed JSON with task generator");
    } catch (const std::exception& e) {
        spdlog::error("Failed to process JSON with generator: {}", e.what());
        // Continue without throwing to make the system more robust
        spdlog::warn("Continuing with unprocessed JSON");
    }
}

/**
 * @brief Saves the sequence to the database with enhanced format.
 * @throws std::runtime_error If the database operation fails.
 */
void ExposureSequence::saveToDatabase() {
    if (!db_ || !sequenceTable_) {
        spdlog::error("Database connection not initialized");
        THROW_RUNTIME_ERROR("Database connection not initialized");
    }

    try {
        SequenceModel model;
        model.uuid = uuid_;
        model.name = targets_.empty() ? "Unnamed Sequence"
                                      : targets_[0]->getName() + " Sequence";
        model.data = serializeToJson().dump();
        model.createdAt = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());

        sequenceTable_->insert(model);
        spdlog::info("Sequence saved to database with UUID: {}", uuid_);
    } catch (const std::exception& e) {
        spdlog::error("Failed to save sequence to database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to save sequence to database: " +
                            std::string(e.what()));
    }
}

/**
 * @brief Loads a sequence from the database with enhanced format.
 * @param uuid The UUID of the sequence.
 * @throws std::runtime_error If the database operation fails or sequence not
 * found.
 */
void ExposureSequence::loadFromDatabase(const std::string& uuid) {
    if (!db_ || !sequenceTable_) {
        spdlog::error("Database connection not initialized");
        THROW_RUNTIME_ERROR("Database connection not initialized");
    }

    try {
        std::string condition = "uuid = '" + uuid + "'";
        auto results = sequenceTable_->query(condition);
        if (results.empty()) {
            spdlog::error("Sequence not found in database: {}", uuid);
            THROW_RUNTIME_ERROR("Sequence not found in database: " + uuid);
        }

        auto& model = results[0];
        json data = json::parse(model.data);
        deserializeFromJson(data);
        spdlog::info("Sequence loaded from database: {} ({})", model.name,
                     uuid);
    } catch (const json::exception& e) {
        spdlog::error("Failed to parse sequence data: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to parse sequence data: " +
                            std::string(e.what()));
    } catch (const std::exception& e) {
        spdlog::error("Failed to load sequence from database: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to load sequence from database: " +
                            std::string(e.what()));
    }
}

auto ExposureSequence::getTargetNames() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(targets_.size());

    for (const auto& target : targets_) {
        names.emplace_back(target->getName());
    }

    return names;
}

auto ExposureSequence::getTargetStatus(const std::string& name) const
    -> TargetStatus {
    std::shared_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });

    if (it != targets_.end()) {
        return (*it)->getStatus();
    }

    return TargetStatus::Skipped;  // Default if target not found
}

auto ExposureSequence::getProgress() const -> double {
    size_t completed = completedTargets_.load();
    size_t total;
    {
        std::shared_lock lock(mutex_);
        total = totalTargets_;
    }

    if (total == 0) {
        return 100.0;
    }

    return (static_cast<double>(completed) / static_cast<double>(total)) *
           100.0;
}

// Callback setter methods

void ExposureSequence::setOnSequenceStart(SequenceCallback callback) {
    std::unique_lock lock(mutex_);
    onSequenceStart_ = std::move(callback);
}

void ExposureSequence::setOnSequenceEnd(SequenceCallback callback) {
    std::unique_lock lock(mutex_);
    onSequenceEnd_ = std::move(callback);
}

void ExposureSequence::setOnTargetStart(TargetCallback callback) {
    std::unique_lock lock(mutex_);
    onTargetStart_ = std::move(callback);
}

void ExposureSequence::setOnTargetEnd(TargetCallback callback) {
    std::unique_lock lock(mutex_);
    onTargetEnd_ = std::move(callback);
}

void ExposureSequence::setOnError(ErrorCallback callback) {
    std::unique_lock lock(mutex_);
    onError_ = std::move(callback);
}

// Notification helper methods

void ExposureSequence::notifySequenceStart() {
    SequenceCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onSequenceStart_;
    }

    if (callbackCopy) {
        try {
            callbackCopy();
        } catch (const std::exception& e) {
            spdlog::error("Exception in sequence start callback: {}", e.what());
        }
    }
}

void ExposureSequence::notifySequenceEnd() {
    SequenceCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onSequenceEnd_;
    }

    if (callbackCopy) {
        try {
            callbackCopy();
        } catch (const std::exception& e) {
            spdlog::error("Exception in sequence end callback: {}", e.what());
        }
    }
}

void ExposureSequence::notifyTargetStart(const std::string& name) {
    TargetCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onTargetStart_;
    }

    if (callbackCopy) {
        try {
            callbackCopy(name, TargetStatus::InProgress);
        } catch (const std::exception& e) {
            spdlog::error("Exception in target start callback for {}: {}", name,
                          e.what());
        }
    }
}

void ExposureSequence::notifyTargetEnd(const std::string& name,
                                       TargetStatus status) {
    TargetCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onTargetEnd_;
    }

    if (callbackCopy) {
        try {
            callbackCopy(name, status);
        } catch (const std::exception& e) {
            spdlog::error("Exception in target end callback for {}: {}", name,
                          e.what());
        }
    }
}

void ExposureSequence::notifyError(const std::string& name,
                                   const std::exception& e) {
    ErrorCallback callbackCopy;
    {
        std::shared_lock lock(mutex_);
        callbackCopy = onError_;
    }

    if (callbackCopy) {
        try {
            callbackCopy(name, e);
        } catch (const std::exception& ex) {
            spdlog::error("Exception in error callback for {}: {}", name,
                          ex.what());
        }
    }
}

void ExposureSequence::executeSequence() {
    stats_.startTime = std::chrono::steady_clock::now();

    while (auto* target = getNextExecutableTarget()) {
        if (state_.load() == SequenceState::Stopping) {
            break;
        }

        try {
            spdlog::info("Executing target: {} ({}/{} completed)",
                         target->getName(), completedTargets_.load(),
                         totalTargets_);

            stats_.totalExecutions++;
            notifyTargetStart(target->getName());
            target->execute();

            if (target->getStatus() == TargetStatus::Completed) {
                stats_.successfulExecutions++;
                completedTargets_.fetch_add(1);
            } else if (target->getStatus() == TargetStatus::Failed) {
                stats_.failedExecutions++;
                failedTargets_.fetch_add(1);

                std::unique_lock lock(mutex_);
                failedTargetNames_.push_back(target->getName());
            }

            notifyTargetEnd(target->getName(), target->getStatus());

        } catch (const std::exception& e) {
            spdlog::error("Target execution failed: {} - {}", target->getName(),
                          e.what());
            handleTargetError(target, e);
        }
    }

    state_.store(SequenceState::Idle);
    notifySequenceEnd();
}

auto ExposureSequence::getNextExecutableTarget() -> Target* {
    if (state_.load() == SequenceState::Stopping) {
        return nullptr;
    }

    // Check concurrent execution limits
    size_t runningTargets = 0;
    {
        std::shared_lock lock(mutex_);
        if (maxConcurrentTargets_ > 0) {
            for (const auto& target : targets_) {
                if (target->getStatus() == TargetStatus::InProgress) {
                    runningTargets++;
                }
            }

            if (runningTargets >= maxConcurrentTargets_) {
                return nullptr;
            }
        }

        // Find next ready target
        for (const auto& target : targets_) {
            if (target->getStatus() == TargetStatus::Pending &&
                isTargetReady(target->getName())) {
                return target.get();
            }
        }
    }

    return nullptr;
}

void ExposureSequence::handleTargetError(Target* target,
                                         const std::exception& e) {
    RecoveryStrategy strategy;
    {
        std::shared_lock lock(mutex_);
        strategy = recoveryStrategy_;
    }

    switch (strategy) {
        case RecoveryStrategy::Stop:
            state_.store(SequenceState::Stopping);
            break;

        case RecoveryStrategy::Skip:
            target->setStatus(TargetStatus::Skipped);
            notifyTargetEnd(target->getName(), TargetStatus::Skipped);
            break;

        case RecoveryStrategy::Retry:
            // Implementation would handle retry logic
            spdlog::info("Retry strategy selected for target: {}",
                         target->getName());
            break;

        case RecoveryStrategy::Alternative: {
            std::shared_lock lock(mutex_);
            // Try to execute alternative target if available
            if (auto it = alternativeTargets_.find(target->getName());
                it != alternativeTargets_.end()) {
                spdlog::info("Executing alternative target for: {}",
                             target->getName());
                it->second->execute();
            }
            break;
        }
    }

    notifyError(target->getName(), e);
}

void ExposureSequence::setSchedulingStrategy(SchedulingStrategy strategy) {
    std::unique_lock lock(mutex_);
    schedulingStrategy_ = strategy;

    // Reorder targets based on new strategy
    switch (strategy) {
        case SchedulingStrategy::Dependencies:
            reorderTargetsByDependencies();
            break;
        case SchedulingStrategy::Priority:
            reorderTargetsByPriority();
            break;
        default:
            // No reordering needed for FIFO
            break;
    }
}

void ExposureSequence::setRecoveryStrategy(RecoveryStrategy strategy) {
    std::unique_lock lock(mutex_);
    recoveryStrategy_ = strategy;
    spdlog::info("Recovery strategy set to: {}", static_cast<int>(strategy));
}

void ExposureSequence::addAlternativeTarget(
    const std::string& targetName, std::unique_ptr<Target> alternative) {
    if (!alternative) {
        spdlog::error("Cannot add null alternative target for {}", targetName);
        throw std::invalid_argument("Cannot add null alternative target");
    }

    std::unique_lock lock(mutex_);
    alternativeTargets_[targetName] = std::move(alternative);
    spdlog::info("Alternative target added for: {}", targetName);
}

void ExposureSequence::reorderTargetsByDependencies() {
    std::vector<std::unique_ptr<Target>> sorted;
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> inStack;
    std::stack<std::unique_ptr<Target>> stack;

    auto visit = [&](const std::unique_ptr<Target>& target,
                     auto&& visit_ref) -> void {
        const std::string& name = target->getName();
        if (visited[name]) {
            return;
        }

        if (inStack[name]) {
            THROW_RUNTIME_ERROR("Circular dependency detected in target '" +
                                name + "'");
        }

        inStack[name] = true;
        for (const auto& dep : target->getDependencies()) {
            auto it = std::find_if(targets_.begin(), targets_.end(),
                                   [&dep](const std::unique_ptr<Target>& t) {
                                       return t->getName() == dep;
                                   });
            if (it != targets_.end()) {
                visit_ref(*it, visit_ref);
            }
        }

        inStack[name] = false;
        visited[name] = true;

        // Use std::move with care - we're manipulating a const reference
        auto& mutableTarget = const_cast<std::unique_ptr<Target>&>(target);
        stack.push(std::move(mutableTarget));
    };

    // Create a copy of targets since we'll be moving them
    auto targetsCopy = std::move(targets_);
    targets_.clear();

    for (auto& target : targetsCopy) {
        if (!visited[target->getName()]) {
            visit(target, visit);
        }
    }

    while (!stack.empty()) {
        targets_.push_back(std::move(stack.top()));
        stack.pop();
    }

    spdlog::info("Targets reordered by dependencies");
}

void ExposureSequence::reorderTargetsByPriority() {
    // Sort targets by priority
    std::sort(
        targets_.begin(), targets_.end(),
        [](const std::unique_ptr<Target>& a, const std::unique_ptr<Target>& b) {
            // Higher priority first - implementation would depend on how
            // priority is stored
            return false;  // Placeholder for actual priority comparison
        });
    spdlog::info("Targets reordered by priority");
}

void ExposureSequence::addTargetDependency(const std::string& targetName,
                                           const std::string& dependsOn) {
    std::unique_lock lock(mutex_);
    // Add dependency if not already present
    auto& dependencies = targetDependencies_[targetName];
    if (std::find(dependencies.begin(), dependencies.end(), dependsOn) ==
        dependencies.end()) {
        dependencies.push_back(dependsOn);
    }

    // Check for cyclic dependencies
    if (auto cycle = findCyclicDependency()) {
        // Remove the dependency we just added
        dependencies.pop_back();
        THROW_RUNTIME_ERROR("Cyclic dependency detected in target: " +
                            cycle.value());
    }

    updateTargetReadyStatus();
    spdlog::info("Added dependency: {} depends on {}", targetName, dependsOn);
}

void ExposureSequence::removeTargetDependency(const std::string& targetName,
                                              const std::string& dependsOn) {
    std::unique_lock lock(mutex_);
    auto& deps = targetDependencies_[targetName];
    deps.erase(std::remove(deps.begin(), deps.end(), dependsOn), deps.end());
    updateTargetReadyStatus();
    spdlog::info("Removed dependency: {} no longer depends on {}", targetName,
                 dependsOn);
}

auto ExposureSequence::isTargetReady(const std::string& targetName) const
    -> bool {
    std::shared_lock lock(mutex_);
    auto it = targetReadyStatus_.find(targetName);
    return it != targetReadyStatus_.end() && it->second;
}

auto ExposureSequence::getTargetDependencies(
    const std::string& targetName) const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    auto it = targetDependencies_.find(targetName);
    if (it != targetDependencies_.end()) {
        return it->second;
    }
    return {};
}

void ExposureSequence::updateTargetReadyStatus() {
    // First mark all targets as ready
    for (const auto& target : targets_) {
        targetReadyStatus_[target->getName()] = true;
    }

    // Check dependencies for each target
    bool changed;
    do {
        changed = false;
        for (const auto& [targetName, dependencies] : targetDependencies_) {
            // If any dependency is not ready, the target is also not ready
            for (const auto& dep : dependencies) {
                if (!targetReadyStatus_[dep]) {
                    if (targetReadyStatus_[targetName]) {
                        targetReadyStatus_[targetName] = false;
                        changed = true;
                    }
                    break;
                }
            }
        }
    } while (changed);
}

auto ExposureSequence::findCyclicDependency() const
    -> std::optional<std::string> {
    std::unordered_map<std::string, bool> visited;
    std::unordered_map<std::string, bool> recursionStack;

    std::function<bool(const std::string&)> hasCycle;
    hasCycle = [&](const std::string& targetName) -> bool {
        visited[targetName] = true;
        recursionStack[targetName] = true;

        auto it = targetDependencies_.find(targetName);
        if (it != targetDependencies_.end()) {
            for (const auto& dep : it->second) {
                if (!visited[dep]) {
                    if (hasCycle(dep)) {
                        return true;
                    }
                } else if (recursionStack[dep]) {
                    return true;
                }
            }
        }

        recursionStack[targetName] = false;
        return false;
    };

    for (const auto& [targetName, _] : targetDependencies_) {
        if (!visited[targetName]) {
            if (hasCycle(targetName)) {
                return targetName;
            }
        }
    }

    return std::nullopt;
}

void ExposureSequence::setMaxConcurrentTargets(size_t max) {
    std::unique_lock lock(mutex_);
    maxConcurrentTargets_ = max;
    spdlog::info("Maximum concurrent targets set to: {}", max);
}

void ExposureSequence::setTargetTimeout(const std::string& name,
                                        std::chrono::seconds timeout) {
    std::shared_lock lock(mutex_);
    auto it = std::find_if(
        targets_.begin(), targets_.end(),
        [&name](const auto& target) { return target->getName() == name; });
    if (it != targets_.end()) {
        // Implementation depends on Target interface
        spdlog::info("Set timeout for target {}: {} seconds", name,
                     timeout.count());
        // (*it)->setTimeout(timeout);  // Assuming Target has setTimeout method
    } else {
        spdlog::error("Target not found: {}", name);
        THROW_RUNTIME_ERROR("Target not found: " + name);
    }
}

void ExposureSequence::setGlobalTimeout(std::chrono::seconds timeout) {
    std::unique_lock lock(mutex_);
    globalTimeout_ = timeout;
    spdlog::info("Global timeout set to: {}s", timeout.count());
}

auto ExposureSequence::getFailedTargets() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    return failedTargetNames_;
}

auto ExposureSequence::getExecutionStats() const -> json {
    std::shared_lock lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    auto uptime =
        std::chrono::duration_cast<std::chrono::seconds>(now - stats_.startTime)
            .count();

    return json{{"totalExecutions", stats_.totalExecutions},
                {"successfulExecutions", stats_.successfulExecutions},
                {"failedExecutions", stats_.failedExecutions},
                {"averageExecutionTime", stats_.averageExecutionTime},
                {"uptime", uptime}};
}

auto ExposureSequence::getResourceUsage() const -> json {
    // Implementation would collect resource usage metrics
    return json{
        {"memoryUsage", getTotalMemoryUsage()},
        {"cpuUsage",
         0.0},  // Would be implemented with actual CPU usage tracking
        {"diskUsage", 0}
        // Would be implemented with actual disk usage tracking
    };
}

void ExposureSequence::retryFailedTargets() {
    std::vector<std::string> targetsToRetry;
    {
        std::unique_lock lock(mutex_);
        targetsToRetry = failedTargetNames_;
        failedTargetNames_.clear();
        failedTargets_.store(0);
    }

    for (const auto& targetName : targetsToRetry) {
        std::shared_lock lock(mutex_);
        auto it = std::find_if(targets_.begin(), targets_.end(),
                               [&](const auto& target) {
                                   return target->getName() == targetName;
                               });
        if (it != targets_.end()) {
            (*it)->setStatus(TargetStatus::Pending);
            spdlog::info("Retrying failed target: {}", targetName);
        }
    }
}

void ExposureSequence::skipFailedTargets() {
    std::vector<std::string> targetsToSkip;
    {
        std::unique_lock lock(mutex_);
        targetsToSkip = failedTargetNames_;
        failedTargetNames_.clear();
        failedTargets_.store(0);
    }

    for (const auto& targetName : targetsToSkip) {
        std::shared_lock lock(mutex_);
        auto it = std::find_if(targets_.begin(), targets_.end(),
                               [&](const auto& target) {
                                   return target->getName() == targetName;
                               });
        if (it != targets_.end()) {
            (*it)->setStatus(TargetStatus::Skipped);
            spdlog::info("Skipping failed target: {}", targetName);
        }
    }
}

void ExposureSequence::setTargetTaskParams(const std::string& targetName,
                                           const std::string& taskUUID,
                                           const json& params) {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target == targets_.end()) {
        spdlog::error("Target not found: {}", targetName);
        THROW_RUNTIME_ERROR("Target not found: " + targetName);
    }

    (*target)->setTaskParams(taskUUID, params);
    spdlog::info("Set parameters for task {} in target {}", taskUUID,
                 targetName);
}

auto ExposureSequence::getTargetTaskParams(const std::string& targetName,
                                           const std::string& taskUUID) const
    -> std::optional<json> {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target == targets_.end()) {
        spdlog::warn("Target not found when getting task params: {}",
                     targetName);
        return std::nullopt;
    }

    return (*target)->getTaskParams(taskUUID);
}

void ExposureSequence::setTargetParams(const std::string& targetName,
                                       const json& params) {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target != targets_.end()) {
        (*target)->setParams(params);
        spdlog::info("Set parameters for target {}", targetName);
    } else {
        spdlog::error("Target not found: {}", targetName);
        THROW_RUNTIME_ERROR("Target not found: " + targetName);
    }
}

auto ExposureSequence::getTargetParams(const std::string& targetName) const
    -> std::optional<json> {
    std::shared_lock lock(mutex_);
    auto target =
        std::find_if(targets_.begin(), targets_.end(),
                     [&](const auto& t) { return t->getName() == targetName; });
    if (target != targets_.end()) {
        return (*target)->getParams();
    }

    spdlog::warn("Target not found when getting params: {}", targetName);
    return std::nullopt;
}

std::string ExposureSequence::exportToFormat(SerializationFormat format) const {
    json j = serializeToJson();

    try {
        // Use ConfigSerializer for enhanced format support
        lithium::SerializationOptions options;

        switch (format) {
            case SerializationFormat::COMPACT_JSON:
                options = lithium::SerializationOptions::compact();
                break;
            case SerializationFormat::PRETTY_JSON:
                options = lithium::SerializationOptions::pretty(4);
                break;
            case SerializationFormat::JSON5:
                options = lithium::SerializationOptions::json5();
                break;
            case SerializationFormat::BINARY:
                // For binary format, we'll use the default binary serialization
                break;
            default:
                options = lithium::SerializationOptions::pretty(4);
                break;
        }

        auto result = configSerializer_->serialize(j, options);
        if (!result.isValid()) {
            spdlog::error("Failed to export sequence: {}", result.errorMessage);
            THROW_RUNTIME_ERROR("Failed to export sequence: " +
                                result.errorMessage);
        }

        return result.data;
    } catch (const std::exception& e) {
        spdlog::error("Failed to export sequence: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to export sequence: " +
                            std::string(e.what()));
    }
}

/**
 * @brief Convert a specific target format to a common JSON format
 * @param targetJson The target-specific JSON data
 * @return Standardized JSON format
 */
json convertTargetToStandardFormat(const json& targetJson) {
    // Create a standardized format
    json standardJson = targetJson;

    // Handle version differences
    if (!standardJson.contains("version")) {
        standardJson["version"] = "2.0.0";
    }

    // Ensure essential fields exist
    if (!standardJson.contains("uuid")) {
        standardJson["uuid"] = atom::utils::UUID().toString();
    }

    // Ensure tasks array exists
    if (!standardJson.contains("tasks")) {
        standardJson["tasks"] = json::array();
    }

    // Standardize task format
    for (auto& taskJson : standardJson["tasks"]) {
        if (!taskJson.contains("version")) {
            taskJson["version"] = "2.0.0";
        }

        // Ensure task has a UUID
        if (!taskJson.contains("uuid")) {
            taskJson["uuid"] = atom::utils::UUID().toString();
        }
    }

    return standardJson;
}

/**
 * @brief Convert a JSON object from one schema to another
 * @param sourceJson Source JSON object
 * @param sourceVersion Source schema version
 * @param targetVersion Target schema version
 * @return Converted JSON object
 */
json convertBetweenSchemaVersions(const json& sourceJson,
                                  const std::string& sourceVersion,
                                  const std::string& targetVersion) {
    // If versions match, no conversion needed
    if (sourceVersion == targetVersion) {
        return sourceJson;
    }

    json result = sourceJson;

    // Handle specific version upgrades
    if (sourceVersion == "1.0.0" && targetVersion == "2.0.0") {
        // Upgrade from 1.0 to 2.0
        result["version"] = "2.0.0";

        // Add additional fields for 2.0.0 schema
        if (!result.contains("schedulingStrategy")) {
            result["schedulingStrategy"] = 0;  // Default strategy
        }

        if (!result.contains("recoveryStrategy")) {
            result["recoveryStrategy"] = 0;  // Default strategy
        }

        // Update task format if needed
        if (result.contains("targets") && result["targets"].is_array()) {
            for (auto& target : result["targets"]) {
                target["version"] = "2.0.0";

                // Update task format
                if (target.contains("tasks") && target["tasks"].is_array()) {
                    for (auto& task : target["tasks"]) {
                        task["version"] = "2.0.0";
                    }
                }
            }
        }
    }

    return result;
}

}  // namespace lithium::task
