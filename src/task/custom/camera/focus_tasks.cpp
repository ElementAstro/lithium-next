// ==================== Includes and Declarations ====================
#include "focus_tasks.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "../factory.hpp"

#include "atom/error/exception.hpp"

namespace lithium::task::task {

// ==================== Mock Classes for Testing ====================
#define MOCK_CAMERA
#ifdef MOCK_CAMERA

class MockFocuser {
public:
    MockFocuser()
        : position_(25000),
          tempComp_(false),
          temperature_(20.0),
          moving_(false) {}

    void setPosition(int pos) {
        position_ = std::clamp(pos, 0, 50000);
        moving_ = true;
        spdlog::info("MockFocuser: Moving to position {}", position_);

        // Simulate movement time
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            moving_ = false;
        }).detach();
    }

    int getPosition() const { return position_; }
    bool isMoving() const { return moving_; }
    void setTemperatureCompensation(bool enable) { tempComp_ = enable; }
    bool getTemperatureCompensation() const { return tempComp_; }
    double getTemperature() const { return temperature_; }
    void setTemperature(double temp) { temperature_ = temp; }

private:
    int position_;
    bool tempComp_;
    double temperature_;
    bool moving_;
};

class MockCamera {
public:
    MockCamera()
        : exposureStatus_(false),
          exposureTime_(0),
          gain_(100),
          offset_(10),
          binningX_(1),
          binningY_(1) {
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    }

    bool getExposureStatus() const { return exposureStatus_; }
    void setGain(int g) { gain_ = std::clamp(g, 0, 1000); }
    int getGain() const { return gain_; }
    void setOffset(int o) { offset_ = std::clamp(o, 0, 100); }
    int getOffset() const { return offset_; }
    void setBinning(int bx, int by) {
        binningX_ = std::clamp(bx, 1, 4);
        binningY_ = std::clamp(by, 1, 4);
    }
    std::tuple<int, int> getBinning() const { return {binningX_, binningY_}; }

    void startExposure(double t) {
        exposureTime_ = t;
        exposureStatus_ = true;
        spdlog::info("MockCamera: Starting {:.1f}s exposure", t);

        // Simulate exposure time in a separate thread
        std::thread([this, t]() {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(t * 100)));
            exposureStatus_ = false;
        }).detach();
    }

    void saveExposureResult() {
        exposureStatus_ = false;
        spdlog::info("MockCamera: Exposure saved");
    }

    double calculateHFR() {
        // Simulate realistic HFR calculation with some randomness
        std::uniform_real_distribution<double> dist(1.5, 4.0);
        double hfr = dist(rng_);
        spdlog::info("MockCamera: Calculated HFR = {:.2f}", hfr);
        return hfr;
    }

private:
    bool exposureStatus_;
    double exposureTime_;
    int gain_;
    int offset_;
    int binningX_;
    int binningY_;
    mutable std::mt19937 rng_;
};

// Static instances for mock testing
static std::shared_ptr<MockFocuser> mockFocuser =
    std::make_shared<MockFocuser>();
static std::shared_ptr<MockCamera> mockCamera = std::make_shared<MockCamera>();
#endif

// ==================== AutoFocusTask Implementation ====================

auto AutoFocusTask::taskName() -> std::string { return "AutoFocus"; }

void AutoFocusTask::execute(const json& params) { executeImpl(params); }

