/**
 * @file sequencer_template.cpp
 * @brief Implementation of the enhanced template functionality for ExposureSequence
 */

#include "sequencer.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stack>

#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/type/json.hpp"
#include "spdlog/spdlog.h"

#include "constant/constant.hpp"
#include "uuid.hpp"

namespace lithium::task {

using json = nlohmann::json;
namespace fs = std::filesystem;

/**
 * @brief Validates a sequence file against the schema.
 * @param filename The name of the file to validate.
 * @return True if valid, false otherwise.
 */
bool ExposureSequence::validateSequenceFile(const std::string& filename) const {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file '{}' for validation", filename);
            return false;
        }

        json j;
        file >> j;

        std::string errorMessage;
        bool isValid = validateSequenceJson(j, errorMessage);

        if (!isValid) {
            spdlog::error("Sequence validation failed: {}", errorMessage);
        }

        return isValid;
    } catch (const json::exception& e) {
        spdlog::error("JSON parsing error during validation: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error during sequence validation: {}", e.what());
        return false;
    }
}

/**
 * @brief Validates a sequence JSON against the schema.
 * @param data The JSON data to validate.
 * @param errorMessage Output parameter for error message if validation fails.
 * @return True if valid, false otherwise with error message set.
 */
bool ExposureSequence::validateSequenceJson(const json& data, std::string& errorMessage) const {
    // Basic structure validation
    if (!data.is_object()) {
        errorMessage = "Sequence JSON must be an object";
        return false;
    }

    // Check required fields
    if (!data.contains("targets")) {
        errorMessage = "Sequence JSON must contain a 'targets' array";
        return false;
    }

    if (!data["targets"].is_array()) {
        errorMessage = "Sequence 'targets' must be an array";
        return false;
    }

    // Check each target
    for (const auto& target : data["targets"]) {
        if (!target.is_object()) {
            errorMessage = "Each target must be an object";
            return false;
        }

        if (!target.contains("name") || !target["name"].is_string()) {
            errorMessage = "Each target must have a name string";
            return false;
        }

        // Check tasks if present
        if (target.contains("tasks")) {
            if (!target["tasks"].is_array()) {
                errorMessage = "Target tasks must be an array";
                return false;
            }

            for (const auto& task : target["tasks"]) {
                if (!task.is_object()) {
                    errorMessage = "Each task must be an object";
                    return false;
                }

                if (!task.contains("name") || !task["name"].is_string()) {
                    errorMessage = "Each task must have a name string";
                    return false;
                }
            }
        }
    }

    // Check optional fields with specific types
    if (data.contains("state") && !data["state"].is_number_integer()) {
        errorMessage = "Sequence 'state' must be an integer";
        return false;
    }

    if (data.contains("maxConcurrentTargets") && !data["maxConcurrentTargets"].is_number_unsigned()) {
        errorMessage = "Sequence 'maxConcurrentTargets' must be an unsigned integer";
        return false;
    }

    if (data.contains("globalTimeout") && !data["globalTimeout"].is_number_integer()) {
        errorMessage = "Sequence 'globalTimeout' must be an integer";
        return false;
    }

    if (data.contains("dependencies") && !data["dependencies"].is_object()) {
        errorMessage = "Sequence 'dependencies' must be an object";
        return false;
    }

    // All checks passed
    return true;
}

/**
 * @brief Exports a sequence as a reusable template.
 * @param filename The name of the file to save the template to.
 */
