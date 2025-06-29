/*
 * camera_usage_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Comprehensive example demonstrating QHY and ASI camera usage

*************************************************/

#include "camera_factory.hpp"
#include "atom/log/loguru.hpp"

#include <iostream>
#include <thread>
#include <chrono>

using namespace lithium::device;

class CameraExample {
public:
    void runExample() {
        LOG_F(INFO, "Starting camera usage example");

        // Scan for available cameras
        demonstrateCameraScanning();

        // Test QHY cameras
        testQHYCameras();

        // Test ASI cameras
        testASICameras();

        // Test automatic camera detection
        testAutomaticDetection();

        // Test advanced camera features
        demonstrateAdvancedFeatures();

        LOG_F(INFO, "Camera usage example completed");
    }

private:
    void demonstrateCameraScanning() {
        std::cout << "\n=== Camera Scanning Demo ===\n";

        // Scan for all available cameras
        auto cameras = scanCameras();

        std::cout << "Found " << cameras.size() << " cameras:\n";
        for (const auto& camera : cameras) {
            std::cout << "  - " << camera.name
                     << " (" << camera.manufacturer << ")"
                     << " [" << CameraFactory::driverTypeToString(camera.type) << "]\n";
            std::cout << "    Description: " << camera.description << "\n";
            std::cout << "    Available: " << (camera.isAvailable ? "Yes" : "No") << "\n\n";
        }
    }

    void testQHYCameras() {
        std::cout << "\n=== QHY Camera Test ===\n";

        if (!CameraFactory::getInstance().isDriverSupported(CameraDriverType::QHY)) {
            std::cout << "QHY driver not available\n";
            return;
        }

        // Create QHY camera instance
        auto qhyCamera = createCamera(CameraDriverType::QHY, "QHY Camera Test");
        if (!qhyCamera) {
            std::cout << "Failed to create QHY camera\n";
            return;
        }

        // Initialize and connect
        if (!qhyCamera->initialize()) {
            std::cout << "Failed to initialize QHY camera\n";
            return;
        }

        // Scan for devices
        auto devices = qhyCamera->scan();
        if (devices.empty()) {
            std::cout << "No QHY cameras found\n";
            qhyCamera->destroy();
            return;
        }

        std::cout << "Found QHY devices: ";
        for (const auto& device : devices) {
            std::cout << device << " ";
        }
        std::cout << "\n";

        // Connect to first device
        if (qhyCamera->connect(devices[0])) {
            std::cout << "Connected to QHY camera: " << devices[0] << "\n";

            testBasicCameraOperations(qhyCamera, "QHY");
            testQHYSpecificFeatures(qhyCamera);

            qhyCamera->disconnect();
        } else {
            std::cout << "Failed to connect to QHY camera\n";
        }

        qhyCamera->destroy();
    }

    void testASICameras() {
        std::cout << "\n=== ASI Camera Test ===\n";

        if (!CameraFactory::getInstance().isDriverSupported(CameraDriverType::ASI)) {
            std::cout << "ASI driver not available\n";
            return;
        }

        // Create ASI camera instance
        auto asiCamera = createCamera(CameraDriverType::ASI, "ASI Camera Test");
        if (!asiCamera) {
            std::cout << "Failed to create ASI camera\n";
            return;
        }

        // Initialize and connect
        if (!asiCamera->initialize()) {
            std::cout << "Failed to initialize ASI camera\n";
            return;
        }

        // Scan for devices
        auto devices = asiCamera->scan();
        if (devices.empty()) {
            std::cout << "No ASI cameras found\n";
            asiCamera->destroy();
            return;
        }

        std::cout << "Found ASI devices: ";
        for (const auto& device : devices) {
            std::cout << device << " ";
        }
        std::cout << "\n";

        // Connect to first device
        if (asiCamera->connect(devices[0])) {
            std::cout << "Connected to ASI camera: " << devices[0] << "\n";

            testBasicCameraOperations(asiCamera, "ASI");
            testASISpecificFeatures(asiCamera);

            asiCamera->disconnect();
        } else {
            std::cout << "Failed to connect to ASI camera\n";
        }

        asiCamera->destroy();
    }

    void testAutomaticDetection() {
        std::cout << "\n=== Automatic Camera Detection Test ===\n";

        // Test automatic detection with different camera patterns
        std::vector<std::string> testNames = {
            "QHY5III462C",  // Should detect QHY
            "ASI120MM",     // Should detect ASI
            "CCD Simulator" // Should detect Simulator
        };

        for (const auto& name : testNames) {
            std::cout << "Testing automatic detection for: " << name << "\n";

            auto camera = createCamera(name);
            if (camera) {
                std::cout << "  Successfully created camera instance\n";

                if (camera->initialize()) {
                    std::cout << "  Camera initialized successfully\n";
                    camera->destroy();
                } else {
                    std::cout << "  Failed to initialize camera\n";
                }
            } else {
                std::cout << "  Failed to create camera instance\n";
            }
        }
    }

