/*
 * indigo_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Client - Wrapper for libindigo client functionality

**************************************************/

#ifndef LITHIUM_CLIENT_INDIGO_CLIENT_HPP
#define LITHIUM_CLIENT_INDIGO_CLIENT_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "atom/type/json.hpp"
#include "device/common/device_result.hpp"

// Platform check for INDIGO availability
#if defined(__linux__) || defined(__APPLE__)
#define INDIGO_PLATFORM_SUPPORTED 1
#else
#define INDIGO_PLATFORM_SUPPORTED 0
#endif

namespace lithium::client::indigo {

using json = nlohmann::json;
using namespace lithium::device;

// Forward declarations
class INDIGODeviceBase;

// ============================================================================
// INDIGO Property Types (compatible with INDI)
// ============================================================================

/**
 * @brief Property state enumeration
 */
enum class PropertyState : uint8_t {
    Idle,    ///< Values uninitialized
    Ok,      ///< Values valid
    Busy,    ///< Operation in progress
    Alert,   ///< Error state
    Unknown
};

/**
 * @brief Property type enumeration
 */
enum class PropertyType : uint8_t {
    Text,    ///< Text vector
    Number,  ///< Number vector
    Switch,  ///< Switch vector
    Light,   ///< Light vector (read-only status)
    Blob,    ///< Binary data
    Unknown
};

/**
 * @brief Property permission
 */
enum class PropertyPermission : uint8_t {
    ReadOnly,   ///< Read-only property
    WriteOnly,  ///< Write-only property
    ReadWrite   ///< Read-write property
};

/**
 * @brief Switch rule for switch properties
 */
enum class SwitchRule : uint8_t {
    OneOfMany,   ///< Only one switch can be on
    AtMostOne,   ///< Zero or one switch can be on
    AnyOfMany    ///< Any number of switches can be on
};

/**
 * @brief INDIGO device interface flags (bitmask)
 */
enum class DeviceInterface : uint32_t {
    None = 0,
    General = (1 << 0),
    CCD = (1 << 1),
    Guider = (1 << 2),
    Focuser = (1 << 3),
    FilterWheel = (1 << 4),
    Dome = (1 << 5),
    GPS = (1 << 6),
    Weather = (1 << 7),
    AO = (1 << 8),           // Adaptive Optics
    Dustcap = (1 << 9),
    Lightbox = (1 << 10),
    Detector = (1 << 11),
    Rotator = (1 << 12),
    Spectrograph = (1 << 13),
    Correlator = (1 << 14),
    AuxInterface = (1 << 15),
    Mount = (1 << 16),
    Wheel = FilterWheel,     // Alias
    Telescope = Mount        // Alias
};

inline DeviceInterface operator|(DeviceInterface a, DeviceInterface b) {
    return static_cast<DeviceInterface>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline DeviceInterface operator&(DeviceInterface a, DeviceInterface b) {
    return static_cast<DeviceInterface>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// ============================================================================
// Property Elements
// ============================================================================

/**
 * @brief Text property element
 */
struct TextElement {
    std::string name;
    std::string label;
    std::string value;
};

/**
 * @brief Number property element
 */
struct NumberElement {
    std::string name;
    std::string label;
    double value{0.0};
    double min{0.0};
    double max{0.0};
    double step{0.0};
    std::string format;  // printf format
    double target{0.0};  // Target value (INDIGO specific)
};

/**
 * @brief Switch property element
 */
struct SwitchElement {
    std::string name;
    std::string label;
    bool value{false};
};

/**
 * @brief Light property element
 */
struct LightElement {
    std::string name;
    std::string label;
    PropertyState state{PropertyState::Idle};
};

/**
 * @brief BLOB property element
 */
struct BlobElement {
    std::string name;
    std::string label;
    std::string format;
    std::vector<uint8_t> data;
    std::string url;  // INDIGO URL mode for efficient transfer
    size_t size{0};
};

// ============================================================================
// Property Structures
// ============================================================================

/**
 * @brief Generic property structure
 */
struct Property {
    std::string device;
    std::string name;
    std::string group;
    std::string label;
    PropertyType type{PropertyType::Unknown};
    PropertyState state{PropertyState::Idle};
    PropertyPermission permission{PropertyPermission::ReadOnly};

    // Type-specific data
    std::vector<TextElement> textElements;
    std::vector<NumberElement> numberElements;
    std::vector<SwitchElement> switchElements;
    std::vector<LightElement> lightElements;
    std::vector<BlobElement> blobElements;
    SwitchRule switchRule{SwitchRule::OneOfMany};

    /**
     * @brief Convert property to JSON
     */
    [[nodiscard]] auto toJson() const -> json;

    /**
     * @brief Create property from JSON
     */
    static auto fromJson(const json& j) -> Property;
};

// ============================================================================
// Discovered Device Information
// ============================================================================

/**
 * @brief Discovered device information
 */
struct DiscoveredDevice {
    std::string name;
    std::string driver;
    DeviceInterface interfaces{DeviceInterface::None};
    bool connected{false};
    std::string version;
    json metadata;

    [[nodiscard]] auto toJson() const -> json;
};

// ============================================================================
// Callback Types
// ============================================================================

/// Device attach/detach callback
using DeviceCallback = std::function<void(const std::string& deviceName, bool attached)>;

/// Property defined/deleted callback
using PropertyDefineCallback = std::function<void(const Property& property, bool defined)>;

/// Property update callback
using PropertyUpdateCallback = std::function<void(const Property& property)>;

/// Connection status callback
using ConnectionCallback = std::function<void(bool connected, const std::string& message)>;

/// Message callback
using MessageCallback = std::function<void(const std::string& device, const std::string& message)>;

// ============================================================================
// INDIGO Client Class
// ============================================================================

/**
 * @brief INDIGO Client - Wrapper for libindigo functionality
 *
 * This class provides a C++ interface to INDIGO servers, supporting:
 * - Asynchronous connection management
 * - Device discovery and enumeration
 * - Property get/set operations
 * - BLOB URL mode for efficient image transfer
 * - Callback-based event handling
 *
 * @note This implementation uses libindigo and is only available on Linux/macOS.
 *       On Windows, the class is a stub that returns errors.
 */
class INDIGOClient {
public:
    /**
     * @brief Connection configuration
     */
    struct Config {
        std::string host{"localhost"};
        int port{7624};
        std::chrono::seconds connectTimeout{10};
        std::chrono::seconds commandTimeout{30};
        bool enableBlobUrl{true};  // Use INDIGO URL mode for BLOBs
        bool autoReconnect{false};
        std::chrono::seconds reconnectInterval{5};
    };

    /**
     * @brief Connection state
     */
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
        Error
    };

    /**
     * @brief Default constructor
     */
    INDIGOClient();

    /**
     * @brief Constructor with configuration
     */
    explicit INDIGOClient(const Config& config);

    /**
     * @brief Destructor - ensures disconnection
     */
    ~INDIGOClient();

    // Non-copyable
    INDIGOClient(const INDIGOClient&) = delete;
    INDIGOClient& operator=(const INDIGOClient&) = delete;

    // Movable
    INDIGOClient(INDIGOClient&&) noexcept;
    INDIGOClient& operator=(INDIGOClient&&) noexcept;

    // ==================== Connection Management ====================

    /**
     * @brief Connect to INDIGO server
     * @param host Server hostname (default from config)
     * @param port Server port (default from config)
     * @return Success or error
     */
    auto connect(const std::string& host = "", int port = 0) -> DeviceResult<bool>;

    /**
     * @brief Disconnect from server
     * @return Success or error
     */
    auto disconnect() -> DeviceResult<bool>;

    /**
     * @brief Check if connected
     */
    [[nodiscard]] auto isConnected() const -> bool;

    /**
     * @brief Get connection state
     */
    [[nodiscard]] auto getConnectionState() const -> ConnectionState;

    /**
     * @brief Get last error message
     */
    [[nodiscard]] auto getLastError() const -> std::string;

    // ==================== Device Discovery ====================

    /**
     * @brief Discover all devices on server
     * @return List of discovered devices
     */
    auto discoverDevices() -> DeviceResult<std::vector<DiscoveredDevice>>;

    /**
     * @brief Get cached discovered devices
     */
    [[nodiscard]] auto getDiscoveredDevices() const -> std::vector<DiscoveredDevice>;

    /**
     * @brief Connect to a specific device
     * @param deviceName Device name
     * @return Success or error
     */
    auto connectDevice(const std::string& deviceName) -> DeviceResult<bool>;

    /**
     * @brief Disconnect from a specific device
     * @param deviceName Device name
     * @return Success or error
     */
    auto disconnectDevice(const std::string& deviceName) -> DeviceResult<bool>;

    // ==================== Property Operations ====================

    /**
     * @brief Get a property from device
     * @param deviceName Device name
     * @param propertyName Property name
     * @return Property or error
     */
    auto getProperty(const std::string& deviceName, const std::string& propertyName)
        -> DeviceResult<Property>;

    /**
     * @brief Get all properties from device
     * @param deviceName Device name
     * @return List of properties
     */
    auto getDeviceProperties(const std::string& deviceName)
        -> DeviceResult<std::vector<Property>>;

    /**
     * @brief Set text property
     * @param deviceName Device name
     * @param propertyName Property name
     * @param elements Element name-value pairs
     * @return Success or error
     */
    auto setTextProperty(const std::string& deviceName, const std::string& propertyName,
                         const std::vector<std::pair<std::string, std::string>>& elements)
        -> DeviceResult<bool>;

    /**
     * @brief Set number property
     * @param deviceName Device name
     * @param propertyName Property name
     * @param elements Element name-value pairs
     * @return Success or error
     */
    auto setNumberProperty(const std::string& deviceName, const std::string& propertyName,
                           const std::vector<std::pair<std::string, double>>& elements)
        -> DeviceResult<bool>;

    /**
     * @brief Set switch property
     * @param deviceName Device name
     * @param propertyName Property name
     * @param elements Element name-value pairs
     * @return Success or error
     */
    auto setSwitchProperty(const std::string& deviceName, const std::string& propertyName,
                           const std::vector<std::pair<std::string, bool>>& elements)
        -> DeviceResult<bool>;

    // ==================== BLOB Operations ====================

    /**
     * @brief Enable/disable BLOB transfer for a device
     * @param deviceName Device name (empty for all devices)
     * @param enable Enable or disable
     * @param urlMode Use URL mode (INDIGO efficient transfer)
     * @return Success or error
     */
    auto enableBlob(const std::string& deviceName, bool enable, bool urlMode = true)
        -> DeviceResult<bool>;

    /**
     * @brief Fetch BLOB data from URL (INDIGO URL mode)
     * @param url BLOB URL
     * @return BLOB data or error
     */
    auto fetchBlobUrl(const std::string& url) -> DeviceResult<std::vector<uint8_t>>;

    // ==================== Callback Registration ====================

    /**
     * @brief Set device attach/detach callback
     */
    void setDeviceCallback(DeviceCallback callback);

    /**
     * @brief Set property define/delete callback
     */
    void setPropertyDefineCallback(PropertyDefineCallback callback);

    /**
     * @brief Set property update callback
     */
    void setPropertyUpdateCallback(PropertyUpdateCallback callback);

    /**
     * @brief Set connection status callback
     */
    void setConnectionCallback(ConnectionCallback callback);

    /**
     * @brief Set message callback
     */
    void setMessageCallback(MessageCallback callback);

    // ==================== Utility ====================

    /**
     * @brief Get client configuration
     */
    [[nodiscard]] auto getConfig() const -> const Config&;

    /**
     * @brief Set client configuration (must be disconnected)
     */
    auto setConfig(const Config& config) -> DeviceResult<bool>;

    /**
     * @brief Get statistics
     */
    [[nodiscard]] auto getStatistics() const -> json;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// Conversion Functions
// ============================================================================

/**
 * @brief Convert property state to string
 */
[[nodiscard]] constexpr auto propertyStateToString(PropertyState state) noexcept
    -> std::string_view {
    switch (state) {
        case PropertyState::Idle: return "Idle";
        case PropertyState::Ok: return "Ok";
        case PropertyState::Busy: return "Busy";
        case PropertyState::Alert: return "Alert";
        default: return "Unknown";
    }
}

/**
 * @brief Convert string to property state
 */
[[nodiscard]] auto propertyStateFromString(std::string_view str) -> PropertyState;

/**
 * @brief Convert property type to string
 */
[[nodiscard]] constexpr auto propertyTypeToString(PropertyType type) noexcept
    -> std::string_view {
    switch (type) {
        case PropertyType::Text: return "Text";
        case PropertyType::Number: return "Number";
        case PropertyType::Switch: return "Switch";
        case PropertyType::Light: return "Light";
        case PropertyType::Blob: return "BLOB";
        default: return "Unknown";
    }
}

/**
 * @brief Convert string to property type
 */
[[nodiscard]] auto propertyTypeFromString(std::string_view str) -> PropertyType;

/**
 * @brief Convert device interface to string
 */
[[nodiscard]] auto deviceInterfaceToString(DeviceInterface iface) -> std::string;

/**
 * @brief Check if interface has specific capability
 */
[[nodiscard]] inline bool hasInterface(DeviceInterface iface, DeviceInterface check) {
    return (static_cast<uint32_t>(iface) & static_cast<uint32_t>(check)) != 0;
}

}  // namespace lithium::client::indigo

#endif  // LITHIUM_CLIENT_INDIGO_CLIENT_HPP
