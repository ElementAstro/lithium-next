/*
 * client_base.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Unified base class for all client components

*************************************************/

#ifndef LITHIUM_CLIENT_BASE_HPP
#define LITHIUM_CLIENT_BASE_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/utils/uuid.hpp"

namespace lithium::client {

/**
 * @brief Client capability flags
 */
enum class ClientCapability : uint32_t {
    None = 0,
    Connect = 1 << 0,         // Can connect/disconnect
    Scan = 1 << 1,            // Can scan for available instances
    Configure = 1 << 2,       // Supports configuration
    AsyncOperation = 1 << 3,  // Supports async operations
    StatusQuery = 1 << 4,     // Can query status
    EventCallback = 1 << 5,   // Supports event callbacks
    BatchProcess = 1 << 6,    // Supports batch processing
};

inline ClientCapability operator|(ClientCapability a, ClientCapability b) {
    return static_cast<ClientCapability>(static_cast<uint32_t>(a) |
                                         static_cast<uint32_t>(b));
}

inline ClientCapability operator&(ClientCapability a, ClientCapability b) {
    return static_cast<ClientCapability>(static_cast<uint32_t>(a) &
                                         static_cast<uint32_t>(b));
}

inline bool hasCapability(ClientCapability caps, ClientCapability flag) {
    return (static_cast<uint32_t>(caps) & static_cast<uint32_t>(flag)) != 0;
}

/**
 * @brief Client state enumeration
 */
enum class ClientState {
    Uninitialized,
    Initialized,
    Connecting,
    Connected,
    Disconnecting,
    Disconnected,
    Error
};

/**
 * @brief Client type enumeration
 */
enum class ClientType {
    Unknown,
    Solver,     // Plate solvers (ASTAP, Astrometry, StellarSolver)
    Guider,     // Guiding software (PHD2)
    Server,     // Device servers (INDI)
    Connector,  // Connection managers
    Custom
};

/**
 * @brief Client error information
 */
struct ClientError {
    int code{0};
    std::string message;
    std::chrono::system_clock::time_point timestamp;

    bool hasError() const { return code != 0; }
    void clear() {
        code = 0;
        message.clear();
    }
};

/**
 * @brief Client configuration options
 */
struct ClientConfig {
    std::string executablePath;
    std::string configPath;
    std::string dataPath;
    int connectionTimeout{5000};  // ms
    int operationTimeout{30000};  // ms
    int maxRetries{3};
    std::unordered_map<std::string, std::string> extraOptions;
};

/**
 * @brief Event callback type
 */
using ClientEventCallback =
    std::function<void(const std::string& event, const std::string& data)>;

/**
 * @brief Status change callback type
 */
using ClientStatusCallback =
    std::function<void(ClientState oldState, ClientState newState)>;

/**
 * @brief Base class for all client components
 *
 * Provides unified lifecycle management, configuration, and status tracking
 * for various client implementations (solvers, guiders, servers, etc.)
 */
class ClientBase : public std::enable_shared_from_this<ClientBase> {
public:
    /**
     * @brief Construct a new ClientBase
     * @param name Unique name for this client instance
     * @param type Client type
     */
    explicit ClientBase(std::string name,
                        ClientType type = ClientType::Unknown);

    /**
     * @brief Virtual destructor
     */
    virtual ~ClientBase();

    // Non-copyable, movable
    ClientBase(const ClientBase&) = delete;
    ClientBase& operator=(const ClientBase&) = delete;
    ClientBase(ClientBase&&) noexcept = default;
    ClientBase& operator=(ClientBase&&) noexcept = default;

    // ==================== Lifecycle Methods ====================

    /**
     * @brief Initialize the client
     * @return true if initialization successful
     */
    virtual bool initialize() = 0;

    /**
     * @brief Destroy/cleanup the client
     * @return true if cleanup successful
     */
    virtual bool destroy() = 0;

    /**
     * @brief Connect to the target service/executable
     * @param target Connection target (path, host:port, etc.)
     * @param timeout Connection timeout in milliseconds
     * @param maxRetry Maximum retry attempts
     * @return true if connection successful
     */
    virtual bool connect(const std::string& target, int timeout = 5000,
                         int maxRetry = 3) = 0;

    /**
     * @brief Disconnect from the service
     * @return true if disconnection successful
     */
    virtual bool disconnect() = 0;

    /**
     * @brief Check if connected
     * @return true if currently connected
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Scan for available instances/executables
     * @return Vector of discovered targets
     */
    virtual std::vector<std::string> scan() = 0;

    // ==================== Configuration ====================

    /**
     * @brief Configure the client with options
     * @param config Configuration options
     * @return true if configuration successful
     */
    virtual bool configure(const ClientConfig& config);

    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const ClientConfig& getConfig() const { return config_; }

    // ==================== Status & Information ====================

    /**
     * @brief Get client UUID
     * @return Unique identifier string
     */
    std::string getUUID() const { return uuid_; }

    /**
     * @brief Get client name
     * @return Client name
     */
    std::string getName() const { return name_; }