    void testBasicCameraOperations(std::shared_ptr<AtomCamera> camera, const std::string& type) {
        std::cout << "Testing basic " << type << " camera operations:\n";

        // Test capabilities
        auto caps = camera->getCameraCapabilities();
        std::cout << "  Capabilities:\n";
        std::cout << "    Can abort: " << caps.canAbort << "\n";
        std::cout << "    Can bin: " << caps.canBin << "\n";
        std::cout << "    Has cooler: " << caps.hasCooler << "\n";
        std::cout << "    Has gain: " << caps.hasGain << "\n";
        std::cout << "    Can stream: " << caps.canStream << "\n";

        // Test basic parameters
        if (caps.hasGain) {
            auto gainRange = camera->getGainRange();
            std::cout << "  Gain range: " << gainRange.first << " - " << gainRange.second << "\n";
        }

        if (caps.hasOffset) {
            auto offsetRange = camera->getOffsetRange();
            std::cout << "  Offset range: " << offsetRange.first << " - " << offsetRange.second << "\n";
        }

        // Test resolution
        auto maxRes = camera->getMaxResolution();
        std::cout << "  Max resolution: " << maxRes.width << "x" << maxRes.height << "\n";

        // Test pixel size
        std::cout << "  Pixel size: " << camera->getPixelSize() << " microns\n";

        // Test bit depth
        std::cout << "  Bit depth: " << camera->getBitDepth() << " bits\n";

        // Test exposure (short exposure for demo)
        std::cout << "  Testing 1-second exposure...\n";
        if (camera->startExposure(1.0)) {
            // Monitor exposure progress
            while (camera->isExposing()) {
                auto progress = camera->getExposureProgress();
                auto remaining = camera->getExposureRemaining();
                std::cout << "    Progress: " << (progress * 100) << "%, Remaining: " << remaining << "s\r" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::cout << "\n    Exposure completed\n";

            // Get exposure result
            auto frame = camera->getExposureResult();
            if (frame && frame->data) {
                std::cout << "    Frame data received: " << frame->size << " bytes\n";
                std::cout << "    Resolution: " << frame->resolution.width << "x" << frame->resolution.height << "\n";
            }
        } else {
            std::cout << "    Failed to start exposure\n";
        }

        // Test temperature (if available)
        if (caps.hasCooler) {
            auto temp = camera->getTemperature();
            if (temp.has_value()) {
                std::cout << "  Current temperature: " << temp.value() << "°C\n";
            }
        }
    }

    void testQHYSpecificFeatures(std::shared_ptr<AtomCamera> camera) {
        std::cout << "Testing QHY-specific features:\n";

        // Note: In a real implementation, you would cast to QHYCamera
        // and access QHY-specific methods
        // auto qhyCamera = std::dynamic_pointer_cast<lithium::device::qhy::camera::QHYCamera>(camera);
        // if (qhyCamera) {
        //     std::cout << "  QHY SDK Version: " << qhyCamera->getQHYSDKVersion() << "\n";
        //     std::cout << "  Camera Model: " << qhyCamera->getCameraModel() << "\n";
        //     std::cout << "  Serial Number: " << qhyCamera->getSerialNumber() << "\n";
        // }

        std::cout << "  QHY-specific features would be tested here\n";
    }

    void testASISpecificFeatures(std::shared_ptr<AtomCamera> camera) {
        std::cout << "Testing ASI-specific features:\n";

        // Note: In a real implementation, you would cast to ASICamera
        // and access ASI-specific methods
        // auto asiCamera = std::dynamic_pointer_cast<lithium::device::asi::camera::ASICamera>(camera);
        // if (asiCamera) {
        //     std::cout << "  ASI SDK Version: " << asiCamera->getASISDKVersion() << "\n";
        //     std::cout << "  Camera Model: " << asiCamera->getCameraModel() << "\n";
        //     std::cout << "  USB Bandwidth: " << asiCamera->getUSBBandwidth() << "\n";
        // }

        std::cout << "  ASI-specific features would be tested here\n";
    }

    void demonstrateAdvancedFeatures() {
        std::cout << "\n=== Advanced Features Demo ===\n";

        // Create a simulator camera for reliable testing
        auto camera = createCamera(CameraDriverType::SIMULATOR, "Advanced Demo Camera");
        if (!camera) {
            std::cout << "Failed to create simulator camera\n";
            return;
        }

        if (!camera->initialize() || !camera->connect("CCD Simulator")) {
            std::cout << "Failed to initialize/connect simulator camera\n";
            return;
        }

        std::cout << "Testing advanced features with simulator camera:\n";

        // Test video streaming
        std::cout << "  Testing video streaming...\n";
        if (camera->startVideo()) {
            std::cout << "    Video started\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Get a few video frames
            for (int i = 0; i < 5; ++i) {
                auto frame = camera->getVideoFrame();
                if (frame) {
                    std::cout << "    Got video frame " << (i+1) << "\n";
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            camera->stopVideo();
            std::cout << "    Video stopped\n";
        } else {
            std::cout << "    Failed to start video\n";
        }

        // Test image sequence
        std::cout << "  Testing image sequence (3 frames, 0.5s exposure)...\n";
        if (camera->startSequence(3, 0.5, 0.1)) {
            while (camera->isSequenceRunning()) {
                auto progress = camera->getSequenceProgress();
                std::cout << "    Sequence progress: " << progress.first << "/" << progress.second << "\r" << std::flush;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            std::cout << "\n    Sequence completed\n";
        } else {
            std::cout << "    Failed to start sequence\n";
        }

        // Test statistics
        auto stats = camera->getFrameStatistics();
        std::cout << "  Frame statistics:\n";
        for (const auto& [key, value] : stats) {
            std::cout << "    " << key << ": " << value << "\n";
        }

        camera->disconnect();
        camera->destroy();
    }
};

int main() {
    // Initialize logging
    loguru::g_stderr_verbosity = loguru::Verbosity_INFO;

    try {
        CameraExample example;
        example.runExample();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in camera example: {}", e.what());
        return 1;
    }

    return 0;
}
