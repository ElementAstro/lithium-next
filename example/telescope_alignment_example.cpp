/*
 * alignment_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Example usage of the ASCOM Telescope Alignment Manager

This example demonstrates how to use the alignment manager to:
- Set alignment modes
- Add alignment points
- Check alignment status
- Clear alignment data

*************************************************/

#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>

#include "components/alignment_manager.hpp"
#include "components/hardware_interface.hpp"

using namespace lithium::device::ascom::telescope::components;

int main() {
    try {
        // Initialize logging
        spdlog::set_level(spdlog::level::info);
        spdlog::info("Starting ASCOM Telescope Alignment Example");

        // Create IO context for async operations
        boost::asio::io_context io_context;

        // Create hardware interface
        auto hardware = std::make_shared<HardwareInterface>(io_context);
        
        // Initialize hardware interface
        if (!hardware->initialize()) {
            spdlog::error("Failed to initialize hardware interface");
            return -1;
        }

        // Create connection settings for ASCOM Alpaca
        HardwareInterface::ConnectionSettings settings;
        settings.type = ConnectionType::ALPACA_REST;
        settings.host = "localhost";
        settings.port = 11111;
        settings.deviceNumber = 0;
        settings.deviceName = "ASCOM.Simulator.Telescope";

        // Connect to telescope
        spdlog::info("Connecting to telescope...");
        if (!hardware->connect(settings)) {
            spdlog::error("Failed to connect to telescope: {}", hardware->getLastError());
            return -1;
        }

        // Create alignment manager
        auto alignmentManager = std::make_unique<AlignmentManager>(hardware);

        // Example 1: Check current alignment mode
        spdlog::info("=== Checking Current Alignment Mode ===");
        auto currentMode = alignmentManager->getAlignmentMode();
        switch (currentMode) {
            case ::AlignmentMode::EQ_NORTH_POLE:
                spdlog::info("Current alignment mode: Equatorial North Pole");
                break;
            case ::AlignmentMode::EQ_SOUTH_POLE:
                spdlog::info("Current alignment mode: Equatorial South Pole");
                break;
            case ::AlignmentMode::ALTAZ:
                spdlog::info("Current alignment mode: Alt-Az");
                break;
            case ::AlignmentMode::GERMAN_POLAR:
                spdlog::info("Current alignment mode: German Polar");
                break;
            case ::AlignmentMode::FORK:
                spdlog::info("Current alignment mode: Fork Mount");
                break;
            default:
                spdlog::warn("Unknown alignment mode");
                break;
        }

        // Example 2: Set alignment mode to German Polar
        spdlog::info("=== Setting Alignment Mode ===");
        if (alignmentManager->setAlignmentMode(::AlignmentMode::GERMAN_POLAR)) {
            spdlog::info("Successfully set alignment mode to German Polar");
        } else {
            spdlog::error("Failed to set alignment mode: {}", alignmentManager->getLastError());
        }

        // Example 3: Check alignment point count
        spdlog::info("=== Checking Alignment Point Count ===");
        int pointCount = alignmentManager->getAlignmentPointCount();
        if (pointCount >= 0) {
            spdlog::info("Current alignment points: {}", pointCount);
        } else {
            spdlog::error("Failed to get alignment point count: {}", alignmentManager->getLastError());
        }

        // Example 4: Add alignment points
        spdlog::info("=== Adding Alignment Points ===");
        
        // First alignment point: Vega (approximate coordinates)
        ::EquatorialCoordinates vega_target = {18.615, 38.784}; // RA: 18h 36m 56s, DEC: +38° 47' 01"
        ::EquatorialCoordinates vega_measured = {18.616, 38.785}; // Slightly offset measured position
        
        if (alignmentManager->addAlignmentPoint(vega_measured, vega_target)) {
            spdlog::info("Successfully added Vega alignment point");
        } else {
            spdlog::error("Failed to add Vega alignment point: {}", alignmentManager->getLastError());
        }

        // Second alignment point: Altair (approximate coordinates)
        ::EquatorialCoordinates altair_target = {19.846, 8.868}; // RA: 19h 50m 47s, DEC: +08° 52' 06"
        ::EquatorialCoordinates altair_measured = {19.847, 8.869}; // Slightly offset measured position
        
        if (alignmentManager->addAlignmentPoint(altair_measured, altair_target)) {
            spdlog::info("Successfully added Altair alignment point");
        } else {
            spdlog::error("Failed to add Altair alignment point: {}", alignmentManager->getLastError());
        }

        // Check point count after adding
        pointCount = alignmentManager->getAlignmentPointCount();
        if (pointCount >= 0) {
            spdlog::info("Alignment points after adding: {}", pointCount);
        }

        // Example 5: Test coordinate validation
        spdlog::info("=== Testing Coordinate Validation ===");
        
        // Invalid RA (negative)
        ::EquatorialCoordinates invalid_coords = {-1.0, 45.0};
        ::EquatorialCoordinates valid_coords = {12.0, 45.0};
        
        if (!alignmentManager->addAlignmentPoint(invalid_coords, valid_coords)) {
            spdlog::info("Correctly rejected invalid RA coordinate: {}", alignmentManager->getLastError());
        }

        // Invalid DEC (too high)
        invalid_coords = {12.0, 95.0};
        if (!alignmentManager->addAlignmentPoint(valid_coords, invalid_coords)) {
            spdlog::info("Correctly rejected invalid DEC coordinate: {}", alignmentManager->getLastError());
        }

        // Example 6: Clear alignment
        spdlog::info("=== Clearing Alignment ===");
        if (alignmentManager->clearAlignment()) {
            spdlog::info("Successfully cleared all alignment points");
            
            // Verify clearing worked
            pointCount = alignmentManager->getAlignmentPointCount();
            if (pointCount >= 0) {
                spdlog::info("Alignment points after clearing: {}", pointCount);
            }
        } else {
            spdlog::error("Failed to clear alignment: {}", alignmentManager->getLastError());
        }

        // Disconnect from telescope
        spdlog::info("=== Disconnecting ===");
        hardware->disconnect();
        hardware->shutdown();

        spdlog::info("Alignment example completed successfully");
        return 0;

    } catch (const std::exception& e) {
        spdlog::error("Exception in alignment example: {}", e.what());
        return -1;
    }
}
