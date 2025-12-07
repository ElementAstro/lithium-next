/*
 * device_event_bus.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device event bus implementation

**************************************************/

#include "device_event_bus.hpp"

#include <algorithm>

#include <loguru.hpp>

namespace lithium::device {

// ============================================================================
// String Conversion Functions
// ============================================================================

auto eventCategoryToString(DeviceEventCategory category) -> std::string {
    switch (category) {
        case DeviceEventCategory::Device:
            return "Device";
        case DeviceEventCategory::Plugin:
            return "Plugin";
        case DeviceEventCategory::Backend:
            return "Backend";
        case DeviceEventCategory::Component:
            return "Component";
        case DeviceEventCategory::System:
            return "System";
        default:
            return "Unknown";
    }
}

auto eventTypeToString(DeviceEventType type) -> std::string {
    switch (type) {
        case DeviceEventType::DeviceAdded:
            return "DeviceAdded";
        case DeviceEventType::DeviceRemoved:
            return "DeviceRemoved";
        case DeviceEventType::DeviceConnected:
            return "DeviceConnected";
        case DeviceEventType::DeviceDisconnected:
            return "DeviceDisconnected";
        case DeviceEventType::DeviceStateChanged:
            return "DeviceStateChanged";
        case DeviceEventType::DevicePropertyChanged:
            return "DevicePropertyChanged";
        case DeviceEventType::DeviceError:
            return "DeviceError";
        case DeviceEventType::PluginLoaded:
            return "PluginLoaded";
        case DeviceEventType::PluginUnloaded:
            return "PluginUnloaded";
        case DeviceEventType::PluginReloading:
            return "PluginReloading";
        case DeviceEventType::PluginReloaded:
            return "PluginReloaded";
        case DeviceEventType::PluginError:
            return "PluginError";
        case DeviceEventType::BackendConnected:
            return "BackendConnected";
        case DeviceEventType::BackendDisconnected:
            return "BackendDisconnected";
        case DeviceEventType::BackendDiscovery:
            return "BackendDiscovery";
        case DeviceEventType::BackendError:
            return "BackendError";
        case DeviceEventType::ComponentRegistered:
            return "ComponentRegistered";
        case DeviceEventType::ComponentUnregistered:
            return "ComponentUnregistered";
        case DeviceEventType::ComponentStateSync:
            return "ComponentStateSync";
        case DeviceEventType::SystemStartup:
            return "SystemStartup";
        case DeviceEventType::SystemShutdown:
            return "SystemShutdown";
        case DeviceEventType::ConfigurationChanged:
            return "ConfigurationChanged";
        default:
            return "Unknown";
    }
}

auto getEventCategory(DeviceEventType type) -> DeviceEventCategory {
    switch (type) {
        case DeviceEventType::DeviceAdded:
        case DeviceEventType::DeviceRemoved:
        case DeviceEventType::DeviceConnected:
        case DeviceEventType::DeviceDisconnected:
        case DeviceEventType::DeviceStateChanged:
        case DeviceEventType::DevicePropertyChanged:
        case DeviceEventType::DeviceError:
            return DeviceEventCategory::Device;

        case DeviceEventType::PluginLoaded:
        case DeviceEventType::PluginUnloaded:
        case DeviceEventType::PluginReloading:
        case DeviceEventType::PluginReloaded:
        case DeviceEventType::PluginError:
            return DeviceEventCategory::Plugin;

        case DeviceEventType::BackendConnected:
        case DeviceEventType::BackendDisconnected:
        case DeviceEventType::BackendDiscovery:
        case DeviceEventType::BackendError:
            return DeviceEventCategory::Backend;

        case DeviceEventType::ComponentRegistered:
        case DeviceEventType::ComponentUnregistered:
        case DeviceEventType::ComponentStateSync:
            return DeviceEventCategory::Component;

        case DeviceEventType::SystemStartup:
        case DeviceEventType::SystemShutdown:
        case DeviceEventType::ConfigurationChanged:
            return DeviceEventCategory::System;

        default:
            return DeviceEventCategory::System;
    }
}

// ============================================================================
// DeviceEvent Implementation
// ============================================================================

auto DeviceEvent::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["type"] = eventTypeToString(type);
    j["category"] = eventCategoryToString(category);
    j["source"] = source;
    j["target"] = target;
    j["message"] = message;
    j["data"] = data;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                         timestamp.time_since_epoch())
                         .count();
    j["sequenceNumber"] = sequenceNumber;
    return j;
}

