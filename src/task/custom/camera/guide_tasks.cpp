#include "guide_tasks.hpp"
#include <memory>
#include <thread>
#include <chrono>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::sequencer::task {

#ifdef MOCK_CAMERA
class MockGuider {
public:
    MockGuider() = default;
    
    bool isGuiding() const { return guiding_; }
    void startGuiding() { guiding_ = true; }
    void stopGuiding() { guiding_ = false; }
    void dither(double pixels) { 
        LOG_F(INFO, "Dithering by {} pixels", pixels);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    bool calibrate() { 
        LOG_F(INFO, "Calibrating guider");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return true; 
    }
    
private:
    bool guiding_{false};
};
#endif

// ==================== GuidedExposureTask ====================

auto GuidedExposureTask::taskName() -> std::string {
    return "GuidedExposure";
}

void GuidedExposureTask::execute(const json& params) {
    LOG_F(INFO, "Executing GuidedExposure task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        double exposureTime = params.at("exposure").get<double>();
        ExposureType type = params.at("type").get<ExposureType>();
        int gain = params.value("gain", 100);
        int offset = params.value("offset", 10);
        bool useGuiding = params.value("guiding", true);
        
        LOG_F(INFO, "Starting guided exposure for {} seconds with guiding {}", 
              exposureTime, useGuiding ? "enabled" : "disabled");
        
#ifdef MOCK_CAMERA
        auto guider = std::make_shared<MockGuider>();
#endif
        
        if (useGuiding) {
#ifdef MOCK_CAMERA
            if (!guider->isGuiding()) {
                LOG_F(INFO, "Starting guiding");
                guider->startGuiding();
                // Wait for guiding to stabilize
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
#endif
        }
        
        // Execute the exposure using TakeExposureTask
        json exposureParams = {
            {"exposure", exposureTime},
            {"type", type},
            {"gain", gain},
            {"offset", offset}
        };
        
        // 调用实际的曝光任务执行
        GuidedExposureTask::execute(exposureParams);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "GuidedExposure task completed in {} ms", duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "GuidedExposure task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto GuidedExposureTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced GuidedExposure task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(8); // High priority for guided exposure
    task->setTimeout(std::chrono::seconds(600)); // 10 minute timeout
    task->setLogLevel(2);
    
    return task;
}

void GuidedExposureTask::defineParameters(Task& task) {
    task.addParamDefinition("exposure", "double", true, 1.0, "Exposure time in seconds");
    task.addParamDefinition("type", "string", true, "light", "Exposure type");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain value");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset value");
    task.addParamDefinition("guiding", "bool", false, true, "Enable autoguiding");
}

void GuidedExposureTask::validateGuidingParameters(const json& params) {
    if (!params.contains("exposure") || !params["exposure"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid exposure parameter");
    }
    
    double exposure = params["exposure"].get<double>();
    if (exposure <= 0 || exposure > 3600) {
        THROW_INVALID_ARGUMENT("Exposure time must be between 0 and 3600 seconds");
    }
}

// ==================== DitherSequenceTask ====================

auto DitherSequenceTask::taskName() -> std::string {
    return "DitherSequence";
}

void DitherSequenceTask::execute(const json& params) {
    LOG_F(INFO, "Executing DitherSequence task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        int count = params.at("count").get<int>();
        double exposure = params.at("exposure").get<double>();
        double ditherPixels = params.value("dither_pixels", 5.0);
        int settleTime = params.value("settle_time", 5);
        
        LOG_F(INFO, "Starting dither sequence with {} exposures, {} pixel dither, {} second settle", 
              count, ditherPixels, settleTime);
        
#ifdef MOCK_CAMERA
        auto guider = std::make_shared<MockGuider>();
#endif
        
        // Start guiding if not already active
#ifdef MOCK_CAMERA
        if (!guider->isGuiding()) {
            guider->startGuiding();
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
#endif
        
        for (int i = 0; i < count; ++i) {
            LOG_F(INFO, "Taking dithered exposure {} of {}", i + 1, count);
            
            // Dither before each exposure (except first)
            if (i > 0) {
#ifdef MOCK_CAMERA
                LOG_F(INFO, "Dithering by {} pixels", ditherPixels);
                guider->dither(ditherPixels);
#endif
                // Wait for settling
                LOG_F(INFO, "Waiting {} seconds for guiding to settle", settleTime);
                std::this_thread::sleep_for(std::chrono::seconds(settleTime));
            }
            
            // Take the exposure
            json exposureParams = {
                {"exposure", exposure},
                {"type", ExposureType::LIGHT},
                {"gain", params.value("gain", 100)},
                {"offset", params.value("offset", 10)}
            };
            
            // 调用实际的曝光任务执行
            GuidedExposureTask::execute(exposureParams);
            LOG_F(INFO, "Dithered exposure {} of {} completed", i + 1, count);
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "DitherSequence task completed {} exposures in {} ms", count, duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "DitherSequence task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto DitherSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced DitherSequence task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(7);
    task->setTimeout(std::chrono::seconds(3600)); // 1 hour timeout
    task->setLogLevel(2);
    
    return task;
}

void DitherSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("count", "int", true, 1, "Number of dithered exposures");
    task.addParamDefinition("exposure", "double", true, 1.0, "Exposure time per frame");
    task.addParamDefinition("dither_pixels", "double", false, 5.0, "Dither distance in pixels");
    task.addParamDefinition("settle_time", "int", false, 5, "Settling time after dither");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void DitherSequenceTask::validateDitheringParameters(const json& params) {
    if (!params.contains("count") || !params["count"].is_number_integer()) {
        THROW_INVALID_ARGUMENT("Missing or invalid count parameter");
    }
    
    int count = params["count"].get<int>();
    if (count <= 0 || count > 1000) {
        THROW_INVALID_ARGUMENT("Count must be between 1 and 1000");
    }
    
    if (params.contains("dither_pixels")) {
        double pixels = params["dither_pixels"].get<double>();
        if (pixels < 0 || pixels > 50) {
            THROW_INVALID_ARGUMENT("Dither pixels must be between 0 and 50");
        }
    }
}

// ==================== AutoGuidingTask ====================

auto AutoGuidingTask::taskName() -> std::string {
    return "AutoGuiding";
}

void AutoGuidingTask::execute(const json& params) {
    LOG_F(INFO, "Executing AutoGuiding task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        bool calibrate = params.value("calibrate", true);
        double tolerance = params.value("tolerance", 1.0);
        int maxAttempts = params.value("max_attempts", 3);
        
        LOG_F(INFO, "Setting up autoguiding with calibration {}, tolerance {} pixels", 
              calibrate ? "enabled" : "disabled", tolerance);
        
#ifdef MOCK_CAMERA
        auto guider = std::make_shared<MockGuider>();
#endif
        
        if (calibrate) {
            LOG_F(INFO, "Starting guider calibration");
            
            for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
                LOG_F(INFO, "Calibration attempt {} of {}", attempt, maxAttempts);
                
#ifdef MOCK_CAMERA
                if (guider->calibrate()) {
                    LOG_F(INFO, "Guider calibration successful");
                    break;
                }
#endif
                
                if (attempt == maxAttempts) {
                    THROW_RUNTIME_ERROR("Guider calibration failed after {} attempts", maxAttempts);
                }
                
                LOG_F(WARNING, "Calibration attempt {} failed, retrying...", attempt);
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }
        
        // Start guiding
        LOG_F(INFO, "Starting autoguiding");
#ifdef MOCK_CAMERA
        guider->startGuiding();
#endif
        
        // Wait for guiding to stabilize
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "AutoGuiding task completed in {} ms", duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "AutoGuiding task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto AutoGuidingTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced AutoGuiding task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(6); // Medium-high priority
    task->setTimeout(std::chrono::seconds(300)); // 5 minute timeout
    task->setLogLevel(2);
    
    return task;
}

void AutoGuidingTask::defineParameters(Task& task) {
    task.addParamDefinition("calibrate", "bool", false, true, "Perform calibration before guiding");
    task.addParamDefinition("tolerance", "double", false, 1.0, "Guiding tolerance in pixels");
    task.addParamDefinition("max_attempts", "int", false, 3, "Maximum calibration attempts");
}

void AutoGuidingTask::validateAutoGuidingParameters(const json& params) {
    if (params.contains("tolerance")) {
        double tolerance = params["tolerance"].get<double>();
        if (tolerance < 0.1 || tolerance > 10.0) {
            THROW_INVALID_ARGUMENT("Tolerance must be between 0.1 and 10.0 pixels");
        }
    }
    
    if (params.contains("max_attempts")) {
        int attempts = params["max_attempts"].get<int>();
        if (attempts < 1 || attempts > 10) {
            THROW_INVALID_ARGUMENT("Max attempts must be between 1 and 10");
        }
    }
}

}  // namespace lithium::sequencer::task
