/*
 * example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Example usage of the modular INDI FilterWheel system

*************************************************/

#include "filterwheel.hpp"
#include <iostream>
#include <thread>
#include <chrono>

// Example 1: Basic filterwheel operations
void basicFilterwheelExample() {
    std::cout << "\n=== Basic FilterWheel Example ===\n";

    // Create filterwheel instance
    auto filterwheel = std::make_shared<INDIFilterwheel>("Example FilterWheel");

    // Initialize the device
    if (!filterwheel->initialize()) {
        std::cerr << "Failed to initialize filterwheel\n";
        return;
    }

    // Connect to device (replace with actual device name)
    if (!filterwheel->connect("ASI Filter Wheel", 5000, 3)) {
        std::cerr << "Failed to connect to filterwheel\n";
        return;
    }

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (filterwheel->isConnected()) {
        std::cout << "Successfully connected to filterwheel!\n";

        // Get current position
        auto position = filterwheel->getPosition();
        if (position) {
            std::cout << "Current position: " << *position << "\n";
        }

        // Get filter count
        int count = filterwheel->getFilterCount();
        std::cout << "Total filters: " << count << "\n";

        // Set filter position
        if (filterwheel->setPosition(2)) {
            std::cout << "Successfully moved to position 2\n";
        }

        // Get current filter name
        std::string filterName = filterwheel->getCurrentFilterName();
        std::cout << "Current filter: " << filterName << "\n";
    }

    // Disconnect
    filterwheel->disconnect();
    filterwheel->destroy();
}

// Example 2: Filter management operations
void filterManagementExample() {
    std::cout << "\n=== Filter Management Example ===\n";

    auto filterwheel = std::make_shared<INDIFilterwheel>("Filter Manager");
    filterwheel->initialize();

    // Set filter names
    filterwheel->setSlotName(0, "Luminance");
    filterwheel->setSlotName(1, "Red");
    filterwheel->setSlotName(2, "Green");
    filterwheel->setSlotName(3, "Blue");
    filterwheel->setSlotName(4, "Hydrogen Alpha");

    // Set detailed filter information
    FilterInfo lumaInfo;
    lumaInfo.name = "Luminance";
    lumaInfo.type = "L";
    lumaInfo.wavelength = 550.0;  // nm
    lumaInfo.bandwidth = 200.0;   // nm
    lumaInfo.description = "Broadband luminance filter";
    filterwheel->setFilterInfo(0, lumaInfo);

    FilterInfo haInfo;
    haInfo.name = "Hydrogen Alpha";
    haInfo.type = "Ha";
    haInfo.wavelength = 656.3;   // nm
    haInfo.bandwidth = 7.0;      // nm
    haInfo.description = "Narrowband hydrogen alpha filter";
    filterwheel->setFilterInfo(4, haInfo);

    // Get all slot names
    auto slotNames = filterwheel->getAllSlotNames();
    std::cout << "Filter slots:\n";
    for (size_t i = 0; i < slotNames.size(); ++i) {
        std::cout << "  " << i << ": " << slotNames[i] << "\n";
    }

    // Find filter by name
    auto lumaSlot = filterwheel->findFilterByName("Luminance");
    if (lumaSlot) {
        std::cout << "Luminance filter is in slot: " << *lumaSlot << "\n";
    }

    // Select filter by name
    if (filterwheel->selectFilterByName("Red")) {
        std::cout << "Successfully selected Red filter\n";
    }

    // Find filters by type
    auto narrowbandFilters = filterwheel->findFilterByType("Ha");
    std::cout << "Narrowband filters found: " << narrowbandFilters.size() << "\n";

    filterwheel->destroy();
}