    /**
     * @brief Set client name
     * @param name New name
     */
    void setName(const std::string& name) { name_ = name; }

    /**
     * @brief Get client type
     * @return Client type enum
     */
    ClientType getType() const { return type_; }

    /**
     * @brief Get client type as string
     * @return Type name string
     */
    std::string getTypeName() const;

    /**
     * @brief Get current state
     * @return Current client state
     */
    ClientState getState() const { return state_.load(); }

    /**
     * @brief Get state as string
     * @return State name string
     */
    std::string getStateName() const;

    /**
     * @brief Get supported capabilities
     * @return Capability flags
     */
    ClientCapability getCapabilities() const { return capabilities_; }

    /**
     * @brief Check if client has specific capability
     * @param cap Capability to check
     * @return true if capability is supported
     */
    bool hasCapability(ClientCapability cap) const {
        return lithium::client::hasCapability(capabilities_, cap);
    }

    /**
     * @brief Get last error
     * @return Last error information
     */
    const ClientError& getLastError() const { return lastError_; }

    /**
     * @brief Clear last error
     */
    void clearError() { lastError_.clear(); }

    /**
     * @brief Get client version string
     * @return Version string
     */
    virtual std::string getVersion() const { return version_; }

    // ==================== Callbacks ====================

    /**
     * @brief Set event callback
     * @param callback Callback function
     */
    void setEventCallback(ClientEventCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        eventCallback_ = std::move(callback);
    }

    /**
     * @brief Set status change callback
     * @param callback Callback function
     */
    void setStatusCallback(ClientStatusCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        statusCallback_ = std::move(callback);
    }

protected:
    // ==================== Protected Helpers ====================

    /**
     * @brief Set client state with callback notification
     * @param newState New state
     */
    void setState(ClientState newState);

    /**
     * @brief Set error information
     * @param code Error code
     * @param message Error message
     */
    void setError(int code, const std::string& message);

    /**
     * @brief Emit event to callback
     * @param event Event name
     * @param data Event data
     */
    void emitEvent(const std::string& event, const std::string& data = "");

    /**
     * @brief Set capabilities
     * @param caps Capability flags
     */
    void setCapabilities(ClientCapability caps) { capabilities_ = caps; }

    /**
     * @brief Set version string
     * @param version Version string
     */
    void setVersion(const std::string& version) { version_ = version; }

    // Member variables
    std::string name_;
    std::string uuid_;
    std::string version_;
    ClientType type_;
    std::atomic<ClientState> state_{ClientState::Uninitialized};
    ClientCapability capabilities_{ClientCapability::None};
    ClientConfig config_;
    ClientError lastError_;

    mutable std::mutex callbackMutex_;
    ClientEventCallback eventCallback_;
    ClientStatusCallback statusCallback_;
};

/**
 * @brief Client descriptor for registration
 */
struct ClientDescriptor {
    std::string name;
    std::string description;
    ClientType type;
    std::string version;
    std::vector<std::string> requiredBinaries;
    std::function<std::shared_ptr<ClientBase>()> factory;
};

/**
 * @brief Client registry for managing client instances
 */
class ClientRegistry {
public:
    /**
     * @brief Get singleton instance
     * @return Registry reference
     */
    static ClientRegistry& instance();

    /**
     * @brief Register a client type
     * @param descriptor Client descriptor
     * @return true if registration successful
     */
    bool registerClient(const ClientDescriptor& descriptor);

    /**
     * @brief Unregister a client type
     * @param name Client name
     * @return true if unregistration successful
     */
    bool unregisterClient(const std::string& name);

    /**
     * @brief Create a client instance
     * @param name Registered client name
     * @return Shared pointer to client, or nullptr if not found
     */
    std::shared_ptr<ClientBase> createClient(const std::string& name);

    /**
     * @brief Get all registered client names
     * @return Vector of client names
     */
    std::vector<std::string> getRegisteredClients() const;

    /**
     * @brief Get clients by type
     * @param type Client type
     * @return Vector of client names matching type
     */
    std::vector<std::string> getClientsByType(ClientType type) const;

    /**
     * @brief Get client descriptor
     * @param name Client name
     * @return Optional descriptor
     */
    std::optional<ClientDescriptor> getDescriptor(
        const std::string& name) const;

private:
    ClientRegistry() = default;
    ~ClientRegistry() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ClientDescriptor> descriptors_;
};

/**
 * @brief Helper macro for client registration
 */
#define LITHIUM_REGISTER_CLIENT(ClientClass, Name, Description, Type, Version, \
                                ...)                                           \
    namespace {                                                                \
    static bool _registered_##ClientClass = []() {                             \
        lithium::client::ClientDescriptor desc;                                \
        desc.name = Name;                                                      \
        desc.description = Description;                                        \
        desc.type = Type;                                                      \
        desc.version = Version;                                                \
        desc.requiredBinaries = {__VA_ARGS__};                                 \
        desc.factory = []() { return std::make_shared<ClientClass>(Name); };   \
        return lithium::client::ClientRegistry::instance().registerClient(     \
            desc);                                                             \
    }();                                                                       \
    }

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_BASE_HPP
