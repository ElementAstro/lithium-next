/*
 * switch_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Switch Client - Main Client Implementation

*************************************************/

#include "switch_client.hpp"

#include <spdlog/spdlog.h>
#include <thread>

INDISwitchClient::INDISwitchClient(std::string name)
    : AtomSwitch(std::move(name)) {
    initializeComponents();
}

INDISwitchClient::~INDISwitchClient() {
    if (monitoring_active_) {
        monitoring_active_ = false;
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
}

void INDISwitchClient::initializeComponents() {
    // Initialize all component managers
    switch_manager_ = std::make_shared<SwitchManager>(this);
    timer_manager_ = std::make_shared<SwitchTimer>(this);
    power_manager_ = std::make_shared<SwitchPower>(this);
    safety_manager_ = std::make_shared<SwitchSafety>(this);
    stats_manager_ = std::make_shared<SwitchStats>(this);
    persistence_manager_ = std::make_shared<SwitchPersistence>(this);

    // Set up component callbacks
    timer_manager_->setTimerCallback([this](uint32_t switchIndex,
                                            bool expired) {
        if (expired) {
            // Timer expired, turn off switch
            bool ok =
                switch_manager_->setSwitchState(switchIndex, SwitchState::OFF);
            if (!ok) {
                spdlog::error("Failed to set switch {} to OFF on timer expiry",
                              switchIndex);
            }
            stats_manager_->stopSwitchUptime(switchIndex);
            spdlog::info("Timer expired for switch index: {}", switchIndex);
        }
    });

    power_manager_->setPowerCallback(
        [this](double totalPower, bool limitExceeded) {
            if (limitExceeded && safety_manager_->isSafetyModeEnabled()) {
                spdlog::warn(
                    "Power limit exceeded in safety mode, shutting down all "
                    "switches");
                bool ok = switch_manager_->setAllSwitches(SwitchState::OFF);
                if (!ok) {
                    spdlog::error(
                        "Failed to set all switches OFF due to power limit "
                        "exceeded");
                }
            }
        });

    safety_manager_->setEmergencyCallback([this](bool emergencyActive) {
        if (emergencyActive) {
            spdlog::critical(
                "Emergency stop activated - All switches turned OFF");
            bool ok = switch_manager_->setAllSwitches(SwitchState::OFF);
            if (!ok) {
                spdlog::error(
                    "Failed to set all switches OFF during emergency stop");
            }
        } else {
            spdlog::info("Emergency stop cleared");
        }
    });
}

auto INDISwitchClient::initialize() -> bool {
    try {
        spdlog::info("Initializing INDI Switch Client");

        // Load saved configuration
        if (!persistence_manager_->loadState()) {
            spdlog::warn("Failed to load saved state, using defaults");
        }

        // Start timer thread
        timer_manager_->startTimerThread();

        spdlog::info("INDI Switch Client initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("Failed to initialize: {}", ex.what());
        return false;
    }
}

auto INDISwitchClient::destroy() -> bool {
    try {
        spdlog::info("Destroying INDI Switch Client");

        // Save current state
        persistence_manager_->saveState();

        // Stop timer thread
        timer_manager_->stopTimerThread();

        // Disconnect if connected
        if (connected_) {
            disconnect();
        }

        spdlog::info("INDI Switch Client destroyed successfully");
        return true;
    } catch (const std::exception& ex) {
        spdlog::error("Failed to destroy: {}", ex.what());
        return false;
    }
}

auto INDISwitchClient::connect(const std::string& deviceName, int timeout,
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

                // Wait for device connection
                if (waitForProperty("CONNECTION", timeout)) {
                    spdlog::info("Connected to device: {}", device_name_);

                    // Start monitoring thread
                    monitoring_active_ = true;
                    monitoring_thread_ = std::thread(
                        &INDISwitchClient::monitoringThreadFunction, this);

                    // Synchronize with device
                    switch_manager_->synchronizeWithDevice();

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

auto INDISwitchClient::disconnect() -> bool {
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

auto INDISwitchClient::reconnect(int timeout, int maxRetry) -> bool {
    disconnect();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return connect(device_name_, timeout, maxRetry);
}

auto INDISwitchClient::scan() -> std::vector<std::string> {
    std::vector<std::string> devices;

    // This would typically scan for available INDI devices
    // For now, return empty vector
    spdlog::info("Scanning for INDI devices...");

    return devices;
}

auto INDISwitchClient::isConnected() const -> bool {
    return connected_ && device_connected_;
}

// INDI Client interface implementations
void INDISwitchClient::newDevice(INDI::BaseDevice device) {
    spdlog::info("New device discovered: {}", device.getDeviceName());

    if (device.getDeviceName() == device_name_) {
        base_device_ = device;
        device_connected_ = true;
        spdlog::info("Connected to target device: {}", device_name_);
    }
}

void INDISwitchClient::removeDevice(INDI::BaseDevice device) {
    spdlog::info("Device removed: {}", device.getDeviceName());

    if (device.getDeviceName() == device_name_) {
        device_connected_ = false;
        spdlog::warn("Target device disconnected: {}", device_name_);
    }
}

void INDISwitchClient::newProperty(INDI::Property property) {
    handleSwitchProperty(property);
}

void INDISwitchClient::updateProperty(INDI::Property property) {
    handleSwitchProperty(property);
}

void INDISwitchClient::removeProperty(INDI::Property property) {
    spdlog::info("Property removed: {}", property.getName());
}

void INDISwitchClient::newMessage(INDI::BaseDevice device, int messageID) {
    spdlog::info("New message from device: {} (ID: {})", device.getDeviceName(),
                 messageID);
}

void INDISwitchClient::serverConnected() {
    connected_ = true;
    spdlog::info("Server connected");
}

void INDISwitchClient::serverDisconnected(int exit_code) {
    connected_ = false;
    device_connected_ = false;
    spdlog::warn("Server disconnected with exit code: {}", exit_code);
}

void INDISwitchClient::monitoringThreadFunction() {
    spdlog::info("Monitoring thread started");

    while (monitoring_active_) {
        try {
            if (isConnected()) {
                updateFromDevice();
                power_manager_->updatePowerConsumption();
                safety_manager_->performSafetyChecks();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& ex) {
            spdlog::error("Monitoring thread error: {}", ex.what());
        }
    }

    spdlog::info("Monitoring thread stopped");
}

auto INDISwitchClient::waitForConnection(int timeout) -> bool {
    auto start = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::seconds(timeout);

    while (!connected_ &&
           (std::chrono::steady_clock::now() - start) < timeoutDuration) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return connected_;
}

auto INDISwitchClient::waitForProperty(const std::string& propertyName,
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

void INDISwitchClient::updateFromDevice() {
    if (!isConnected()) {
        return;
    }

    // Update switch states from device properties
    switch_manager_->synchronizeWithDevice();
}

void INDISwitchClient::handleSwitchProperty(const INDI::Property& property) {
    if (property.getType() == INDI_SWITCH) {
        switch_manager_->handleSwitchProperty(property);
    }
}
