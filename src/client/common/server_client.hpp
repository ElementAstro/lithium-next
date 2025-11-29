/*
 * server_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Base class for device server clients (INDI, etc.)

*************************************************/

#ifndef LITHIUM_CLIENT_SERVER_CLIENT_HPP
#define LITHIUM_CLIENT_SERVER_CLIENT_HPP

#include "client_base.hpp"

#include <chrono>
#include <unordered_map>
#include <vector>

#include "atom/type/json.hpp"

namespace lithium::client {

/**
 * @brief Device interface type flags
 */
enum class DeviceInterface : uint32_t {
    None = 0,
    General = 1 << 0,
    Telescope = 1 << 1,
    CCD = 1 << 2,
    Guider = 1 << 3,
    Focuser = 1 << 4,
    FilterWheel = 1 << 5,
    Dome = 1 << 6,
    GPS = 1 << 7,
    Weather = 1 << 8,
    AO = 1 << 9,
    Dustcap = 1 << 10,
    Lightbox = 1 << 11,
    Detector = 1 << 12,
    Rotator = 1 << 13,
    Spectrograph = 1 << 14,
    Correlator = 1 << 15,
    AuxiliaryDevice = 1 << 16,
    Output = 1 << 17,
    Input = 1 << 18,
    Power = 1 << 19,
    SafetyMonitor = 1 << 20,
    Switch = 1 << 21,
    Video = 1 << 22,
};

inline DeviceInterface operator|(DeviceInterface a, DeviceInterface b) {
    return static_cast<DeviceInterface>(static_cast<uint32_t>(a) |
                                        static_cast<uint32_t>(b));
}

inline DeviceInterface operator&(DeviceInterface a, DeviceInterface b) {
    return static_cast<DeviceInterface>(static_cast<uint32_t>(a) &
                                        static_cast<uint32_t>(b));
}

inline bool hasInterface(DeviceInterface caps, DeviceInterface flag) {
    return (static_cast<uint32_t>(caps) & static_cast<uint32_t>(flag)) != 0;
}

/**
 * @brief Device property value variant
 */
struct PropertyValue {
    enum class Type { Number, Text, Switch, Light, Blob, Unknown };

    Type type{Type::Unknown};
    std::string name;
    std::string label;
    std::string group;
    std::string state;  // Idle, Ok, Busy, Alert

    // Value storage
    double numberValue{0.0};
    double numberMin{0.0};
    double numberMax{0.0};
    double numberStep{1.0};
    std::string textValue;
    bool switchValue{false};
    std::vector<uint8_t> blobData;
    std::string blobFormat;

    // For multi-element properties
    std::unordered_map<std::string, PropertyValue> elements;

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j;
        j["name"] = name;
        j["label"] = label;
        j["group"] = group;
        j["state"] = state;
        switch (type) {
            case Type::Number:
                j["type"] = "number";
                j["value"] = numberValue;
                j["min"] = numberMin;
                j["max"] = numberMax;
                j["step"] = numberStep;
                break;
            case Type::Text:
                j["type"] = "text";
                j["value"] = textValue;
                break;
            case Type::Switch:
                j["type"] = "switch";
                j["value"] = switchValue;
                break;
            case Type::Light:
                j["type"] = "light";
                break;
            case Type::Blob:
                j["type"] = "blob";
                j["size"] = blobData.size();
                j["format"] = blobFormat;
                break;
            default:
                j["type"] = "unknown";
                break;
        }
        if (!elements.empty()) {
            nlohmann::json elems = nlohmann::json::object();
            for (const auto& [k, v] : elements) {
                elems[k] = v.toJson();
            }
            j["elements"] = elems;
        }
        return j;
    }
};

/**
 * @brief Device health status
 */
enum class DeviceHealth { Unknown, Good, Warning, Error, Critical };

/**
 * @brief Device information structure
 */
struct DeviceInfo {
    // Basic identification
    std::string id;           // Unique device ID
    std::string name;         // Device name
    std::string displayName;  // Human-readable display name

    // Driver information
    std::string driver;         // Driver name
    std::string driverVersion;  // Driver version
    std::string driverExec;     // Driver executable path
    std::string backend;        // Backend type: "INDI", "ASCOM", etc.

    // Interface and capabilities
    DeviceInterface interfaces{DeviceInterface::None};
    std::string interfaceString;  // Legacy string representation

    // Connection state
    bool connected{false};
    bool initialized{false};
    bool busy{false};
    DeviceHealth health{DeviceHealth::Unknown};
    std::string lastError;

