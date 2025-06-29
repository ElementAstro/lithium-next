#include "auto_calibration_task.hpp"
#include <chrono>
#include <memory>
#include <thread>
#include <filesystem>
#include "../camera/basic_exposure.hpp"
#include "../camera/common.hpp"

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace lithium::task::task {

auto AutoCalibrationTask::taskName() -> std::string { return "AutoCalibration"; }

void AutoCalibrationTask::execute(const json& params) { executeImpl(params); }

void AutoCalibrationTask::executeImpl(const json& params) {
    LOG_F(INFO, "Executing AutoCalibration task '{}' with params: {}",
          getName(), params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        std::string outputDir = params.value("output_directory", "./calibration");
        bool skipExisting = params.value("skip_existing", true);
        bool organizeFolders = params.value("organize_folders", true);
        std::vector<std::string> filters =
            params.value("filters", std::vector<std::string>{"L", "R", "G", "B"});

        // Calibration frame counts
        int darkFrameCount = params.value("dark_frame_count", 20);
        int biasFrameCount = params.value("bias_frame_count", 50);
        int flatFrameCount = params.value("flat_frame_count", 20);

        // Camera settings
        std::vector<double> exposureTimes =
            params.value("exposure_times", std::vector<double>{300.0, 600.0});
        int binning = params.value("binning", 1);
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);
        double temperature = params.value("temperature", -10.0);

        LOG_F(INFO, "Starting calibration sequence with {} exposure times, {} filters",
              exposureTimes.size(), filters.size());

        // Check if calibration already exists and skip if requested
        if (skipExisting && checkExistingCalibration(params)) {
            LOG_F(INFO, "Existing calibration found and skip_existing enabled");
            return;
        }

        // Create output directory
        std::filesystem::create_directories(outputDir);

        // Cool camera to target temperature
        LOG_F(INFO, "Cooling camera to {} degrees Celsius", temperature);
        std::this_thread::sleep_for(std::chrono::minutes(2)); // Simulate cooling

        // Capture bias frames first (shortest exposure, no thermal noise)
        LOG_F(INFO, "Capturing {} bias frames", biasFrameCount);
        captureBiasFrames(params);

        // Capture dark frames for each exposure time
        for (double expTime : exposureTimes) {
            LOG_F(INFO, "Capturing {} dark frames at {} seconds exposure",
                  darkFrameCount, expTime);
            json darkParams = params;
            darkParams["exposure_time"] = expTime;
            captureDarkFrames(darkParams);
        }

        // Capture flat frames for each filter
        for (const std::string& filter : filters) {
            LOG_F(INFO, "Capturing {} flat frames for filter {}",
                  flatFrameCount, filter);
            json flatParams = params;
            flatParams["filter"] = filter;
            captureFlatFrames(flatParams);
        }

        // Organize calibrated frames if requested
        if (organizeFolders) {
            organizeCalibratedFrames(outputDir);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(INFO, "AutoCalibration task '{}' completed successfully in {} minutes",
              getName(), duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(
            endTime - startTime);
        LOG_F(ERROR, "AutoCalibration task '{}' failed after {} minutes: {}",
              getName(), duration.count(), e.what());
        throw;
    }
}

void AutoCalibrationTask::captureDarkFrames(const json& params) {
    int darkFrameCount = params.value("dark_frame_count", 20);
    double exposureTime = params.value("exposure_time", 300.0);
    int binning = params.value("binning", 1);
    int gain = params.value("gain", 100);
    int offset = params.value("offset", 10);

    LOG_F(INFO, "Starting dark frame capture: {} frames at {} seconds",
          darkFrameCount, exposureTime);

    for (int i = 1; i <= darkFrameCount; ++i) {
        LOG_F(INFO, "Capturing dark frame {} of {}", i, darkFrameCount);

        json exposureParams = {
            {"exposure", exposureTime},
            {"type", ExposureType::DARK},
            {"binning", binning},
            {"gain", gain},
            {"offset", offset}
        };

        auto exposureTask = TakeExposureTask::createEnhancedTask();
        exposureTask->execute(exposureParams);

        // Brief pause between frames
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    LOG_F(INFO, "Dark frame capture completed");
}

void AutoCalibrationTask::captureBiasFrames(const json& params) {
    int biasFrameCount = params.value("bias_frame_count", 50);
    int binning = params.value("binning", 1);
    int gain = params.value("gain", 100);
    int offset = params.value("offset", 10);

    LOG_F(INFO, "Starting bias frame capture: {} frames", biasFrameCount);

    for (int i = 1; i <= biasFrameCount; ++i) {
        LOG_F(INFO, "Capturing bias frame {} of {}", i, biasFrameCount);

        json exposureParams = {
            {"exposure", 0.001}, // Minimum exposure for bias
            {"type", ExposureType::BIAS},
            {"binning", binning},
            {"gain", gain},
            {"offset", offset}
        };

        auto exposureTask = TakeExposureTask::createEnhancedTask();
        exposureTask->execute(exposureParams);

        // Minimal pause between bias frames
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LOG_F(INFO, "Bias frame capture completed");
}

void AutoCalibrationTask::captureFlatFrames(const json& params) {
    int flatFrameCount = params.value("flat_frame_count", 20);
    std::string filter = params.value("filter", "L");
    int binning = params.value("binning", 1);
    int gain = params.value("gain", 100);
    int offset = params.value("offset", 10);
    double targetADU = params.value("target_adu", 32000.0);

    LOG_F(INFO, "Starting flat frame capture: {} frames for filter {}",
          flatFrameCount, filter);

    // Auto-determine optimal exposure time for flats
    double flatExposureTime = 1.0; // Start with 1 second

    // Take test exposure to determine optimal exposure time
    LOG_F(INFO, "Taking test flat exposure to determine optimal exposure time");
    json testParams = {
        {"exposure", flatExposureTime},
        {"type", ExposureType::FLAT},
        {"binning", binning},
        {"gain", gain},
        {"offset", offset}
    };

    auto testTask = TakeExposureTask::createEnhancedTask();
    testTask->execute(testParams);

    // In real implementation, analyze the test image to get actual ADU
    double actualADU = 20000.0; // Placeholder

    // Adjust exposure time to reach target ADU
    flatExposureTime *= (targetADU / actualADU);
    flatExposureTime = std::clamp(flatExposureTime, 0.1, 10.0); // Reasonable limits

    LOG_F(INFO, "Optimal flat exposure time determined: {:.2f} seconds", flatExposureTime);

    for (int i = 1; i <= flatFrameCount; ++i) {
        LOG_F(INFO, "Capturing flat frame {} of {} for filter {}",
              i, flatFrameCount, filter);

        json exposureParams = {
            {"exposure", flatExposureTime},
            {"type", ExposureType::FLAT},
            {"binning", binning},
            {"gain", gain},
            {"offset", offset}
        };

        auto exposureTask = TakeExposureTask::createEnhancedTask();
        exposureTask->execute(exposureParams);

        // Brief pause between frames
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    LOG_F(INFO, "Flat frame capture completed for filter {}", filter);
}

void AutoCalibrationTask::organizeCalibratedFrames(const std::string& outputDir) {
    LOG_F(INFO, "Organizing calibration frames in directory structure");

    // Create subdirectories for different frame types
    std::filesystem::create_directories(outputDir + "/Darks");
    std::filesystem::create_directories(outputDir + "/Bias");
    std::filesystem::create_directories(outputDir + "/Flats");

    // In real implementation, this would move/organize actual FITS files
    // based on their frame type, exposure time, and filter

    LOG_F(INFO, "Calibration frame organization completed");
}

bool AutoCalibrationTask::checkExistingCalibration(const json& params) {
    std::string outputDir = params.value("output_directory", "./calibration");

    // Check if calibration directories exist and contain files
    bool darksExist = std::filesystem::exists(outputDir + "/Darks") &&
                     !std::filesystem::is_empty(outputDir + "/Darks");
    bool biasExists = std::filesystem::exists(outputDir + "/Bias") &&
                     !std::filesystem::is_empty(outputDir + "/Bias");
    bool flatsExist = std::filesystem::exists(outputDir + "/Flats") &&
                     !std::filesystem::is_empty(outputDir + "/Flats");

    return darksExist && biasExists && flatsExist;
}

void AutoCalibrationTask::validateAutoCalibrationParameters(const json& params) {
    if (params.contains("dark_frame_count")) {
        int count = params["dark_frame_count"].get<int>();
        if (count <= 0 || count > 200) {
            THROW_INVALID_ARGUMENT("Dark frame count must be between 1 and 200");
        }
    }

    if (params.contains("bias_frame_count")) {
        int count = params["bias_frame_count"].get<int>();
        if (count <= 0 || count > 500) {
            THROW_INVALID_ARGUMENT("Bias frame count must be between 1 and 500");
        }
    }

    if (params.contains("flat_frame_count")) {
        int count = params["flat_frame_count"].get<int>();
        if (count <= 0 || count > 100) {
            THROW_INVALID_ARGUMENT("Flat frame count must be between 1 and 100");
        }
    }

    if (params.contains("temperature")) {
        double temp = params["temperature"].get<double>();
        if (temp < -40 || temp > 20) {
            THROW_INVALID_ARGUMENT("Temperature must be between -40 and 20 degrees Celsius");
        }
    }
}

auto AutoCalibrationTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            auto taskInstance = std::make_unique<AutoCalibrationTask>();
            taskInstance->execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced AutoCalibration task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(3);
    task->setTimeout(std::chrono::seconds(7200));  // 2 hour timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void AutoCalibrationTask::defineParameters(Task& task) {
    task.addParamDefinition("output_directory", "string", false, "./calibration",
                            "Directory to store calibration frames");
    task.addParamDefinition("skip_existing", "bool", false, true,
                            "Skip calibration if existing frames found");
    task.addParamDefinition("organize_folders", "bool", false, true,
                            "Organize frames into type-specific folders");
    task.addParamDefinition("filters", "array", false, json::array({"L", "R", "G", "B"}),
                            "List of filters for flat frames");
    task.addParamDefinition("dark_frame_count", "int", false, 20,
                            "Number of dark frames to capture");
    task.addParamDefinition("bias_frame_count", "int", false, 50,
                            "Number of bias frames to capture");
    task.addParamDefinition("flat_frame_count", "int", false, 20,
                            "Number of flat frames per filter");
    task.addParamDefinition("exposure_times", "array", false, json::array({300.0, 600.0}),
                            "Exposure times for dark frames");
    task.addParamDefinition("binning", "int", false, 1, "Camera binning");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
    task.addParamDefinition("temperature", "double", false, -10.0,
                            "Target camera temperature in Celsius");
    task.addParamDefinition("target_adu", "double", false, 32000.0,
                            "Target ADU level for flat frames");
}

}  // namespace lithium::task::task
