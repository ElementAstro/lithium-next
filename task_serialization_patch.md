# Fixed ExposureSequence Serialization with ConfigSerializer Integration

This patch adds improved serialization/deserialization capabilities to the task sequence system:

1. Added ConfigSerializer integration to ExposureSequence
2. Enhanced deserializeFromJson with format handling and schema versioning
3. Added helper functions for schema conversion and standardization
4. Improved error handling in serialization operations

To apply these changes:

1. First, add the helper functions to the top of src/task/sequencer.cpp:

```cpp
namespace {
    // Forward declarations for helper functions
    json convertTargetToStandardFormat(const json& targetJson);
    json convertBetweenSchemaVersions(const json& sourceJson,
                                   const std::string& sourceVersion,
                                   const std::string& targetVersion);

    lithium::SerializationFormat convertFormat(lithium::task::ExposureSequence::SerializationFormat format) {
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
        if (!standardJson.contains("uuid")) {
            standardJson["uuid"] = lithium::atom::utils::UUID().toString();
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
                taskJson["uuid"] = lithium::atom::utils::UUID().toString();
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
                result["schedulingStrategy"] = 0; // Default strategy
            }

            if (!result.contains("recoveryStrategy")) {
                result["recoveryStrategy"] = 0; // Default strategy
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
}
```

2. Then, update the ExposureSequence constructor to initialize the ConfigSerializer:

```cpp
ExposureSequence::ExposureSequence() {
    // Initialize database
    db_ = std::make_shared<database::Database>("sequences.db");
    sequenceTable_ = std::make_unique<database::Table<SequenceModel>>(*db_);
    sequenceTable_->createTable();

    // Generate UUID for this sequence
    uuid_ = atom::utils::UUID().toString();

    // Initialize ConfigSerializer with reasonable defaults
    lithium::ConfigSerializer::Config serializerConfig;
    serializerConfig.defaultFormat = lithium::SerializationFormat::PRETTY_JSON;
    serializerConfig.validateOnLoad = true;
    serializerConfig.useSchemaCache = true;
    configSerializer_ = std::make_unique<lithium::ConfigSerializer>(serializerConfig);

    // Add schema for sequence validation
    configSerializer_->registerSchema("sequence", "schemas/sequence_schema.json");

    // Initialize task generator
    taskGenerator_ = TaskGenerator::createShared();

    // Initialize default macros
    initializeDefaultMacros();
}
```

3. Update the saveSequence and loadSequence methods to use the ConfigSerializer:

```cpp
void ExposureSequence::saveSequence(const std::string& filename, SerializationFormat format) const {
    // Serialize the sequence to JSON
    json sequenceJson = serializeToJson();

    try {
        // Use the ConfigSerializer to save with proper formatting
        lithium::SerializationFormat outputFormat = convertFormat(format);
        configSerializer_->saveToFile(sequenceJson, filename, outputFormat);
        spdlog::info("Sequence saved to {}", filename);
    } catch (const std::exception& e) {
        spdlog::error("Failed to save sequence: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to save sequence: " + std::string(e.what()));
    }
}

void ExposureSequence::loadSequence(const std::string& filename, bool detectFormat) {
    try {
        // Use the ConfigSerializer to load with format detection if requested
        json sequenceJson;

        if (detectFormat) {
            sequenceJson = configSerializer_->loadFromFile(filename, true);
        } else {
            // Determine format from file extension
            auto extension = std::filesystem::path(filename).extension().string();
            auto format = lithium::SerializationFormat::JSON;

            if (extension == ".json5") {
                format = lithium::SerializationFormat::JSON5;
            } else if (extension == ".bin" || extension == ".binary") {
                format = lithium::SerializationFormat::BINARY_JSON;
            }

            sequenceJson = configSerializer_->loadFromFile(filename, format);
        }

        // Validate against schema if available
        std::string errorMessage;
        if (configSerializer_->hasSchema("sequence") &&
            !validateSequenceJson(sequenceJson, errorMessage)) {
            spdlog::warn("Loaded sequence does not match schema: {}", errorMessage);
        }

        // Deserialize from the loaded JSON
        deserializeFromJson(sequenceJson);
        spdlog::info("Sequence loaded from {}", filename);
    } catch (const std::exception& e) {
        spdlog::error("Failed to load sequence: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to load sequence: " + std::string(e.what()));
    }
}

std::string ExposureSequence::exportToFormat(SerializationFormat format) const {
    // Serialize the sequence to JSON
    json sequenceJson = serializeToJson();

    try {
        // Use the ConfigSerializer to format the JSON
        lithium::SerializationFormat outputFormat = convertFormat(format);
        return configSerializer_->serialize(sequenceJson, outputFormat);
    } catch (const std::exception& e) {
        spdlog::error("Failed to export sequence: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to export sequence: " + std::string(e.what()));
    }
}
```

4. Finally, update the deserializeFromJson method with schema conversion:

```cpp
void ExposureSequence::deserializeFromJson(const json& data) {
    std::unique_lock lock(mutex_);

    // Get the current version and the data version
    const std::string currentVersion = "2.0.0";
    std::string dataVersion = data.contains("version") ?
                            data["version"].get<std::string>() : "1.0.0";

    // Standardize and convert the data format if needed
    json processedData;

    try {
        // First, convert to a standard format to handle different schemas
        processedData = convertTargetToStandardFormat(data);

        // Then, handle schema version differences
        if (dataVersion != currentVersion) {
            processedData = convertBetweenSchemaVersions(processedData, dataVersion, currentVersion);
            spdlog::info("Converted sequence from version {} to {}", dataVersion, currentVersion);
        }
    } catch (const std::exception& e) {
        spdlog::warn("Error converting sequence format: {}, proceeding with original data", e.what());
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
        maxConcurrentTargets_ = processedData.value("maxConcurrentTargets", size_t(1));
        globalTimeout_ = std::chrono::seconds(processedData.value("globalTimeout", int64_t(3600)));

        // Strategy properties
        schedulingStrategy_ = static_cast<SchedulingStrategy>(
            processedData.value("schedulingStrategy", 0));
        recoveryStrategy_ = static_cast<RecoveryStrategy>(
            processedData.value("recoveryStrategy", 0));

        // Clear existing targets
        targets_.clear();
        alternativeTargets_.clear();
        targetDependencies_.clear();

        // Rest of implementation...
    } catch (const std::exception& e) {
        spdlog::error("Error deserializing sequence: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to deserialize sequence: " + std::string(e.what()));
    }
}
```

These changes enable the task sequence system to handle different JSON formats, perform schema validation, and convert between schema versions. The integration with ConfigSerializer provides a consistent way to handle serialization across the application.