    // Timestamps
    std::chrono::system_clock::time_point lastUpdate;
    std::chrono::system_clock::time_point connectedSince;

    // Properties
    std::unordered_map<std::string, PropertyValue> properties;
    std::unordered_map<std::string, std::string> metadata;

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["displayName"] = displayName;
        j["driver"] = driver;
        j["driverVersion"] = driverVersion;
        j["driverExec"] = driverExec;
        j["backend"] = backend;
        j["interfaces"] = static_cast<uint32_t>(interfaces);
        j["interfaceString"] = interfaceString;
        j["connected"] = connected;
        j["initialized"] = initialized;
        j["busy"] = busy;
        j["health"] = static_cast<int>(health);
        j["lastError"] = lastError;

        nlohmann::json props = nlohmann::json::object();
        for (const auto& [k, v] : properties) {
            props[k] = v.toJson();
        }
        j["properties"] = props;
        j["metadata"] = metadata;
        return j;
    }

    static auto fromJson(const nlohmann::json& j) -> DeviceInfo {
        DeviceInfo info;
        info.id = j.value("id", "");
        info.name = j.value("name", "");
        info.displayName = j.value("displayName", "");
        info.driver = j.value("driver", "");
        info.driverVersion = j.value("driverVersion", "");
        info.driverExec = j.value("driverExec", "");
        info.backend = j.value("backend", "");
        info.interfaces =
            static_cast<DeviceInterface>(j.value("interfaces", 0u));
        info.interfaceString = j.value("interfaceString", "");
        info.connected = j.value("connected", false);
        info.initialized = j.value("initialized", false);
        info.busy = j.value("busy", false);
        info.health = static_cast<DeviceHealth>(j.value("health", 0));
        info.lastError = j.value("lastError", "");
        if (j.contains("metadata")) {
            info.metadata =
                j["metadata"]
                    .get<std::unordered_map<std::string, std::string>>();
        }
        return info;
    }
};

/**
 * @brief Driver family/category
 */
enum class DriverFamily {
    Unknown,
    Telescope,
    CCD,
    Focuser,
    FilterWheel,
    Dome,
    Weather,
    GPS,
    AuxiliaryDevice,
    Spectrograph,
    Video,
    Agent
};

/**
 * @brief Driver information structure
 */
struct DriverInfo {
    // Basic identification
    std::string id;       // Unique driver ID
    std::string name;     // Internal name
    std::string label;    // Display label
    std::string version;  // Driver version

    // Executable information
    std::string binary;      // Executable path/name
    std::string skeleton;    // Skeleton file path
    std::string configPath;  // Configuration path

    // Classification
    DriverFamily family{DriverFamily::Unknown};
    std::string manufacturer;  // Device manufacturer
    std::string backend;       // Backend: "INDI", "ASCOM"

    // State
    bool running{false};
    bool available{true};
    int pid{0};  // Process ID if running

    // Capabilities
    DeviceInterface supportedInterfaces{DeviceInterface::None};
    std::vector<std::string> supportedDevices;

    // Metadata
    std::unordered_map<std::string, std::string> metadata;

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["label"] = label;
        j["version"] = version;
        j["binary"] = binary;
        j["skeleton"] = skeleton;
        j["configPath"] = configPath;
        j["family"] = static_cast<int>(family);
        j["manufacturer"] = manufacturer;
        j["backend"] = backend;
        j["running"] = running;
        j["available"] = available;
        j["pid"] = pid;
        j["supportedInterfaces"] = static_cast<uint32_t>(supportedInterfaces);
        j["supportedDevices"] = supportedDevices;
        j["metadata"] = metadata;
        return j;
    }

    static auto fromJson(const nlohmann::json& j) -> DriverInfo {
        DriverInfo info;
        info.id = j.value("id", "");
        info.name = j.value("name", "");
        info.label = j.value("label", "");
        info.version = j.value("version", "");
        info.binary = j.value("binary", "");
        info.skeleton = j.value("skeleton", "");
        info.configPath = j.value("configPath", "");
        info.family = static_cast<DriverFamily>(j.value("family", 0));
        info.manufacturer = j.value("manufacturer", "");
        info.backend = j.value("backend", "");
        info.running = j.value("running", false);
        info.available = j.value("available", true);
        info.pid = j.value("pid", 0);
        info.supportedInterfaces =
            static_cast<DeviceInterface>(j.value("supportedInterfaces", 0u));
        if (j.contains("supportedDevices")) {
            info.supportedDevices =
                j["supportedDevices"].get<std::vector<std::string>>();
        }
        if (j.contains("metadata")) {
            info.metadata =
                j["metadata"]
                    .get<std::unordered_map<std::string, std::string>>();
        }
        return info;
    }
};

