/*
 * device_event_bus.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device event bus for inter-component communication

**************************************************/

#ifndef LITHIUM_DEVICE_EVENTS_DEVICE_EVENT_BUS_HPP
#define LITHIUM_DEVICE_EVENTS_DEVICE_EVENT_BUS_HPP

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/async/message_bus.hpp"
#include "atom/type/json.hpp"

namespace lithium::device {

// ============================================================================
// Event Type Definitions
// ============================================================================

/**
 * @brief Device event categories
 */
enum class DeviceEventCategory {
    Device,     ///< Device-related events
    Plugin,     ///< Plugin-related events
    Backend,    ///< Backend-related events
    Component,  ///< Component integration events
    System      ///< System-level events
};

/**
 * @brief Device event types
 */
enum class DeviceEventType {
    // Device lifecycle
    DeviceAdded,          ///< New device added
    DeviceRemoved,        ///< Device removed
    DeviceConnected,      ///< Device connected
    DeviceDisconnected,   ///< Device disconnected
    DeviceStateChanged,   ///< Device state changed
    DevicePropertyChanged,///< Device property changed
    DeviceError,          ///< Device error occurred

    // Plugin events
    PluginLoaded,         ///< Plugin loaded
    PluginUnloaded,       ///< Plugin unloaded
    PluginReloading,      ///< Plugin hot-reload started
    PluginReloaded,       ///< Plugin hot-reload completed
    PluginError,          ///< Plugin error occurred

    // Backend events
    BackendConnected,     ///< Backend connected to server
    BackendDisconnected,  ///< Backend disconnected
    BackendDiscovery,     ///< Backend device discovery event
    BackendError,         ///< Backend error occurred

    // Component events
    ComponentRegistered,  ///< Device registered as component
    ComponentUnregistered,///< Device unregistered from components
    ComponentStateSync,   ///< Component state synchronized

    // System events
    SystemStartup,        ///< Device system startup
    SystemShutdown,       ///< Device system shutdown
    ConfigurationChanged  ///< Configuration changed
};

/**
 * @brief Convert event category to string
 */
[[nodiscard]] auto eventCategoryToString(DeviceEventCategory category)
    -> std::string;

/**
 * @brief Convert event type to string
 */
[[nodiscard]] auto eventTypeToString(DeviceEventType type) -> std::string;

/**
 * @brief Get event category from type
 */
[[nodiscard]] auto getEventCategory(DeviceEventType type)
    -> DeviceEventCategory;

// ============================================================================
// Event Data Structures
// ============================================================================

/**
 * @brief Base device event structure
 */
struct DeviceEvent {
    DeviceEventType type;
    DeviceEventCategory category;
    std::string source;                     ///< Event source (plugin/device name)
    std::string target;                     ///< Event target (if applicable)
    std::string message;                    ///< Human-readable message
    nlohmann::json data;                    ///< Additional event data
    std::chrono::system_clock::time_point timestamp;
    uint64_t sequenceNumber;                ///< Event sequence number

    DeviceEvent() : timestamp(std::chrono::system_clock::now()) {}

    [[nodiscard]] auto toJson() const -> nlohmann::json;
    static auto fromJson(const nlohmann::json& j) -> DeviceEvent;

    /**
     * @brief Get the message bus topic for this event
     */
    [[nodiscard]] auto getTopic() const -> std::string;
};

/**
 * @brief Device state change event
 */
struct DeviceStateChangeEvent : DeviceEvent {
    std::string deviceId;
    std::string deviceName;
    std::string deviceType;
    std::string oldState;
    std::string newState;

    DeviceStateChangeEvent() {
        type = DeviceEventType::DeviceStateChanged;
        category = DeviceEventCategory::Device;
    }

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Device property change event
 */
struct DevicePropertyChangeEvent : DeviceEvent {
    std::string deviceId;
    std::string propertyName;
    nlohmann::json oldValue;
    nlohmann::json newValue;

    DevicePropertyChangeEvent() {
        type = DeviceEventType::DevicePropertyChanged;
        category = DeviceEventCategory::Device;
    }

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Device error event
 */
struct DeviceErrorEvent : DeviceEvent {
    std::string deviceId;
    std::string errorCode;
    std::string errorMessage;
    bool recoverable{true};

    DeviceErrorEvent() {
        type = DeviceEventType::DeviceError;
        category = DeviceEventCategory::Device;
    }

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Plugin event
 */
struct PluginEvent : DeviceEvent {
    std::string pluginName;
    std::string pluginVersion;

    PluginEvent() {
        category = DeviceEventCategory::Plugin;
    }

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

/**
 * @brief Backend discovery event
 */
struct BackendDiscoveryEvent : DeviceEvent {
    std::string backendName;
    std::vector<std::string> discoveredDevices;
    size_t deviceCount{0};

    BackendDiscoveryEvent() {
        type = DeviceEventType::BackendDiscovery;
        category = DeviceEventCategory::Backend;
    }

