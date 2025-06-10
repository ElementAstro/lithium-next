#include "filter_tasks.hpp"
#include "basic_exposure.hpp"
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace lithium::sequencer::task {

#ifdef MOCK_CAMERA
class MockFilterWheel {
public:
    MockFilterWheel() = default;
    
    void setFilter(const std::string& filterName) {
        currentFilter_ = filterName;
        LOG_F(INFO, "Filter wheel set to: {}", filterName);
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Simulate movement
    }
    
    std::string getCurrentFilter() const { return currentFilter_; }
    bool isMoving() const { return false; }
    
    std::vector<std::string> getAvailableFilters() const {
        return {"Red", "Green", "Blue", "Luminance", "Ha", "OIII", "SII", "Clear"};
    }
    
private:
    std::string currentFilter_{"Luminance"};
};
#endif

// ==================== FilterSequenceTask ====================

auto FilterSequenceTask::taskName() -> std::string {
    return "FilterSequence";
}

#include "basic_exposure.hpp"

void FilterSequenceTask::execute(const json& params) {
    LOG_F(INFO, "Executing FilterSequence task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        auto filters = params.at("filters").get<std::vector<std::string>>();
        double exposure = params.at("exposure").get<double>();
        int count = params.value("count", 1);
        
        LOG_F(INFO, "Starting filter sequence with {} filters, {} second exposures, {} frames per filter", 
              filters.size(), exposure, count);
        
#ifdef MOCK_CAMERA
        auto filterWheel = std::make_shared<MockFilterWheel>();
#endif
        
        int totalFrames = 0;
        
        for (const auto& filter : filters) {
            LOG_F(INFO, "Switching to filter: {}", filter);
#ifdef MOCK_CAMERA
            filterWheel->setFilter(filter);
#endif
            
            // Wait for filter wheel to settle
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            for (int i = 0; i < count; ++i) {
                LOG_F(INFO, "Taking frame {} of {} with filter {}", i + 1, count, filter);
                
                // Take exposure with current filter
                json exposureParams = {
                    {"exposure", exposure},
                    {"type", ExposureType::LIGHT},
                    {"gain", params.value("gain", 100)},
                    {"offset", params.value("offset", 10)},
                    {"filter", filter}
                };
                
                TakeExposureTask::execute(exposureParams);
                totalFrames++;
                
                LOG_F(INFO, "Frame {} with filter {} completed", i + 1, filter);
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "FilterSequence completed {} total frames in {} ms", totalFrames, duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "FilterSequence task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto FilterSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced FilterSequence task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(7);
    task->setTimeout(std::chrono::seconds(3600)); // 1 hour timeout
    task->setLogLevel(2);
    
    return task;
}

void FilterSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("filters", "array", true, json::array({"Red", "Green", "Blue"}), "List of filters to use");
    task.addParamDefinition("exposure", "double", true, 60.0, "Exposure time per frame");
    task.addParamDefinition("count", "int", false, 1, "Number of frames per filter");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void FilterSequenceTask::validateFilterSequenceParameters(const json& params) {
    if (!params.contains("filters") || !params["filters"].is_array()) {
        THROW_INVALID_ARGUMENT("Missing or invalid filters parameter");
    }
    
    auto filters = params["filters"];
    if (filters.empty() || filters.size() > 10) {
        THROW_INVALID_ARGUMENT("Filter list must contain 1-10 filters");
    }
    
    if (!params.contains("exposure") || !params["exposure"].is_number()) {
        THROW_INVALID_ARGUMENT("Missing or invalid exposure parameter");
    }
    
    double exposure = params["exposure"].get<double>();
    if (exposure <= 0 || exposure > 3600) {
        THROW_INVALID_ARGUMENT("Exposure time must be between 0 and 3600 seconds");
    }
}

// ==================== RGBSequenceTask ====================

auto RGBSequenceTask::taskName() -> std::string {
    return "RGBSequence";
}

void RGBSequenceTask::execute(const json& params) {
    LOG_F(INFO, "Executing RGBSequence task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        double redExposure = params.value("red_exposure", 60.0);
        double greenExposure = params.value("green_exposure", 60.0);
        double blueExposure = params.value("blue_exposure", 60.0);
        int count = params.value("count", 5);
        
        LOG_F(INFO, "Starting RGB sequence: R={:.1f}s, G={:.1f}s, B={:.1f}s, {} frames each", 
              redExposure, greenExposure, blueExposure, count);
        
#ifdef MOCK_CAMERA
        auto filterWheel = std::make_shared<MockFilterWheel>();
#endif
        
        // RGB filter sequence
        std::vector<std::pair<std::string, double>> rgbSequence = {
            {"Red", redExposure},
            {"Green", greenExposure},
            {"Blue", blueExposure}
        };
        
        int totalFrames = 0;
        
        for (const auto& [filter, exposure] : rgbSequence) {
            LOG_F(INFO, "Switching to {} filter", filter);
#ifdef MOCK_CAMERA
            filterWheel->setFilter(filter);
#endif
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            for (int i = 0; i < count; ++i) {
                LOG_F(INFO, "Taking {} frame {} of {}", filter, i + 1, count);
                
                json exposureParams = {
                    {"exposure", exposure},
                    {"type", ExposureType::LIGHT},
                    {"gain", params.value("gain", 100)},
                    {"offset", params.value("offset", 10)},
                    {"filter", filter}
                };
                
                TakeExposureTask::execute(exposureParams);
                totalFrames++;
                
                LOG_F(INFO, "{} frame {} of {} completed", filter, i + 1, count);
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "RGBSequence completed {} total frames in {} ms", totalFrames, duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "RGBSequence task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto RGBSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced RGBSequence task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(7);
    task->setTimeout(std::chrono::seconds(7200)); // 2 hour timeout
    task->setLogLevel(2);
    
    return task;
}

void RGBSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("red_exposure", "double", false, 60.0, "Red filter exposure time");
    task.addParamDefinition("green_exposure", "double", false, 60.0, "Green filter exposure time");
    task.addParamDefinition("blue_exposure", "double", false, 60.0, "Blue filter exposure time");
    task.addParamDefinition("count", "int", false, 5, "Number of frames per filter");
    task.addParamDefinition("gain", "int", false, 100, "Camera gain");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void RGBSequenceTask::validateRGBParameters(const json& params) {
    std::vector<std::string> exposureParams = {"red_exposure", "green_exposure", "blue_exposure"};
    
    for (const auto& param : exposureParams) {
        if (params.contains(param)) {
            double exposure = params[param].get<double>();
            if (exposure <= 0 || exposure > 3600) {
                THROW_INVALID_ARGUMENT("RGB exposure times must be between 0 and 3600 seconds");
            }
        }
    }
    
    if (params.contains("count")) {
        int count = params["count"].get<int>();
        if (count < 1 || count > 100) {
            THROW_INVALID_ARGUMENT("Frame count must be between 1 and 100");
        }
    }
}

// ==================== NarrowbandSequenceTask ====================

auto NarrowbandSequenceTask::taskName() -> std::string {
    return "NarrowbandSequence";
}

void NarrowbandSequenceTask::execute(const json& params) {
    LOG_F(INFO, "Executing NarrowbandSequence task with params: {}", params.dump(4));
    
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        double haExposure = params.value("ha_exposure", 300.0);
        double oiiiExposure = params.value("oiii_exposure", 300.0);
        double siiExposure = params.value("sii_exposure", 300.0);
        int count = params.value("count", 10);
        bool useHOS = params.value("use_hos", true); // H-alpha, OIII, SII sequence
        
        LOG_F(INFO, "Starting narrowband sequence: Ha={:.1f}s, OIII={:.1f}s, SII={:.1f}s, {} frames each", 
              haExposure, oiiiExposure, siiExposure, count);
        
#ifdef MOCK_CAMERA
        auto filterWheel = std::make_shared<MockFilterWheel>();
#endif
        
        std::vector<std::pair<std::string, double>> narrowbandSequence;
        
        if (useHOS) {
            narrowbandSequence = {
                {"Ha", haExposure},
                {"OIII", oiiiExposure},
                {"SII", siiExposure}
            };
        } else {
            // Custom sequence based on parameters
            if (params.contains("ha_exposure")) narrowbandSequence.emplace_back("Ha", haExposure);
            if (params.contains("oiii_exposure")) narrowbandSequence.emplace_back("OIII", oiiiExposure);
            if (params.contains("sii_exposure")) narrowbandSequence.emplace_back("SII", siiExposure);
        }
        
        int totalFrames = 0;
        
        for (const auto& [filter, exposure] : narrowbandSequence) {
            LOG_F(INFO, "Switching to {} filter", filter);
#ifdef MOCK_CAMERA
            filterWheel->setFilter(filter);
#endif
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Longer settle time for narrowband
            
            for (int i = 0; i < count; ++i) {
                LOG_F(INFO, "Taking {} frame {} of {}", filter, i + 1, count);
                
                json exposureParams = {
                    {"exposure", exposure},
                    {"type", ExposureType::LIGHT},
                    {"gain", params.value("gain", 200)}, // Higher gain for narrowband
                    {"offset", params.value("offset", 10)},
                    {"filter", filter}
                };
                
                TakeExposureTask::execute(exposureParams);
                totalFrames++;
                
                LOG_F(INFO, "{} frame {} of {} completed", filter, i + 1, count);
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(INFO, "NarrowbandSequence completed {} total frames in {} ms", totalFrames, duration.count());
        
    } catch (const std::exception& e) {
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        LOG_F(ERROR, "NarrowbandSequence task failed after {} ms: {}", duration.count(), e.what());
        throw;
    }
}

auto NarrowbandSequenceTask::createEnhancedTask() -> std::unique_ptr<Task> {
    auto task = std::make_unique<Task>(taskName(), [](const json& params) {
        try {
            execute(params);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Enhanced NarrowbandSequence task failed: {}", e.what());
            throw;
        }
    });
    
    defineParameters(*task);
    task->setPriority(6);
    task->setTimeout(std::chrono::seconds(14400)); // 4 hour timeout
    task->setLogLevel(2);
    
    return task;
}

void NarrowbandSequenceTask::defineParameters(Task& task) {
    task.addParamDefinition("ha_exposure", "double", false, 300.0, "H-alpha exposure time");
    task.addParamDefinition("oiii_exposure", "double", false, 300.0, "OIII exposure time");
    task.addParamDefinition("sii_exposure", "double", false, 300.0, "SII exposure time");
    task.addParamDefinition("count", "int", false, 10, "Number of frames per filter");
    task.addParamDefinition("use_hos", "bool", false, true, "Use H-alpha, OIII, SII sequence");
    task.addParamDefinition("gain", "int", false, 200, "Camera gain for narrowband");
    task.addParamDefinition("offset", "int", false, 10, "Camera offset");
}

void NarrowbandSequenceTask::validateNarrowbandParameters(const json& params) {
    std::vector<std::string> exposureParams = {"ha_exposure", "oiii_exposure", "sii_exposure"};
    
    for (const auto& param : exposureParams) {
        if (params.contains(param)) {
            double exposure = params[param].get<double>();
            if (exposure <= 0 || exposure > 1800) { // Max 30 minutes
                THROW_INVALID_ARGUMENT("Narrowband exposure times must be between 0 and 1800 seconds");
            }
        }
    }
    
    if (params.contains("count")) {
        int count = params["count"].get<int>();
        if (count < 1 || count > 200) {
            THROW_INVALID_ARGUMENT("Frame count must be between 1 and 200");
        }
    }
}

}  // namespace lithium::sequencer::task
