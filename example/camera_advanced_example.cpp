/*
 * camera_advanced_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Advanced example demonstrating multi-camera coordination and professional workflows

*************************************************/

#include "../src/device/camera_factory.hpp"
#include "atom/log/loguru.hpp"

#include <iostream>
#include <thread>
#include <chrono>
#include <future>
#include <fstream>
#include <iomanip>
#include <map>

using namespace lithium::device;

class AdvancedCameraController {
public:
    struct CameraConfiguration {
        std::string name;
        CameraDriverType driverType;
        double exposureTime{1.0};
        int gain{100};
        int offset{0};
        bool enableCooling{true};
        double targetTemperature{-10.0};
        std::pair<int, int> binning{1, 1};
        bool enableSequence{false};
        int sequenceFrames{1};
        double sequenceInterval{0.0};
    };

    void demonstrateAdvancedFeatures() {
        LOG_F(INFO, "Starting advanced camera demonstration");
        
        // Setup multiple cameras for different purposes
        setupCameraConfigurations();
        
        // Initialize all cameras
        if (!initializeAllCameras()) {
            LOG_F(ERROR, "Failed to initialize cameras");
            return;
        }
        
        // Demonstrate coordinated operations
        demonstrateCoordinatedCapture();
        demonstrateTemperatureMonitoring();
        demonstrateSequenceCapture();
        demonstrateVideoStreaming();
        demonstrateAdvancedAnalysis();
        
        // Cleanup
        shutdownAllCameras();
        
        LOG_F(INFO, "Advanced camera demonstration completed");
    }

private:
    std::map<std::string, CameraConfiguration> camera_configs_;
    std::map<std::string, std::shared_ptr<AtomCamera>> cameras_;
    std::map<std::string, std::future<void>> camera_tasks_;

    void setupCameraConfigurations() {
        // Main imaging camera (high resolution, long exposures)
        camera_configs_["main"] = {
            .name = "Main Imaging Camera",
            .driverType = CameraDriverType::AUTO_DETECT,
            .exposureTime = 10.0,
            .gain = 100,
            .offset = 10,
            .enableCooling = true,
            .targetTemperature = -15.0,
            .binning = {1, 1},
            .enableSequence = true,
            .sequenceFrames = 10,
            .sequenceInterval = 2.0
        };

        // Guide camera (fast, small exposures)
        camera_configs_["guide"] = {
            .name = "Guide Camera",
            .driverType = CameraDriverType::AUTO_DETECT,
            .exposureTime = 0.5,
            .gain = 300,
            .offset = 0,
            .enableCooling = false,
            .targetTemperature = 0.0,
            .binning = {2, 2},
            .enableSequence = false,
            .sequenceFrames = 1,
            .sequenceInterval = 0.0
        };

        // Planetary camera (very fast, video)
        camera_configs_["planetary"] = {
            .name = "Planetary Camera",
            .driverType = CameraDriverType::AUTO_DETECT,
            .exposureTime = 0.01,
            .gain = 200,
            .offset = 0,
            .enableCooling = false,
            .targetTemperature = 0.0,
            .binning = {1, 1},
            .enableSequence = false,
            .sequenceFrames = 1,
            .sequenceInterval = 0.0
        };

        LOG_F(INFO, "Configured {} camera setups", camera_configs_.size());
    }

    bool initializeAllCameras() {
        for (const auto& [role, config] : camera_configs_) {
            LOG_F(INFO, "Initializing {} camera", role);
            
            // Create camera instance
            auto camera = createCamera(config.driverType, config.name);
            if (!camera) {
                LOG_F(ERROR, "Failed to create {} camera", role);
                continue;
            }

            // Initialize and connect
            if (!camera->initialize()) {
                LOG_F(ERROR, "Failed to initialize {} camera", role);
                continue;
            }

            // Scan and connect to first available device
            auto devices = camera->scan();
            if (devices.empty()) {
                LOG_F(WARNING, "No devices found for {} camera, using simulator", role);
                if (!camera->connect("CCD Simulator")) {
                    LOG_F(ERROR, "Failed to connect {} camera to simulator", role);
                    continue;
                }
            } else {
                if (!camera->connect(devices[0])) {
                    LOG_F(ERROR, "Failed to connect {} camera to device: {}", role, devices[0]);
                    continue;
                }
            }

            // Apply configuration
            applyCameraConfiguration(camera, config);
            
            cameras_[role] = camera;
            LOG_F(INFO, "Successfully initialized {} camera", role);
        }

        LOG_F(INFO, "Initialized {}/{} cameras", cameras_.size(), camera_configs_.size());
        return !cameras_.empty();
    }