    [[nodiscard]] auto toJson() const -> nlohmann::json;
};

// ============================================================================
// Event Callback Types
// ============================================================================

/**
 * @brief Generic event callback
 */
using DeviceEventCallback = std::function<void(const DeviceEvent&)>;

/**
 * @brief Typed event callback
 */
template <typename T>
using TypedEventCallback = std::function<void(const T&)>;

/**
 * @brief Event subscription handle
 */
using EventSubscriptionId = uint64_t;

// ============================================================================
// Device Event Bus
// ============================================================================

/**
 * @brief Device event bus for publishing and subscribing to device events
 *
 * This class provides a centralized event bus for device-related events.
 * It integrates with atom::async::MessageBus for asynchronous event delivery.
 *
 * Usage:
 * 1. Get the singleton instance
 * 2. Set the MessageBus instance (if using async delivery)
 * 3. Subscribe to events by type or category
 * 4. Publish events using publish() methods
 */
class DeviceEventBus {
public:
    /**
     * @brief Get singleton instance
     */
    static auto getInstance() -> DeviceEventBus&;

    // Disable copy
    DeviceEventBus(const DeviceEventBus&) = delete;
    DeviceEventBus& operator=(const DeviceEventBus&) = delete;

    // ==================== Initialization ====================

    /**
     * @brief Initialize the event bus
     * @param config Configuration options
     */
    void initialize(const nlohmann::json& config = {});

    /**
     * @brief Shutdown the event bus
     */
    void shutdown();

    /**
     * @brief Set the MessageBus for async event delivery
     * @param messageBus MessageBus instance
     */
    void setMessageBus(std::shared_ptr<atom::async::MessageBus> messageBus);

    /**
     * @brief Get the MessageBus
     */
    [[nodiscard]] auto getMessageBus() const
        -> std::shared_ptr<atom::async::MessageBus>;

    // ==================== Event Publishing ====================

    /**
     * @brief Publish an event
     * @param event Event to publish
     */
    void publish(const DeviceEvent& event);

    /**
     * @brief Publish a typed event
     * @tparam T Event type (must derive from DeviceEvent)
     * @param event Event to publish
     */
    template <typename T>
    void publish(const T& event);

    /**
     * @brief Publish an event asynchronously via MessageBus
     * @param event Event to publish
     */
    void publishAsync(const DeviceEvent& event);

    /**
     * @brief Publish an event with delay
     * @param event Event to publish
     * @param delayMs Delay in milliseconds
     */
    void publishDelayed(const DeviceEvent& event,
                        std::chrono::milliseconds delayMs);

    // ==================== Event Subscription ====================

    /**
     * @brief Subscribe to all events
     * @param callback Event callback
     * @return Subscription ID
     */
    auto subscribeAll(DeviceEventCallback callback) -> EventSubscriptionId;

    /**
     * @brief Subscribe to events by type
     * @param type Event type to subscribe to
     * @param callback Event callback
     * @return Subscription ID
     */
    auto subscribe(DeviceEventType type, DeviceEventCallback callback)
        -> EventSubscriptionId;

    /**
     * @brief Subscribe to events by category
     * @param category Event category to subscribe to
     * @param callback Event callback
     * @return Subscription ID
     */
    auto subscribeCategory(DeviceEventCategory category,
                           DeviceEventCallback callback)
        -> EventSubscriptionId;

    /**
     * @brief Subscribe to events from a specific source
     * @param source Source name (plugin/device)
     * @param callback Event callback
     * @return Subscription ID
     */
    auto subscribeSource(const std::string& source,
                         DeviceEventCallback callback)
        -> EventSubscriptionId;

    /**
     * @brief Subscribe to typed events
     * @tparam T Event type
     * @param callback Typed event callback
     * @return Subscription ID
     */
    template <typename T>
    auto subscribeTyped(TypedEventCallback<T> callback)
        -> EventSubscriptionId;

    /**
     * @brief Unsubscribe from events
     * @param subscriptionId Subscription ID
     */
    void unsubscribe(EventSubscriptionId subscriptionId);

    /**
     * @brief Unsubscribe all callbacks for an event type
     * @param type Event type
     */
    void unsubscribeAll(DeviceEventType type);

    /**
     * @brief Clear all subscriptions
     */
    void clearSubscriptions();

    // ==================== Event History ====================

    /**
     * @brief Get recent events
     * @param count Maximum number of events to return
     * @return Vector of recent events
     */
    [[nodiscard]] auto getRecentEvents(size_t count = 100) const
        -> std::vector<DeviceEvent>;

    /**
     * @brief Get events by type
     * @param type Event type
     * @param count Maximum number of events
     * @return Vector of events
     */
    [[nodiscard]] auto getEventsByType(DeviceEventType type,
                                        size_t count = 50) const
        -> std::vector<DeviceEvent>;

