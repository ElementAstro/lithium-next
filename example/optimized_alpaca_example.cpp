/*
 * optimized_alpaca_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-31

Description: Example usage of the Optimized ASCOM Alpaca Client
Demonstrates modern C++20 features and performance optimizations

**************************************************/

#include "optimized_alpaca_client.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <format>

using namespace lithium::device::ascom;

// Example: High-performance camera control with coroutines
boost::asio::awaitable<void> camera_imaging_session() {
    boost::asio::io_context ioc;
    
    // Create optimized camera client with custom configuration
    OptimizedAlpacaClient::Config config;
    config.max_connections = 5;
    config.enable_compression = true;
    config.timeout = std::chrono::seconds(30);
    
    CameraClient camera(ioc, config);
    
    try {
        // Discover devices on network
        std::cout << "Discovering Alpaca devices...\n";
        auto devices = co_await camera.discover_devices("192.168.1.0/24");
        
        if (devices.empty()) {
            std::cout << "No devices found!\n";
            co_return;
        }
        
        // Connect to first camera device
        auto camera_device = std::ranges::find_if(devices, [](const DeviceInfo& dev) {
            return dev.type == DeviceType::Camera;
        });
        
        if (camera_device == devices.end()) {
            std::cout << "No camera found!\n";
            co_return;
        }
        
        std::cout << std::format("Connecting to camera: {}\n", camera_device->name);
        co_await camera.connect(*camera_device);
        
        // Check camera status
        auto temperature = co_await camera.get_ccd_temperature();
        if (temperature) {
            std::cout << std::format("Camera temperature: {:.2f}°C\n", *temperature);
        }
        
        auto cooler_on = co_await camera.get_cooler_on();
        if (cooler_on && !*cooler_on) {
            std::cout << "Turning on cooler...\n";
            co_await camera.set_cooler_on(true);
        }
        
        // Take exposure
        std::cout << "Starting 5-second exposure...\n";
        co_await camera.start_exposure(5.0, true);
        
        // Wait for exposure to complete
        bool image_ready = false;
        while (!image_ready) {
            co_await boost::asio::steady_timer(ioc, std::chrono::seconds(1)).async_wait(boost::asio::use_awaitable);
            auto ready_result = co_await camera.get_image_ready();
            if (ready_result) {
                image_ready = *ready_result;
            }
        }
        
        // Download image with high performance
        std::cout << "Downloading image...\n";
        auto start_time = std::chrono::steady_clock::now();
        
        auto image_data = co_await camera.get_image_array_uint16();
        if (image_data) {
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            std::cout << std::format("Downloaded {} pixels in {}ms\n", 
                                   image_data->size(), duration.count());
            
            // Process image data here...
        }
        
        // Display statistics
        const auto& stats = camera.get_stats();
        std::cout << std::format("Session statistics:\n");
        std::cout << std::format("  Requests sent: {}\n", stats.requests_sent.load());
        std::cout << std::format("  Success rate: {:.1f}%\n", 
                                (100.0 * stats.requests_successful.load()) / stats.requests_sent.load());
        std::cout << std::format("  Average response time: {}ms\n", stats.average_response_time_ms.load());
        std::cout << std::format("  Connections reused: {}\n", stats.connections_reused.load());
        
    } catch (const std::exception& e) {
        std::cerr << std::format("Error: {}\n", e.what());
    }
}

// Example: Telescope control with error handling
boost::asio::awaitable<void> telescope_control_session() {
    boost::asio::io_context ioc;
    TelescopeClient telescope(ioc);
    
    try {
        // Connect to telescope (assuming device info is known)
        DeviceInfo telescope_device{
            .name = "Simulator Telescope",
            .type = DeviceType::Telescope,
            .number = 0,
            .host = "localhost",
            .port = 11111
        };
        
        co_await telescope.connect(telescope_device);
        
        // Get current position
        auto ra_result = co_await telescope.get_right_ascension();
        auto dec_result = co_await telescope.get_declination();
        
        if (ra_result && dec_result) {
            std::cout << std::format("Current position: RA={:.6f}h, Dec={:.6f}°\n", 
                                   *ra_result, *dec_result);
        }
        
        // Check if telescope is parked
        auto slewing = co_await telescope.get_slewing();
        if (slewing && !*slewing) {
            std::cout << "Slewing to target...\n";
            co_await telescope.slew_to_coordinates(12.5, 45.0); // Example coordinates
            
            // Wait for slew to complete
            bool is_slewing = true;
            while (is_slewing) {
                co_await boost::asio::steady_timer(ioc, std::chrono::seconds(1)).async_wait(boost::asio::use_awaitable);
                auto slew_result = co_await telescope.get_slewing();
                if (slew_result) {
                    is_slewing = *slew_result;
                }
            }
            
            std::cout << "Slew completed!\n";
        }
        
    } catch (const std::exception& e) {
        std::cerr << std::format("Telescope error: {}\n", e.what());
    }
}

// Example: Parallel device operations
boost::asio::awaitable<void> parallel_device_operations() {
    boost::asio::io_context ioc;
    
    // Create multiple clients
    CameraClient camera(ioc);
    TelescopeClient telescope(ioc);
    FocuserClient focuser(ioc);
    
    // Example device infos (would normally come from discovery)
    std::vector<DeviceInfo> devices = {
        {.name = "Camera", .type = DeviceType::Camera, .number = 0, .host = "192.168.1.100", .port = 11111},
        {.name = "Telescope", .type = DeviceType::Telescope, .number = 0, .host = "192.168.1.101", .port = 11111},
        {.name = "Focuser", .type = DeviceType::Focuser, .number = 0, .host = "192.168.1.102", .port = 11111}
    };
    
    try {
        // Connect to all devices in parallel
        auto camera_connect = camera.connect(devices[0]);
        auto telescope_connect = telescope.connect(devices[1]);
        auto focuser_connect = focuser.connect(devices[2]);
        
        // Wait for all connections
        co_await camera_connect;
        co_await telescope_connect;
        co_await focuser_connect;
        
        std::cout << "All devices connected!\n";
        
        // Perform parallel operations
        auto camera_temp = camera.get_ccd_temperature();
        auto telescope_ra = telescope.get_right_ascension();
        auto focuser_pos = focuser.get_property<int>("position");
        
        // Wait for all results
        if (auto temp = co_await camera_temp) {
            std::cout << std::format("Camera temperature: {:.2f}°C\n", *temp);
        }
        
        if (auto ra = co_await telescope_ra) {
            std::cout << std::format("Telescope RA: {:.6f}h\n", *ra);
        }
        
        if (auto pos = co_await focuser_pos) {
            std::cout << std::format("Focuser position: {}\n", *pos);
        }
        
    } catch (const std::exception& e) {
        std::cerr << std::format("Parallel operations error: {}\n", e.what());
    }
}

int main() {
    boost::asio::io_context ioc;
    
    std::cout << "=== Optimized ASCOM Alpaca Client Demo ===\n\n";
    
    // Run camera imaging session
    boost::asio::co_spawn(ioc, camera_imaging_session(), boost::asio::detached);
    
    // Run telescope control session
    boost::asio::co_spawn(ioc, telescope_control_session(), boost::asio::detached);
    
    // Run parallel operations example
    boost::asio::co_spawn(ioc, parallel_device_operations(), boost::asio::detached);
    
    // Start the event loop
    ioc.run();
    
    std::cout << "\n=== Demo completed ===\n";
    return 0;
}