    void applyCameraConfiguration(std::shared_ptr<AtomCamera> camera, const CameraConfiguration& config) {
        // Set gain and offset
        camera->setGain(config.gain);
        camera->setOffset(config.offset);
        
        // Set binning
        camera->setBinning(config.binning.first, config.binning.second);
        
        // Enable cooling if requested
        if (config.enableCooling && camera->hasCooler()) {
            camera->startCooling(config.targetTemperature);
            LOG_F(INFO, "Started cooling to {} °C", config.targetTemperature);
        }
        
        LOG_F(INFO, "Applied configuration: gain={}, offset={}, binning={}x{}", 
              config.gain, config.offset, config.binning.first, config.binning.second);
    }

    void demonstrateCoordinatedCapture() {
        std::cout << "\n=== Coordinated Multi-Camera Capture ===\n";
        
        if (cameras_.empty()) {
            std::cout << "No cameras available for coordinated capture\n";
            return;
        }

        // Start exposures on all cameras simultaneously
        auto start_time = std::chrono::system_clock::now();
        std::map<std::string, std::future<bool>> exposure_futures;
        
        for (const auto& [role, camera] : cameras_) {
            const auto& config = camera_configs_[role];
            
            // Start exposure asynchronously
            exposure_futures[role] = std::async(std::launch::async, [camera, config]() {
                return camera->startExposure(config.exposureTime);
            });
            
            std::cout << "Started " << config.exposureTime << "s exposure on " << role << " camera\n";
        }

        // Wait for all exposures to start
        bool all_started = true;
        for (auto& [role, future] : exposure_futures) {
            if (!future.get()) {
                std::cout << "Failed to start exposure on " << role << " camera\n";
                all_started = false;
            }
        }

        if (!all_started) {
            std::cout << "Some exposures failed to start\n";
            return;
        }

        // Monitor progress
        bool any_exposing = true;
        while (any_exposing) {
            any_exposing = false;
            std::cout << "Progress: ";
            
            for (const auto& [role, camera] : cameras_) {
                if (camera->isExposing()) {
                    any_exposing = true;
                    auto progress = camera->getExposureProgress();
                    auto remaining = camera->getExposureRemaining();
                    std::cout << role << "=" << std::fixed << std::setprecision(1) 
                             << (progress * 100) << "% (" << remaining << "s) ";
                } else {
                    std::cout << role << "=DONE ";
                }
            }
            std::cout << "\r" << std::flush;
            
            if (any_exposing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
        std::cout << "\n";

        // Collect results
        for (const auto& [role, camera] : cameras_) {
            auto frame = camera->getExposureResult();
            if (frame) {
                std::cout << role << " camera: captured " << frame->resolution.width 
                         << "x" << frame->resolution.height << " frame (" 
                         << frame->size << " bytes)\n";
                
                // Save to file
                std::string filename = "capture_" + role + "_" + 
                    std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                        start_time.time_since_epoch()).count()) + ".fits";
                camera->saveImage(filename);
                std::cout << "Saved to: " << filename << "\n";
            }
        }
    }

    void demonstrateTemperatureMonitoring() {
        std::cout << "\n=== Temperature Monitoring ===\n";
        
        std::map<std::string, bool> has_cooler;
        for (const auto& [role, camera] : cameras_) {
            has_cooler[role] = camera->hasCooler();
        }

        if (std::none_of(has_cooler.begin(), has_cooler.end(), 
                        [](const auto& pair) { return pair.second; })) {
            std::cout << "No cameras with cooling capability\n";
            return;
        }

        // Monitor temperatures for 30 seconds
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::seconds(30)) {
            std::cout << "Temperatures: ";
            
            for (const auto& [role, camera] : cameras_) {
                if (has_cooler[role]) {
                    auto temp = camera->getTemperature();
                    auto info = camera->getTemperatureInfo();
                    
                    std::cout << role << "=" << std::fixed << std::setprecision(1);
                    if (temp.has_value()) {
                        std::cout << temp.value() << "°C";
                        if (info.coolerOn) {
                            std::cout << " (cooling to " << info.target << "°C, " 
                                     << info.coolingPower << "% power)";
                        }
                    } else {
                        std::cout << "N/A";
                    }
                    std::cout << " ";
                }
            }
            std::cout << "\r" << std::flush;
            
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        std::cout << "\n";
    }