auto DeviceEvent::fromJson(const nlohmann::json& j) -> DeviceEvent {
    DeviceEvent event;
    // Parse type and category from strings would need additional logic
    if (j.contains("source")) {
        event.source = j["source"].get<std::string>();
    }
    if (j.contains("target")) {
        event.target = j["target"].get<std::string>();
    }
    if (j.contains("message")) {
        event.message = j["message"].get<std::string>();
    }
    if (j.contains("data")) {
        event.data = j["data"];
    }
    if (j.contains("sequenceNumber")) {
        event.sequenceNumber = j["sequenceNumber"].get<uint64_t>();
    }
    return event;
}

auto DeviceEvent::getTopic() const -> std::string {
    switch (type) {
        case DeviceEventType::DeviceAdded:
            return event_topics::DEVICE_ADDED;
        case DeviceEventType::DeviceRemoved:
            return event_topics::DEVICE_REMOVED;
        case DeviceEventType::DeviceConnected:
            return event_topics::DEVICE_CONNECTED;
        case DeviceEventType::DeviceDisconnected:
            return event_topics::DEVICE_DISCONNECTED;
        case DeviceEventType::DeviceStateChanged:
            return event_topics::DEVICE_STATE;
        case DeviceEventType::DevicePropertyChanged:
            return event_topics::DEVICE_PROPERTY;
        case DeviceEventType::DeviceError:
            return event_topics::DEVICE_ERROR;
        case DeviceEventType::PluginLoaded:
            return event_topics::PLUGIN_LOADED;
        case DeviceEventType::PluginUnloaded:
            return event_topics::PLUGIN_UNLOADED;
        case DeviceEventType::PluginReloading:
            return event_topics::PLUGIN_RELOADING;
        case DeviceEventType::PluginReloaded:
            return event_topics::PLUGIN_RELOADED;
        default:
            return event_topics::DEVICE_PREFIX;
    }
}

// ============================================================================
// Typed Event Implementations
// ============================================================================

auto DeviceStateChangeEvent::toJson() const -> nlohmann::json {
    nlohmann::json j = DeviceEvent::toJson();
    j["deviceId"] = deviceId;
    j["deviceName"] = deviceName;
    j["deviceType"] = deviceType;
    j["oldState"] = oldState;
    j["newState"] = newState;
    return j;
}

auto DevicePropertyChangeEvent::toJson() const -> nlohmann::json {
    nlohmann::json j = DeviceEvent::toJson();
    j["deviceId"] = deviceId;
    j["propertyName"] = propertyName;
    j["oldValue"] = oldValue;
    j["newValue"] = newValue;
    return j;
}

auto DeviceErrorEvent::toJson() const -> nlohmann::json {
    nlohmann::json j = DeviceEvent::toJson();
    j["deviceId"] = deviceId;
    j["errorCode"] = errorCode;
    j["errorMessage"] = errorMessage;
    j["recoverable"] = recoverable;
    return j;
}

auto PluginEvent::toJson() const -> nlohmann::json {
    nlohmann::json j = DeviceEvent::toJson();
    j["pluginName"] = pluginName;
    j["pluginVersion"] = pluginVersion;
    return j;
}

auto BackendDiscoveryEvent::toJson() const -> nlohmann::json {
    nlohmann::json j = DeviceEvent::toJson();
    j["backendName"] = backendName;
    j["discoveredDevices"] = discoveredDevices;
    j["deviceCount"] = deviceCount;
    return j;
}

// ============================================================================
// DeviceEventBus Implementation
// ============================================================================

DeviceEventBus::DeviceEventBus() = default;

auto DeviceEventBus::getInstance() -> DeviceEventBus& {
    static DeviceEventBus instance;
    return instance;
}

void DeviceEventBus::initialize(const nlohmann::json& config) {
    std::unique_lock lock(mutex_);

    if (initialized_) {
        return;
    }

    config_ = config;

    // Parse configuration
    if (config.contains("maxHistorySize")) {
        maxHistorySize_ = config["maxHistorySize"].get<size_t>();
    }
    if (config.contains("asyncEnabled")) {
        asyncEnabled_ = config["asyncEnabled"].get<bool>();
    }
    if (config.contains("historyEnabled")) {
        historyEnabled_ = config["historyEnabled"].get<bool>();
    }

    initialized_ = true;
    LOG_F(INFO, "DeviceEventBus initialized");
}

void DeviceEventBus::shutdown() {
    std::unique_lock lock(mutex_);

    if (!initialized_) {
        return;
    }

    subscriptions_.clear();
    eventHistory_.clear();
    messageBus_.reset();

    initialized_ = false;
    LOG_F(INFO, "DeviceEventBus shutdown");
}

