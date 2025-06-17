#include "temperature_tasks.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

#include "../../task.hpp"

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

#define MOCK_CAMERA

namespace lithium::task::task {

// ==================== Mock Camera Temperature System ====================
#ifdef MOCK_CAMERA
class MockTemperatureController {
public:
    static auto getInstance() -> MockTemperatureController& {
        static MockTemperatureController instance;
        return instance;
    }

    auto startCooling(double targetTemp) -> bool {
        if (targetTemp < -50.0 || targetTemp > 50.0) {
            return false;
        }
        coolingEnabled_ = true;
        targetTemperature_ = targetTemp;
        coolingStartTime_ = std::chrono::steady_clock::now();
        spdlog::info("Cooling started, target: {}°C", targetTemp);
        return true;
    }

    auto stopCooling() -> bool {
        coolingEnabled_ = false;
        spdlog::info("Cooling stopped");
        return true;
    }

    auto isCoolerOn() const -> bool {
        return coolingEnabled_;
    }

    auto getTemperature() -> double {
        // Simulate temperature with cooling effects
        auto now = std::chrono::steady_clock::now();
        if (coolingEnabled_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - coolingStartTime_).count();
            // Exponential cooling curve
            double coolingRate = 0.1; // K/s
            double ambientTemp = 25.0; // °C
            currentTemperature_ = targetTemperature_ + 
                (ambientTemp - targetTemperature_) * std::exp(-coolingRate * elapsed);
        } else {
            // Gradual warming to ambient
            currentTemperature_ = std::min(currentTemperature_ + 0.1, 25.0);
        }
        return currentTemperature_;
    }

    auto getCoolingPower() -> double {
        if (!coolingEnabled_) return 0.0;
        
        double tempDiff = std::abs(currentTemperature_ - targetTemperature_);
        // Higher power needed for larger temperature differences
        return std::min(100.0, tempDiff * 10.0); // 0-100%
    }

    auto hasCooler() const -> bool {
        return true; // Mock camera always has cooler
    }

    auto getTargetTemperature() const -> double {
        return targetTemperature_;
    }

    auto isStabilized(double tolerance = 1.0) const -> bool {
        return std::abs(currentTemperature_ - targetTemperature_) <= tolerance;
    }

private:
    bool coolingEnabled_ = false;
    double currentTemperature_ = 25.0; // Start at ambient
    double targetTemperature_ = 25.0;
    std::chrono::steady_clock::time_point coolingStartTime_;
};
#endif

// ==================== CoolingControlTask Implementation ====================

auto CoolingControlTask::taskName() -> std::string {
    return "CoolingControl";
}

void CoolingControlTask::execute(const json& params) {
    try {
        validateCoolingParameters(params);
        
        bool enable = params.value("enable", true);
        double targetTemp = params.value("target_temperature", -10.0);
        
        spdlog::info("Cooling control: {} to {}°C", enable ? "Start" : "Stop", targetTemp);
        
#ifdef MOCK_CAMERA
        auto& controller = MockTemperatureController::getInstance();
        
        if (enable) {
            if (!controller.startCooling(targetTemp)) {
                throw atom::error::RuntimeError("Failed to start cooling system");
            }
            
            // Optional: Wait for initial cooling
            if (params.value("wait_for_stabilization", false)) {
                int maxWaitTime = params.value("max_wait_time", 300); // 5 minutes
                int checkInterval = params.value("check_interval", 10); // 10 seconds
                double tolerance = params.value("tolerance", 1.0);
                
                auto startTime = std::chrono::steady_clock::now();
                while (std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count() < maxWaitTime) {
                    
                    double currentTemp = controller.getTemperature();
                    spdlog::info("Current temperature: {:.2f}°C, Target: {:.2f}°C", 
                                currentTemp, targetTemp);
                    
                    if (controller.isStabilized(tolerance)) {
                        spdlog::info("Temperature stabilized within {:.1f}°C tolerance", tolerance);
                        break;
                    }
                    
                    std::this_thread::sleep_for(std::chrono::seconds(checkInterval));
                }
            }
        } else {
            controller.stopCooling();
        }
#endif
        
        LOG_F(INFO, "Cooling control task completed successfully");
        
    } catch (const std::exception& e) {
        handleCoolingError(*this, e);
        throw;
    }
}

