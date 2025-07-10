/*
 * main.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Filter Wheel Main Entry Point

*************************************************/

#include "controller.hpp"

#include <iostream>
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace lithium::device::ascom::filterwheel;

int main() {
    // Set up logging
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);

    try {
        // Create and initialize the controller
        auto controller = std::make_unique<ASCOMFilterwheelController>("ASCOM Test Filterwheel");
        
        if (!controller->initialize()) {
            spdlog::error("Failed to initialize ASCOM filterwheel controller");
            return -1;
        }

        // Scan for available devices
        spdlog::info("Scanning for ASCOM filterwheel devices...");
        auto devices = controller->scan();
        
        if (devices.empty()) {
            spdlog::warn("No ASCOM filterwheel devices found");
            // Try connecting to a default device for testing
            devices.push_back("http://localhost:11111/api/v1/filterwheel/0");
        }

        for (const auto& device : devices) {
            spdlog::info("Found device: {}", device);
        }

        // Connect to the first available device
        if (!devices.empty()) {
            const auto& device = devices[0];
            spdlog::info("Connecting to device: {}", device);

            if (controller->connect(device, 30, 3)) {
                spdlog::info("Successfully connected to {}", device);
                spdlog::info("Connection type: {}", controller->getConnectionType());
                spdlog::info("Connection status: {}", controller->getConnectionStatus());

                // Get device information
                auto driver_info = controller->getASCOMDriverInfo();
                if (driver_info) {
                    spdlog::info("Driver info: {}", *driver_info);
                }

                auto driver_version = controller->getASCOMVersion();
                if (driver_version) {
                    spdlog::info("Driver version: {}", *driver_version);
                }

                auto interface_version = controller->getASCOMInterfaceVersion();
                if (interface_version) {
                    spdlog::info("Interface version: {}", *interface_version);
                }

                // Get filter wheel information
                int filter_count = controller->getFilterCount();
                spdlog::info("Filter count: {}", filter_count);

                auto current_position = controller->getPosition();
                if (current_position) {
                    spdlog::info("Current position: {}", *current_position);
                    spdlog::info("Current filter: {}", controller->getCurrentFilterName());
                }

                // Get all filter names
                auto filter_names = controller->getAllSlotNames();
                spdlog::info("Filter names:");
                for (size_t i = 0; i < filter_names.size(); ++i) {
                    spdlog::info("  Slot {}: {}", i, filter_names[i]);
                }

                // Test movement (if we have multiple filters)
                if (filter_count > 1 && current_position) {
                    int target_position = (*current_position + 1) % filter_count;
                    spdlog::info("Moving to position: {}", target_position);

                    if (controller->setPosition(target_position)) {
                        spdlog::info("Move command sent successfully");

                        // Wait for movement to complete
                        int timeout = 30; // 30 seconds
                        while (controller->isMoving() && timeout > 0) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            timeout--;
                        }

                        auto new_position = controller->getPosition();
                        if (new_position) {
                            spdlog::info("New position: {}", *new_position);
                            spdlog::info("New filter: {}", controller->getCurrentFilterName());
                        }
                    } else {
                        spdlog::error("Failed to move to position {}", target_position);
                    }
                }

                // Test sequence functionality
                spdlog::info("Creating test sequence...");
                std::vector<int> sequence_positions = {0, 1, 2, 1, 0};
                if (controller->createSequence("test_sequence", sequence_positions, 2000)) {
                    spdlog::info("Test sequence created successfully");

                    if (controller->startSequence("test_sequence")) {
                        spdlog::info("Test sequence started");

                        // Monitor sequence progress
                        while (controller->isSequenceRunning()) {
                            double progress = controller->getSequenceProgress();
                            spdlog::info("Sequence progress: {:.1f}%", progress * 100.0);
                            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        }

                        spdlog::info("Test sequence completed");
                    } else {
                        spdlog::error("Failed to start test sequence");
                    }
                } else {
                    spdlog::error("Failed to create test sequence");
                }

                // Test calibration
                spdlog::info("Performing calibration...");
                if (controller->calibrateFilterWheel()) {
                    spdlog::info("Calibration completed successfully");
                } else {
                    spdlog::warn("Calibration failed or not supported");
                }

                // Get statistics
                uint64_t total_moves = controller->getTotalMoves();
                int last_move_time = controller->getLastMoveTime();
                spdlog::info("Total moves: {}", total_moves);
                spdlog::info("Last move time: {} ms", last_move_time);

                // Test temperature sensor (if available)
                if (controller->hasTemperatureSensor()) {
                    auto temperature = controller->getTemperature();
                    if (temperature) {
                        spdlog::info("Temperature: {:.1f}°C", *temperature);
                    }
                } else {
                    spdlog::info("No temperature sensor available");
                }

                // Disconnect
                spdlog::info("Disconnecting from device...");
                controller->disconnect();
                spdlog::info("Disconnected successfully");

            } else {
                spdlog::error("Failed to connect to device: {}", device);
                spdlog::error("Last error: {}", controller->getLastError());
            }
        }

        // Shutdown
        controller->destroy();
        spdlog::info("Controller shutdown completed");

    } catch (const std::exception& e) {
        spdlog::error("Exception occurred: {}", e.what());
        return -1;
    }

    spdlog::info("ASCOM filterwheel test completed successfully");
    return 0;
}
