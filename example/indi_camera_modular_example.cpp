/*
 * indi_camera_modular_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: INDI Camera Modular Architecture Usage Example

This example demonstrates how to use the modular INDI Camera controller
following the ASCOM architecture pattern for advanced astrophotography operations.

*************************************************/

#include <iostream>
#include <chrono>
#include <thread>
#include "../src/device/indi/camera/indi_camera.hpp"
#include "../src/device/indi/camera/factory/indi_camera_factory.hpp"

using namespace lithium::device::indi::camera;

/**
 * @brief Basic Camera Operations Example
 */
void basicCameraExample() {
    std::cout << "\n=== Basic INDI Camera Operations Example ===\n";

    // Create modular controller using factory (following ASCOM pattern)
    auto controller = INDICameraFactory::createModularController("INDI CCD");

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
    if (devices.empty()) {
        std::cout << "No INDI devices found, please start INDI server\n";
        return;
    }

    std::cout << "Found " << devices.size() << " INDI device(s):\n";
    for (const auto& device : devices) {
        std::cout << "  - " << device << "\n";
    }

    // Connect to first camera
    if (!controller->connect(devices[0])) {
        std::cerr << "Failed to connect to camera: " << devices[0] << "\n";
        return;
    }

    std::cout << "Connected to INDI camera: " << devices[0] << "\n";

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
            controller->saveImage("indi_test_exposure.fits");
        }
    }

    controller->disconnect();
    controller->destroy();
}

/**
 * @brief Temperature Control Example (following ASCOM pattern)
 */
void temperatureControlExample() {
    std::cout << "\n=== Temperature Control Example ===\n";

    auto controller = INDICameraFactory::createSharedController("INDI CCD");

    if (!controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }

    auto devices = controller->scan();
    if (devices.empty()) {
        std::cout << "No INDI devices found\n";
        return;
    }

    if (!controller->connect(devices[0])) {
        std::cerr << "Failed to connect to camera\n";
        return;
    }

    // Check if camera has cooling capability
    if (!controller->hasCooler()) {
        std::cout << "Camera does not support cooling\n";
        controller->disconnect();
        controller->destroy();
        return;
    }

    std::cout << "Camera supports cooling\n";

    // Get current temperature info
    auto tempInfo = controller->getTemperatureInfo();
    std::cout << "Current temperature: " << tempInfo.current << "°C\n";
    std::cout << "Target temperature: " << tempInfo.target << "°C\n";
    std::cout << "Cooling power: " << tempInfo.coolingPower << "%\n";
    std::cout << "Cooler on: " << (tempInfo.coolerOn ? "Yes" : "No") << "\n";

    // Start cooling to -10°C
    std::cout << "\nStarting cooling to -10°C...\n";
    if (controller->startCooling(-10.0)) {
        // Monitor cooling for 30 seconds
        for (int i = 0; i < 30; ++i) {
            tempInfo = controller->getTemperatureInfo();
            std::cout << "Temperature: " << tempInfo.current
                      << "°C, Power: " << tempInfo.coolingPower << "%\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "Stopping cooling...\n";
        controller->stopCooling();
    }

    controller->disconnect();
    controller->destroy();
}

/**
 * @brief Component Access Example (following ASCOM pattern)
 */
void componentAccessExample() {
    std::cout << "\n=== Component Access Example ===\n";

    auto controller = INDICameraFactory::createSharedController("INDI CCD");

    if (!controller->initialize()) {
        std::cerr << "Failed to initialize controller\n";
        return;
    }

    // Access individual components (similar to ASCOM's component access)
    auto exposureController = controller->getExposureController();
    auto temperatureController = controller->getTemperatureController();
    auto hardwareController = controller->getHardwareController();
    auto videoController = controller->getVideoController();
    auto imageProcessor = controller->getImageProcessor();
    auto sequenceManager = controller->getSequenceManager();

    std::cout << "Component access successful:\n";
    std::cout << "  - Exposure Controller: " << exposureController->getComponentName() << "\n";
    std::cout << "  - Temperature Controller: " << temperatureController->getComponentName() << "\n";
    std::cout << "  - Hardware Controller: " << hardwareController->getComponentName() << "\n";
    std::cout << "  - Video Controller: " << videoController->getComponentName() << "\n";
    std::cout << "  - Image Processor: " << imageProcessor->getComponentName() << "\n";
    std::cout << "  - Sequence Manager: " << sequenceManager->getComponentName() << "\n";

    controller->destroy();
}

/**
 * @brief Main function
 */
int main() {
    std::cout << "INDI Camera Modular Architecture Example\n";
    std::cout << "Following ASCOM design patterns\n";
    std::cout << "========================================\n";

    try {
        basicCameraExample();
        temperatureControlExample();
        componentAccessExample();

        std::cout << "\n=== All examples completed successfully ===\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << "\n";
        return 1;
    }
}