auto CoolingControlTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<CoolingControlTask>("CoolingControl", 
        [](const json& params) {
            CoolingControlTask taskInstance("CoolingControl", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void CoolingControlTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "enable",
        .type = "boolean",
        .required = false,
        .defaultValue = true,
        .description = "Enable or disable cooling"
    });
    
    task.addParameter({
        .name = "target_temperature",
        .type = "number",
        .required = false,
        .defaultValue = -10.0,
        .description = "Target temperature in Celsius"
    });
    
    task.addParameter({
        .name = "wait_for_stabilization",
        .type = "boolean",
        .required = false,
        .defaultValue = false,
        .description = "Wait for temperature to stabilize"
    });
    
    task.addParameter({
        .name = "max_wait_time",
        .type = "integer",
        .required = false,
        .defaultValue = 300,
        .description = "Maximum time to wait for stabilization (seconds)"
    });
    
    task.addParameter({
        .name = "tolerance",
        .type = "number",
        .required = false,
        .defaultValue = 1.0,
        .description = "Temperature tolerance for stabilization (°C)"
    });
}

void CoolingControlTask::validateCoolingParameters(const json& params) {
    if (params.contains("target_temperature")) {
        double temp = params["target_temperature"];
        if (temp < -50.0 || temp > 50.0) {
            throw atom::error::InvalidArgument("Target temperature must be between -50°C and 50°C");
        }
    }
    
    if (params.contains("max_wait_time")) {
        int waitTime = params["max_wait_time"];
        if (waitTime < 0 || waitTime > 3600) {
            throw atom::error::InvalidArgument("Max wait time must be between 0 and 3600 seconds");
        }
    }
}

void CoolingControlTask::handleCoolingError(Task& task, const std::exception& e) {
    task.setErrorType(TaskErrorType::DeviceError);
    spdlog::error("Cooling control error: {}", e.what());
}

// ==================== TemperatureMonitorTask Implementation ====================

auto TemperatureMonitorTask::taskName() -> std::string {
    return "TemperatureMonitor";
}

void TemperatureMonitorTask::execute(const json& params) {
    try {
        validateMonitoringParameters(params);
        
        int duration = params.value("duration", 60);
        int interval = params.value("interval", 5);
        
        spdlog::info("Starting temperature monitoring for {} seconds", duration);
        
#ifdef MOCK_CAMERA
        auto& controller = MockTemperatureController::getInstance();
        
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(duration);
        
        while (std::chrono::steady_clock::now() < endTime) {
            double currentTemp = controller.getTemperature();
            double coolingPower = controller.getCoolingPower();
            bool coolerOn = controller.isCoolerOn();
            
            json statusReport = {
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count()},
                {"temperature", currentTemp},
                {"cooling_power", coolingPower},
                {"cooler_enabled", coolerOn},
                {"target_temperature", controller.getTargetTemperature()}
            };
            
            spdlog::info("Temperature status: {}", statusReport.dump());
            
            std::this_thread::sleep_for(std::chrono::seconds(interval));
        }
#endif
        
        LOG_F(INFO, "Temperature monitoring completed");
        
    } catch (const std::exception& e) {
        spdlog::error("TemperatureMonitorTask failed: {}", e.what());
        throw;
    }
}

auto TemperatureMonitorTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<TemperatureMonitorTask>("TemperatureMonitor", 
        [](const json& params) {
            TemperatureMonitorTask taskInstance("TemperatureMonitor", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void TemperatureMonitorTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "duration",
        .type = "integer",
        .required = false,
        .defaultValue = 60,
        .description = "Monitoring duration in seconds"
    });
    
    task.addParameter({
        .name = "interval",
        .type = "integer",
        .required = false,
        .defaultValue = 5,
        .description = "Monitoring interval in seconds"
    });
}

void TemperatureMonitorTask::validateMonitoringParameters(const json& params) {
    if (params.contains("duration")) {
        int duration = params["duration"];
        if (duration <= 0 || duration > 86400) {
            throw atom::error::InvalidArgument("Duration must be between 1 and 86400 seconds");
        }
    }
    
    if (params.contains("interval")) {
        int interval = params["interval"];
        if (interval < 1 || interval > 3600) {
            throw atom::error::InvalidArgument("Interval must be between 1 and 3600 seconds");
        }
    }
}

// ==================== TemperatureStabilizationTask Implementation ====================

auto TemperatureStabilizationTask::taskName() -> std::string {
    return "TemperatureStabilization";
}

