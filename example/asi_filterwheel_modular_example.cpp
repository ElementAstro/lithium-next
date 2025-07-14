/*
 * asi_filterwheel_modular_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Example demonstrating the modular ASI Filterwheel Controller V2

This example shows how to use the new modular architecture for ASI filterwheel
control, including basic operations, profile management, sequences, monitoring,
and calibration.

*************************************************/

#include "controller/asi_filterwheel_controller_v2.hpp"
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>
#include <chrono>

using namespace lithium::device::asi::filterwheel;

class FilterwheelExample {
public:
    FilterwheelExample() {
        // Initialize logging
        loguru::g_stderr_verbosity = loguru::Verbosity_INFO;
        loguru::init_mutex();

        // Create controller
        controller_ = std::make_unique<ASIFilterwheelControllerV2>();
    }

    bool initialize() {
        std::cout << "=== Initializing ASI Filterwheel Controller V2 ===" << std::endl;

        if (!controller_->initialize()) {
            std::cerr << "Failed to initialize controller: " << controller_->getLastError() << std::endl;
            return false;
        }

        std::cout << "Controller initialized successfully!" << std::endl;
        std::cout << "Device info: " << controller_->getDeviceInfo() << std::endl;
        std::cout << "Controller version: " << controller_->getVersion() << std::endl;
        std::cout << "Number of slots: " << controller_->getSlotCount() << std::endl;
        std::cout << "Current position: " << controller_->getCurrentPosition() << std::endl;

        return true;
    }