    void demonstrateSequenceCapture() {
        std::cout << "\n=== Sequence Capture ===\n";
        
        auto main_camera = cameras_.find("main");
        if (main_camera == cameras_.end()) {
            std::cout << "Main camera not available for sequence capture\n";
            return;
        }

        const auto& config = camera_configs_["main"];
        if (!config.enableSequence) {
            std::cout << "Sequence capture not enabled for main camera\n";
            return;
        }

        auto camera = main_camera->second;
        std::cout << "Starting sequence: " << config.sequenceFrames 
                 << " frames, " << config.exposureTime << "s exposure, " 
                 << config.sequenceInterval << "s interval\n";

        if (camera->startSequence(config.sequenceFrames, config.exposureTime, config.sequenceInterval)) {
            while (camera->isSequenceRunning()) {
                auto progress = camera->getSequenceProgress();
                std::cout << "Sequence progress: " << progress.first << "/" << progress.second 
                         << " frames completed\r" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            std::cout << "\nSequence completed\n";
        } else {
            std::cout << "Failed to start sequence\n";
        }
    }

    void demonstrateVideoStreaming() {
        std::cout << "\n=== Video Streaming ===\n";
        
        auto planetary_camera = cameras_.find("planetary");
        if (planetary_camera == cameras_.end()) {
            std::cout << "Planetary camera not available for video streaming\n";
            return;
        }

        auto camera = planetary_camera->second;
        std::cout << "Starting video stream for 10 seconds...\n";
        
        if (camera->startVideo()) {
            auto start = std::chrono::steady_clock::now();
            int frame_count = 0;
            
            while (std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
                auto frame = camera->getVideoFrame();
                if (frame) {
                    frame_count++;
                    if (frame_count % 30 == 0) {  // Display every 30th frame info
                        std::cout << "Received frame " << frame_count 
                                 << ": " << frame->resolution.width << "x" << frame->resolution.height 
                                 << " (" << frame->size << " bytes)\n";
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30 FPS
            }
            
            camera->stopVideo();
            std::cout << "Video streaming completed. Total frames: " << frame_count << "\n";
            
            double fps = frame_count / 10.0;
            std::cout << "Average frame rate: " << std::fixed << std::setprecision(1) << fps << " FPS\n";
        } else {
            std::cout << "Failed to start video streaming\n";
        }
    }

    void demonstrateAdvancedAnalysis() {
        std::cout << "\n=== Advanced Image Analysis ===\n";
        
        for (const auto& [role, camera] : cameras_) {
            std::cout << "\nAnalyzing " << role << " camera:\n";
            
            // Get frame statistics
            auto stats = camera->getFrameStatistics();
            std::cout << "Frame Statistics:\n";
            for (const auto& [key, value] : stats) {
                std::cout << "  " << key << ": " << value << "\n";
            }
            
            // Get camera capabilities
            auto caps = camera->getCameraCapabilities();
            std::cout << "Capabilities:\n";
            std::cout << "  Can abort: " << (caps.canAbort ? "Yes" : "No") << "\n";
            std::cout << "  Can bin: " << (caps.canBin ? "Yes" : "No") << "\n";
            std::cout << "  Has cooler: " << (caps.hasCooler ? "Yes" : "No") << "\n";
            std::cout << "  Has gain: " << (caps.hasGain ? "Yes" : "No") << "\n";
            std::cout << "  Can stream: " << (caps.canStream ? "Yes" : "No") << "\n";
            std::cout << "  Supports sequences: " << (caps.supportsSequences ? "Yes" : "No") << "\n";
            
            // Performance metrics
            std::cout << "Performance:\n";
            std::cout << "  Total frames: " << camera->getTotalFramesReceived() << "\n";
            std::cout << "  Dropped frames: " << camera->getDroppedFrames() << "\n";
            std::cout << "  Average frame rate: " << camera->getAverageFrameRate() << " FPS\n";
            
            // Get last image quality if available
            auto quality = camera->getLastImageQuality();
            if (!quality.empty()) {
                std::cout << "Last Image Quality:\n";
                for (const auto& [metric, value] : quality) {
                    std::cout << "  " << metric << ": " << value << "\n";
                }
            }
        }
    }

    void shutdownAllCameras() {
        LOG_F(INFO, "Shutting down all cameras");
        
        for (auto& [role, camera] : cameras_) {
            if (camera->isExposing()) {
                camera->abortExposure();
            }
            if (camera->isVideoRunning()) {
                camera->stopVideo();
            }
            if (camera->isSequenceRunning()) {
                camera->stopSequence();
            }
            if (camera->isCoolerOn()) {
                camera->stopCooling();
            }
            
            camera->disconnect();
            camera->destroy();
            
            LOG_F(INFO, "Shutdown {} camera", role);
        }
        
        cameras_.clear();
    }
};

int main() {
    // Initialize logging
    loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
    loguru::init(0, nullptr);
    
    try {
        AdvancedCameraController controller;
        controller.demonstrateAdvancedFeatures();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in advanced camera example: {}", e.what());
        return 1;
    }

    return 0;
}