void TemperatureStabilizationTask::execute(const json& params) {
    try {
        validateStabilizationParameters(params);
        
        double targetTemp = params.value("target_temperature", -10.0);
        double tolerance = params.value("tolerance", 1.0);
        int maxWaitTime = params.value("max_wait_time", 600);
        int checkInterval = params.value("check_interval", 10);
        
        spdlog::info("Waiting for temperature stabilization: {:.1f}°C ±{:.1f}°C", 
                    targetTemp, tolerance);
        
#ifdef MOCK_CAMERA
        auto& controller = MockTemperatureController::getInstance();
        
        // Start cooling if not already running
        if (!controller.isCoolerOn()) {
            controller.startCooling(targetTemp);
        }
        
        auto startTime = std::chrono::steady_clock::now();
        bool stabilized = false;
        
        while (std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count() < maxWaitTime) {
            
            double currentTemp = controller.getTemperature();
            spdlog::info("Current: {:.2f}°C, Target: {:.2f}°C", currentTemp, targetTemp);
            
            if (std::abs(currentTemp - targetTemp) <= tolerance) {
                stabilized = true;
                spdlog::info("Temperature stabilized!");
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(checkInterval));
        }
        
        if (!stabilized) {
            throw atom::error::RuntimeError("Temperature failed to stabilize within timeout period");
        }
#endif
        
        LOG_F(INFO, "Temperature stabilization completed");
        
    } catch (const std::exception& e) {
        spdlog::error("TemperatureStabilizationTask failed: {}", e.what());
        throw;
    }
}

auto TemperatureStabilizationTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<TemperatureStabilizationTask>("TemperatureStabilization", 
        [](const json& params) {
            TemperatureStabilizationTask taskInstance("TemperatureStabilization", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void TemperatureStabilizationTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "target_temperature",
        .type = "number",
        .required = true,
        .defaultValue = -10.0,
        .description = "Target temperature for stabilization"
    });
    
    task.addParameter({
        .name = "tolerance",
        .type = "number",
        .required = false,
        .defaultValue = 1.0,
        .description = "Temperature tolerance (±°C)"
    });
    
    task.addParameter({
        .name = "max_wait_time",
        .type = "integer",
        .required = false,
        .defaultValue = 600,
        .description = "Maximum wait time in seconds"
    });
    
    task.addParameter({
        .name = "check_interval",
        .type = "integer",
        .required = false,
        .defaultValue = 10,
        .description = "Check interval in seconds"
    });
}

void TemperatureStabilizationTask::validateStabilizationParameters(const json& params) {
    if (params.contains("target_temperature")) {
        double temp = params["target_temperature"];
        if (temp < -50.0 || temp > 50.0) {
            throw atom::error::InvalidArgument("Target temperature must be between -50°C and 50°C");
        }
    }
    
    if (params.contains("tolerance")) {
        double tolerance = params["tolerance"];
        if (tolerance <= 0 || tolerance > 20.0) {
            throw atom::error::InvalidArgument("Tolerance must be between 0 and 20°C");
        }
    }
}

// ==================== CoolingOptimizationTask Implementation ====================

auto CoolingOptimizationTask::taskName() -> std::string {
    return "CoolingOptimization";
}

