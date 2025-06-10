/**
 * @file task_templates.hpp
 * @brief Pre-defined task templates for common operations
 *
 * This file contains pre-defined task templates that can be used
 * to quickly create common task sequences.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_TEMPLATES_HPP
#define LITHIUM_TASK_TEMPLATES_HPP

#include <string>
#include <vector>
#include "atom/type/json.hpp"

namespace lithium::sequencer::templates {

using json = nlohmann::json;

/**
 * @brief Template manager for task sequences
 */
class TemplateManager {
public:
    /**
     * @brief Get imaging sequence template
     */
    static json getImagingSequenceTemplate();

    /**
     * @brief Get calibration sequence template
     */
    static json getCalibrationSequenceTemplate();

    /**
     * @brief Get focus sequence template
     */
    static json getFocusSequenceTemplate();

    /**
     * @brief Get plate solving template
     */
    static json getPlateSolvingTemplate();

    /**
     * @brief Get device setup template
     */
    static json getDeviceSetupTemplate();

    /**
     * @brief Get safety check template
     */
    static json getSafetyCheckTemplate();

    /**
     * @brief Get custom script execution template
     */
    static json getScriptExecutionTemplate();

    /**
     * @brief Get filter change template
     */
    static json getFilterChangeTemplate();

    /**
     * @brief Get guiding setup template
     */
    static json getGuidingSetupTemplate();

    /**
     * @brief Get complete observation template
     */
    static json getCompleteObservationTemplate();

    /**
     * @brief Create template from existing sequence
     */
    static json createTemplateFromSequence(const json& sequence,
                                           const std::string& templateName);

    /**
     * @brief Apply template with parameters
     */
    static json applyTemplate(const json& templateDef, const json& parameters);

    /**
     * @brief Validate template definition
     */
    static bool validateTemplate(const json& templateDef);

    /**
     * @brief Get all available templates
     */
    static std::vector<std::string> getAvailableTemplates();

    /**
     * @brief Register custom template
     */
    static void registerTemplate(const std::string& name,
                                 const json& templateDef);

    /**
     * @brief Unregister template
     */
    static void unregisterTemplate(const std::string& name);
};

/**
 * @brief Common task parameter sets
 */
namespace CommonTasks {

/**
 * @brief Standard exposure parameters
 */
json standardExposure(double exposureTime, int gain, int binning = 1,
                      const std::string& filter = "");

/**
 * @brief Dark frame parameters
 */
json darkFrame(double exposureTime, int gain, int binning = 1, int count = 10);

/**
 * @brief Flat frame parameters
 */
json flatFrame(double exposureTime, int gain, int binning = 1,
               const std::string& filter = "", int count = 10);

/**
 * @brief Bias frame parameters
 */
json biasFrame(int gain, int binning = 1, int count = 20);

/**
 * @brief Auto focus parameters
 */
json autoFocus(const std::string& filter = "", int samples = 7,
               double stepSize = 100);

/**
 * @brief Plate solve parameters
 */
json plateSolve(double exposureTime = 5.0, int gain = 100,
                double timeout = 60.0);

/**
 * @brief Device connection parameters
 */
json deviceConnect(const std::string& deviceName, const std::string& deviceType,
                   int timeout = 5000);

/**
 * @brief Filter change parameters
 */
json filterChange(const std::string& filterName, int timeout = 30);

/**
 * @brief Start guiding parameters
 */
json startGuiding(double exposureTime = 2.0, int gain = 100,
                  double tolerance = 1.0);

/**
 * @brief Safety check parameters
 */
json safetyCheck(bool checkWeather = true, bool checkHorizon = true,
                 bool checkSun = true);

}  // namespace CommonTasks

/**
 * @brief Sequence patterns for common workflows
 */
namespace SequencePatterns {

/**
 * @brief Create LRGB imaging sequence
 */
json createLRGBSequence(const json& target, const json& exposureConfig);

/**
 * @brief Create narrowband imaging sequence
 */
json createNarrowbandSequence(const json& target, const json& exposureConfig);

/**
 * @brief Create full calibration sequence
 */
json createFullCalibrationSequence(const json& cameraConfig);

/**
 * @brief Create meridian flip sequence
 */
json createMeridianFlipSequence(const json& target);

/**
 * @brief Create dithering sequence
 */
json createDitheringSequence(const json& baseSequence, int ditherSteps = 5);

/**
 * @brief Create mosaic sequence
 */
json createMosaicSequence(const std::vector<json>& targets,
                          const json& exposureConfig);

/**
 * @brief Create focus maintenance sequence
 */
json createFocusMaintenanceSequence(int intervalMinutes = 60);

/**
 * @brief Create weather monitoring sequence
 */
json createWeatherMonitoringSequence(int checkIntervalMinutes = 10);

}  // namespace SequencePatterns

/**
 * @brief Task validation helpers
 */
namespace TaskValidation {

/**
 * @brief Validate exposure parameters
 */
bool validateExposureParams(const json& params);

/**
 * @brief Validate device parameters
 */
bool validateDeviceParams(const json& params);

/**
 * @brief Validate filter parameters
 */
bool validateFilterParams(const json& params);

/**
 * @brief Validate focus parameters
 */
bool validateFocusParams(const json& params);

/**
 * @brief Validate guiding parameters
 */
bool validateGuidingParams(const json& params);

/**
 * @brief Validate script parameters
 */
bool validateScriptParams(const json& params);

/**
 * @brief Get parameter requirements for task type
 */
std::vector<std::string> getRequiredParameters(const std::string& taskType);

/**
 * @brief Get parameter schema for task type
 */
json getParameterSchema(const std::string& taskType);

}  // namespace TaskValidation

/**
 * @brief Task execution utilities
 */
namespace TaskUtils {

/**
 * @brief Calculate total sequence time
 */
std::chrono::seconds calculateSequenceTime(const json& sequence);

/**
 * @brief Estimate disk space requirements
 */
size_t estimateDiskSpace(const json& sequence);

/**
 * @brief Check resource availability
 */
bool checkResourceAvailability(const json& sequence);

/**
 * @brief Optimize sequence order
 */
json optimizeSequenceOrder(const json& sequence);

/**
 * @brief Split sequence into chunks
 */
std::vector<json> splitSequence(const json& sequence, size_t maxChunkSize);

/**
 * @brief Merge sequence chunks
 */
json mergeSequences(const std::vector<json>& sequences);

/**
 * @brief Generate sequence report
 */
json generateSequenceReport(const json& sequence);

}  // namespace TaskUtils

}  // namespace lithium::sequencer::templates

#endif  // LITHIUM_TASK_TEMPLATES_HPP