/**
 * @brief Server configuration
 */
struct ServerConfig {
    // Connection settings
    std::string host{"localhost"};
    int port{7624};
    std::string protocol{"tcp"};  // tcp, websocket, etc.

    // Paths
    std::string configPath;
    std::string dataPath;
    std::string fifoPath;
    std::string logPath;

    // Server options
    int maxClients{100};
    int connectionTimeout{5000};  // ms
    int operationTimeout{30000};  // ms
    bool verbose{false};
    bool autoStart{false};
    bool enableBlobCompression{false};

    // Authentication (optional)
    std::string username;
    std::string password;
    std::string apiKey;

    // Backend-specific options
    std::unordered_map<std::string, std::string> extraOptions;

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j;
        j["host"] = host;
        j["port"] = port;
        j["protocol"] = protocol;
        j["configPath"] = configPath;
        j["dataPath"] = dataPath;
        j["fifoPath"] = fifoPath;
        j["logPath"] = logPath;
        j["maxClients"] = maxClients;
        j["connectionTimeout"] = connectionTimeout;
        j["operationTimeout"] = operationTimeout;
        j["verbose"] = verbose;
        j["autoStart"] = autoStart;
        j["enableBlobCompression"] = enableBlobCompression;
        j["extraOptions"] = extraOptions;
        return j;
    }

    static auto fromJson(const nlohmann::json& j) -> ServerConfig {
        ServerConfig cfg;
        cfg.host = j.value("host", "localhost");
        cfg.port = j.value("port", 7624);
        cfg.protocol = j.value("protocol", "tcp");
        cfg.configPath = j.value("configPath", "");
        cfg.dataPath = j.value("dataPath", "");
        cfg.fifoPath = j.value("fifoPath", "");
        cfg.logPath = j.value("logPath", "");
        cfg.maxClients = j.value("maxClients", 100);
        cfg.connectionTimeout = j.value("connectionTimeout", 5000);
        cfg.operationTimeout = j.value("operationTimeout", 30000);
        cfg.verbose = j.value("verbose", false);
        cfg.autoStart = j.value("autoStart", false);
        cfg.enableBlobCompression = j.value("enableBlobCompression", false);
        cfg.username = j.value("username", "");
        cfg.password = j.value("password", "");
        cfg.apiKey = j.value("apiKey", "");
        if (j.contains("extraOptions")) {
            cfg.extraOptions =
                j["extraOptions"]
                    .get<std::unordered_map<std::string, std::string>>();
        }
        return cfg;
    }
};

/**
 * @brief Server event types
 */
enum class ServerEventType {
    ServerStarted,
    ServerStopped,
    ServerError,
    ClientConnected,
    ClientDisconnected,
    DriverStarted,
    DriverStopped,
    DriverError,
    DeviceAdded,
    DeviceRemoved,
    DeviceConnected,
    DeviceDisconnected,
    PropertyDefined,
    PropertyUpdated,
    PropertyDeleted,
    MessageReceived,
    BlobReceived
};

/**
 * @brief Server event data
 */
struct ServerEvent {
    ServerEventType type;
    std::string source;  // Server, driver, or device name
    std::string message;
    nlohmann::json data;
    std::chrono::system_clock::time_point timestamp;

    [[nodiscard]] auto toJson() const -> nlohmann::json {
        nlohmann::json j;
        j["type"] = static_cast<int>(type);
        j["source"] = source;
        j["message"] = message;
        j["data"] = data;
        j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                             timestamp.time_since_epoch())
                             .count();
        return j;
    }
};

/**
 * @brief Server event callback type
 */
using ServerEventCallback = std::function<void(const ServerEvent&)>;

/**
 * @brief Base class for device server clients
 *
 * Provides common interface for INDI server and similar device servers
 */
class ServerClient : public ClientBase {
public:
    /**
     * @brief Construct a new ServerClient
     * @param name Server name
     */
    explicit ServerClient(std::string name);

    /**
     * @brief Virtual destructor
     */
    ~ServerClient() override;

    // ==================== Server Control ====================

    /**
     * @brief Start the server
     * @return true if server started successfully
     */
    virtual bool startServer() = 0;