void DeviceEventBus::setMessageBus(
    std::shared_ptr<atom::async::MessageBus> messageBus) {
    std::unique_lock lock(mutex_);
    messageBus_ = std::move(messageBus);
}

auto DeviceEventBus::getMessageBus() const
    -> std::shared_ptr<atom::async::MessageBus> {
    std::shared_lock lock(mutex_);
    return messageBus_;
}

void DeviceEventBus::publish(const DeviceEvent& event) {
    // Create a mutable copy to set sequence number
    DeviceEvent eventCopy = event;
    eventCopy.sequenceNumber = nextSequenceNumber();
    eventCopy.category = getEventCategory(event.type);

    publishedCount_++;

    // Update event counts
    {
        std::unique_lock lock(mutex_);
        eventCounts_[event.type]++;
    }

    // Record in history
    if (historyEnabled_) {
        recordEvent(eventCopy);
    }

    // Dispatch to local subscribers
    dispatchEvent(eventCopy);

    // Publish to MessageBus if available
    if (asyncEnabled_ && messageBus_) {
        messageBus_->publish<DeviceEvent>(eventCopy.getTopic(), eventCopy);
    }

    LOG_F(1, "Published event: %s from %s",
          eventTypeToString(event.type).c_str(), event.source.c_str());
}

void DeviceEventBus::publishAsync(const DeviceEvent& event) {
    if (!messageBus_) {
        publish(event);
        return;
    }

    DeviceEvent eventCopy = event;
    eventCopy.sequenceNumber = nextSequenceNumber();
    eventCopy.category = getEventCategory(event.type);

    messageBus_->publish<DeviceEvent>(eventCopy.getTopic(), eventCopy);
}

void DeviceEventBus::publishDelayed(const DeviceEvent& event,
                                    std::chrono::milliseconds delayMs) {
    if (!messageBus_) {
        // Without MessageBus, just publish immediately
        publish(event);
        return;
    }

    DeviceEvent eventCopy = event;
    eventCopy.sequenceNumber = nextSequenceNumber();
    eventCopy.category = getEventCategory(event.type);

    messageBus_->publish<DeviceEvent>(eventCopy.getTopic(), eventCopy, delayMs);
}

auto DeviceEventBus::subscribeAll(DeviceEventCallback callback)
    -> EventSubscriptionId {
    std::unique_lock lock(mutex_);

    EventSubscriptionId id = nextSubscriptionId_++;
    subscriptions_.push_back(
        Subscription{id, std::move(callback), std::nullopt, std::nullopt, std::nullopt});

    return id;
}

auto DeviceEventBus::subscribe(DeviceEventType type,
                               DeviceEventCallback callback)
    -> EventSubscriptionId {
    std::unique_lock lock(mutex_);

    EventSubscriptionId id = nextSubscriptionId_++;
    subscriptions_.push_back(
        Subscription{id, std::move(callback), type, std::nullopt, std::nullopt});

    return id;
}

auto DeviceEventBus::subscribeCategory(DeviceEventCategory category,
                                       DeviceEventCallback callback)
    -> EventSubscriptionId {
    std::unique_lock lock(mutex_);

    EventSubscriptionId id = nextSubscriptionId_++;
    subscriptions_.push_back(
        Subscription{id, std::move(callback), std::nullopt, category, std::nullopt});

    return id;
}

auto DeviceEventBus::subscribeSource(const std::string& source,
                                     DeviceEventCallback callback)
    -> EventSubscriptionId {
    std::unique_lock lock(mutex_);

    EventSubscriptionId id = nextSubscriptionId_++;
    subscriptions_.push_back(
        Subscription{id, std::move(callback), std::nullopt, std::nullopt, source});

    return id;
}

void DeviceEventBus::unsubscribe(EventSubscriptionId subscriptionId) {
    std::unique_lock lock(mutex_);

    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [subscriptionId](const Subscription& sub) {
                           return sub.id == subscriptionId;
                       }),
        subscriptions_.end());
}

void DeviceEventBus::unsubscribeAll(DeviceEventType type) {
    std::unique_lock lock(mutex_);

    subscriptions_.erase(
        std::remove_if(subscriptions_.begin(), subscriptions_.end(),
                       [type](const Subscription& sub) {
                           return sub.type.has_value() && *sub.type == type;
                       }),
        subscriptions_.end());
}

void DeviceEventBus::clearSubscriptions() {
    std::unique_lock lock(mutex_);
    subscriptions_.clear();
}

auto DeviceEventBus::getRecentEvents(size_t count) const
    -> std::vector<DeviceEvent> {
    std::shared_lock lock(mutex_);

    if (eventHistory_.empty()) {
        return {};
    }

    size_t start = (eventHistory_.size() > count)
                       ? eventHistory_.size() - count
                       : 0;
    return std::vector<DeviceEvent>(eventHistory_.begin() + start,
                                    eventHistory_.end());
}

