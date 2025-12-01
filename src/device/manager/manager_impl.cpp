/*
 * manager_impl.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device Manager Implementation Details

**************************************************/

#include "manager_impl.hpp"

#include "atom/log/spdlog_logger.hpp"

#include <algorithm>
#include <cmath>

namespace lithium {

// ==================== DeviceManagerImpl ====================

DeviceManagerImpl::DeviceManagerImpl() {
    LOG_INFO("DeviceManagerImpl: Initialized");
}

DeviceManagerImpl::~DeviceManagerImpl() {
    stopHealthMonitorInternal();
    LOG_INFO("DeviceManagerImpl: Destroyed");
}

auto DeviceManagerImpl::findDeviceByName(const std::string& name) const
    -> std::shared_ptr<AtomDriver> {
    for (const auto& [type, deviceList] : devices) {
        for (const auto& device : deviceList) {
            if (device->getName() == name) {
                return device;
            }
        }
    }
    return nullptr;
}

auto DeviceManagerImpl::findDeviceType(const std::string& name) const
    -> std::string {
    for (const auto& [type, deviceList] : devices) {
        for (const auto& device : deviceList) {
            if (device->getName() == name) {
                return type;
            }
        }
    }
    return "";
}

void DeviceManagerImpl::emitEvent(const DeviceEvent& event) {
    // Store in pending events
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        pendingEvents.push_back(event);
        while (pendingEvents.size() > MAX_PENDING_EVENTS) {
            pendingEvents.pop_front();
        }
    }

    // Call legacy callback
    if (legacyEventCallback) {
        try {
            legacyEventCallback(event);
        } catch (const std::exception& e) {
            LOG_ERROR("DeviceManagerImpl: Legacy event callback error: {}",
                      e.what());
        }
    }

    // Call subscribed callbacks
    std::vector<EventSubscription> subs;
    {
        std::lock_guard<std::mutex> lock(eventMutex);
        subs = eventSubscriptions;
    }

    for (const auto& sub : subs) {
        // Check if subscription is for this event type
        if (!sub.eventTypes.empty() &&
            sub.eventTypes.find(static_cast<int>(event.type)) ==
                sub.eventTypes.end()) {
            continue;
        }
        try {
            sub.callback(event);
        } catch (const std::exception& e) {
            LOG_ERROR("DeviceManagerImpl: Event callback {} error: {}", sub.id,
                      e.what());
        }
    }
}

void DeviceManagerImpl::updateDeviceState(const std::string& name,
                                          bool connected) {
    auto& state = deviceStates[name];
    state.isConnected = connected;
    state.lastActivity = std::chrono::system_clock::now();
}

auto DeviceManagerImpl::getRetryConfig(const std::string& name) const
    -> DeviceRetryConfig {
    auto it = retryConfigs.find(name);
    if (it != retryConfigs.end()) {
        return it->second;
    }
    return DeviceRetryConfig{};  // Default config
}

auto DeviceManagerImpl::calculateRetryDelay(const DeviceRetryConfig& config,
                                            int attempt) const
    -> std::chrono::milliseconds {
    switch (config.strategy) {
        case DeviceRetryConfig::Strategy::None:
            return std::chrono::milliseconds(0);
        case DeviceRetryConfig::Strategy::Linear:
            return config.initialDelay;
        case DeviceRetryConfig::Strategy::Exponential: {
            auto delay = config.initialDelay.count() *
                         std::pow(config.multiplier, attempt);
            return std::chrono::milliseconds(
                std::min(static_cast<long long>(delay),
                         config.maxDelay.count()));
        }
    }
    return config.initialDelay;
}

void DeviceManagerImpl::startHealthMonitorInternal() {
    if (healthMonitorRunning.exchange(true)) {
        return;  // Already running
    }

    healthMonitorThread = std::thread([this]() { healthCheckLoop(); });
}

void DeviceManagerImpl::stopHealthMonitorInternal() {
    if (!healthMonitorRunning.exchange(false)) {
        return;  // Not running
    }

    healthMonitorCv.notify_all();
    if (healthMonitorThread.joinable()) {
        healthMonitorThread.join();
    }
}

void DeviceManagerImpl::healthCheckLoop() {
    LOG_INFO("DeviceManagerImpl: Health monitor started");

    while (healthMonitorRunning) {
        {
            std::unique_lock<std::mutex> lock(healthMonitorMutex);
            healthMonitorCv.wait_for(lock, healthCheckInterval,
                                     [this] { return !healthMonitorRunning; });
        }

        if (!healthMonitorRunning) {
            break;
        }

        // Check health of all devices
        std::shared_lock lock(mtx);
        for (const auto& [type, deviceList] : devices) {
            for (const auto& device : deviceList) {
                try {
                    bool connected = device->isConnected();
                    auto& state = deviceStates[device->getName()];

                    if (state.isConnected != connected) {
                        state.isConnected = connected;
                        state.lastActivity = std::chrono::system_clock::now();

                        // Emit state change event
                        DeviceEvent event;
                        event.type = connected ? DeviceEventType::Connected
                                               : DeviceEventType::Disconnected;
                        event.deviceName = device->getName();
                        event.deviceType = type;
                        event.timestamp = std::chrono::system_clock::now();
                        emitEvent(event);
                    }
                } catch (const std::exception& e) {
                    LOG_WARN(
                        "DeviceManagerImpl: Health check failed for {}: {}",
                        device->getName(), e.what());
                }
            }
        }
    }

    LOG_INFO("DeviceManagerImpl: Health monitor stopped");
}

}  // namespace lithium
