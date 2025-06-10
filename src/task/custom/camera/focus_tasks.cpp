#include "focus_tasks.hpp"
#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>
#include <vector>

// 修正：确保 Focuser 的完整类型定义
#ifndef MOCK_CAMERA
#include "../../../../modules/device/focuser/eaf.hpp"
// Camera 头文件如有需补充
#endif

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#ifndef MOCK_CAMERA
#include "device/indi/focuser.hpp"
#include "device/indi/camera.hpp"
#endif

#ifndef MOCK_CAMERA
// 全局focuser/camera指针定义（需在某个cpp文件定义一次，这里仅声明）
extern std::shared_ptr<Focuser> focuser;
extern std::shared_ptr<Camera> camera;
#endif

namespace lithium::sequencer::task {

#ifndef MOCK_CAMERA
#include "device/indi/focuser.hpp"
#include "device/indi/camera.hpp"
extern std::shared_ptr<Focuser> focuser;
extern std::shared_ptr<Camera> camera;
#endif

#ifdef MOCK_CAMERA
class MockFocuser {
public:
    MockFocuser() = default;
    
    void setPosition(int pos) { 
        position_ = std::clamp(pos, 0, 50000);
        LOG_F(INFO, "Focuser moved to position {}", position_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    int getPosition() const { return position_; }
    bool isMoving() const { return false; }
    void setTemperatureCompensation(bool enabled) { tempComp_ = enabled; }
    double getTemperature() const { return temperature_; }
    void setTemperature(double temp) { temperature_ = temp; }
    
private:
    int position_{25000};
    bool tempComp_{false};
    double temperature_{20.0};
};

class MockCamera {
public:
    MockCamera() = default;
    
    bool getExposureStatus() const { return exposureStatus_; }
    void setGain(int g) { gain_ = g; }
    int getGain() const { return gain_; }
    void setOffset(int o) { offset_ = o; }
    int getOffset() const { return offset_; }
    void setBinning(int bx, int by) { binningX_ = bx; binningY_ = by; }
    std::tuple<int, int> getBinning() const { return {binningX_, binningY_}; }
    void startExposure(double t) {
        exposureStatus_ = true;
        exposureTime_ = t;
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(t * 100)));
        exposureStatus_ = false;
    }
    void saveExposureResult() { exposureStatus_ = false; }
    double calculateHFR() { return 2.5 + (rand() % 100) / 100.0; } // Mock HFR calculation
    
private:
    bool exposureStatus_{false};
    double exposureTime_{1.0};
    int gain_{100};
    int offset_{10};
    int binningX_{1};
    int binningY_{1};
};
#endif

// ==================== AutoFocusTask ====================

auto AutoFocusTask::taskName() -> std::string {
    return "AutoFocus";
}

void AutoFocusTask::execute(const json& params) {
    LOG_F(INFO, "Executing AutoFocus task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        double exposure = params.value("exposure", 1.0);
        int stepSize = params.value("step_size", 100);
        int maxSteps = params.value("max_steps", 50);
        double tolerance = params.value("tolerance", 0.1);
        
        LOG_F(INFO, "Starting autofocus with {} second exposures, step size {}, max {} steps", 
              exposure, stepSize, maxSteps);
        
#ifdef MOCK_CAMERA
        auto focuser = std::make_shared<MockFocuser>();
        auto camera = std::make_shared<MockCamera>();
#endif
        
#ifdef MOCK_CAMERA
        auto focuser = std::make_shared<MockFocuser>();
#else
                // 使用全局 focuser 指针，确保类型完整且已初始化
                if (!focuser) {
                    throw std::runtime_error("Focuser pointer is null");
                }
                int startPosition = focuser->getPosition();
        #endif
        int bestPosition = startPosition;
        double bestHFR = 999.0;
        
        // Coarse focus sweep
        std::vector<std::pair<int, double>> measurements;
        
        for (int step = -maxSteps/2; step <= maxSteps/2; step += 5) {
            int position = startPosition + (step * stepSize);
            focuser->setPosition(position);
            
            // Take test exposure
            camera->startExposure(exposure);
            while (camera->getExposureStatus()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // Calculate HFR
            double hfr = camera->calculateHFR();
            measurements.emplace_back(position, hfr);
            
            LOG_F(INFO, "Position {}: HFR = {:.2f}", position, hfr);
            
            if (hfr < bestHFR) {
                bestHFR = hfr;
                bestPosition = position;
            }
        }
        
        // Fine focus around best position
        LOG_F(INFO, "Fine focusing around position {} (HFR: {:.2f})", bestPosition, bestHFR);
        
        for (int step = -2; step <= 2; ++step) {
            int position = bestPosition + (step * stepSize / 5);
            focuser->setPosition(position);
            
            camera->startExposure(exposure);
            while (camera->getExposureStatus()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            double hfr = camera->calculateHFR();
            LOG_F(INFO, "Fine position {}: HFR = {:.2f}", position, hfr);
            
            if (hfr < bestHFR) {
                bestHFR = hfr;
                bestPosition = position;
            }
        }
        
        // Move to best position
        focuser->setPosition(bestPosition);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "AutoFocus completed: Best position {} with HFR {:.2f} in {} ms", 
              bestPosition, bestHFR, duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "AutoFocus task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto AutoFocusTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced AutoFocus task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(8); // High priority for focus tasks
    task->setTimeout(std::chrono::seconds(600)); // 10 minute timeout
    task->setLogLevel(2);
    
    return task;
}

void AutoFocusTask::defineParameters(Task& task) {
    task.addParamDefinition("exposure", "double", false, 1.0, "Focus test exposure time");
    task.addParamDefinition("step_size", "int", false, 100, "Focuser step size");
    task.addParamDefinition("max_steps", "int", false, 50, "Maximum focus steps");
    task.addParamDefinition("tolerance", "double", false, 0.1, "Focus tolerance");
}

void AutoFocusTask::validateAutoFocusParameters(const json& params) {
    if (params.contains("exposure")) {
        double exposure = params["exposure"].get<double>();
        if (exposure <= 0 || exposure > 60) {
            THROW_INVALID_ARGUMENT("Focus exposure must be between 0 and 60 seconds");
        }
    }
    
    if (params.contains("step_size")) {
        int stepSize = params["step_size"].get<int>();
        if (stepSize < 1 || stepSize > 1000) {
            THROW_INVALID_ARGUMENT("Step size must be between 1 and 1000");
        }
    }
}

// ==================== FocusSeriesTask ====================

auto FocusSeriesTask::taskName() -> std::string {
    return "FocusSeries";
}

void FocusSeriesTask::execute(const json& params) {
    LOG_F(INFO, "Executing FocusSeries task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        int startPos = params.at("start_position").get<int>();
        int endPos = params.at("end_position").get<int>();
        int stepSize = params.value("step_size", 100);
        double exposure = params.value("exposure", 2.0);
        
        LOG_F(INFO, "Taking focus series from {} to {} with step {}", 
              startPos, endPos, stepSize);
        
#ifdef MOCK_CAMERA
        auto focuser = std::make_shared<MockFocuser>();
        auto camera = std::make_shared<MockCamera>();
#else
        extern std::shared_ptr<Focuser> focuser;
        extern std::shared_ptr<Camera> camera;
#endif

        int direction = (endPos > startPos) ? 1 : -1;
        int currentPos = startPos;
        int frameCount = 0;
        
        while ((direction > 0 && currentPos <= endPos) || 
               (direction < 0 && currentPos >= endPos)) {
            
            LOG_F(INFO, "Taking focus frame {} at position {}", frameCount + 1, currentPos);
            
            focuser->setPosition(currentPos);
            
            // Take exposure
            camera->startExposure(exposure);
            while (camera->getExposureStatus()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            double hfr = camera->calculateHFR();
            LOG_F(INFO, "Position {}: HFR = {:.2f}", currentPos, hfr);
            
            currentPos += (stepSize * direction);
            frameCount++;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "FocusSeries completed {} frames in {} ms", frameCount, duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "FocusSeries task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto FocusSeriesTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced FocusSeries task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(6);
    task->setTimeout(std::chrono::seconds(1800)); // 30 minute timeout
    task->setLogLevel(2);
    
    return task;
}

void FocusSeriesTask::defineParameters(Task& task) {
    task.addParamDefinition("start_position", "int", true, 20000, "Starting focuser position");
    task.addParamDefinition("end_position", "int", true, 30000, "Ending focuser position");
    task.addParamDefinition("step_size", "int", false, 100, "Step size between positions");
    task.addParamDefinition("exposure", "double", false, 2.0, "Exposure time per frame");
}

void FocusSeriesTask::validateFocusSeriesParameters(const json& params) {
    if (!params.contains("start_position") || !params.contains("end_position")) {
        THROW_INVALID_ARGUMENT("Missing start_position or end_position parameters");
    }
    
    int startPos = params["start_position"].get<int>();
    int endPos = params["end_position"].get<int>();
    
    if (startPos < 0 || startPos > 100000 || endPos < 0 || endPos > 100000) {
        THROW_INVALID_ARGUMENT("Focus positions must be between 0 and 100000");
    }
    
    if (std::abs(endPos - startPos) < 100) {
        THROW_INVALID_ARGUMENT("Focus range too small (minimum 100 steps)");
    }
}

// ==================== TemperatureFocusTask ====================

auto TemperatureFocusTask::taskName() -> std::string {
    return "TemperatureFocus";
}

void TemperatureFocusTask::execute(const json& params) {
    LOG_F(INFO, "Executing TemperatureFocus task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        double targetTemp = params.at("target_temperature").get<double>();
        double tempTolerance = params.value("temperature_tolerance", 0.5);
        double compensationRate = params.value("compensation_rate", 2.0); // steps per degree
        
        LOG_F(INFO, "Setting up temperature focus compensation for {}°C with rate {} steps/°C", 
              targetTemp, compensationRate);
        
#ifdef MOCK_CAMERA
        auto focuser = std::make_shared<MockFocuser>();
#else
        extern std::shared_ptr<Focuser> focuser;
#endif

        focuser->setTemperatureCompensation(true);

        // Monitor temperature and adjust focus
        double currentTemp = focuser->getTemperature();
        LOG_F(INFO, "Current temperature: {:.1f}°C, target: {:.1f}°C", currentTemp, targetTemp);

        double tempDifference = targetTemp - currentTemp;
        if (std::abs(tempDifference) > tempTolerance) {
            int focusAdjustment = static_cast<int>(tempDifference * compensationRate);
            int currentPos = focuser->getPosition();
            int newPos = currentPos + focusAdjustment;

            LOG_F(INFO, "Temperature difference {:.1f}°C requires {} step adjustment",
                  tempDifference, focusAdjustment);
            focuser->setPosition(newPos);
        }

        // Simulate temperature stabilization
        focuser->setTemperature(targetTemp);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "TemperatureFocus task completed in {} ms", duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "TemperatureFocus task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto TemperatureFocusTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced TemperatureFocus task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(5);
    task->setTimeout(std::chrono::seconds(300)); // 5 minute timeout
    task->setLogLevel(2);
    
    return task;
}

void TemperatureFocusTask::defineParameters(Task& task) {
    task.addParamDefinition("target_temperature", "double", true, 20.0, "Target temperature in Celsius");
    task.addParamDefinition("temperature_tolerance", "double", false, 0.5, "Temperature tolerance");
    task.addParamDefinition("compensation_rate", "double", false, 2.0, "Focus steps per degree");
}

void TemperatureFocusTask::validateTemperatureFocusParameters(const json& params) {
    if (!params.contains("target_temperature")) {
        THROW_INVALID_ARGUMENT("Missing target_temperature parameter");
    }
    
    double targetTemp = params["target_temperature"].get<double>();
    if (targetTemp < -50 || targetTemp > 50) {
        THROW_INVALID_ARGUMENT("Target temperature must be between -50 and 50°C");
    }
    
    if (params.contains("compensation_rate")) {
        double rate = params["compensation_rate"].get<double>();
        if (rate < 0.1 || rate > 20.0) {
            THROW_INVALID_ARGUMENT("Compensation rate must be between 0.1 and 20.0 steps/°C");
        }
    }
}

}  // namespace lithium::sequencer::task