auto DeviceEventBus::getEventsByType(DeviceEventType type, size_t count) const
    -> std::vector<DeviceEvent> {
    std::shared_lock lock(mutex_);

    std::vector<DeviceEvent> result;
    for (auto it = eventHistory_.rbegin();
         it != eventHistory_.rend() && result.size() < count; ++it) {
        if (it->type == type) {
            result.push_back(*it);
        }
    }
    std::reverse(result.begin(), result.end());
    return result;
}

void DeviceEventBus::clearHistory() {
    std::unique_lock lock(mutex_);
    eventHistory_.clear();
}

auto DeviceEventBus::getStatistics() const -> nlohmann::json {
    std::shared_lock lock(mutex_);

    nlohmann::json stats;
    stats["publishedCount"] = publishedCount_.load();
    stats["subscriptionCount"] = subscriptions_.size();
    stats["historySize"] = eventHistory_.size();
    stats["maxHistorySize"] = maxHistorySize_;
    stats["asyncEnabled"] = asyncEnabled_;
    stats["historyEnabled"] = historyEnabled_;
    stats["hasMessageBus"] = (messageBus_ != nullptr);

    // Event counts by type
    nlohmann::json counts;
    for (const auto& [type, count] : eventCounts_) {
        counts[eventTypeToString(type)] = count;
    }
    stats["eventCounts"] = counts;

    return stats;
}

auto DeviceEventBus::getSubscriptionCount() const -> size_t {
    std::shared_lock lock(mutex_);
    return subscriptions_.size();
}

auto DeviceEventBus::getPublishedCount() const -> uint64_t {
    return publishedCount_.load();
}

auto DeviceEventBus::createStateChangeEvent(const std::string& deviceId,
                                             const std::string& deviceName,
                                             const std::string& deviceType,
                                             const std::string& oldState,
                                             const std::string& newState)
    -> DeviceStateChangeEvent {
    DeviceStateChangeEvent event;
    event.deviceId = deviceId;
    event.deviceName = deviceName;
    event.deviceType = deviceType;
    event.oldState = oldState;
    event.newState = newState;
    event.source = deviceId;
    event.message = "Device state changed from " + oldState + " to " + newState;
    return event;
}

auto DeviceEventBus::createErrorEvent(const std::string& deviceId,
                                       const std::string& errorCode,
                                       const std::string& errorMessage,
                                       bool recoverable) -> DeviceErrorEvent {
    DeviceErrorEvent event;
    event.deviceId = deviceId;
    event.errorCode = errorCode;
    event.errorMessage = errorMessage;
    event.recoverable = recoverable;
    event.source = deviceId;
    event.message = errorMessage;
    return event;
}

auto DeviceEventBus::createPluginEvent(DeviceEventType type,
                                        const std::string& pluginName,
                                        const std::string& version,
                                        const std::string& message)
    -> PluginEvent {
    PluginEvent event;
    event.type = type;
    event.pluginName = pluginName;
    event.pluginVersion = version;
    event.source = pluginName;
    event.message =
        message.empty() ? eventTypeToString(type) + ": " + pluginName : message;
    return event;
}

void DeviceEventBus::dispatchEvent(const DeviceEvent& event) {
    std::shared_lock lock(mutex_);

    for (const auto& sub : subscriptions_) {
        bool matches = true;

        // Check type filter
        if (sub.type.has_value() && *sub.type != event.type) {
            matches = false;
        }

        // Check category filter
        if (sub.category.has_value() && *sub.category != event.category) {
            matches = false;
        }

        // Check source filter
        if (sub.source.has_value() && *sub.source != event.source) {
            matches = false;
        }

        if (matches) {
            try {
                sub.callback(event);
            } catch (const std::exception& e) {
                LOG_F(WARNING, "Event callback exception: %s", e.what());
            } catch (...) {
                LOG_F(WARNING, "Event callback unknown exception");
            }
        }
    }
}

void DeviceEventBus::recordEvent(const DeviceEvent& event) {
    std::unique_lock lock(mutex_);

    eventHistory_.push_back(event);

    // Trim history if needed
    if (eventHistory_.size() > maxHistorySize_) {
        eventHistory_.erase(eventHistory_.begin(),
                            eventHistory_.begin() +
                                (eventHistory_.size() - maxHistorySize_));
    }
}

uint64_t DeviceEventBus::nextSequenceNumber() {
    return sequenceCounter_++;
}

}  // namespace lithium::device
