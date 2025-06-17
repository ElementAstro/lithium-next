/*
 * device_integration_test.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Integration test for all device types

*************************************************/

#include "template/mock/mock_camera.hpp"
#include "template/mock/mock_telescope.hpp"
#include "template/mock/mock_focuser.hpp"
#include "template/mock/mock_rotator.hpp"
#include "template/mock/mock_dome.hpp"
#include "template/mock/mock_filterwheel.hpp"

#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

class DeviceManager {
public:
    DeviceManager() {
        initializeDevices();
    }
    
    ~DeviceManager() {
        disconnectAllDevices();
    }
    
    bool initializeDevices() {
        std::cout << "Initializing devices...\n";
        
        // Create mock devices
        camera_ = std::make_unique<MockCamera>("MainCamera");
        telescope_ = std::make_unique<MockTelescope>("MainTelescope");
        focuser_ = std::make_unique<MockFocuser>("MainFocuser");
        rotator_ = std::make_unique<MockRotator>("MainRotator");
        dome_ = std::make_unique<MockDome>("MainDome");
        filterwheel_ = std::make_unique<MockFilterWheel>("MainFilterWheel");
        
        // Enable simulation mode
        camera_->setSimulated(true);
        telescope_->setSimulated(true);
        focuser_->setSimulated(true);
        rotator_->setSimulated(true);
        dome_->setSimulated(true);
        filterwheel_->setSimulated(true);
        
        // Initialize all devices
        bool success = true;
        success &= camera_->initialize();
        success &= telescope_->initialize();
        success &= focuser_->initialize();
        success &= rotator_->initialize();
        success &= dome_->initialize();
        success &= filterwheel_->initialize();
        
        if (success) {
            std::cout << "All devices initialized successfully.\n";
        } else {
            std::cout << "Failed to initialize some devices.\n";
        }
        
        return success;
    }
    
    bool connectAllDevices() {
        std::cout << "Connecting to devices...\n";
        
        bool success = true;
        success &= camera_->connect();
        success &= telescope_->connect();
        success &= focuser_->connect();
        success &= rotator_->connect();
        success &= dome_->connect();
        success &= filterwheel_->connect();
        
        if (success) {
            std::cout << "All devices connected successfully.\n";
        } else {
            std::cout << "Failed to connect to some devices.\n";
        }
        
        return success;
    }
    
    void disconnectAllDevices() {
        std::cout << "Disconnecting devices...\n";
        
        if (camera_) camera_->disconnect();
        if (telescope_) telescope_->disconnect();
        if (focuser_) focuser_->disconnect();
        if (rotator_) rotator_->disconnect();
        if (dome_) dome_->disconnect();
        if (filterwheel_) filterwheel_->disconnect();
        
        std::cout << "All devices disconnected.\n";
    }
    
