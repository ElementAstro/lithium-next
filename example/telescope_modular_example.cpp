/*
 * telescope_modular_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Modular Architecture Usage Example

This example demonstrates how to use the modular INDI Telescope controller
and its individual components for advanced telescope operations.

*************************************************/

#include <iostream>
#include <chrono>
#include <thread>
#include "src/device/indi/telescope_modular.hpp"
#include "src/device/indi/telescope/controller_factory.hpp"

using namespace lithium::device::indi;
using namespace lithium::device::indi::telescope;

/**
 * @brief Basic Telescope Operations Example
 */
void basicTelescopeExample() {
    std::cout << "\n=== Basic Telescope Operations Example ===\n";

    // Create modular telescope
    auto telescope = std::make_unique<INDITelescopeModular>("SimulatorTelescope");

    if (!telescope->initialize()) {
        std::cerr << "Failed to initialize telescope\n";
        return;
    }

    // Scan for available telescopes
    auto devices = telescope->scan();
    std::cout << "Found " << devices.size() << " telescope(s):\n";
    for (const auto& device : devices) {
        std::cout << "  - " << device << "\n";
    }

    if (devices.empty()) {
        std::cout << "No telescopes found, using simulation mode\n";
        // You can still demonstrate with a simulator
        devices.push_back("Telescope Simulator");
    }

    // Connect to first telescope
    if (!telescope->connect(devices[0], 30000, 3)) {
        std::cerr << "Failed to connect to telescope: " << devices[0] << "\n";
        return;
    }

    std::cout << "Connected to: " << devices[0] << "\n";

    // Get telescope status
    auto status = telescope->getStatus();
    if (status.has_value()) {
        std::cout << "Telescope Status: " << status.value() << "\n";
    }

    // Basic slewing example
    std::cout << "\nSlewing to M42 (Orion Nebula)...\n";
    double m42_ra = 5.583; // hours
    double m42_dec = -5.389; // degrees

    if (telescope->slewToRADECJNow(m42_ra, m42_dec, true)) {
        // Monitor slewing progress
        while (telescope->isMoving()) {
            std::cout << "Slewing in progress...\r";
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::cout << "\nSlew completed!\n";

        // Get current position
        auto currentPos = telescope->getRADECJNow();
        if (currentPos.has_value()) {
            std::cout << "Current Position - RA: " << currentPos->ra
                     << " hours, DEC: " << currentPos->dec << " degrees\n";
        }
    }

    telescope->disconnect();
    telescope->destroy();
}

/**
 * @brief Advanced Component Usage Example
 */
void advancedComponentExample() {
    std::cout << "\n=== Advanced Component Usage Example ===\n";

    // Create telescope with custom configuration
    auto config = ControllerFactory::getDefaultConfig();
    config.enableGuiding = true;
    config.enableAdvancedFeatures = true;
    config.guiding.enableGuideCalibration = true;

    auto controller = ControllerFactory::createModularController(config);

    if (!controller->initialize()) {
        std::cerr << "Failed to initialize advanced controller\n";
        return;
    }

    // Access individual components
    auto motionController = controller->getMotionController();
    auto trackingManager = controller->getTrackingManager();
    auto guideManager = controller->getGuideManager();
    auto parkingManager = controller->getParkingManager();

    std::cout << "Component access example:\n";
    std::cout << "  Motion Controller: " << (motionController ? "Available" : "Not Available") << "\n";
    std::cout << "  Tracking Manager: " << (trackingManager ? "Available" : "Not Available") << "\n";
    std::cout << "  Guide Manager: " << (guideManager ? "Available" : "Not Available") << "\n";
    std::cout << "  Parking Manager: " << (parkingManager ? "Available" : "Not Available") << "\n";

    // Example: Configure tracking
    if (trackingManager) {
        std::cout << "\nTracking configuration example:\n";
        trackingManager->setSiderealTracking();
        std::cout << "  Set to sidereal tracking mode\n";

        // Set custom tracking rates
        MotionRates customRates;
        customRates.guideRateNS = 0.5;  // arcsec/sec
        customRates.guideRateEW = 0.5;  // arcsec/sec
        customRates.slewRateRA = 3.0;   // degrees/sec
        customRates.slewRateDEC = 3.0;  // degrees/sec

        if (trackingManager->setTrackRates(customRates)) {
            std::cout << "  Custom tracking rates set successfully\n";
        }
    }

    // Example: Parking operations
    if (parkingManager) {
        std::cout << "\nParking configuration example:\n";

        // Check if telescope can park
        if (parkingManager->canPark()) {
            std::cout << "  Telescope supports parking\n";

            // Save current position as a custom park position
            if (parkingManager->saveParkPosition("ObservingPosition", "Good viewing position")) {
                std::cout << "  Saved custom park position\n";
            }

            // Get all saved park positions
            auto parkPositions = parkingManager->getAllParkPositions();
            std::cout << "  Available park positions: " << parkPositions.size() << "\n";
        }
    }

    // Example: Guide calibration
    if (guideManager) {
        std::cout << "\nGuiding configuration example:\n";

        // Set guide rates
        if (guideManager->setGuideRate(0.5)) { // 0.5 arcsec/sec
            std::cout << "  Guide rate set to 0.5 arcsec/sec\n";
        }

        // Set pulse limits for safety
        guideManager->setMaxPulseDuration(std::chrono::milliseconds(5000)); // 5 seconds max
        guideManager->setMinPulseDuration(std::chrono::milliseconds(10));   // 10 ms min

        std::cout << "  Guide pulse limits configured\n";
    }

    controller->destroy();
}

/**
 * @brief Error Handling and Recovery Example
 */
void errorHandlingExample() {
    std::cout << "\n=== Error Handling and Recovery Example ===\n";

    auto telescope = std::make_unique<INDITelescopeModular>("TestTelescope");

    // Try to connect without initialization (should fail)
    if (!telescope->connect("NonExistentTelescope", 5000, 1)) {
        std::cout << "Expected failure: " << telescope->getLastError() << "\n";
    }

    // Proper initialization and connection
    if (!telescope->initialize()) {
        std::cerr << "Failed to initialize: " << telescope->getLastError() << "\n";
        return;
    }

    // Try invalid coordinates (should fail gracefully)
    if (!telescope->slewToRADECJNow(25.0, 100.0)) { // Invalid RA and DEC
        std::cout << "Expected failure for invalid coordinates: " << telescope->getLastError() << "\n";
    }

    // Demonstrate emergency stop
    std::cout << "Testing emergency stop functionality...\n";
    if (telescope->emergencyStop()) {
        std::cout << "Emergency stop executed successfully\n";
    }

    telescope->destroy();
}

/**
 * @brief Performance and Statistics Example
 */
void performanceExample() {
    std::cout << "\n=== Performance and Statistics Example ===\n";

    // Create high-performance configuration
    auto config = ControllerFactory::getDefaultConfig();
    config.coordinates.coordinateUpdateRate = 10.0; // 10 Hz updates
    config.motion.enableSlewProgressTracking = true;
    config.tracking.enableTrackingStatistics = true;
    config.guiding.enableGuideStatistics = true;

    auto controller = ControllerFactory::createModularController(config);

    if (!controller->initialize()) {
        std::cerr << "Failed to initialize performance controller\n";
        return;
    }

    std::cout << "High-performance telescope controller created\n";
    std::cout << "  Coordinate update rate: 10 Hz\n";
    std::cout << "  Slew progress tracking: Enabled\n";
    std::cout << "  Tracking statistics: Enabled\n";
    std::cout << "  Guide statistics: Enabled\n";

    // In a real implementation, you would:
    // - Monitor tracking accuracy over time
    // - Collect guiding statistics
    // - Measure slew performance
    // - Generate performance reports

    controller->destroy();
}

int main() {
    std::cout << "INDI Telescope Modular Architecture Demo\n";
    std::cout << "========================================\n";

    try {
        basicTelescopeExample();
        advancedComponentExample();
        errorHandlingExample();
        performanceExample();

        std::cout << "\n=== Demo Completed Successfully ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
