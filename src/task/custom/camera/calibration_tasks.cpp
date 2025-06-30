// ==================== Includes and Declarations ====================
#include "calibration_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include "basic_exposure.hpp"

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

#define MOCK_CAMERA

namespace lithium::task::task {

// ==================== Mock Camera Class ====================
#ifdef MOCK_CAMERA
class MockCamera {
public:
    MockCamera() = default;

    bool getExposureStatus() const { return exposureStatus_; }
    void setGain(int g) { gain_ = g; }
    int getGain() const { return gain_; }
    void setOffset(int o) { offset_ = o; }
    int getOffset() const { return offset_; }
    void setBinning(int bx, int by) {
        binningX_ = bx;
        binningY_ = by;
    }
    std::tuple<int, int> getBinning() const { return {binningX_, binningY_}; }
    void startExposure(double t) {
        exposureStatus_ = true;
        exposureTime_ = t;
    }
    void saveExposureResult() { exposureStatus_ = false; }
    double getTemperature() const { return temperature_; }
    void setTargetTemperature(double temp) { targetTemperature_ = temp; }
    bool getCoolerEnabled() const { return coolerEnabled_; }
    void setCoolerEnabled(bool enabled) { coolerEnabled_ = enabled; }

private:
    bool exposureStatus_{false};
    double exposureTime_{0.0};
    int gain_{100};
    int offset_{10};
    int binningX_{1};
    int binningY_{1};
    double temperature_{-10.0};  // Mock temperature
    double targetTemperature_{-10.0};
    bool coolerEnabled_{false};
};
#endif

// ==================== AutoCalibrationTask Implementation ====================

auto AutoCalibrationTask::taskName() -> std::string {
    return "AutoCalibration";
}

void AutoCalibrationTask::execute(const json& params) { executeImpl(params); }

void AutoCalibrationTask::executeImpl(const json& params) {
    spdlog::info("Executing AutoCalibration task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        validateCalibrationParameters(params);

        int darkCount = params.value("dark_count", 10);
        int biasCount = params.value("bias_count", 20);
        int flatCount = params.value("flat_count", 10);
        double darkExposure = params.value("dark_exposure", 120.0);
        double flatExposure = params.value("flat_exposure", 1.0);
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);

        spdlog::info("Starting auto calibration: {} darks, {} bias, {} flats", darkCount, biasCount, flatCount);

        // Take bias frames
        if (biasCount > 0) {
            spdlog::info("Taking {} bias frames", biasCount);
            json biasParams = {{"count", biasCount},
                               {"exposure", 0.0001},  // Minimum exposure for bias
                               {"type", "bias"},
                               {"binning", binning},
                               {"gain", gain},
                               {"offset", offset}};

            TakeManyExposureTask manyExposureTask("TakeManyExposure", [](const json&) {});
            manyExposureTask.execute(biasParams);
        }

        // Take dark frames
        if (darkCount > 0) {
            spdlog::info("Taking {} dark frames at {} seconds", darkCount, darkExposure);
            json darkParams = {{"count", darkCount}, {"exposure", darkExposure},
                               {"type", "dark"},     {"binning", binning},
                               {"gain", gain},       {"offset", offset}};

            TakeManyExposureTask manyExposureTask("TakeManyExposure", [](const json&) {});
            manyExposureTask.execute(darkParams);
        }

        // Take flat frames
        if (flatCount > 0) {
            spdlog::info("Taking {} flat frames at {} seconds", flatCount, flatExposure);
            json flatParams = {{"count", flatCount}, {"exposure", flatExposure},
                               {"type", "flat"},     {"binning", binning},
                               {"gain", gain},       {"offset", offset}};

            TakeManyExposureTask manyExposureTask("TakeManyExposure", [](const json&) {});
            manyExposureTask.execute(flatParams);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        spdlog::info("AutoCalibration task completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        spdlog::error("AutoCalibration task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto AutoCalibrationTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            AutoCalibrationTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced AutoCalibration task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(4);  // Medium priority for calibration
    task->setTimeout(std::chrono::seconds(7200));  // 2 hour timeout
    task->setLogLevel(2);                          // INFO level
    task->setTaskType(taskName());

    return task;
}

void AutoCalibrationTask::defineParameters(Task& task) {
    task.addParamDefinition("dark_count", "int", false, 10, "Number of dark frames to take");
    task.addParamDefinition("bias_count", "int", false, 20, "Number of bias frames to take");
    task.addParamDefinition("flat_count", "int", false, 10, "Number of flat frames to take");
    task.addParamDefinition("dark_exposure", "double", false, 120.0, "Dark frame exposure time in seconds");
    task.addParamDefinition("flat_exposure", "double", false, 1.0, "Flat frame exposure time in seconds");
    task.addParamDefinition("binning", "int", false, 1, "Camera binning factor");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset/brightness value");
}

void AutoCalibrationTask::validateCalibrationParameters(const json& params) {
    if (params.contains("dark_count")) {
        int count = params["dark_count"].get<int>();
        if (count < 0 || count > 100) {
            THROW_INVALID_ARGUMENT("Dark count must be between 0 and 100");
        }
    }

    if (params.contains("bias_count")) {
        int count = params["bias_count"].get<int>();
        if (count < 0 || count > 100) {
            THROW_INVALID_ARGUMENT("Bias count must be between 0 and 100");
        }
    }

    if (params.contains("flat_count")) {
        int count = params["flat_count"].get<int>();
        if (count < 0 || count > 100) {
            THROW_INVALID_ARGUMENT("Flat count must be between 0 and 100");
        }
    }

    if (params.contains("dark_exposure")) {
        double exposure = params["dark_exposure"].get<double>();
        if (exposure <= 0 || exposure > 3600) {  // Max 1 hour
            THROW_INVALID_ARGUMENT("Dark exposure must be between 0 and 3600 seconds");
        }
    }

    if (params.contains("flat_exposure")) {
        double exposure = params["flat_exposure"].get<double>();
        if (exposure <= 0 || exposure > 60) {  // Max 1 minute for flats
            THROW_INVALID_ARGUMENT("Flat exposure must be between 0 and 60 seconds");
        }
    }
}

// ==================== ThermalCycleTask Implementation ====================

auto ThermalCycleTask::taskName() -> std::string { return "ThermalCycle"; }

void ThermalCycleTask::execute(const json& params) { executeImpl(params); }

void ThermalCycleTask::executeImpl(const json& params) {
    spdlog::info("Executing ThermalCycle task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        validateThermalParameters(params);

        double startTemp = params.value("start_temp", 20.0);
        double endTemp = params.value("end_temp", -20.0);
        double stepTemp = params.value("step_temp", -5.0);
        int exposuresPerTemp = params.value("exposures_per_temp", 5);
        double exposureTime = params.value("exposure_time", 60.0);
        int stabilizeTime = params.value("stabilize_time", 300);  // 5 minutes default

        spdlog::info("Starting thermal cycle from {} to {} °C in {} °C steps", startTemp, endTemp, stepTemp);

        // Validate step direction
        if ((endTemp > startTemp && stepTemp < 0) ||
            (endTemp < startTemp && stepTemp > 0)) {
            THROW_INVALID_ARGUMENT("Step temperature direction doesn't match start/end temperature range");
        }

#ifdef MOCK_CAMERA
        std::shared_ptr<MockCamera> camera = std::make_shared<MockCamera>();

        if (!camera) {
            spdlog::error("Mock camera creation failed");
            THROW_RUNTIME_ERROR("Mock camera creation failed");
        }

        // Enable cooling
        camera->setCoolerEnabled(true);

        double currentTemp = startTemp;
        int tempStep = 0;

        while ((stepTemp > 0 && currentTemp <= endTemp) ||
               (stepTemp < 0 && currentTemp >= endTemp)) {
            spdlog::info("Setting temperature to {} °C (step {})", currentTemp, tempStep);
            camera->setTargetTemperature(currentTemp);

            // Wait for temperature stabilization
            spdlog::info("Waiting {} seconds for temperature stabilization", stabilizeTime);
            std::this_thread::sleep_for(std::chrono::seconds(stabilizeTime));

            // Take exposures at this temperature
            spdlog::info("Taking {} exposures at {} °C", exposuresPerTemp, currentTemp);
            json exposureParams = {{"count", exposuresPerTemp},
                                   {"exposure", exposureTime},
                                   {"type", "dark"},
                                   {"binning", 1},
                                   {"gain", 100},
                                   {"offset", 10}};

            TakeManyExposureTask manyExposureTask("TakeManyExposure", [](const json&) {});
            manyExposureTask.execute(exposureParams);

            currentTemp += stepTemp;
            tempStep++;

            // Safety check to prevent infinite loops
            if (tempStep > 100) {
                spdlog::warn("Maximum temperature steps (100) reached, stopping thermal cycle");
                break;
            }
        }
#else
        spdlog::error("Real camera thermal control not implemented yet");
        THROW_RUNTIME_ERROR("Real camera thermal control not implemented yet");
#endif

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        spdlog::info("ThermalCycle task completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        spdlog::error("ThermalCycle task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto ThermalCycleTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            ThermalCycleTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced ThermalCycle task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(2);  // Low priority for long-running tasks
    task->setTimeout(std::chrono::seconds(14400));  // 4 hour timeout
    task->setLogLevel(2);                           // INFO level
    task->setTaskType(taskName());

    return task;
}

void ThermalCycleTask::defineParameters(Task& task) {
    task.addParamDefinition("start_temp", "double", false, 20.0, "Starting temperature in Celsius");
    task.addParamDefinition("end_temp", "double", false, -20.0, "Ending temperature in Celsius");
    task.addParamDefinition("step_temp", "double", false, -5.0, "Temperature step in Celsius");
    task.addParamDefinition("exposures_per_temp", "int", false, 5, "Number of exposures per temperature");
    task.addParamDefinition("exposure_time", "double", false, 60.0, "Exposure time in seconds");
    task.addParamDefinition("stabilize_time", "int", false, 300, "Temperature stabilization time in seconds");
}

void ThermalCycleTask::validateThermalParameters(const json& params) {
    if (params.contains("start_temp")) {
        double temp = params["start_temp"].get<double>();
        if (temp < -50.0 || temp > 50.0) {
            THROW_INVALID_ARGUMENT("Start temperature must be between -50 and 50 °C");
        }
    }

    if (params.contains("end_temp")) {
        double temp = params["end_temp"].get<double>();
        if (temp < -50.0 || temp > 50.0) {
            THROW_INVALID_ARGUMENT("End temperature must be between -50 and 50 °C");
        }
    }

    if (params.contains("step_temp")) {
        double step = params["step_temp"].get<double>();
        if (std::abs(step) > 20.0 || step == 0.0) {
            THROW_INVALID_ARGUMENT("Temperature step must be between -20 and 20 °C and not zero");
        }
    }

    if (params.contains("exposures_per_temp")) {
        int count = params["exposures_per_temp"].get<int>();
        if (count <= 0 || count > 50) {
            THROW_INVALID_ARGUMENT("Exposures per temperature must be between 1 and 50");
        }
    }

    if (params.contains("exposure_time")) {
        double time = params["exposure_time"].get<double>();
        if (time <= 0 || time > 3600) {
            THROW_INVALID_ARGUMENT("Exposure time must be between 0 and 3600 seconds");
        }
    }

    if (params.contains("stabilize_time")) {
        int time = params["stabilize_time"].get<int>();
        if (time < 0 || time > 3600) {
            THROW_INVALID_ARGUMENT("Stabilization time must be between 0 and 3600 seconds");
        }
    }
}

// ==================== FlatFieldSequenceTask Implementation ====================

auto FlatFieldSequenceTask::taskName() -> std::string {
    return "FlatFieldSequence";
}

void FlatFieldSequenceTask::execute(const json& params) { executeImpl(params); }

void FlatFieldSequenceTask::executeImpl(const json& params) {
    spdlog::info("Executing FlatFieldSequence task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        validateFlatFieldParameters(params);

        std::vector<std::string> filters = params.value(
            "filters", std::vector<std::string>{"L", "R", "G", "B"});
        int exposuresPerFilter = params.value("exposures_per_filter", 10);
        double baseExposure = params.value("base_exposure", 1.0);
        bool autoExposure = params.value("auto_exposure", true);
        int targetADU = params.value("target_adu", 30000);
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);

        spdlog::info("Starting flat field sequence for {} filters with {} exposures each", filters.size(), exposuresPerFilter);

        for (const auto& filter : filters) {
            spdlog::info("Taking flat frames for filter: {}", filter);

            double exposureTime = baseExposure;

            // Auto-exposure logic for flats (simplified)
            if (autoExposure) {
                spdlog::info("Determining optimal exposure time for filter {}", filter);
                // Take a test exposure
                json testParams = {{"exposure", baseExposure},
                                   {"type", "flat"},
                                   {"binning", binning},
                                   {"gain", gain},
                                   {"offset", offset}};

                TakeExposureTask exposureTask("TakeExposure", [](const json&) {});
                exposureTask.execute(testParams);

                // In a real implementation, we would analyze the histogram
                // For now, just use the base exposure
                spdlog::info("Using exposure time {} seconds for filter {}", exposureTime, filter);
            }

            // Take the flat sequence
            json flatParams = {{"count", exposuresPerFilter},
                               {"exposure", exposureTime},
                               {"type", "flat"},
                               {"binning", binning},
                               {"gain", gain},
                               {"offset", offset}};

            TakeManyExposureTask manyExposureTask("TakeManyExposure", [](const json&) {});
            manyExposureTask.execute(flatParams);

            spdlog::info("Completed {} flat frames for filter {}", exposuresPerFilter, filter);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        spdlog::info("FlatFieldSequence task completed in {} ms", duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        spdlog::error("FlatFieldSequence task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto FlatFieldSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            FlatFieldSequenceTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced FlatFieldSequence task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(3);  // Medium-low priority for calibration
    task->setTimeout(std::chrono::seconds(3600));  // 1 hour timeout
    task->setLogLevel(2);                          // INFO level
    task->setTaskType(taskName());

    return task;
}

void FlatFieldSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("filters", "array", false, json::array({"L", "R", "G", "B"}), "List of filters to take flats for");
    task.addParamDefinition("exposures_per_filter", "int", false, 10, "Number of flat exposures per filter");
    task.addParamDefinition("base_exposure", "double", false, 1.0, "Base exposure time for flats");
    task.addParamDefinition("auto_exposure", "bool", false, true, "Automatically determine optimal exposure");
    task.addParamDefinition("target_adu", "int", false, 30000, "Target ADU level for auto-exposure");
    task.addParamDefinition("binning", "int", false, 1, "Camera binning factor");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset/brightness value");
}

void FlatFieldSequenceTask::validateFlatFieldParameters(const json& params) {
    if (params.contains("exposures_per_filter")) {
        int count = params["exposures_per_filter"].get<int>();
        if (count <= 0 || count > 100) {
            THROW_INVALID_ARGUMENT("Exposures per filter must be between 1 and 100");
        }
    }

    if (params.contains("base_exposure")) {
        double exposure = params["base_exposure"].get<double>();
        if (exposure <= 0 || exposure > 60) {
            THROW_INVALID_ARGUMENT("Base exposure must be between 0 and 60 seconds");
        }
    }

    if (params.contains("target_adu")) {
        int adu = params["target_adu"].get<int>();
        if (adu <= 0 || adu > 65535) {
            THROW_INVALID_ARGUMENT("Target ADU must be between 1 and 65535");
        }
    }

    if (params.contains("filters") && params["filters"].is_array()) {
        auto filters = params["filters"].get<std::vector<std::string>>();
        if (filters.empty() || filters.size() > 20) {
            THROW_INVALID_ARGUMENT("Filter list must contain between 1 and 20 filters");
        }
    }

    // Validate individual camera parameters
    TakeExposureTask::validateCameraParameters(params);
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register AutoCalibrationTask
AUTO_REGISTER_TASK(
    AutoCalibrationTask, "AutoCalibration",
    (TaskInfo{
        .name = "AutoCalibration",
        .description = "Automatically takes calibration frames (darks, bias, flats)",
        .category = "Calibration",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"dark_count", json{{"type", "integer"},
                                           {"minimum", 0},
                                           {"maximum", 100}}},
                       {"bias_count", json{{"type", "integer"},
                                           {"minimum", 0},
                                           {"maximum", 100}}},
                       {"flat_count", json{{"type", "integer"},
                                           {"minimum", 0},
                                           {"maximum", 100}}},
                       {"dark_exposure", json{{"type", "number"},
                                              {"minimum", 0},
                                              {"maximum", 3600}}},
                       {"flat_exposure", json{{"type", "number"},
                                              {"minimum", 0},
                                              {"maximum", 60}}},
                       {"binning", json{{"type", "integer"},
                                        {"minimum", 1}}},
                       {"gain", json{{"type", "integer"},
                                     {"minimum", 0}}},
                       {"offset", json{{"type", "integer"},
                                       {"minimum", 0}}}}}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

// Register ThermalCycleTask
AUTO_REGISTER_TASK(
    ThermalCycleTask, "ThermalCycle",
    (TaskInfo{
        .name = "ThermalCycle",
        .description = "Performs thermal cycling while taking dark frames",
        .category = "Calibration",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"start_temp", json{{"type", "number"},
                                           {"minimum", -50},
                                           {"maximum", 50}}},
                       {"end_temp", json{{"type", "number"},
                                         {"minimum", -50},
                                         {"maximum", 50}}},
                       {"step_temp", json{{"type", "number"},
                                          {"minimum", -20},
                                          {"maximum", 20}}},
                       {"exposures_per_temp", json{{"type", "integer"},
                                                   {"minimum", 1},
                                                   {"maximum", 50}}},
                       {"exposure_time", json{{"type", "number"},
                                              {"minimum", 0},
                                              {"maximum", 3600}}},
                       {"stabilize_time", json{{"type", "integer"},
                                               {"minimum", 0},
                                               {"maximum", 3600}}}}}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

// Register FlatFieldSequenceTask
AUTO_REGISTER_TASK(
    FlatFieldSequenceTask, "FlatFieldSequence",
    (TaskInfo{
        .name = "FlatFieldSequence",
        .description = "Takes flat field frames for multiple filters",
        .category = "Calibration",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"filters", json{{"type", "array"},
                                        {"items", json{{"type", "string"}}}}},
                       {"exposures_per_filter", json{{"type", "integer"},
                                                     {"minimum", 1},
                                                     {"maximum", 100}}},
                       {"base_exposure", json{{"type", "number"},
                                              {"minimum", 0},
                                              {"maximum", 60}}},
                       {"auto_exposure", json{{"type", "boolean"}}},
                       {"target_adu", json{{"type", "integer"},
                                           {"minimum", 1},
                                           {"maximum", 65535}}},
                       {"binning", json{{"type", "integer"},
                                        {"minimum", 1}}},
                       {"gain", json{{"type", "integer"},
                                     {"minimum", 0}}},
                       {"offset", json{{"type", "integer"},
                                       {"minimum", 0}}}}}},
        .version = "1.0.0",
        .dependencies = {"TakeExposure"}}));

}  // namespace
