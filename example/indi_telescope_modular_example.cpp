/*
 * indi_telescope_modular_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Modular Architecture Usage Example

This example demonstrates how to use the modular INDI Telescope controller
and its individual components for advanced telescope operations, following
the same pattern as the ASI Camera modular example.

*************************************************/

#include <iostream>
#include <chrono>
#include <thread>
#include "src/device/indi/telescope/telescope_controller.hpp"
#include "src/device/indi/telescope/controller_factory.hpp"
#include "src/device/indi/telescope_v2.hpp"

using namespace lithium::device::indi::telescope;
using namespace lithium::device::indi::telescope::components;

/**
 * @brief Basic Telescope Operations Example
 */
void basicTelescopeExample() {
    std::cout << "\n=== Basic Telescope Operations Example ===\n";
    
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
    
    std::vector<std::string> devices = controller->scan();
    
    std::cout << "Found " << devices.size() << " telescope(s):\n";
    for (const auto& device : devices) {
        std::cout << "  - " << device << "\n";
    }
    
    if (devices.empty()) {
        std::cout << "No telescopes found, using simulation mode\n";
        return;
    }
    
    // Connect to first telescope
    if (!controller->connect(devices[0], 30000, 3)) {
        std::cerr << "Failed to connect to telescope: " << devices[0] << "\n";
        return;
    }
    
    std::cout << "Connected to: " << devices[0] << "\n";
    
    // Get telescope information
    auto telescopeInfo = controller->getTelescopeInfo();
    if (telescopeInfo) {
        std::cout << "Telescope Info:\n";
        std::cout << "  Aperture: " << telescopeInfo->aperture << "mm\n";
        std::cout << "  Focal Length: " << telescopeInfo->focalLength << "mm\n";
    }
    
    // Get current position
    auto currentPos = controller->getRADECJNow();
    if (currentPos) {
        std::cout << "Current Position:\n";
        std::cout << "  RA: " << currentPos->ra << "h\n";
        std::cout << "  DEC: " << currentPos->dec << "°\n";
    }
    
    // Basic slewing
    std::cout << "\nSlewing to Vega (RA: 18.61h, DEC: 38.78°)...\n";
    if (controller->slewToRADECJNow(18.61, 38.78, true)) {
        while (controller->isMoving()) {
            auto status = controller->getStatus();
            if (status) {
                std::cout << "Status: " << *status << "\r";
                std::cout.flush();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::cout << "\nSlew complete!\n";
        
        // Check if tracking is enabled
        if (controller->isTrackingEnabled()) {
            std::cout << "Tracking is enabled\n";
        }
    }
    
    controller->disconnect();
    controller->destroy();
}

/**
 * @brief Component-Level Access Example
 */
void componentAccessExample() {
    std::cout << "\n=== Component-Level Access Example ===\n";
    
    // Create telescope controller
    auto controller = ControllerFactory::createModularController();
    
    if (!controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    // Get individual components for advanced operations
    auto hardware = controller->getHardwareInterface();
    auto motionController = controller->getMotionController();
    auto trackingManager = controller->getTrackingManager();
    auto parkingManager = controller->getParkingManager();
    auto coordinateManager = controller->getCoordinateManager();
    auto guideManager = controller->getGuideManager();
    
    std::cout << "Component access example:\n";
    
    // Hardware interface example
    if (hardware) {
        std::cout << "Hardware component available\n";
        auto devices = hardware->scanDevices();
        std::cout << "Found " << devices.size() << " devices via hardware interface\n";
    }
    
    // Motion controller example
    if (motionController) {
        std::cout << "Motion controller available\n";
        auto motionStatus = motionController->getMotionStatus();
        std::cout << "Motion state: " << motionController->getMotionStateString() << "\n";
    }
    
    // Tracking manager example
    if (trackingManager) {
        std::cout << "Tracking manager available\n";
        auto trackingStatus = trackingManager->getTrackingStatus();
        std::cout << "Tracking enabled: " << (trackingStatus.isEnabled ? "Yes" : "No") << "\n";
    }
    
    // Parking manager example
    if (parkingManager) {
        std::cout << "Parking manager available\n";
        auto parkingStatus = parkingManager->getParkingStatus();
        std::cout << "Park state: " << parkingManager->getParkStateString() << "\n";
    }
    
    // Coordinate manager example
    if (coordinateManager) {
        std::cout << "Coordinate manager available\n";
        auto coordStatus = coordinateManager->getCoordinateStatus();
        std::cout << "Coordinates valid: " << (coordStatus.coordinatesValid ? "Yes" : "No") << "\n";
    }
    
    // Guide manager example
    if (guideManager) {
        std::cout << "Guide manager available\n";
        auto guideStats = guideManager->getGuideStatistics();
        std::cout << "Total guide pulses: " << guideStats.totalPulses << "\n";
    }
    
    controller->destroy();
}

/**
 * @brief Advanced Tracking Example
 */
void advancedTrackingExample() {
    std::cout << "\n=== Advanced Tracking Example ===\n";
    
    auto controller = ControllerFactory::createModularController();
    
    if (!controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    auto devices = controller->scan();
    if (devices.empty()) {
        std::cout << "No telescopes found\n";
        return;
    }
    
    if (!controller->connect(devices[0], 30000, 3)) {
        std::cerr << "Failed to connect to telescope\n";
        return;
    }
    
    // Enable sidereal tracking
    std::cout << "Enabling sidereal tracking...\n";
    if (controller->setTrackRate(TrackMode::SIDEREAL)) {
        controller->enableTracking(true);
        
        if (controller->isTrackingEnabled()) {
            std::cout << "Sidereal tracking enabled\n";
            
            // Get tracking rates
            auto trackRates = controller->getTrackRates();
            std::cout << "RA Rate: " << trackRates.slewRateRA << " arcsec/sec\n";
            std::cout << "DEC Rate: " << trackRates.slewRateDEC << " arcsec/sec\n";
        }
    }
    
    // Access tracking manager for advanced features
    auto trackingManager = controller->getTrackingManager();
    if (trackingManager) {
        // Set custom tracking rates
        std::cout << "\nSetting custom tracking rates...\n";
        if (trackingManager->setCustomTracking(15.0, 0.0)) {
            std::cout << "Custom tracking rates set\n";
        }
        
        // Get tracking statistics
        auto stats = trackingManager->getTrackingStatistics();
        std::cout << "Tracking session time: " << stats.totalTrackingTime.count() << " seconds\n";
        std::cout << "Average tracking error: " << stats.avgTrackingError << " arcsec\n";
        
        // Monitor tracking quality
        double quality = trackingManager->calculateTrackingQuality();
        std::cout << "Tracking quality: " << (quality * 100.0) << "%\n";
        std::cout << "Quality description: " << trackingManager->getTrackingQualityDescription() << "\n";
    }
    
    controller->disconnect();
    controller->destroy();
}

/**
 * @brief Parking and Home Position Example
 */
void parkingExample() {
    std::cout << "\n=== Parking and Home Position Example ===\n";
    
    auto controller = ControllerFactory::createModularController();
    
    if (!controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    auto devices = controller->scan();
    if (devices.empty()) {
        std::cout << "No telescopes found\n";
        return;
    }
    
    if (!controller->connect(devices[0], 30000, 3)) {
        std::cerr << "Failed to connect to telescope\n";
        return;
    }
    
    auto parkingManager = controller->getParkingManager();
    if (!parkingManager) {
        std::cerr << "Parking manager not available\n";
        return;
    }
    
    std::cout << "Parking capabilities:\n";
    std::cout << "  Can park: " << (controller->canPark() ? "Yes" : "No") << "\n";
    std::cout << "  Is parked: " << (controller->isParked() ? "Yes" : "No") << "\n";
    
    // Save current position as a custom park position
    if (parkingManager->setParkPositionFromCurrent("MyCustomPark")) {
        std::cout << "Saved current position as 'MyCustomPark'\n";
    }
    
    // Get all saved park positions
    auto parkPositions = parkingManager->getAllParkPositions();
    std::cout << "Saved park positions (" << parkPositions.size() << "):\n";
    for (const auto& pos : parkPositions) {
        std::cout << "  - " << pos.name << ": RA=" << pos.ra << "h, DEC=" << pos.dec << "°\n";
    }
    
    // Demonstrate parking sequence
    if (!controller->isParked()) {
        std::cout << "\nStarting parking sequence...\n";
        if (controller->park()) {
            while (parkingManager->isParking()) {
                double progress = parkingManager->getParkingProgress();
                std::cout << "Parking progress: " << (progress * 100.0) << "%\r";
                std::cout.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            std::cout << "\nParking complete!\n";
        }
    }
    
    // Demonstrate unparking
    if (controller->isParked()) {
        std::cout << "\nStarting unparking sequence...\n";
        if (controller->unpark()) {
            while (parkingManager->isUnparking()) {
                std::cout << "Unparking...\r";
                std::cout.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            std::cout << "\nUnparking complete!\n";
        }
    }
    
    controller->disconnect();
    controller->destroy();
}

/**
 * @brief Guiding Operations Example
 */
void guidingExample() {
    std::cout << "\n=== Guiding Operations Example ===\n";
    
    auto controller = ControllerFactory::createModularController();
    
    if (!controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }
    
    auto devices = controller->scan();
    if (devices.empty()) {
        std::cout << "No telescopes found\n";
        return;
    }
    
    if (!controller->connect(devices[0], 30000, 3)) {
        std::cerr << "Failed to connect to telescope\n";
        return;
    }
    
    auto guideManager = controller->getGuideManager();
    if (!guideManager) {
        std::cerr << "Guide manager not available\n";
        return;
    }
    
    std::cout << "Guide system status:\n";
    std::cout << "  Is calibrated: " << (guideManager->isCalibrated() ? "Yes" : "No") << "\n";
    std::cout << "  Is guiding: " << (guideManager->isGuiding() ? "Yes" : "No") << "\n";
    
    // Demonstrate guide calibration
    if (!guideManager->isCalibrated()) {
        std::cout << "\nStarting guide calibration...\n";
        if (guideManager->autoCalibrate(std::chrono::milliseconds(1000))) {
            while (guideManager->isCalibrating()) {
                std::cout << "Calibrating...\r";
                std::cout.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            std::cout << "\nCalibration complete!\n";
            
            auto calibration = guideManager->getCalibration();
            if (calibration.isValid) {
                std::cout << "Calibration results:\n";
                std::cout << "  North rate: " << calibration.northRate << " arcsec/ms\n";
                std::cout << "  South rate: " << calibration.southRate << " arcsec/ms\n";
                std::cout << "  East rate: " << calibration.eastRate << " arcsec/ms\n";
                std::cout << "  West rate: " << calibration.westRate << " arcsec/ms\n";
            }
        }
    }
    
    // Demonstrate guide pulses
    std::cout << "\nSending test guide pulses...\n";
    
    // North pulse
    if (guideManager->guideNorth(std::chrono::milliseconds(1000))) {
        std::cout << "North guide pulse sent (1 second)\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    }
    
    // East pulse
    if (guideManager->guideEast(std::chrono::milliseconds(500))) {
        std::cout << "East guide pulse sent (0.5 seconds)\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
    }
    
    // Get guide statistics
    auto stats = guideManager->getGuideStatistics();
    std::cout << "\nGuide session statistics:\n";
    std::cout << "  Total pulses: " << stats.totalPulses << "\n";
    std::cout << "  North pulses: " << stats.northPulses << "\n";
    std::cout << "  East pulses: " << stats.eastPulses << "\n";
    std::cout << "  Guide RMS: " << stats.guideRMS << " arcsec\n";
    
    controller->disconnect();
    controller->destroy();
}

/**
 * @brief Backward Compatibility Example with INDITelescopeV2
 */
void backwardCompatibilityExample() {
    std::cout << "\n=== Backward Compatibility Example ===\n";
    
    // Create telescope using the new V2 interface (backward compatible)
    auto telescope = std::make_unique<INDITelescopeV2>("TestTelescope");
    
    if (!telescope->initialize()) {
        std::cerr << "Failed to initialize telescope\n";
        return;
    }
    
    auto devices = telescope->scan();
    std::cout << "Found " << devices.size() << " telescope(s) using V2 interface\n";
    
    if (!devices.empty()) {
        // Use the traditional interface
        if (telescope->connect(devices[0], 30000, 3)) {
            std::cout << "Connected using backward-compatible interface\n";
            
            // Traditional operations
            auto status = telescope->getStatus();
            if (status) {
                std::cout << "Status: " << *status << "\n";
            }
            
            // But also access modern components if needed
            auto controller = telescope->getController();
            if (controller) {
                std::cout << "Advanced controller features are also available\n";
                
                // Access specific components
                auto trackingManager = telescope->getComponent<TrackingManager>();
                if (trackingManager) {
                    std::cout << "Direct component access works\n";
                }
            }
            
            telescope->disconnect();
        }
    }
    
    telescope->destroy();
}

/**
 * @brief Configuration Example
 */
void configurationExample() {
    std::cout << "\n=== Configuration Example ===\n";
    
    // Create custom configuration
    TelescopeControllerConfig config = ControllerFactory::getDefaultConfig();
    
    // Customize settings
    config.name = "MyCustomTelescope";
    config.enableGuiding = true;
    config.enableTracking = true;
    config.enableParking = true;
    
    // Hardware settings
    config.hardware.connectionTimeout = 60000;  // 60 seconds
    config.hardware.enableAutoReconnect = true;
    
    // Motion settings
    config.motion.maxSlewSpeed = 3.0;  // degrees/sec
    config.motion.enableMotionLimits = true;
    
    // Tracking settings
    config.tracking.enableAutoTracking = true;
    config.tracking.enablePEC = true;
    
    // Guiding settings
    config.guiding.maxPulseDuration = 5000.0;  // 5 seconds max
    config.guiding.enableGuideCalibration = true;
    
    // Create controller with custom configuration
    auto controller = ControllerFactory::createModularController(config);
    
    if (controller) {
        std::cout << "Custom configured controller created successfully\n";
        std::cout << "Configuration applied for: " << config.name << "\n";
        
        // Save configuration for future use
        if (ControllerFactory::saveConfigToFile(config, "my_telescope_config.json")) {
            std::cout << "Configuration saved to file\n";
        }
    }
    
    // Create telescope using the V2 interface with configuration
    auto telescopeV2 = INDITelescopeV2::createWithConfig("ConfiguredTelescope", config);
    if (telescopeV2) {
        std::cout << "INDITelescopeV2 created with custom configuration\n";
    }
}

int main() {
    std::cout << "INDI Telescope Modular Architecture Examples\n";
    std::cout << "============================================\n";
    
    try {
        // Run all examples
        basicTelescopeExample();
        componentAccessExample();
        advancedTrackingExample();
        parkingExample();
        guidingExample();
        backwardCompatibilityExample();
        configurationExample();
        
        std::cout << "\n=== All Examples Completed Successfully ===\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error running examples: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
