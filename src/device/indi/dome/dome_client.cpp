/*
 * dome_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Client - Main Client Implementation

*************************************************/

#include "dome_client.hpp"

#include <spdlog/spdlog.h>
#include <string_view>
#include <thread>

INDIDomeClient::INDIDomeClient(std::string name) : AtomDome(std::move(name)) {
    initializeComponents();
}

INDIDomeClient::~INDIDomeClient() {
    if (monitoring_active_) {
        monitoring_active_ = false;
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
}

void INDIDomeClient::initializeComponents() {
    // Initialize all component managers
    motion_manager_ = std::make_shared<DomeMotionManager>(this);
    shutter_manager_ = std::make_shared<DomeShutterManager>(this);
    parking_manager_ = std::make_shared<DomeParkingManager>(this);
    weather_manager_ = std::make_shared<DomeWeatherManager>(this);
    telescope_manager_ = std::make_shared<DomeTelescopeManager>(this);
    home_manager_ = std::make_shared<DomeHomeManager>(this);

    // Set up component callbacks
    motion_manager_->setMotionCallback(
        [this](double currentAz, double targetAz, bool moving) {
            if (!moving) {
                spdlog::info("Dome motion completed at azimuth: {}", currentAz);
            }
        });

    shutter_manager_->setShutterCallback([this](ShutterState state) {
        switch (state) {
            case ShutterState::OPEN:
                spdlog::info("Dome shutter opened");
                break;
            case ShutterState::CLOSED:
                spdlog::info("Dome shutter closed");
                break;
            case ShutterState::OPENING:
            case ShutterState::CLOSING:
                spdlog::info("Dome shutter moving");
                break;
            default:
                break;
        }
    });

    parking_manager_->setParkingCallback([this](bool parked, bool parking) {
        if (parked) {
            spdlog::info("Dome parked successfully");
        } else if (parking) {
            spdlog::info("Dome parking in progress");
        } else {
            spdlog::info("Dome unparked");
        }
    });

    weather_manager_->setWeatherCallback(
        [this](bool safe, const std::string& details) {
            if (!safe) {
                spdlog::warn("Unsafe weather conditions detected: {}", details);

                // Auto-close if enabled
                if (weather_manager_->isAutoCloseEnabled()) {
                    spdlog::info("Auto-closing dome due to unsafe weather");
                    if (!shutter_manager_->closeShutter()) {
                        spdlog::warn("Failed to auto-close dome shutter");
                    }
                }
            } else {
                spdlog::info("Weather conditions are safe");
            }
        });

    telescope_manager_->setTelescopeCallback(
        [this](double telescopeAz, double telescopeAlt, double domeAz) {
            spdlog::debug("Telescope tracking: Tel({}°, {}°) -> Dome({}°)",
                          telescopeAz, telescopeAlt, domeAz);
        });

    home_manager_->setHomeCallback([this](bool homeFound, double homePosition) {
        if (homeFound) {
            spdlog::info("Home position found at azimuth: {}", homePosition);
        } else {
            spdlog::warn("Home position not found");
        }
    });
}

auto INDIDomeClient::initialize() -> bool {
    try {
        spdlog::info("Initializing INDI Dome Client");

        // Auto-home on startup if enabled
        if (home_manager_->isAutoHomeOnStartupEnabled()) {
            spdlog::info("Auto-home on startup enabled, finding home position");
            if (!home_manager_->findHome()) {
                spdlog::warn("Failed to find home position");
            }
        }

        spdlog::info("INDI Dome Client initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("Failed to initialize: {}", ex.what());
        return false;
    }
}

auto INDIDomeClient::destroy() -> bool {
    try {
        spdlog::info("Destroying INDI Dome Client");

        // Stop monitoring thread
        if (monitoring_active_) {
            monitoring_active_ = false;
            if (monitoring_thread_.joinable()) {
                monitoring_thread_.join();
            }
        }

        // Close shutter for safety
        if (shutter_manager_ &&
            shutter_manager_->getShutterState() == ShutterState::OPEN) {
            spdlog::info("Closing shutter for safety during shutdown");
            if (shutter_manager_->closeShutter()) {
                spdlog::info("Shutter closed successfully");
            } else {
                spdlog::warn("Failed to close shutter during shutdown");
            }
        }

        // Disconnect if connected
        if (connected_) {
            disconnect();
        }

        spdlog::info("INDI Dome Client destroyed successfully");
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("Failed to destroy: {}", ex.what());
        return false;
    }
}

auto INDIDomeClient::connect(const std::string& deviceName, int timeout,
                             int maxRetry) -> bool {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (connected_) {
        spdlog::warn("Already connected to INDI server");
        return true;
    }

    device_name_ = deviceName;

    spdlog::info("Connecting to INDI server: {}:{}", server_host_,
                 server_port_);

    // Connect to INDI server
    setServer(server_host_.c_str(), server_port_);

    int attempts = 0;
    while (attempts < maxRetry && !connected_) {
        try {
            connectServer();

            if (waitForConnection(timeout)) {
                spdlog::info("Connected to INDI server successfully");

                // Connect to device
                connectDevice(device_name_.c_str());
                if (device_connected_) {
                    spdlog::info("Connected to device: {}", device_name_);

                    // Start monitoring thread
                    monitoring_active_ = true;
                    monitoring_thread_ = std::thread(
                        &INDIDomeClient::monitoringThreadFunction, this);

                    // Synchronize with device
                    motion_manager_->synchronizeWithDevice();
                    shutter_manager_->synchronizeWithDevice();
                    parking_manager_->synchronizeWithDevice();
                    weather_manager_->synchronizeWithDevice();
                    telescope_manager_->synchronizeWithDevice();
                    home_manager_->synchronizeWithDevice();

                    return true;
                } else {
                    spdlog::error("Failed to connect to device: {}",
                                  device_name_);
                }
            }
        } catch (const std::exception& ex) {
            spdlog::error("Connection attempt failed: {}", ex.what());
        }

        attempts++;
        if (attempts < maxRetry) {
            spdlog::info("Retrying connection in 2 seconds... (attempt {}/{})",
                         attempts + 1, maxRetry);
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }

    spdlog::error("Failed to connect after {} attempts", maxRetry);
    return false;
}

auto INDIDomeClient::disconnect() -> bool {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (!connected_) {
        return true;
    }

    spdlog::info("Disconnecting from INDI server");

    // Stop monitoring thread
    if (monitoring_active_) {
        monitoring_active_ = false;
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }

    // Disconnect from server
    disconnectServer();

    connected_ = false;
    device_connected_ = false;

    spdlog::info("Disconnected from INDI server");
    return true;
}

auto INDIDomeClient::reconnect(int timeout, int maxRetry) -> bool {
    disconnect();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return connect(device_name_, timeout, maxRetry);
}

auto INDIDomeClient::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

    // This would typically scan for available INDI dome devices
    // For now, return empty vector
    spdlog::info("Scanning for INDI dome devices...");

    return devices;
}

auto INDIDomeClient::isConnected() const -> bool {
    return connected_ && device_connected_;
}

// INDI Client interface implementations
void INDIDomeClient::newDevice(INDI::BaseDevice device) {
    spdlog::info("New device discovered: {}", device.getDeviceName());

    if (device.getDeviceName() == device_name_) {
        base_device_ = device;
        device_connected_ = true;
        spdlog::info("Connected to target device: {}", device_name_);
    }
}

void INDIDomeClient::removeDevice(INDI::BaseDevice device) {
    spdlog::info("Device removed: {}", device.getDeviceName());

    if (device.getDeviceName() == device_name_) {
        device_connected_ = false;
        spdlog::warn("Target device disconnected: {}", device_name_);
    }
}

void INDIDomeClient::newProperty(INDI::Property property) {
    handleDomeProperty(property);
}

void INDIDomeClient::updateProperty(INDI::Property property) {
    handleDomeProperty(property);
}

void INDIDomeClient::removeProperty(INDI::Property property) {
    spdlog::info("Property removed: {}", property.getName());
}

void INDIDomeClient::newMessage(INDI::BaseDevice device, int messageID) {
    spdlog::info("New message from device: {} (ID: {})", device.getDeviceName(),
                 messageID);
}

void INDIDomeClient::serverConnected() {
    connected_ = true;
    spdlog::info("Server connected");
}

void INDIDomeClient::serverDisconnected(int exit_code) {
    connected_ = false;
    device_connected_ = false;
    spdlog::warn("Server disconnected with exit code: {}", exit_code);
}

void INDIDomeClient::monitoringThreadFunction() {
    spdlog::info("Monitoring thread started");

    while (monitoring_active_) {
        try {
            if (isConnected()) {
                updateFromDevice();
                weather_manager_->checkWeatherStatus();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& ex) {
            spdlog::error("Monitoring thread error: {}", ex.what());
        }
    }

    spdlog::info("Monitoring thread stopped");
}

auto INDIDomeClient::waitForConnection(int timeout) -> bool {
    auto start = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::seconds(timeout);

    while (!connected_ &&
           (std::chrono::steady_clock::now() - start) < timeoutDuration) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return connected_;
}

auto INDIDomeClient::waitForProperty(const std::string& propertyName,
                                     int timeout) -> bool {
    if (!isConnected()) {
        return false;
    }

    auto start = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::seconds(timeout);

    while ((std::chrono::steady_clock::now() - start) < timeoutDuration) {
        auto property = base_device_.getProperty(propertyName.c_str());
        if (property.isValid()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

void INDIDomeClient::updateFromDevice() {
    if (!isConnected()) {
        return;
    }

    // Update components from device properties
    motion_manager_->synchronizeWithDevice();
    shutter_manager_->synchronizeWithDevice();
    parking_manager_->synchronizeWithDevice();
    weather_manager_->synchronizeWithDevice();
    telescope_manager_->synchronizeWithDevice();
    home_manager_->synchronizeWithDevice();
}

void INDIDomeClient::handleDomeProperty(const INDI::Property& property) {
    std::string_view propertyName = property.getName();
    // Route property updates to appropriate component managers
    if (propertyName.starts_with("DOME_") ||
        propertyName.starts_with("ABS_DOME")) {
        motion_manager_->handleMotionProperty(property);
    }
    if (propertyName.find("SHUTTER") != std::string_view::npos ||
        propertyName.find("DOME_SHUTTER") != std::string_view::npos) {
        shutter_manager_->handleShutterProperty(property);
    }
    if (propertyName.find("PARK") != std::string_view::npos ||
        propertyName.find("DOME_PARK") != std::string_view::npos) {
        parking_manager_->handleParkingProperty(property);
    }
    if (propertyName.find("WEATHER") != std::string_view::npos ||
        propertyName.find("SAFETY") != std::string_view::npos) {
        weather_manager_->handleWeatherProperty(property);
    }
    if (propertyName.find("HOME") != std::string_view::npos ||
        propertyName.find("DOME_HOME") != std::string_view::npos) {
        home_manager_->handleHomeProperty(property);
    }
}