void CoolingOptimizationTask::execute(const json& params) {
    try {
        validateOptimizationParameters(params);
        
        double targetTemp = params.value("target_temperature", -10.0);
        int optimizationTime = params.value("optimization_time", 300);
        
        spdlog::info("Starting cooling optimization for {}°C over {} seconds", 
                    targetTemp, optimizationTime);
        
#ifdef MOCK_CAMERA
        auto& controller = MockTemperatureController::getInstance();
        
        if (!controller.isCoolerOn()) {
            controller.startCooling(targetTemp);
        }
        
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(optimizationTime);
        
        double bestEfficiency = 0.0;
        double optimalPower = 50.0;
        
        while (std::chrono::steady_clock::now() < endTime) {
            double currentTemp = controller.getTemperature();
            double currentPower = controller.getCoolingPower();
            
            // Calculate efficiency (cooling per unit power)
            double tempDiff = std::abs(25.0 - currentTemp); // Cooling from ambient
            double efficiency = tempDiff / (currentPower + 1.0); // Avoid division by zero
            
            if (efficiency > bestEfficiency) {
                bestEfficiency = efficiency;
                optimalPower = currentPower;
            }
            
            spdlog::info("Temp: {:.2f}°C, Power: {:.1f}%, Efficiency: {:.3f}", 
                        currentTemp, currentPower, efficiency);
            
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
        
        spdlog::info("Optimization complete. Optimal power: {:.1f}%, Best efficiency: {:.3f}", 
                    optimalPower, bestEfficiency);
#endif
        
        LOG_F(INFO, "Cooling optimization completed");
        
    } catch (const std::exception& e) {
        spdlog::error("CoolingOptimizationTask failed: {}", e.what());
        throw;
    }
}

auto CoolingOptimizationTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<CoolingOptimizationTask>("CoolingOptimization", 
        [](const json& params) {
            CoolingOptimizationTask taskInstance("CoolingOptimization", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void CoolingOptimizationTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "target_temperature",
        .type = "number",
        .required = false,
        .defaultValue = -10.0,
        .description = "Target temperature for optimization"
    });
    
    task.addParameter({
        .name = "optimization_time",
        .type = "integer",
        .required = false,
        .defaultValue = 300,
        .description = "Time to spend optimizing in seconds"
    });
}

void CoolingOptimizationTask::validateOptimizationParameters(const json& params) {
    if (params.contains("target_temperature")) {
        double temp = params["target_temperature"];
        if (temp < -50.0 || temp > 50.0) {
            throw atom::error::InvalidArgument("Target temperature must be between -50°C and 50°C");
        }
    }
    
    if (params.contains("optimization_time")) {
        int time = params["optimization_time"];
        if (time < 60 || time > 3600) {
            throw atom::error::InvalidArgument("Optimization time must be between 60 and 3600 seconds");
        }
    }
}

// ==================== TemperatureAlertTask Implementation ====================

auto TemperatureAlertTask::taskName() -> std::string {
    return "TemperatureAlert";
}

void TemperatureAlertTask::execute(const json& params) {
    try {
        validateAlertParameters(params);
        
        double maxTemp = params.value("max_temperature", 40.0);
        double minTemp = params.value("min_temperature", -30.0);
        int monitorTime = params.value("monitor_time", 300);
        int checkInterval = params.value("check_interval", 30);
        
        spdlog::info("Temperature alert monitoring: {:.1f}°C to {:.1f}°C for {} seconds", 
                    minTemp, maxTemp, monitorTime);
        
#ifdef MOCK_CAMERA
        auto& controller = MockTemperatureController::getInstance();
        
        auto startTime = std::chrono::steady_clock::now();
        auto endTime = startTime + std::chrono::seconds(monitorTime);
        
        while (std::chrono::steady_clock::now() < endTime) {
            double currentTemp = controller.getTemperature();
            
            if (currentTemp > maxTemp) {
                spdlog::error("TEMPERATURE ALERT: {:.2f}°C exceeds maximum {:.1f}°C!", 
                             currentTemp, maxTemp);
                // Could trigger emergency cooling or shutdown
            } else if (currentTemp < minTemp) {
                spdlog::error("TEMPERATURE ALERT: {:.2f}°C below minimum {:.1f}°C!", 
                             currentTemp, minTemp);
                // Could trigger reduced cooling
            } else {
                spdlog::info("Temperature OK: {:.2f}°C", currentTemp);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(checkInterval));
        }
#endif
        
        LOG_F(INFO, "Temperature alert monitoring completed");
        
    } catch (const std::exception& e) {
        spdlog::error("TemperatureAlertTask failed: {}", e.what());
        throw;
    }
}

auto TemperatureAlertTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<TemperatureAlertTask>("TemperatureAlert", 
        [](const json& params) {
            TemperatureAlertTask taskInstance("TemperatureAlert", nullptr);
            taskInstance.execute(params);
        });
    
    defineParameters(*task);
    return task;
}

void TemperatureAlertTask::defineParameters(Task& task) {
    task.addParameter({
        .name = "max_temperature",
        .type = "number",
        .required = false,
        .defaultValue = 40.0,
        .description = "Maximum allowed temperature"
    });
    
    task.addParameter({
        .name = "min_temperature",
        .type = "number",
        .required = false,
        .defaultValue = -30.0,
        .description = "Minimum allowed temperature"
    });
    
    task.addParameter({
        .name = "monitor_time",
        .type = "integer",
        .required = false,
        .defaultValue = 300,
        .description = "Monitoring duration in seconds"
    });
    
    task.addParameter({
        .name = "check_interval",
        .type = "integer",
        .required = false,
        .defaultValue = 30,
        .description = "Check interval in seconds"
    });
}