    void demonstrateBasicOperations() {
        std::cout << "\n=== Basic Operations Demo ===" << std::endl;

        // Test movement to different positions
        std::vector<int> test_positions = {0, 2, 1, 3};

        for (int pos : test_positions) {
            std::cout << "Moving to position " << pos << "..." << std::endl;

            if (controller_->moveToPosition(pos)) {
                // Wait for movement to complete
                if (controller_->waitForMovement(10000)) { // 10 second timeout
                    std::cout << "Successfully moved to position " << controller_->getCurrentPosition() << std::endl;
                } else {
                    std::cout << "Movement timeout!" << std::endl;
                }
            } else {
                std::cout << "Failed to start movement: " << controller_->getLastError() << std::endl;
            }

            // Small delay between movements
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    void demonstrateProfileManagement() {
        std::cout << "\n=== Profile Management Demo ===" << std::endl;

        // Create LRGB profile
        std::cout << "Creating LRGB profile..." << std::endl;
        if (controller_->createProfile("LRGB", "Standard LRGB filter set")) {
            std::cout << "LRGB profile created successfully" << std::endl;
        }

        // Configure filters with names and focus offsets
        controller_->setFilterName(0, "Luminance");
        controller_->setFocusOffset(0, 0.0);

        controller_->setFilterName(1, "Red");
        controller_->setFocusOffset(1, -15.2);

        controller_->setFilterName(2, "Green");
        controller_->setFocusOffset(2, -8.7);

        controller_->setFilterName(3, "Blue");
        controller_->setFocusOffset(3, 12.3);

        std::cout << "Filter configuration:" << std::endl;
        auto filter_names = controller_->getFilterNames();
        for (size_t i = 0; i < filter_names.size(); ++i) {
            std::cout << "  Slot " << i << ": " << filter_names[i]
                     << " (offset: " << controller_->getFocusOffset(static_cast<int>(i)) << ")" << std::endl;
        }

        // Create Narrowband profile
        std::cout << "\nCreating Narrowband profile..." << std::endl;
        if (controller_->createProfile("Narrowband", "Ha-OIII-SII narrowband filters")) {
            controller_->setCurrentProfile("Narrowband");

            controller_->setFilterName(0, "Ha 7nm");
            controller_->setFocusOffset(0, -5.8);

            controller_->setFilterName(1, "OIII 8.5nm");
            controller_->setFocusOffset(1, 3.2);

            controller_->setFilterName(2, "SII 8nm");
            controller_->setFocusOffset(2, -2.1);

            std::cout << "Narrowband profile configured" << std::endl;
        }

        // List all profiles
        std::cout << "Available profiles:" << std::endl;
        auto profiles = controller_->getProfiles();
        for (const auto& profile : profiles) {
            std::cout << "  - " << profile << std::endl;
        }

        // Switch back to LRGB
        controller_->setCurrentProfile("LRGB");
        std::cout << "Current profile: " << controller_->getCurrentProfile() << std::endl;
    }

    void demonstrateSequenceControl() {
        std::cout << "\n=== Sequence Control Demo ===" << std::endl;

        // Set up sequence callback
        controller_->setSequenceCallback([](const std::string& event, int step, int position) {
            std::cout << "Sequence event: " << event << " (Step " << step << ", Position " << position << ")" << std::endl;
        });

        // Create LRGB sequence
        std::vector<int> lrgb_sequence = {0, 1, 2, 3}; // L-R-G-B
        if (controller_->createSequence("LRGB_sequence", lrgb_sequence, 2000)) { // 2s dwell time
            std::cout << "LRGB sequence created" << std::endl;
        }

        // Start the sequence
        std::cout << "Starting LRGB sequence..." << std::endl;
        if (controller_->startSequence("LRGB_sequence")) {
            // Monitor sequence progress
            while (controller_->isSequenceRunning()) {
                double progress = controller_->getSequenceProgress();
                std::cout << "Sequence progress: " << std::fixed << std::setprecision(1)
                         << (progress * 100.0) << "%" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            std::cout << "Sequence completed!" << std::endl;
        } else {
            std::cout << "Failed to start sequence: " << controller_->getLastError() << std::endl;
        }

        // Demonstrate sequence pause/resume
        std::vector<int> test_sequence = {0, 1, 2, 3, 2, 1, 0}; // Back and forth
        if (controller_->createSequence("test_sequence", test_sequence, 1500)) {
            std::cout << "\nStarting test sequence (will pause/resume)..." << std::endl;

            controller_->startSequence("test_sequence");

            // Let it run for a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));

            // Pause
            std::cout << "Pausing sequence..." << std::endl;
            controller_->pauseSequence();

            std::this_thread::sleep_for(std::chrono::milliseconds(2000));

            // Resume
            std::cout << "Resuming sequence..." << std::endl;
            controller_->resumeSequence();

            // Wait for completion
            while (controller_->isSequenceRunning()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            std::cout << "Test sequence completed!" << std::endl;
        }
    }

    void demonstrateHealthMonitoring() {
        std::cout << "\n=== Health Monitoring Demo ===" << std::endl;

        // Set up health callback
        controller_->setHealthCallback([](const std::string& status, bool is_healthy) {
            std::cout << "Health update: " << status << " [" << (is_healthy ? "HEALTHY" : "UNHEALTHY") << "]" << std::endl;
        });

        // Start health monitoring
        std::cout << "Starting health monitoring..." << std::endl;
        controller_->startHealthMonitoring(3000); // Check every 3 seconds

        // Perform some operations to generate monitoring data
        std::cout << "Performing operations for monitoring..." << std::endl;
        for (int i = 0; i < 5; ++i) {
            int target_pos = i % controller_->getSlotCount();
            controller_->moveToPosition(target_pos);
            controller_->waitForMovement(5000);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }

        // Display health metrics
        std::cout << "\nCurrent health metrics:" << std::endl;
        std::cout << "  Overall health: " << (controller_->isHealthy() ? "HEALTHY" : "UNHEALTHY") << std::endl;
        std::cout << "  Success rate: " << std::fixed << std::setprecision(1)
                 << controller_->getSuccessRate() << "%" << std::endl;
        std::cout << "  Consecutive failures: " << controller_->getConsecutiveFailures() << std::endl;

        // Get detailed health status
        std::cout << "\nDetailed health status:" << std::endl;
        std::cout << controller_->getHealthStatus() << std::endl;

        // Let monitoring run for a bit longer
        std::cout << "Monitoring for 10 more seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));

        // Stop monitoring
        controller_->stopHealthMonitoring();
        std::cout << "Health monitoring stopped" << std::endl;
    }

    void demonstrateCalibrationAndTesting() {
        std::cout << "\n=== Calibration and Testing Demo ===" << std::endl;

        // Check if we have valid calibration
        if (controller_->hasValidCalibration()) {
            std::cout << "Valid calibration found" << std::endl;
        } else {
            std::cout << "No valid calibration found" << std::endl;
        }

        std::cout << "Current calibration status: " << controller_->getCalibrationStatus() << std::endl;

        // Perform self-test
        std::cout << "\nPerforming self-test..." << std::endl;
        if (controller_->performSelfTest()) {
            std::cout << "Self-test PASSED" << std::endl;
        } else {
            std::cout << "Self-test FAILED" << std::endl;
        }

        // Test individual positions
        std::cout << "\nTesting individual positions..." << std::endl;
        int slot_count = controller_->getSlotCount();
        for (int pos = 0; pos < std::min(slot_count, 4); ++pos) {
            std::cout << "Testing position " << pos << "... ";
            if (controller_->testPosition(pos)) {
                std::cout << "PASS" << std::endl;
            } else {
                std::cout << "FAIL" << std::endl;
            }
        }

        // Note: Full calibration can take a long time, so we skip it in this demo
        std::cout << "\nFull calibration skipped in demo (can take several minutes)" << std::endl;
        std::cout << "Use controller->performCalibration() for full calibration" << std::endl;
    }

    void demonstrateAdvancedFeatures() {
        std::cout << "\n=== Advanced Features Demo ===" << std::endl;

        // Access individual components
        auto monitoring = controller_->getMonitoringSystem();
        if (monitoring) {
            std::cout << "Accessing monitoring system directly..." << std::endl;

            // Get operation statistics
            auto stats = monitoring->getOverallStatistics();
            std::cout << "Operation statistics:" << std::endl;
            std::cout << "  Total operations: " << stats.total_operations << std::endl;
            std::cout << "  Successful operations: " << stats.successful_operations << std::endl;
            std::cout << "  Failed operations: " << stats.failed_operations << std::endl;
            if (stats.total_operations > 0) {
                std::cout << "  Average operation time: " << stats.average_operation_time.count() << " ms" << std::endl;
            }

            // Export operation history (optional - creates file)
            // monitoring->exportOperationHistory("filterwheel_operations.csv");
            // std::cout << "Operation history exported to filterwheel_operations.csv" << std::endl;
        }

        auto calibration = controller_->getCalibrationSystem();
        if (calibration) {
            std::cout << "\nAccessing calibration system directly..." << std::endl;

            // Run diagnostic tests
            auto diagnostic_results = calibration->runAllDiagnostics();
            std::cout << "Diagnostic results:" << std::endl;
            for (const auto& result : diagnostic_results) {
                std::cout << "  " << result << std::endl;
            }
        }

        // Configuration persistence
        std::cout << "\nSaving configuration..." << std::endl;
        if (controller_->saveConfiguration()) {
            std::cout << "Configuration saved successfully" << std::endl;
        } else {
            std::cout << "Failed to save configuration: " << controller_->getLastError() << std::endl;
        }
    }

    void shutdown() {
        std::cout << "\n=== Shutting Down ===" << std::endl;

        // Clear all callbacks
        controller_->clearCallbacks();

        // Shutdown controller
        if (controller_->shutdown()) {
            std::cout << "Controller shut down successfully" << std::endl;
        } else {
            std::cout << "Error during shutdown: " << controller_->getLastError() << std::endl;
        }
    }

private:
    std::unique_ptr<ASIFilterwheelControllerV2> controller_;
};

int main() {
    std::cout << "ASI Filterwheel Modular Architecture Demo" << std::endl;
    std::cout << "=========================================" << std::endl;

    try {
        FilterwheelExample example;

        // Initialize
        if (!example.initialize()) {
            std::cerr << "Failed to initialize example" << std::endl;
            return 1;
        }

        // Run demonstrations
        example.demonstrateBasicOperations();
        example.demonstrateProfileManagement();
        example.demonstrateSequenceControl();
        example.demonstrateHealthMonitoring();
        example.demonstrateCalibrationAndTesting();
        example.demonstrateAdvancedFeatures();

        // Shutdown
        example.shutdown();

        std::cout << "\nDemo completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Exception in demo: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