void ExposureSequence::exportAsTemplate(const std::string& filename) const {
    json templateJson = serializeToJson();

    // Replace actual values with placeholders for a template
    if (templateJson.contains("uuid")) {
        templateJson.erase("uuid");
    }

    // Add template metadata
    templateJson["_template"] = {
        {"version", "1.0.0"},
        {"description", "Sequence template"},
        {"createdAt", std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count()},
        {"parameters", json::array()}
    };

    // Reset runtime state
    templateJson["state"] = static_cast<int>(SequenceState::Idle);
    if (templateJson.contains("executionStats")) {
        templateJson.erase("executionStats");
    }

    // Parameterize targets
    for (auto& target : templateJson["targets"]) {
        // Reset target status
        if (target.contains("status")) {
            target["status"] = static_cast<int>(TargetStatus::Pending);
        }

        // Reset task status and execution data
        if (target.contains("tasks") && target["tasks"].is_array()) {
            for (auto& task : target["tasks"]) {
                if (task.contains("status")) {
                    task["status"] = static_cast<int>(TaskStatus::Pending);
                }

                // Remove runtime information
                if (task.contains("executionTime")) task.erase("executionTime");
                if (task.contains("memoryUsage")) task.erase("memoryUsage");
                if (task.contains("cpuUsage")) task.erase("cpuUsage");
                if (task.contains("taskHistory")) task.erase("taskHistory");
                if (task.contains("error")) task.erase("error");
                if (task.contains("errorDetails")) task.erase("errorDetails");
            }
        }
    }

    // Write template to file
    std::ofstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open file '{}' for writing template", filename);
        THROW_RUNTIME_ERROR("Failed to open file '" + filename + "' for writing template");
    }

    file << templateJson.dump(4);
    spdlog::info("Sequence template saved to: {}", filename);
}

/**
 * @brief Creates a sequence from a template.
 * @param filename The name of the template file.
 * @param params The parameters to customize the template.
 */
void ExposureSequence::createFromTemplate(const std::string& filename, const json& params) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        spdlog::error("Failed to open template file '{}' for reading", filename);
        THROW_RUNTIME_ERROR("Failed to open template file '" + filename + "' for reading");
    }

    try {
        json templateJson;
        file >> templateJson;

        // Verify this is a template
        if (!templateJson.contains("_template")) {
            spdlog::error("File '{}' is not a valid sequence template", filename);
            THROW_RUNTIME_ERROR("File is not a valid sequence template");
        }

        // Apply parameters if provided
        if (params.is_object() && !params.empty()) {
            // Process the template with parameters
            applyTemplateParameters(templateJson, params);
        }

        // Remove template metadata
        if (templateJson.contains("_template")) {
            templateJson.erase("_template");
        }

        // Generate new UUID
        templateJson["uuid"] = atom::utils::UUID().toString();

        // Reset state
        templateJson["state"] = static_cast<int>(SequenceState::Idle);

        // Load the sequence from the processed template
        deserializeFromJson(templateJson);

        spdlog::info("Sequence created from template: {}", filename);
    } catch (const json::exception& e) {
        spdlog::error("Failed to parse template JSON: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to parse template JSON: " + std::string(e.what()));
    } catch (const std::exception& e) {
        spdlog::error("Failed to create sequence from template: {}", e.what());
        THROW_RUNTIME_ERROR("Failed to create sequence from template: " + std::string(e.what()));
    }
}

/**
 * @brief Applies template parameters to a template JSON.
 * @param templateJson The template JSON to modify.
 * @param params The parameters to apply.
 */
void ExposureSequence::applyTemplateParameters(json& templateJson, const json& params) {
    // Replace placeholders with parameter values using a recursive function
    std::function<void(json&)> processNode;

    processNode = [&](json& node) {
        if (node.is_string()) {
            std::string value = node.get<std::string>();

            // Check if this is a parameter placeholder (format: ${paramName})
            if (value.size() > 3 && value[0] == '$' && value[1] == '{' && value.back() == '}') {
                std::string paramName = value.substr(2, value.size() - 3);

                if (params.contains(paramName)) {
                    node = params[paramName];
                }
            }
        } else if (node.is_object()) {
            for (auto& [key, value] : node.items()) {
                processNode(value);
            }
        } else if (node.is_array()) {
            for (auto& element : node) {
                processNode(element);
            }
        }
    };

    processNode(templateJson);
}

}  // namespace lithium::task