void TemperatureAlertTask::validateAlertParameters(const json& params) {
    if (params.contains("max_temperature") && params.contains("min_temperature")) {
        double maxTemp = params["max_temperature"];
        double minTemp = params["min_temperature"];
        if (minTemp >= maxTemp) {
            throw atom::error::InvalidArgument("Minimum temperature must be less than maximum temperature");
        }
    }
}

}  // namespace lithium::task::task

// ==================== Task Registration Section ====================

namespace {
using namespace lithium::task;
using namespace lithium::task::task;

// Register CoolingControlTask
AUTO_REGISTER_TASK(
    CoolingControlTask, "CoolingControl",
    (TaskInfo{
        .name = "CoolingControl",
        .description = "Controls camera cooling system",
        .category = "Temperature",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"enable", json{{"type", "boolean"}}},
                       {"target_temperature", json{{"type", "number"},
                                                   {"minimum", -50.0},
                                                   {"maximum", 50.0}}},
                       {"wait_for_stabilization", json{{"type", "boolean"}}},
                       {"max_wait_time", json{{"type", "integer"},
                                              {"minimum", 0},
                                              {"maximum", 3600}}},
                       {"tolerance", json{{"type", "number"},
                                          {"minimum", 0.1},
                                          {"maximum", 10.0}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register TemperatureMonitorTask
AUTO_REGISTER_TASK(
    TemperatureMonitorTask, "TemperatureMonitor",
    (TaskInfo{
        .name = "TemperatureMonitor",
        .description = "Monitors camera temperature continuously",
        .category = "Temperature",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"duration", json{{"type", "integer"},
                                         {"minimum", 1},
                                         {"maximum", 86400}}},
                       {"interval", json{{"type", "integer"},
                                         {"minimum", 1},
                                         {"maximum", 3600}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

// Register TemperatureStabilizationTask
AUTO_REGISTER_TASK(
    TemperatureStabilizationTask, "TemperatureStabilization",
    (TaskInfo{
        .name = "TemperatureStabilization",
        .description = "Waits for camera temperature to stabilize",
        .category = "Temperature",
        .requiredParameters = {"target_temperature"},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"target_temperature", json{{"type", "number"},
                                                   {"minimum", -50.0},
                                                   {"maximum", 50.0}}},
                       {"tolerance", json{{"type", "number"},
                                          {"minimum", 0.1},
                                          {"maximum", 20.0}}},
                       {"max_wait_time", json{{"type", "integer"},
                                              {"minimum", 60},
                                              {"maximum", 3600}}},
                       {"check_interval", json{{"type", "integer"},
                                               {"minimum", 5},
                                               {"maximum", 300}}}}},
                 {"required", json::array({"target_temperature"})}},
        .version = "1.0.0",
        .dependencies = {"CoolingControl"}}));

// Register CoolingOptimizationTask
AUTO_REGISTER_TASK(
    CoolingOptimizationTask, "CoolingOptimization",
    (TaskInfo{
        .name = "CoolingOptimization",
        .description = "Optimizes cooling system performance",
        .category = "Temperature",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"target_temperature", json{{"type", "number"},
                                                   {"minimum", -50.0},
                                                   {"maximum", 50.0}}},
                       {"optimization_time", json{{"type", "integer"},
                                                  {"minimum", 60},
                                                  {"maximum", 3600}}}}}},
        .version = "1.0.0",
        .dependencies = {"CoolingControl"}}));

// Register TemperatureAlertTask
AUTO_REGISTER_TASK(
    TemperatureAlertTask, "TemperatureAlert",
    (TaskInfo{
        .name = "TemperatureAlert",
        .description = "Monitors temperature and triggers alerts",
        .category = "Temperature",
        .requiredParameters = {},
        .parameterSchema =
            json{{"type", "object"},
                 {"properties",
                  json{{"max_temperature", json{{"type", "number"},
                                                {"minimum", -40.0},
                                                {"maximum", 80.0}}},
                       {"min_temperature", json{{"type", "number"},
                                                {"minimum", -60.0},
                                                {"maximum", 40.0}}},
                       {"monitor_time", json{{"type", "integer"},
                                             {"minimum", 60},
                                             {"maximum", 86400}}},
                       {"check_interval", json{{"type", "integer"},
                                               {"minimum", 5},
                                               {"maximum", 3600}}}}}},
        .version = "1.0.0",
        .dependencies = {}}));

}  // namespace
