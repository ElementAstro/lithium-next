/*
 * asi_camera_modular_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASI Camera Modular Architecture Usage Example

This example demonstrates how to use the modular ASI Camera controller
and its individual components for advanced astrophotography operations.

*************************************************/

#include <iostream>
#include <chrono>
#include <thread>
#include "src/device/asi/camera/controller/asi_camera_controller_v2.hpp"
#include "src/device/asi/camera/controller/controller_factory.hpp"

using namespace lithium::device::asi::camera;
using namespace lithium::device::asi::camera::controller;
using namespace lithium::device::asi::camera::components;

/**
 * @brief Basic Camera Operations Example
 */
void basicCameraExample() {
    std::cout << "\n=== Basic Camera Operations Example ===\n";
    
    // Create modular controller using factory
    auto controller = ControllerFactory::createModularController();
    
    if (!controller) {
        std::cerr << "Failed to create modular controller\n";
        return;
    }
    
    // Initialize and connect
    if (!controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    std::vector<std::string> devices;
    if (!controller->scan(devices)) {
        std::cerr << "Failed to scan for devices\n";
        return;
    }
    
    std::cout << "Found " << devices.size() << " camera(s):\n";
    for (const auto& device : devices) {
        std::cout << "  - " << device << "\n";
    }
    
    if (devices.empty()) {
        std::cout << "No cameras found, using simulation mode\n";
        return;
    }
    
    // Connect to first camera
    if (!controller->connect(devices[0])) {
        std::cerr << "Failed to connect to camera: " << devices[0] << "\n";
        return;
    }
    
    std::cout << "Connected to: " << controller->getModelName() << "\n";
    std::cout << "Serial Number: " << controller->getSerialNumber() << "\n";
    std::cout << "Firmware: " << controller->getFirmwareVersion() << "\n";
    
    // Basic exposure
    std::cout << "\nTaking 5-second exposure...\n";
    if (controller->startExposure(5.0)) {
        while (controller->isExposing()) {
            double progress = controller->getExposureProgress();
            double remaining = controller->getExposureRemaining();
            std::cout << "Progress: " << progress << "%, Remaining: " << remaining << "s\r";
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "\nExposure complete!\n";
        
        auto frame = controller->getExposureResult();
        if (frame) {
            std::cout << "Frame size: " << frame->width << "x" << frame->height << "\n";
            controller->saveImage("test_exposure.fits");
        }
    }
    
    controller->disconnect();
    controller->destroy();
}

/**
 * @brief Temperature Control Example
 */
void temperatureControlExample() {
    std::cout << "\n=== Temperature Control Example ===\n";
    
    auto controller = ControllerFactory::createModularController();
    if (!controller || !controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    // Get temperature controller component for advanced operations
    auto tempController = controller->getTemperatureController();
    
    if (!tempController->hasCooler()) {
        std::cout << "Camera does not have a cooler\n";
        return;
    }
    
    // Set temperature callback
    tempController->setTemperatureCallback([](const TemperatureController::TemperatureInfo& info) {
        std::cout << "Temperature: " << info.currentTemperature << "°C, "
                  << "Target: " << info.targetTemperature << "°C, "
                  << "Power: " << info.coolerPower << "%\n";
    });
    
    // Start cooling to -10°C
    std::cout << "Starting cooling to -10°C...\n";
    if (tempController->startCooling(-10.0)) {
        // Wait for temperature stabilization (with timeout)
        auto startTime = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::minutes(5);
        
        while (!tempController->hasReachedTarget()) {
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed > timeout) {
                std::cout << "Cooling timeout reached\n";
                break;
            }
            
            auto state = tempController->getStateString();
            std::cout << "Cooling state: " << state << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        if (tempController->hasReachedTarget()) {
            std::cout << "Target temperature reached!\n";
            
            // Take temperature-stabilized exposure
            std::cout << "Taking cooled exposure...\n";
            controller->startExposure(30.0);
            // ... wait for completion
        }
        
        // Stop cooling
        tempController->stopCooling();
    }
}

/**
 * @brief Video Streaming Example
 */
void videoStreamingExample() {
    std::cout << "\n=== Video Streaming Example ===\n";
    
    auto controller = ControllerFactory::createModularController();
    if (!controller || !controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    // Get video manager for advanced operations
    auto videoManager = controller->getVideoManager();
    
    // Configure video settings
    VideoManager::VideoSettings videoSettings;
    videoSettings.width = 1920;
    videoSettings.height = 1080;
    videoSettings.fps = 30.0;
    videoSettings.format = "RAW16";
    videoSettings.exposure = 33000; // 33ms
    videoSettings.gain = 100;
    
    // Set frame callback
    videoManager->setFrameCallback([](std::shared_ptr<AtomCameraFrame> frame) {
        if (frame) {
            std::cout << "Received video frame: " << frame->width << "x" << frame->height << "\n";
        }
    });
    
    // Set statistics callback
    videoManager->setStatisticsCallback([](const VideoManager::VideoStatistics& stats) {
        std::cout << "Video stats - FPS: " << stats.actualFPS 
                  << ", Received: " << stats.framesReceived
                  << ", Dropped: " << stats.framesDropped << "\n";
    });
    
    // Start video streaming
    std::cout << "Starting video stream...\n";
    if (videoManager->startVideo(videoSettings)) {
        std::cout << "Video streaming for 10 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        // Start recording
        std::cout << "Starting video recording...\n";
        videoManager->startRecording("test_video.mp4");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        videoManager->stopRecording();
        
        videoManager->stopVideo();
        std::cout << "Video streaming stopped\n";
    }
}

/**
 * @brief Automated Sequence Example
 */
void automatedSequenceExample() {
    std::cout << "\n=== Automated Sequence Example ===\n";
    
    auto controller = ControllerFactory::createModularController();
    if (!controller || !controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    // Get sequence manager for advanced operations
    auto sequenceManager = controller->getSequenceManager();
    
    // Create a simple exposure sequence
    auto sequence = sequenceManager->createSimpleSequence(
        10.0,  // 10-second exposures
        5,     // 5 exposures
        std::chrono::seconds(2) // 2-second interval
    );
    
    sequence.name = "Test Sequence";
    sequence.outputDirectory = "./captures";
    sequence.filenameTemplate = "test_{step:03d}_{timestamp}";
    
    // Set progress callback
    sequenceManager->setProgressCallback([](const SequenceManager::SequenceProgress& progress) {
        std::cout << "Sequence progress: " << progress.currentStep << "/" << progress.totalSteps
                  << " (" << progress.progress << "%)\n";
    });
    
    // Set completion callback
    sequenceManager->setCompletionCallback([](const SequenceManager::SequenceResult& result) {
        std::cout << "Sequence completed: " << (result.success ? "SUCCESS" : "FAILED") << "\n";
        std::cout << "Completed exposures: " << result.completedExposures << "\n";
        std::cout << "Duration: " << result.totalDuration.count() << " seconds\n";
        
        if (!result.success) {
            std::cout << "Error: " << result.errorMessage << "\n";
        }
    });
    
    // Start sequence
    std::cout << "Starting automated sequence...\n";
    if (sequenceManager->startSequence(sequence)) {
        // Wait for completion
        while (sequenceManager->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        auto result = sequenceManager->getLastResult();
        std::cout << "Sequence finished with " << result.savedFilenames.size() << " saved files\n";
    }
}

/**
 * @brief Image Processing Example
 */
void imageProcessingExample() {
    std::cout << "\n=== Image Processing Example ===\n";
    
    auto controller = ControllerFactory::createModularController();
    if (!controller || !controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    // Get image processor for advanced operations
    auto imageProcessor = controller->getImageProcessor();
    
    // Take a test exposure
    std::cout << "Taking test exposure for processing...\n";
    controller->startExposure(5.0);
    while (controller->isExposing()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto frame = controller->getExposureResult();
    if (!frame) {
        std::cout << "No frame captured for processing\n";
        return;
    }
    
    // Analyze the image
    std::cout << "Analyzing image...\n";
    auto stats = imageProcessor->analyzeImage(frame);
    std::cout << "Image statistics:\n";
    std::cout << "  Mean: " << stats.mean << "\n";
    std::cout << "  Std Dev: " << stats.stdDev << "\n";
    std::cout << "  SNR: " << stats.snr << "\n";
    std::cout << "  Star Count: " << stats.starCount << "\n";
    std::cout << "  FWHM: " << stats.fwhm << "\n";
    
    // Apply processing
    ImageProcessor::ProcessingSettings settings;
    settings.enableNoiseReduction = true;
    settings.noiseReductionStrength = 30;
    settings.enableSharpening = true;
    settings.sharpeningStrength = 20;
    settings.gamma = 1.2;
    settings.contrast = 1.1;
    
    std::cout << "Processing image...\n";
    auto processedResult = imageProcessor->processImage(frame, settings);
    auto futureResult = processedResult.get();
    
    if (futureResult.success) {
        std::cout << "Processing completed in " << futureResult.processingTime.count() << "ms\n";
        std::cout << "Applied operations: ";
        for (const auto& op : futureResult.appliedOperations) {
            std::cout << op << " ";
        }
        std::cout << "\n";
        
        // Save processed image
        imageProcessor->convertToFITS(futureResult.processedFrame, "processed_image.fits");
        imageProcessor->convertToJPEG(futureResult.processedFrame, "processed_image.jpg", 95);
    } else {
        std::cout << "Processing failed: " << futureResult.errorMessage << "\n";
    }
}

/**
 * @brief Property Management Example
 */
void propertyManagementExample() {
    std::cout << "\n=== Property Management Example ===\n";
    
    auto controller = ControllerFactory::createModularController();
    if (!controller || !controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    // Get property manager for advanced operations
    auto propertyManager = controller->getPropertyManager();
    
    if (!propertyManager->initialize()) {
        std::cout << "Failed to initialize property manager\n";
        return;
    }
    
    // List all available properties
    std::cout << "Available camera properties:\n";
    auto properties = propertyManager->getAllProperties();
    for (const auto& prop : properties) {
        std::cout << "  " << prop.name << ": " << prop.currentValue
                  << " (range: " << prop.minValue << "-" << prop.maxValue << ")\n";
    }
    
    // Configure camera settings
    std::cout << "\nConfiguring camera settings...\n";
    propertyManager->setGain(150);
    propertyManager->setExposure(1000000); // 1 second in microseconds
    propertyManager->setOffset(50);
    
    // Set ROI
    PropertyManager::ROI roi{100, 100, 800, 600};
    if (propertyManager->setROI(roi)) {
        std::cout << "ROI set to: " << roi.x << "," << roi.y << " " << roi.width << "x" << roi.height << "\n";
    }
    
    // Set binning
    PropertyManager::BinningMode binning{2, 2, "2x2"};
    if (propertyManager->setBinning(binning)) {
        std::cout << "Binning set to: " << binning.description << "\n";
    }
    
    // Save settings as preset
    std::cout << "Saving current settings as preset...\n";
    propertyManager->savePreset("high_gain_setup");
    
    // Test auto controls
    std::cout << "Testing auto controls...\n";
    propertyManager->setAutoGain(true);
    propertyManager->setAutoExposure(true);
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    std::cout << "Auto gain: " << (propertyManager->isAutoGainEnabled() ? "ON" : "OFF") << "\n";
    std::cout << "Auto exposure: " << (propertyManager->isAutoExposureEnabled() ? "ON" : "OFF") << "\n";
    std::cout << "Current gain: " << propertyManager->getGain() << "\n";
    std::cout << "Current exposure: " << propertyManager->getExposure() << " μs\n";
}

int main() {
    std::cout << "ASI Camera Modular Architecture Examples\n";
    std::cout << "========================================\n";
    
    // Check component availability
    if (!ControllerFactory::isModularControllerAvailable()) {
        std::cout << "Modular controller is not available\n";
        return 1;
    }
    
    std::cout << "Using modular controller: " 
              << ControllerFactory::getControllerTypeName(ControllerType::MODULAR) << "\n";
    
    try {
        // Run examples
        basicCameraExample();
        temperatureControlExample();
        videoStreamingExample();
        automatedSequenceExample();
        imageProcessingExample();
        propertyManagementExample();
        
        std::cout << "\n=== All examples completed successfully! ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