void AutoFocusTask::executeImpl(const json& params) {
    spdlog::info("Executing AutoFocus task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        validateAutoFocusParameters(params);

        double exposure = params.value("exposure", 1.0);
        int stepSize = params.value("step_size", 100);
        int maxSteps = params.value("max_steps", 50);
        double tolerance = params.value("tolerance", 0.1);

        spdlog::info(
            "Starting autofocus with {:.1f}s exposures, step size {}, max {} "
            "steps",
            exposure, stepSize, maxSteps);

#ifdef MOCK_CAMERA
        auto currentFocuser = mockFocuser;
        auto currentCamera = mockCamera;
#else
        throw std::runtime_error(
            "Real device support not implemented in this example");
#endif

        int startPosition = currentFocuser->getPosition();
        int bestPosition = startPosition;
        double bestHFR = 999.0;

        // Coarse focus sweep
        std::vector<std::pair<int, double>> measurements;

        for (int step = -maxSteps / 2; step <= maxSteps / 2; step += 5) {
            int position = startPosition + (step * stepSize);
            currentFocuser->setPosition(position);

            // Wait for focuser to stop moving
            while (currentFocuser->isMoving()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Take exposure and measure HFR
            currentCamera->startExposure(exposure);
            while (currentCamera->getExposureStatus()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            double hfr = currentCamera->calculateHFR();
            measurements.push_back({position, hfr});

            spdlog::info("Position: {}, HFR: {:.2f}", position, hfr);

            if (hfr < bestHFR) {
                bestHFR = hfr;
                bestPosition = position;
            }
        }

        // Fine focus around best position
        spdlog::info("Fine focusing around position {} (HFR: {:.2f})",
                     bestPosition, bestHFR);

        for (int offset = -2; offset <= 2; ++offset) {
            int position = bestPosition + (offset * stepSize / 5);
            currentFocuser->setPosition(position);

            while (currentFocuser->isMoving()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            currentCamera->startExposure(exposure);
            while (currentCamera->getExposureStatus()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            double hfr = currentCamera->calculateHFR();
            spdlog::info("Fine position: {}, HFR: {:.2f}", position, hfr);

            if (hfr < bestHFR) {
                bestHFR = hfr;
                bestPosition = position;
            }
        }

        // Move to best position
        currentFocuser->setPosition(bestPosition);

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info(
            "AutoFocus completed in {} ms. Best position: {}, HFR: {:.2f}",
            duration.count(), bestPosition, bestHFR);

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("AutoFocus task failed after {} ms: {}", duration.count(),
                      e.what());
        throw;
    }
}

auto AutoFocusTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            AutoFocusTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced AutoFocus task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(8);  // High priority for focus tasks
    task->setTimeout(std::chrono::seconds(600));  // 10 minute timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void AutoFocusTask::defineParameters(Task& task) {
    task.addParamDefinition("exposure", "double", false, 1.0,
                            "Focus test exposure time in seconds");
    task.addParamDefinition("step_size", "int", false, 100,
                            "Focuser step size for each movement");
    task.addParamDefinition("max_steps", "int", false, 50,
                            "Maximum number of focus steps to try");
    task.addParamDefinition("tolerance", "double", false, 0.1,
                            "Focus tolerance for convergence");
}

void AutoFocusTask::validateAutoFocusParameters(const json& params) {
    if (params.contains("exposure")) {
        double exposure = params["exposure"].get<double>();
        if (exposure <= 0 || exposure > 60) {
            THROW_INVALID_ARGUMENT(
                "Exposure time must be between 0 and 60 seconds");
        }
    }

    if (params.contains("step_size")) {
        int stepSize = params["step_size"].get<int>();
        if (stepSize < 1 || stepSize > 1000) {
            THROW_INVALID_ARGUMENT("Step size must be between 1 and 1000");
        }
    }

    if (params.contains("max_steps")) {
        int maxSteps = params["max_steps"].get<int>();
        if (maxSteps < 5 || maxSteps > 200) {
            THROW_INVALID_ARGUMENT("Max steps must be between 5 and 200");
        }
    }
}

// ==================== FocusSeriesTask Implementation ====================

auto FocusSeriesTask::taskName() -> std::string { return "FocusSeries"; }

void FocusSeriesTask::execute(const json& params) { executeImpl(params); }

void FocusSeriesTask::executeImpl(const json& params) {
    spdlog::info("Executing FocusSeries task with params: {}", params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        validateFocusSeriesParameters(params);

        int startPos = params.at("start_position").get<int>();
        int endPos = params.at("end_position").get<int>();
        int stepSize = params.value("step_size", 100);
        double exposure = params.value("exposure", 2.0);

        spdlog::info("Taking focus series from {} to {} with step {}", startPos,
                     endPos, stepSize);

#ifdef MOCK_CAMERA
        auto currentFocuser = mockFocuser;
        auto currentCamera = mockCamera;
#else
        throw std::runtime_error(
            "Real device support not implemented in this example");
#endif

        int direction = (endPos > startPos) ? 1 : -1;
        int currentPos = startPos;
        int frameCount = 0;
        std::vector<std::pair<int, double>> focusData;

        while ((direction > 0 && currentPos <= endPos) ||
               (direction < 0 && currentPos >= endPos)) {
            currentFocuser->setPosition(currentPos);

            // Wait for focuser to reach position
            while (currentFocuser->isMoving()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Take exposure
            currentCamera->startExposure(exposure);
            while (currentCamera->getExposureStatus()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Calculate HFR for this position
            double hfr = currentCamera->calculateHFR();
            focusData.emplace_back(currentPos, hfr);

            spdlog::info("Frame {}: Position {}, HFR {:.2f}", frameCount + 1,
                         currentPos, hfr);

            frameCount++;
            currentPos += (direction * stepSize);
        }

        // Find best focus position from series
        auto bestIt = std::min_element(
            focusData.begin(), focusData.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;  // Compare HFR values
            });

        if (bestIt != focusData.end()) {
            spdlog::info("Best focus found at position {} with HFR {:.2f}",
                         bestIt->first, bestIt->second);

            // Move to best position
            currentFocuser->setPosition(bestIt->first);
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("FocusSeries completed {} frames in {} ms", frameCount,
                     duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("FocusSeries task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto FocusSeriesTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            FocusSeriesTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced FocusSeries task failed: {}", e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(6);
    task->setTimeout(std::chrono::seconds(1800));  // 30 minute timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void FocusSeriesTask::defineParameters(Task& task) {
    task.addParamDefinition("start_position", "int", true, 20000,
                            "Starting focuser position");
    task.addParamDefinition("end_position", "int", true, 30000,
                            "Ending focuser position");
    task.addParamDefinition("step_size", "int", false, 100,
                            "Step size between positions");
    task.addParamDefinition("exposure", "double", false, 2.0,
                            "Exposure time per frame in seconds");
}

void FocusSeriesTask::validateFocusSeriesParameters(const json& params) {
    if (!params.contains("start_position") ||
        !params.contains("end_position")) {
        THROW_INVALID_ARGUMENT(
            "Missing start_position or end_position parameters");
    }

    int startPos = params["start_position"].get<int>();
    int endPos = params["end_position"].get<int>();

    if (startPos < 0 || startPos > 100000 || endPos < 0 || endPos > 100000) {
        THROW_INVALID_ARGUMENT("Focus positions must be between 0 and 100000");
    }

    if (std::abs(endPos - startPos) < 100) {
        THROW_INVALID_ARGUMENT("Focus range too small (minimum 100 steps)");
    }

    if (params.contains("step_size")) {
        int stepSize = params["step_size"].get<int>();
        if (stepSize < 1 || stepSize > 5000) {
            THROW_INVALID_ARGUMENT("Step size must be between 1 and 5000");
        }
    }

    if (params.contains("exposure")) {
        double exposure = params["exposure"].get<double>();
        if (exposure <= 0 || exposure > 300) {
            THROW_INVALID_ARGUMENT(
                "Exposure time must be between 0 and 300 seconds");
        }
    }
}

// ==================== TemperatureFocusTask Implementation ====================

auto TemperatureFocusTask::taskName() -> std::string {
    return "TemperatureFocus";
}

void TemperatureFocusTask::execute(const json& params) { executeImpl(params); }

void TemperatureFocusTask::executeImpl(const json& params) {
    spdlog::info("Executing TemperatureFocus task with params: {}",
                 params.dump(4));

    auto startTime = std::chrono::steady_clock::now();

    try {
        validateTemperatureFocusParameters(params);

        double targetTemp = params.at("target_temperature").get<double>();
        double tempTolerance = params.value("temperature_tolerance", 0.5);
        double compensationRate = params.value("compensation_rate", 2.0);

        spdlog::info(
            "Temperature focus compensation: target={:.1f}°C, "
            "tolerance={:.1f}°C, rate={:.1f}",
            targetTemp, tempTolerance, compensationRate);

#ifdef MOCK_CAMERA
        auto currentFocuser = mockFocuser;
#else
        throw std::runtime_error(
            "Real device support not implemented in this example");
#endif

        // Get current temperature
        double currentTemp = currentFocuser->getTemperature();
        double tempDiff = targetTemp - currentTemp;

        spdlog::info(
            "Current temperature: {:.1f}°C, target: {:.1f}°C, difference: "
            "{:.1f}°C",
            currentTemp, targetTemp, tempDiff);

        if (std::abs(tempDiff) > tempTolerance) {
            // Calculate focus compensation
            int compensation = static_cast<int>(tempDiff * compensationRate);
            int currentPos = currentFocuser->getPosition();
            int newPos = currentPos + compensation;

            spdlog::info("Applying temperature compensation: {} steps ({}→{})",
                         compensation, currentPos, newPos);

            currentFocuser->setPosition(newPos);

            // Wait for focuser to reach position
            while (currentFocuser->isMoving()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Update temperature
            currentFocuser->setTemperature(targetTemp);

            spdlog::info("Temperature focus compensation completed");
        } else {
            spdlog::info(
                "Temperature within tolerance, no compensation needed");
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::info("TemperatureFocus task completed in {} ms",
                     duration.count());

    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);
        spdlog::error("TemperatureFocus task failed after {} ms: {}",
                      duration.count(), e.what());
        throw;
    }
}

auto TemperatureFocusTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            TemperatureFocusTask taskInstance;
            taskInstance.execute(params);
        } catch (const std::exception& e) {
            spdlog::error("Enhanced TemperatureFocus task failed: {}",
                          e.what());
            throw;
        }
    });

    defineParameters(*task);
    task->setPriority(5);
    task->setTimeout(std::chrono::seconds(300));  // 5 minute timeout
    task->setLogLevel(2);
    task->setTaskType(taskName());

    return task;
}

void TemperatureFocusTask::defineParameters(Task& task) {
    task.addParamDefinition("target_temperature", "double", true, 20.0,
                            "Target temperature in Celsius");
    task.addParamDefinition("temperature_tolerance", "double", false, 0.5,
                            "Temperature tolerance in degrees");
    task.addParamDefinition("compensation_rate", "double", false, 2.0,
                            "Focus compensation steps per degree Celsius");
}

void TemperatureFocusTask::validateTemperatureFocusParameters(
    const json& params) {
    if (!params.contains("target_temperature")) {
        THROW_INVALID_ARGUMENT("Missing target_temperature parameter");
    }

    double targetTemp = params["target_temperature"].get<double>();
    if (targetTemp < -50 || targetTemp > 50) {
        THROW_INVALID_ARGUMENT(
            "Target temperature must be between -50 and 50 degrees Celsius");
    }

    if (params.contains("temperature_tolerance")) {
        double tolerance = params["temperature_tolerance"].get<double>();
        if (tolerance < 0.1 || tolerance > 10.0) {
            THROW_INVALID_ARGUMENT(
                "Temperature tolerance must be between 0.1 and 10.0 degrees");
        }
    }

    if (params.contains("compensation_rate")) {
        double rate = params["compensation_rate"].get<double>();
        if (rate < 0.1 || rate > 100.0) {
            THROW_INVALID_ARGUMENT(
                "Compensation rate must be between 0.1 and 100.0 steps per "
                "degree");
        }
    }
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register AutoFocusTask
AUTO_REGISTER_TASK(
    AutoFocusTask, "AutoFocus",
    (TaskInfo{
        .name = "AutoFocus",
        .description = "Automatic focusing using HFR measurement",
        .category = "Focusing",
        .requiredParameters = {},
        .parameterSchema = json{{"type", "object"},
                                {"properties",
                                 json{{"exposure", json{{"type", "number"},
                                                        {"minimum", 0},
                                                        {"maximum", 60}}},
                                      {"step_size", json{{"type", "integer"},
                                                         {"minimum", 1},
                                                         {"maximum", 1000}}},
                                      {"max_steps", json{{"type", "integer"},
                                                         {"minimum", 5},
                                                         {"maximum", 200}}},
                                      {"tolerance", json{{"type", "number"},
                                                         {"minimum", 0.01},
                                                         {"maximum", 10.0}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register FocusSeriesTask
AUTO_REGISTER_TASK(
    FocusSeriesTask, "FocusSeries",
    (TaskInfo{.name = "FocusSeries",
              .description = "Take a series of focus exposures for analysis",
              .category = "Focusing",
              .requiredParameters = {"start_position", "end_position"},
              .parameterSchema =
                  json{{"type", "object"},
                       {"properties",
                        json{{"start_position", json{{"type", "integer"},
                                                     {"minimum", 0},
                                                     {"maximum", 100000}}},
                             {"end_position", json{{"type", "integer"},
                                                   {"minimum", 0},
                                                   {"maximum", 100000}}},
                             {"step_size", json{{"type", "integer"},
                                                {"minimum", 1},
                                                {"maximum", 5000}}},
                             {"exposure", json{{"type", "number"},
                                               {"minimum", 0},
                                               {"maximum", 300}}}}}},
              .version = "1.0.0",
              .dependencies = {}}));

// Register TemperatureFocusTask
AUTO_REGISTER_TASK(
    TemperatureFocusTask, "TemperatureFocus",
    (TaskInfo{.name = "TemperatureFocus",
              .description = "Compensate focus position based on temperature",
              .category = "Focusing",
              .requiredParameters = {"target_temperature"},
              .parameterSchema =
                  json{{"type", "object"},
                       {"properties",
                        json{{"target_temperature", json{{"type", "number"},
                                                         {"minimum", -50},
                                                         {"maximum", 50}}},
                             {"temperature_tolerance", json{{"type", "number"},
                                                            {"minimum", 0.1},
                                                            {"maximum", 10.0}}},
                             {"compensation_rate", json{{"type", "number"},
                                                        {"minimum", 0.1},
                                                        {"maximum", 100.0}}}}}},
              .version = "1.0.0",
              .dependencies = {}}));

}  // namespace