// Example 3: Statistics and monitoring
void statisticsExample() {
    std::cout << "\n=== Statistics Example ===\n";

    auto filterwheel = std::make_shared<INDIFilterwheel>("Statistics Monitor");
    filterwheel->initialize();

    // Simulate some filter movements
    for (int i = 0; i < 5; ++i) {
        filterwheel->setPosition(i % 4);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Get statistics
    auto totalMoves = filterwheel->getTotalMoves();
    auto avgMoveTime = filterwheel->getAverageMoveTime();
    auto movesPerHour = filterwheel->getMovesPerHour();
    auto uptime = filterwheel->getUptimeSeconds();

    std::cout << "Statistics:\n";
    std::cout << "  Total moves: " << totalMoves << "\n";
    std::cout << "  Average move time: " << avgMoveTime << " ms\n";
    std::cout << "  Moves per hour: " << movesPerHour << "\n";
    std::cout << "  Uptime: " << uptime << " seconds\n";

    // Check temperature (if available)
    if (filterwheel->hasTemperatureSensor()) {
        auto temp = filterwheel->getTemperature();
        if (temp) {
            std::cout << "  Temperature: " << *temp << "°C\n";
        }
    } else {
        std::cout << "  Temperature sensor: Not available\n";
    }

    // Reset statistics
    filterwheel->resetTotalMoves();
    std::cout << "Statistics reset\n";

    filterwheel->destroy();
}

// Example 4: Configuration management
void configurationExample() {
    std::cout << "\n=== Configuration Example ===\n";

    auto filterwheel = std::make_shared<INDIFilterwheel>("Config Manager");
    filterwheel->initialize();

    // Set up a filter configuration
    filterwheel->setSlotName(0, "Clear");
    filterwheel->setSlotName(1, "R");
    filterwheel->setSlotName(2, "G");
    filterwheel->setSlotName(3, "B");

    // Save configuration
    if (filterwheel->saveFilterConfiguration("LRGB_Setup")) {
        std::cout << "Configuration saved as 'LRGB_Setup'\n";
    }

    // Change configuration
    filterwheel->setSlotName(0, "Luminance");
    filterwheel->setSlotName(1, "Ha");
    filterwheel->setSlotName(2, "OIII");
    filterwheel->setSlotName(3, "SII");

    // Save another configuration
    if (filterwheel->saveFilterConfiguration("Narrowband_Setup")) {
        std::cout << "Configuration saved as 'Narrowband_Setup'\n";
    }

    // List available configurations
    auto configs = filterwheel->getAvailableConfigurations();
    std::cout << "Available configurations:\n";
    for (const auto& config : configs) {
        std::cout << "  - " << config << "\n";
    }

    // Load a configuration
    if (filterwheel->loadFilterConfiguration("LRGB_Setup")) {
        std::cout << "Loaded 'LRGB_Setup' configuration\n";

        // Show loaded filter names
        auto names = filterwheel->getAllSlotNames();
        std::cout << "Loaded filters: ";
        for (size_t i = 0; i < names.size(); ++i) {
            std::cout << names[i];
            if (i < names.size() - 1) std::cout << ", ";
        }
        std::cout << "\n";
    }

    // Export configuration to file
    if (filterwheel->exportConfiguration("/tmp/my_filterwheel_config.cfg")) {
        std::cout << "Configuration exported to /tmp/my_filterwheel_config.cfg\n";
    }

    filterwheel->destroy();
}

// Example 5: Event callbacks
void callbackExample() {
    std::cout << "\n=== Callback Example ===\n";

    auto filterwheel = std::make_shared<INDIFilterwheel>("Callback Demo");
    filterwheel->initialize();

    // Set position change callback
    filterwheel->setPositionCallback([](int position, const std::string& filterName) {
        std::cout << "Position changed to: " << position << " (" << filterName << ")\n";
    });

    // Set move complete callback
    filterwheel->setMoveCompleteCallback([](bool success, const std::string& message) {
        if (success) {
            std::cout << "Move completed successfully: " << message << "\n";
        } else {
            std::cout << "Move failed: " << message << "\n";
        }
    });

    // Set temperature callback (if available)
    filterwheel->setTemperatureCallback([](double temperature) {
        std::cout << "Temperature update: " << temperature << "°C\n";
    });

    // Simulate some movements to trigger callbacks
    std::cout << "Simulating filter movements...\n";
    for (int i = 0; i < 3; ++i) {
        filterwheel->setPosition(i);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    filterwheel->destroy();
}

int main() {
    std::cout << "=== Modular INDI FilterWheel Examples ===\n";

    try {
        basicFilterwheelExample();
        filterManagementExample();
        statisticsExample();
        configurationExample();
        callbackExample();

        std::cout << "\n=== All examples completed successfully! ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