    /**
     * @brief Stop the server
     * @return true if server stopped successfully
     */
    virtual bool stopServer() = 0;

    /**
     * @brief Check if server is running
     * @return true if server is running
     */
    virtual bool isServerRunning() const = 0;

    /**
     * @brief Check if server software is installed
     * @return true if installed
     */
    virtual bool isInstalled() const = 0;

    // ==================== Driver Management ====================

    /**
     * @brief Start a driver
     * @param driver Driver information
     * @return true if driver started successfully
     */
    virtual bool startDriver(const DriverInfo& driver) = 0;

    /**
     * @brief Stop a driver
     * @param driverName Driver name
     * @return true if driver stopped successfully
     */
    virtual bool stopDriver(const std::string& driverName) = 0;

    /**
     * @brief Get running drivers
     * @return Map of driver name to driver info
     */
    virtual std::unordered_map<std::string, DriverInfo> getRunningDrivers()
        const = 0;

    /**
     * @brief Get available drivers
     * @return Vector of available driver info
     */
    virtual std::vector<DriverInfo> getAvailableDrivers() const = 0;

    // ==================== Device Management ====================

    /**
     * @brief Get connected devices
     * @return Vector of device info
     */
    virtual std::vector<DeviceInfo> getDevices() const = 0;

    /**
     * @brief Get device by name
     * @param name Device name
     * @return Device info or nullopt if not found
     */
    virtual std::optional<DeviceInfo> getDevice(
        const std::string& name) const = 0;

    // ==================== Property Access ====================

    /**
     * @brief Set device property
     * @param device Device name
     * @param property Property name
     * @param element Element name
     * @param value Value to set
     * @return true if property set successfully
     */
    virtual bool setProperty(const std::string& device,
                             const std::string& property,
                             const std::string& element,
                             const std::string& value) = 0;

    /**
     * @brief Get device property
     * @param device Device name
     * @param property Property name
     * @param element Element name
     * @return Property value or empty string if not found
     */
    virtual std::string getProperty(const std::string& device,
                                    const std::string& property,
                                    const std::string& element) const = 0;

    /**
     * @brief Get property state
     * @param device Device name
     * @param property Property name
     * @return Property state string
     */
    virtual std::string getPropertyState(const std::string& device,
                                         const std::string& property) const = 0;

    // ==================== Configuration ====================

    // ==================== Device Connection ====================

    /**
     * @brief Connect to a specific device
     * @param deviceName Device name
     * @return true if connection successful
     */
    virtual bool connectDevice(const std::string& deviceName) = 0;

    /**
     * @brief Disconnect from a specific device
     * @param deviceName Device name
     * @return true if disconnection successful
     */
    virtual bool disconnectDevice(const std::string& deviceName) = 0;

    // ==================== Batch Property Operations ====================

    /**
     * @brief Set multiple properties at once
     * @param device Device name
     * @param properties Map of property name to value
     * @return true if all properties set successfully
     */
    virtual bool setProperties(
        const std::string& device,
        const std::unordered_map<std::string, std::string>& properties);

    /**
     * @brief Get all properties for a device
     * @param device Device name
     * @return Map of property name to PropertyValue
     */
    virtual std::unordered_map<std::string, PropertyValue> getProperties(
        const std::string& device) const;

    // ==================== Event System ====================

    /**
     * @brief Register server event callback
     * @param callback Callback function
     */
    virtual void registerServerEventCallback(ServerEventCallback callback);

    /**
     * @brief Unregister server event callback
     */
    virtual void unregisterServerEventCallback();

    // ==================== Configuration ====================

    /**
     * @brief Configure server
     * @param config Server configuration
     * @return true if configuration successful
     */
    virtual bool configureServer(const ServerConfig& config);

    /**
     * @brief Get server configuration
     * @return Current server configuration
     */
    const ServerConfig& getServerConfig() const { return serverConfig_; }

    /**
     * @brief Get server status as JSON
     * @return Server status
     */
    virtual nlohmann::json getServerStatus() const;

    /**
     * @brief Get backend type name
     * @return Backend name ("INDI", "ASCOM", etc.)
     */
    [[nodiscard]] virtual std::string getBackendName() const = 0;

protected:
    /**
     * @brief Emit server event to registered callback
     */
    void emitServerEvent(const ServerEvent& event);

    ServerConfig serverConfig_;
    std::atomic<bool> serverRunning_{false};
    mutable std::mutex serverEventMutex_;
    ServerEventCallback serverEventCallback_;
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_SERVER_CLIENT_HPP