    /**
     * @brief Clear event history
     */
    void clearHistory();

    // ==================== Statistics ====================

    /**
     * @brief Get event statistics
     */
    [[nodiscard]] auto getStatistics() const -> nlohmann::json;

    /**
     * @brief Get subscription count
     */
    [[nodiscard]] auto getSubscriptionCount() const -> size_t;

    /**
     * @brief Get total published events count
     */
    [[nodiscard]] auto getPublishedCount() const -> uint64_t;

    // ==================== Helper Methods ====================

    /**
     * @brief Create a device state change event
     */
    static auto createStateChangeEvent(const std::string& deviceId,
                                        const std::string& deviceName,
                                        const std::string& deviceType,
                                        const std::string& oldState,
                                        const std::string& newState)
        -> DeviceStateChangeEvent;

    /**
     * @brief Create a device error event
     */
    static auto createErrorEvent(const std::string& deviceId,
                                  const std::string& errorCode,
                                  const std::string& errorMessage,
                                  bool recoverable = true)
        -> DeviceErrorEvent;

    /**
     * @brief Create a plugin event
     */
    static auto createPluginEvent(DeviceEventType type,
                                   const std::string& pluginName,
                                   const std::string& version,
                                   const std::string& message = {})
        -> PluginEvent;

private:
    DeviceEventBus();
    ~DeviceEventBus() = default;

    /**
     * @brief Internal subscription structure
     */
    struct Subscription {
        EventSubscriptionId id;
        DeviceEventCallback callback;
        std::optional<DeviceEventType> type;
        std::optional<DeviceEventCategory> category;
        std::optional<std::string> source;
    };

    /**
     * @brief Dispatch event to subscribers
     */
    void dispatchEvent(const DeviceEvent& event);

    /**
     * @brief Record event in history
     */
    void recordEvent(const DeviceEvent& event);

    /**
     * @brief Get next sequence number
     */
    uint64_t nextSequenceNumber();

    // Members
    mutable std::shared_mutex mutex_;
    std::shared_ptr<atom::async::MessageBus> messageBus_;
    bool initialized_{false};

    // Subscriptions
    std::vector<Subscription> subscriptions_;
    EventSubscriptionId nextSubscriptionId_{1};

    // Event history
    std::vector<DeviceEvent> eventHistory_;
    size_t maxHistorySize_{1000};

    // Statistics
    std::atomic<uint64_t> sequenceCounter_{0};
    std::atomic<uint64_t> publishedCount_{0};
    std::unordered_map<DeviceEventType, uint64_t> eventCounts_;

    // Configuration
    nlohmann::json config_;
    bool asyncEnabled_{true};
    bool historyEnabled_{true};
};

// ============================================================================
// Template Implementations
// ============================================================================

template <typename T>
void DeviceEventBus::publish(const T& event) {
    static_assert(std::is_base_of_v<DeviceEvent, T>,
                  "Event type must derive from DeviceEvent");
    publish(static_cast<const DeviceEvent&>(event));
}

template <typename T>
auto DeviceEventBus::subscribeTyped(TypedEventCallback<T> callback)
    -> EventSubscriptionId {
    static_assert(std::is_base_of_v<DeviceEvent, T>,
                  "Event type must derive from DeviceEvent");

    return subscribeAll([callback = std::move(callback)](const DeviceEvent& event) {
        // Try to cast to the specific type
        const T* typedEvent = dynamic_cast<const T*>(&event);
        if (typedEvent) {
            callback(*typedEvent);
        }
    });
}

// ============================================================================
// Event Topic Constants
// ============================================================================

namespace event_topics {
    constexpr const char* DEVICE_PREFIX = "device";
    constexpr const char* PLUGIN_PREFIX = "device.plugin";
    constexpr const char* BACKEND_PREFIX = "device.backend";
    constexpr const char* COMPONENT_PREFIX = "device.component";
    constexpr const char* SYSTEM_PREFIX = "device.system";

    // Specific topics
    constexpr const char* DEVICE_ADDED = "device.added";
    constexpr const char* DEVICE_REMOVED = "device.removed";
    constexpr const char* DEVICE_CONNECTED = "device.connected";
    constexpr const char* DEVICE_DISCONNECTED = "device.disconnected";
    constexpr const char* DEVICE_STATE = "device.state";
    constexpr const char* DEVICE_PROPERTY = "device.property";
    constexpr const char* DEVICE_ERROR = "device.error";

    constexpr const char* PLUGIN_LOADED = "device.plugin.loaded";
    constexpr const char* PLUGIN_UNLOADED = "device.plugin.unloaded";
    constexpr const char* PLUGIN_RELOADING = "device.plugin.reloading";
    constexpr const char* PLUGIN_RELOADED = "device.plugin.reloaded";
}  // namespace event_topics

}  // namespace lithium::device

#endif  // LITHIUM_DEVICE_EVENTS_DEVICE_EVENT_BUS_HPP