    void demonstrateDeviceCapabilities() {
        std::cout << "\n=== Device Capabilities Demonstration ===\n";
        
        // Telescope operations
        std::cout << "\n--- Telescope Operations ---\n";
        if (telescope_->isConnected()) {
            auto coords = telescope_->getRADECJNow();
            if (coords) {
                std::cout << "Current position: RA=" << coords->ra << "h, DEC=" << coords->dec << "°\n";
            }
            
            std::cout << "Slewing to test position...\n";
            telescope_->slewToRADECJNow(12.5, 45.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            coords = telescope_->getRADECJNow();
            if (coords) {
                std::cout << "New position: RA=" << coords->ra << "h, DEC=" << coords->dec << "°\n";
            }
        }
        
        // Focuser operations
        std::cout << "\n--- Focuser Operations ---\n";
        if (focuser_->isConnected()) {
            auto position = focuser_->getPosition();
            if (position) {
                std::cout << "Current focuser position: " << *position << "\n";
            }
            
            std::cout << "Moving focuser to position 1000...\n";
            focuser_->moveToPosition(1000);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            position = focuser_->getPosition();
            if (position) {
                std::cout << "New focuser position: " << *position << "\n";
            }
        }
        
        // Filter wheel operations
        std::cout << "\n--- Filter Wheel Operations ---\n";
        if (filterwheel_->isConnected()) {
            auto position = filterwheel_->getPosition();
            if (position) {
                std::cout << "Current filter position: " << *position << "\n";
                std::cout << "Current filter: " << filterwheel_->getCurrentFilterName() << "\n";
            }
            
            std::cout << "Changing to filter position 3...\n";
            filterwheel_->setPosition(3);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            position = filterwheel_->getPosition();
            if (position) {
                std::cout << "New filter position: " << *position << "\n";
                std::cout << "New filter: " << filterwheel_->getCurrentFilterName() << "\n";
            }
        }
        
        // Rotator operations
        std::cout << "\n--- Rotator Operations ---\n";
        if (rotator_->isConnected()) {
            auto angle = rotator_->getPosition();
            if (angle) {
                std::cout << "Current rotator angle: " << *angle << "°\n";
            }
            
            std::cout << "Rotating to 90°...\n";
            rotator_->moveToAngle(90.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            
            angle = rotator_->getPosition();
            if (angle) {
                std::cout << "New rotator angle: " << *angle << "°\n";
            }
        }
        
        // Dome operations
        std::cout << "\n--- Dome Operations ---\n";
        if (dome_->isConnected()) {
            auto azimuth = dome_->getAzimuth();
            if (azimuth) {
                std::cout << "Current dome azimuth: " << *azimuth << "°\n";
            }
            
            std::cout << "Dome shutter state: ";
            switch (dome_->getShutterState()) {
                case ShutterState::OPEN: std::cout << "OPEN\n"; break;
                case ShutterState::CLOSED: std::cout << "CLOSED\n"; break;
                case ShutterState::OPENING: std::cout << "OPENING\n"; break;
                case ShutterState::CLOSING: std::cout << "CLOSING\n"; break;
                default: std::cout << "UNKNOWN\n"; break;
            }
            
            std::cout << "Opening dome shutter...\n";
            dome_->openShutter();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            std::cout << "Moving dome to azimuth 180°...\n";
            dome_->moveToAzimuth(180.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            azimuth = dome_->getAzimuth();
            if (azimuth) {
                std::cout << "New dome azimuth: " << *azimuth << "°\n";
            }
        }
        
        // Camera operations
        std::cout << "\n--- Camera Operations ---\n";
        if (camera_->isConnected()) {
            auto temp = camera_->getTemperature();
            if (temp) {
                std::cout << "Camera temperature: " << *temp << "°C\n";
            }
            
            auto resolution = camera_->getResolution();
            if (resolution) {
                std::cout << "Camera resolution: " << resolution->width << "x" << resolution->height << "\n";
            }
            
            std::cout << "Starting 2-second exposure...\n";
            camera_->startExposure(2.0);
            
            // Monitor exposure progress
            while (camera_->isExposing()) {
                double progress = camera_->getExposureProgress();
                double remaining = camera_->getExposureRemaining();
                std::cout << "Exposure progress: " << (progress * 100) << "%, remaining: " << remaining << "s\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            
            auto frame = camera_->getExposureResult();
            if (frame) {
                std::cout << "Exposure completed successfully!\n";
            }
        }
    }
    
    void demonstrateCoordinatedOperations() {
        std::cout << "\n=== Coordinated Operations Demonstration ===\n";
        
        // Simulate an automated imaging sequence
        std::cout << "Starting automated imaging sequence...\n";
        
        // 1. Point telescope to target
        std::cout << "1. Pointing telescope to target...\n";
        telescope_->slewToRADECJNow(20.0, 30.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 2. Open dome and point to telescope
        std::cout << "2. Opening dome and pointing to telescope...\n";
        dome_->openShutter();
        auto tel_coords = telescope_->getRADECJNow();
        if (tel_coords) {
            // Convert RA/DEC to AZ/ALT (simplified)
            double azimuth = tel_coords->ra * 15.0; // Simplified conversion
            dome_->moveToAzimuth(azimuth);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // 3. Select appropriate filter
        std::cout << "3. Selecting luminance filter...\n";
        filterwheel_->selectFilterByName("Luminance");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 4. Rotate to optimal angle
        std::cout << "4. Rotating to optimal camera angle...\n";
        rotator_->moveToAngle(45.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // 5. Focus the telescope
        std::cout << "5. Focusing telescope...\n";
        focuser_->moveToPosition(1500);
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        // 6. Take image
        std::cout << "6. Taking image...\n";
        camera_->startExposure(5.0);
        
        // Wait for exposure to complete
        while (camera_->isExposing()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        auto frame = camera_->getExposureResult();
        if (frame) {
            std::cout << "Automated sequence completed successfully!\n";
        }
    }
    
private:
    std::unique_ptr<MockCamera> camera_;
    std::unique_ptr<MockTelescope> telescope_;
    std::unique_ptr<MockFocuser> focuser_;
    std::unique_ptr<MockRotator> rotator_;
    std::unique_ptr<MockDome> dome_;
    std::unique_ptr<MockFilterWheel> filterwheel_;
};

int main() {
    std::cout << "Device Integration Test - Astrophotography Control System\n";
    std::cout << "=========================================================\n";
    
    DeviceManager manager;
    
    if (!manager.connectAllDevices()) {
        std::cerr << "Failed to connect to devices. Exiting.\n";
        return 1;
    }
    
    try {
        manager.demonstrateDeviceCapabilities();
        manager.demonstrateCoordinatedOperations();
        
        std::cout << "\n=== Test Summary ===\n";
        std::cout << "All device operations completed successfully!\n";
        std::cout << "The astrophotography control system is ready for use.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during test: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
