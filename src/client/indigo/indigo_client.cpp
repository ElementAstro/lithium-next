/*
 * indigo_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Client - Implementation using libindigo

**************************************************/

#include "indigo_client.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#if INDIGO_PLATFORM_SUPPORTED
// Include INDIGO headers on supported platforms
extern "C" {
#include <indigo/indigo_bus.h>
#include <indigo/indigo_client.h>
}
#endif

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// String conversion functions
// ============================================================================

auto propertyStateFromString(std::string_view str) -> PropertyState {
    if (str == "Idle") return PropertyState::Idle;
    if (str == "Ok") return PropertyState::Ok;
    if (str == "Busy") return PropertyState::Busy;
    if (str == "Alert") return PropertyState::Alert;
    return PropertyState::Unknown;
}

auto propertyTypeFromString(std::string_view str) -> PropertyType {
    if (str == "Text") return PropertyType::Text;
    if (str == "Number") return PropertyType::Number;
    if (str == "Switch") return PropertyType::Switch;
    if (str == "Light") return PropertyType::Light;
    if (str == "BLOB" || str == "Blob") return PropertyType::Blob;
    return PropertyType::Unknown;
}

auto deviceInterfaceToString(DeviceInterface iface) -> std::string {
    std::vector<std::string> parts;
    if (hasInterface(iface, DeviceInterface::CCD))
        parts.push_back("CCD");
    if (hasInterface(iface, DeviceInterface::Guider))
        parts.push_back("Guider");
    if (hasInterface(iface, DeviceInterface::Focuser))
        parts.push_back("Focuser");
    if (hasInterface(iface, DeviceInterface::FilterWheel))
        parts.push_back("FilterWheel");
    if (hasInterface(iface, DeviceInterface::Dome))
        parts.push_back("Dome");
    if (hasInterface(iface, DeviceInterface::GPS))
        parts.push_back("GPS");
    if (hasInterface(iface, DeviceInterface::Weather))
        parts.push_back("Weather");
    if (hasInterface(iface, DeviceInterface::Mount))
        parts.push_back("Mount");
    if (hasInterface(iface, DeviceInterface::Rotator))
        parts.push_back("Rotator");
    if (hasInterface(iface, DeviceInterface::AO))
        parts.push_back("AO");
    if (hasInterface(iface, DeviceInterface::Dustcap))
        parts.push_back("Dustcap");
    if (hasInterface(iface, DeviceInterface::Lightbox))
        parts.push_back("Lightbox");
    if (hasInterface(iface, DeviceInterface::Detector))
        parts.push_back("Detector");
    if (hasInterface(iface, DeviceInterface::Spectrograph))
        parts.push_back("Spectrograph");
    if (hasInterface(iface, DeviceInterface::Correlator))
        parts.push_back("Correlator");
    if (hasInterface(iface, DeviceInterface::AuxInterface))
        parts.push_back("Aux");

    if (parts.empty())
        return "General";

    std::string result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0)
            result += "|";
        result += parts[i];
    }
    return result;
}

// ============================================================================
// Property JSON serialization
// ============================================================================

auto Property::toJson() const -> json {
    json j;
    j["device"] = device;
    j["name"] = name;
    j["group"] = group;
    j["label"] = label;
    j["type"] = std::string(propertyTypeToString(type));
    j["state"] = std::string(propertyStateToString(state));

    switch (type) {
        case PropertyType::Text:
            j["elements"] = json::array();
            for (const auto& elem : textElements) {
                j["elements"].push_back({{"name", elem.name},
                                         {"label", elem.label},
                                         {"value", elem.value}});
            }
            break;

        case PropertyType::Number:
            j["elements"] = json::array();
            for (const auto& elem : numberElements) {
                j["elements"].push_back({{"name", elem.name},
                                         {"label", elem.label},
                                         {"value", elem.value},
                                         {"min", elem.min},
                                         {"max", elem.max},
                                         {"step", elem.step},
                                         {"format", elem.format},
                                         {"target", elem.target}});
            }
            break;

        case PropertyType::Switch:
            j["elements"] = json::array();
            j["rule"] = static_cast<int>(switchRule);
            for (const auto& elem : switchElements) {
                j["elements"].push_back({{"name", elem.name},
                                         {"label", elem.label},
                                         {"value", elem.value}});
            }
            break;

        case PropertyType::Light:
            j["elements"] = json::array();
            for (const auto& elem : lightElements) {
                j["elements"].push_back(
                    {{"name", elem.name},
                     {"label", elem.label},
                     {"state", std::string(propertyStateToString(elem.state))}});
            }
            break;

        case PropertyType::Blob:
            j["elements"] = json::array();
            for (const auto& elem : blobElements) {
                j["elements"].push_back({{"name", elem.name},
                                         {"label", elem.label},
                                         {"format", elem.format},
                                         {"size", elem.size},
                                         {"url", elem.url}});
            }
            break;

        default:
            break;
    }

    return j;
}

auto Property::fromJson(const json& j) -> Property {
    Property prop;
    prop.device = j.value("device", "");
    prop.name = j.value("name", "");
    prop.group = j.value("group", "");
    prop.label = j.value("label", "");
    prop.type = propertyTypeFromString(j.value("type", "Unknown"));
    prop.state = propertyStateFromString(j.value("state", "Idle"));

    if (j.contains("elements") && j["elements"].is_array()) {
        switch (prop.type) {
            case PropertyType::Text:
                for (const auto& elem : j["elements"]) {
                    TextElement te;
                    te.name = elem.value("name", "");
                    te.label = elem.value("label", "");
                    te.value = elem.value("value", "");
                    prop.textElements.push_back(te);
                }
                break;

            case PropertyType::Number:
                for (const auto& elem : j["elements"]) {
                    NumberElement ne;
                    ne.name = elem.value("name", "");
                    ne.label = elem.value("label", "");
                    ne.value = elem.value("value", 0.0);
                    ne.min = elem.value("min", 0.0);
                    ne.max = elem.value("max", 0.0);
                    ne.step = elem.value("step", 0.0);
                    ne.format = elem.value("format", "%g");
                    ne.target = elem.value("target", 0.0);
                    prop.numberElements.push_back(ne);
                }
                break;

            case PropertyType::Switch:
                prop.switchRule =
                    static_cast<SwitchRule>(j.value("rule", 0));
                for (const auto& elem : j["elements"]) {
                    SwitchElement se;
                    se.name = elem.value("name", "");
                    se.label = elem.value("label", "");
                    se.value = elem.value("value", false);
                    prop.switchElements.push_back(se);
                }
                break;

            case PropertyType::Light:
                for (const auto& elem : j["elements"]) {
                    LightElement le;
                    le.name = elem.value("name", "");
                    le.label = elem.value("label", "");
                    le.state =
                        propertyStateFromString(elem.value("state", "Idle"));
                    prop.lightElements.push_back(le);
                }
                break;

            case PropertyType::Blob:
                for (const auto& elem : j["elements"]) {
                    BlobElement be;
                    be.name = elem.value("name", "");
                    be.label = elem.value("label", "");
                    be.format = elem.value("format", "");
                    be.size = elem.value("size", size_t{0});
                    be.url = elem.value("url", "");
                    prop.blobElements.push_back(be);
                }
                break;

            default:
                break;
        }
    }

    return prop;
}

auto DiscoveredDevice::toJson() const -> json {
    return {{"name", name},
            {"driver", driver},
            {"interfaces", static_cast<uint32_t>(interfaces)},
            {"connected", connected},
            {"version", version},
            {"metadata", metadata}};
}

// ============================================================================
// INDIGOClient Implementation
// ============================================================================

#if INDIGO_PLATFORM_SUPPORTED

class INDIGOClient::Impl {
public:
    explicit Impl(const Config& config) : config_(config) {}

    ~Impl() {
        if (state_ != ConnectionState::Disconnected) {
            disconnect();
        }
    }

    auto connect(const std::string& host, int port) -> DeviceResult<bool> {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ == ConnectionState::Connected) {
            return DeviceResult<bool>(true);
        }

        state_ = ConnectionState::Connecting;

        std::string actualHost = host.empty() ? config_.host : host;
        int actualPort = (port == 0) ? config_.port : port;

        LOG_F(INFO, "INDIGO: Connecting to {}:{}", actualHost, actualPort);

        // Initialize INDIGO bus
        indigo_set_log_level(INDIGO_LOG_INFO);
        indigo_start();

        // Create client entry point
        std::string serverName = actualHost + ":" + std::to_string(actualPort);
        serverEntry_.name = strdup(serverName.c_str());

        // Set up callbacks
        serverEntry_.client_context = this;
        serverEntry_.attach = attachCallback;
        serverEntry_.define_property = definePropertyCallback;
        serverEntry_.update_property = updatePropertyCallback;
        serverEntry_.delete_property = deletePropertyCallback;
        serverEntry_.send_message = messageCallback;
        serverEntry_.detach = detachCallback;

        // Connect to server
        indigo_result result =
            indigo_connect_server(actualHost.c_str(), actualPort, &serverEntry_);

        if (result != INDIGO_OK) {
            state_ = ConnectionState::Error;
            lastError_ = "Failed to connect to INDIGO server";
            LOG_F(ERROR, "INDIGO: Connection failed with code {}", static_cast<int>(result));
            return DeviceError{DeviceErrorCode::ConnectionFailed, lastError_};
        }

        // Wait for connection with timeout
        std::unique_lock<std::mutex> connectLock(connectMutex_);
        bool connected = connectCv_.wait_for(
            connectLock, config_.connectTimeout,
            [this] { return state_ == ConnectionState::Connected; });

        if (!connected) {
            state_ = ConnectionState::Error;
            lastError_ = "Connection timeout";
            indigo_disconnect_server(&serverEntry_);
            return DeviceError{DeviceErrorCode::Timeout, lastError_};
        }

        LOG_F(INFO, "INDIGO: Connected successfully");
        return DeviceResult<bool>(true);
    }

    auto disconnect() -> DeviceResult<bool> {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ == ConnectionState::Disconnected) {
            return DeviceResult<bool>(true);
        }

        state_ = ConnectionState::Disconnecting;
        LOG_F(INFO, "INDIGO: Disconnecting");

        indigo_disconnect_server(&serverEntry_);
        indigo_stop();

        if (serverEntry_.name) {
            free(const_cast<char*>(serverEntry_.name));
            serverEntry_.name = nullptr;
        }

        state_ = ConnectionState::Disconnected;
        devices_.clear();
        properties_.clear();

        if (connectionCallback_) {
            connectionCallback_(false, "Disconnected");
        }

        LOG_F(INFO, "INDIGO: Disconnected");
        return DeviceResult<bool>(true);
    }

    [[nodiscard]] auto isConnected() const -> bool {
        return state_ == ConnectionState::Connected;
    }

    [[nodiscard]] auto getConnectionState() const -> ConnectionState {
        return state_;
    }

    [[nodiscard]] auto getLastError() const -> std::string {
        return lastError_;
    }

    auto discoverDevices() -> DeviceResult<std::vector<DiscoveredDevice>> {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            return DeviceError{DeviceErrorCode::NotConnected,
                               "Not connected to server"};
        }

        std::vector<DiscoveredDevice> result;
        for (const auto& [name, device] : devices_) {
            result.push_back(device);
        }
        return result;
    }

    [[nodiscard]] auto getDiscoveredDevices() const -> std::vector<DiscoveredDevice> {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<DiscoveredDevice> result;
        for (const auto& [name, device] : devices_) {
            result.push_back(device);
        }
        return result;
    }

    auto connectDevice(const std::string& deviceName) -> DeviceResult<bool> {
        return setSwitchProperty(deviceName, "CONNECTION",
                                 {{"CONNECTED", true}});
    }

    auto disconnectDevice(const std::string& deviceName) -> DeviceResult<bool> {
        return setSwitchProperty(deviceName, "CONNECTION",
                                 {{"DISCONNECTED", true}});
    }

    auto getProperty(const std::string& deviceName,
                     const std::string& propertyName) -> DeviceResult<Property> {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string key = deviceName + "." + propertyName;
        auto it = properties_.find(key);
        if (it != properties_.end()) {
            return it->second;
        }

        return DeviceError{DeviceErrorCode::PropertyNotFound,
                           "Property not found: " + key};
    }

    auto getDeviceProperties(const std::string& deviceName)
        -> DeviceResult<std::vector<Property>> {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<Property> result;
        std::string prefix = deviceName + ".";

        for (const auto& [key, prop] : properties_) {
            if (key.substr(0, prefix.size()) == prefix) {
                result.push_back(prop);
            }
        }

        return result;
    }

    auto setTextProperty(
        const std::string& deviceName, const std::string& propertyName,
        const std::vector<std::pair<std::string, std::string>>& elements)
        -> DeviceResult<bool> {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            return DeviceError{DeviceErrorCode::NotConnected,
                               "Not connected to server"};
        }

        // Build INDIGO property
        indigo_property* property =
            indigo_init_text_property(nullptr, deviceName.c_str(),
                                      propertyName.c_str(), nullptr, nullptr,
                                      INDIGO_OK_STATE, INDIGO_RW_PERM,
                                      elements.size());

        for (size_t i = 0; i < elements.size(); ++i) {
            indigo_init_text_item(property->items + i,
                                  elements[i].first.c_str(), nullptr,
                                  elements[i].second.c_str());
        }

        indigo_change_property(&serverEntry_, property);
        indigo_release_property(property);

        return DeviceResult<bool>(true);
    }

    auto setNumberProperty(
        const std::string& deviceName, const std::string& propertyName,
        const std::vector<std::pair<std::string, double>>& elements)
        -> DeviceResult<bool> {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            return DeviceError{DeviceErrorCode::NotConnected,
                               "Not connected to server"};
        }

        indigo_property* property =
            indigo_init_number_property(nullptr, deviceName.c_str(),
                                        propertyName.c_str(), nullptr, nullptr,
                                        INDIGO_OK_STATE, INDIGO_RW_PERM,
                                        elements.size());

        for (size_t i = 0; i < elements.size(); ++i) {
            indigo_init_number_item(property->items + i,
                                    elements[i].first.c_str(), nullptr, 0, 0, 0,
                                    elements[i].second);
        }

        indigo_change_property(&serverEntry_, property);
        indigo_release_property(property);

        return DeviceResult<bool>(true);
    }

    auto setSwitchProperty(
        const std::string& deviceName, const std::string& propertyName,
        const std::vector<std::pair<std::string, bool>>& elements)
        -> DeviceResult<bool> {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            return DeviceError{DeviceErrorCode::NotConnected,
                               "Not connected to server"};
        }

        indigo_property* property =
            indigo_init_switch_property(nullptr, deviceName.c_str(),
                                        propertyName.c_str(), nullptr, nullptr,
                                        INDIGO_OK_STATE, INDIGO_RW_PERM,
                                        INDIGO_ONE_OF_MANY_RULE,
                                        elements.size());

        for (size_t i = 0; i < elements.size(); ++i) {
            indigo_init_switch_item(property->items + i,
                                    elements[i].first.c_str(), nullptr,
                                    elements[i].second);
        }

        indigo_change_property(&serverEntry_, property);
        indigo_release_property(property);

        return DeviceResult<bool>(true);
    }

    auto enableBlob(const std::string& deviceName, bool enable, bool urlMode)
        -> DeviceResult<bool> {
        std::lock_guard<std::mutex> lock(mutex_);

        if (state_ != ConnectionState::Connected) {
            return DeviceError{DeviceErrorCode::NotConnected,
                               "Not connected to server"};
        }

        indigo_enable_blob_mode mode =
            urlMode ? INDIGO_ENABLE_BLOB_URL : INDIGO_ENABLE_BLOB;

        indigo_enable_blob(&serverEntry_,
                           enable ? mode : INDIGO_ENABLE_BLOB_NEVER,
                           deviceName.empty() ? nullptr : deviceName.c_str(),
                           nullptr);

        return DeviceResult<bool>(true);
    }

    auto fetchBlobUrl(const std::string& url) -> DeviceResult<std::vector<uint8_t>> {
        // INDIGO URL mode: fetch BLOB data from URL
        // This requires HTTP client to fetch from INDIGO server's HTTP endpoint
        // For now, return not implemented
        return DeviceError{DeviceErrorCode::NotImplemented,
                           "BLOB URL fetch not implemented yet"};
    }

    void setDeviceCallback(DeviceCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        deviceCallback_ = std::move(callback);
    }

    void setPropertyDefineCallback(PropertyDefineCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        propertyDefineCallback_ = std::move(callback);
    }

    void setPropertyUpdateCallback(PropertyUpdateCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        propertyUpdateCallback_ = std::move(callback);
    }

    void setConnectionCallback(ConnectionCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        connectionCallback_ = std::move(callback);
    }

    void setMessageCallback(MessageCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        messageCallback_ = std::move(callback);
    }

    [[nodiscard]] auto getConfig() const -> const Config& { return config_; }

    auto setConfig(const Config& config) -> DeviceResult<bool> {
        if (state_ != ConnectionState::Disconnected) {
            return DeviceError{DeviceErrorCode::InvalidState,
                               "Cannot change config while connected"};
        }
        config_ = config;
        return DeviceResult<bool>(true);
    }

    [[nodiscard]] auto getStatistics() const -> json {
        std::lock_guard<std::mutex> lock(mutex_);
        return {{"devices", devices_.size()},
                {"properties", properties_.size()},
                {"connected", state_ == ConnectionState::Connected},
                {"host", config_.host},
                {"port", config_.port}};
    }

private:
    // INDIGO callbacks
    static indigo_result attachCallback(indigo_client* client,
                                        indigo_device* device) {
        auto* impl = static_cast<Impl*>(client->client_context);

        std::lock_guard<std::mutex> lock(impl->mutex_);

        DiscoveredDevice discovered;
        discovered.name = device->name;
        discovered.connected = false;
        impl->devices_[device->name] = discovered;

        LOG_F(INFO, "INDIGO: Device attached: {}", device->name);

        // Notify connection on first device
        if (impl->state_ == ConnectionState::Connecting) {
            impl->state_ = ConnectionState::Connected;
            impl->connectCv_.notify_all();

            if (impl->connectionCallback_) {
                impl->connectionCallback_(true, "Connected");
            }
        }

        if (impl->deviceCallback_) {
            impl->deviceCallback_(device->name, true);
        }

        return INDIGO_OK;
    }

    static indigo_result detachCallback(indigo_client* client,
                                        indigo_device* device) {
        auto* impl = static_cast<Impl*>(client->client_context);

        std::lock_guard<std::mutex> lock(impl->mutex_);

        impl->devices_.erase(device->name);

        // Remove device properties
        std::string prefix = std::string(device->name) + ".";
        auto it = impl->properties_.begin();
        while (it != impl->properties_.end()) {
            if (it->first.substr(0, prefix.size()) == prefix) {
                it = impl->properties_.erase(it);
            } else {
                ++it;
            }
        }

        LOG_F(INFO, "INDIGO: Device detached: {}", device->name);

        if (impl->deviceCallback_) {
            impl->deviceCallback_(device->name, false);
        }

        return INDIGO_OK;
    }

    static indigo_result definePropertyCallback(indigo_client* client,
                                                indigo_device* device,
                                                indigo_property* property,
                                                const char* message) {
        auto* impl = static_cast<Impl*>(client->client_context);

        Property prop = convertProperty(device, property);

        std::lock_guard<std::mutex> lock(impl->mutex_);
        std::string key = prop.device + "." + prop.name;
        impl->properties_[key] = prop;

        // Update device info if INFO property
        if (property->name == std::string("INFO")) {
            impl->updateDeviceInfo(device, property);
        }

        LOG_F(DEBUG, "INDIGO: Property defined: {}.{}", device->name,
              property->name);

        if (impl->propertyDefineCallback_) {
            impl->propertyDefineCallback_(prop, true);
        }

        return INDIGO_OK;
    }

    static indigo_result updatePropertyCallback(indigo_client* client,
                                                indigo_device* device,
                                                indigo_property* property,
                                                const char* message) {
        auto* impl = static_cast<Impl*>(client->client_context);

        Property prop = convertProperty(device, property);

        std::lock_guard<std::mutex> lock(impl->mutex_);
        std::string key = prop.device + "." + prop.name;
        impl->properties_[key] = prop;

        // Check connection status
        if (property->name == std::string("CONNECTION")) {
            impl->updateConnectionStatus(device, property);
        }

        LOG_F(DEBUG, "INDIGO: Property updated: {}.{}", device->name,
              property->name);

        if (impl->propertyUpdateCallback_) {
            impl->propertyUpdateCallback_(prop);
        }

        return INDIGO_OK;
    }

    static indigo_result deletePropertyCallback(indigo_client* client,
                                                indigo_device* device,
                                                indigo_property* property,
                                                const char* message) {
        auto* impl = static_cast<Impl*>(client->client_context);

        std::lock_guard<std::mutex> lock(impl->mutex_);

        if (property) {
            std::string key =
                std::string(device->name) + "." + property->name;
            auto it = impl->properties_.find(key);
            if (it != impl->properties_.end()) {
                Property prop = it->second;
                impl->properties_.erase(it);

                LOG_F(DEBUG, "INDIGO: Property deleted: {}", key);

                if (impl->propertyDefineCallback_) {
                    impl->propertyDefineCallback_(prop, false);
                }
            }
        }

        return INDIGO_OK;
    }

    static indigo_result messageCallback(indigo_client* client,
                                         indigo_device* device,
                                         const char* message) {
        auto* impl = static_cast<Impl*>(client->client_context);

        if (message && impl->messageCallback_) {
            impl->messageCallback_(device ? device->name : "",
                                   std::string(message));
        }

        return INDIGO_OK;
    }

    static Property convertProperty(indigo_device* device,
                                    indigo_property* prop) {
        Property result;
        result.device = device->name;
        result.name = prop->name;
        result.group = prop->group;
        result.label = prop->label;
        result.state = convertState(prop->state);
        result.permission = convertPermission(prop->perm);

        switch (prop->type) {
            case INDIGO_TEXT_VECTOR:
                result.type = PropertyType::Text;
                for (int i = 0; i < prop->count; ++i) {
                    TextElement elem;
                    elem.name = prop->items[i].name;
                    elem.label = prop->items[i].label;
                    elem.value = prop->items[i].text.value;
                    result.textElements.push_back(elem);
                }
                break;

            case INDIGO_NUMBER_VECTOR:
                result.type = PropertyType::Number;
                for (int i = 0; i < prop->count; ++i) {
                    NumberElement elem;
                    elem.name = prop->items[i].name;
                    elem.label = prop->items[i].label;
                    elem.value = prop->items[i].number.value;
                    elem.min = prop->items[i].number.min;
                    elem.max = prop->items[i].number.max;
                    elem.step = prop->items[i].number.step;
                    elem.format = prop->items[i].number.format;
                    elem.target = prop->items[i].number.target;
                    result.numberElements.push_back(elem);
                }
                break;

            case INDIGO_SWITCH_VECTOR:
                result.type = PropertyType::Switch;
                result.switchRule = convertSwitchRule(prop->rule);
                for (int i = 0; i < prop->count; ++i) {
                    SwitchElement elem;
                    elem.name = prop->items[i].name;
                    elem.label = prop->items[i].label;
                    elem.value = prop->items[i].sw.value;
                    result.switchElements.push_back(elem);
                }
                break;

            case INDIGO_LIGHT_VECTOR:
                result.type = PropertyType::Light;
                for (int i = 0; i < prop->count; ++i) {
                    LightElement elem;
                    elem.name = prop->items[i].name;
                    elem.label = prop->items[i].label;
                    elem.state = convertState(prop->items[i].light.value);
                    result.lightElements.push_back(elem);
                }
                break;

            case INDIGO_BLOB_VECTOR:
                result.type = PropertyType::Blob;
                for (int i = 0; i < prop->count; ++i) {
                    BlobElement elem;
                    elem.name = prop->items[i].name;
                    elem.label = prop->items[i].label;
                    elem.format = prop->items[i].blob.format;
                    elem.size = prop->items[i].blob.size;
                    elem.url = prop->items[i].blob.url ? prop->items[i].blob.url : "";
                    // Copy blob data if not using URL mode
                    if (prop->items[i].blob.value && prop->items[i].blob.size > 0 &&
                        elem.url.empty()) {
                        elem.data.assign(
                            static_cast<uint8_t*>(prop->items[i].blob.value),
                            static_cast<uint8_t*>(prop->items[i].blob.value) +
                                prop->items[i].blob.size);
                    }
                    result.blobElements.push_back(elem);
                }
                break;

            default:
                result.type = PropertyType::Unknown;
                break;
        }

        return result;
    }

    static PropertyState convertState(indigo_property_state state) {
        switch (state) {
            case INDIGO_IDLE_STATE:
                return PropertyState::Idle;
            case INDIGO_OK_STATE:
                return PropertyState::Ok;
            case INDIGO_BUSY_STATE:
                return PropertyState::Busy;
            case INDIGO_ALERT_STATE:
                return PropertyState::Alert;
            default:
                return PropertyState::Unknown;
        }
    }

    static PropertyPermission convertPermission(indigo_property_perm perm) {
        switch (perm) {
            case INDIGO_RO_PERM:
                return PropertyPermission::ReadOnly;
            case INDIGO_WO_PERM:
                return PropertyPermission::WriteOnly;
            case INDIGO_RW_PERM:
                return PropertyPermission::ReadWrite;
            default:
                return PropertyPermission::ReadOnly;
        }
    }

    static SwitchRule convertSwitchRule(indigo_rule rule) {
        switch (rule) {
            case INDIGO_ONE_OF_MANY_RULE:
                return SwitchRule::OneOfMany;
            case INDIGO_AT_MOST_ONE_RULE:
                return SwitchRule::AtMostOne;
            case INDIGO_ANY_OF_MANY_RULE:
                return SwitchRule::AnyOfMany;
            default:
                return SwitchRule::OneOfMany;
        }
    }

    void updateDeviceInfo(indigo_device* device, indigo_property* property) {
        auto it = devices_.find(device->name);
        if (it == devices_.end())
            return;

        for (int i = 0; i < property->count; ++i) {
            std::string name = property->items[i].name;
            if (name == "DEVICE_DRIVER") {
                it->second.driver = property->items[i].text.value;
            } else if (name == "DEVICE_VERSION") {
                it->second.version = property->items[i].text.value;
            } else if (name == "DEVICE_INTERFACE") {
                it->second.interfaces = static_cast<DeviceInterface>(
                    static_cast<uint32_t>(std::stoul(property->items[i].text.value)));
            }
        }
    }

    void updateConnectionStatus(indigo_device* device,
                                indigo_property* property) {
        auto it = devices_.find(device->name);
        if (it == devices_.end())
            return;

        for (int i = 0; i < property->count; ++i) {
            if (std::string(property->items[i].name) == "CONNECTED") {
                it->second.connected = property->items[i].sw.value;
                break;
            }
        }
    }

    Config config_;
    std::atomic<ConnectionState> state_{ConnectionState::Disconnected};
    std::string lastError_;

    indigo_client serverEntry_{};

    mutable std::mutex mutex_;
    std::mutex connectMutex_;
    std::condition_variable connectCv_;
    std::mutex callbackMutex_;

    std::unordered_map<std::string, DiscoveredDevice> devices_;
    std::unordered_map<std::string, Property> properties_;

    DeviceCallback deviceCallback_;
    PropertyDefineCallback propertyDefineCallback_;
    PropertyUpdateCallback propertyUpdateCallback_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
};

#else  // !INDIGO_PLATFORM_SUPPORTED

// Stub implementation for unsupported platforms
class INDIGOClient::Impl {
public:
    explicit Impl(const Config& config) : config_(config) {}
    ~Impl() = default;

    auto connect(const std::string& /*host*/, int /*port*/) -> DeviceResult<bool> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto disconnect() -> DeviceResult<bool> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    [[nodiscard]] auto isConnected() const -> bool { return false; }
    [[nodiscard]] auto getConnectionState() const -> ConnectionState {
        return ConnectionState::Disconnected;
    }
    [[nodiscard]] auto getLastError() const -> std::string {
        return "INDIGO is not supported on this platform";
    }

    auto discoverDevices() -> DeviceResult<std::vector<DiscoveredDevice>> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    [[nodiscard]] auto getDiscoveredDevices() const -> std::vector<DiscoveredDevice> {
        return {};
    }

    auto connectDevice(const std::string& /*deviceName*/) -> DeviceResult<bool> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto disconnectDevice(const std::string& /*deviceName*/) -> DeviceResult<bool> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto getProperty(const std::string& /*deviceName*/,
                     const std::string& /*propertyName*/) -> DeviceResult<Property> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto getDeviceProperties(const std::string& /*deviceName*/)
        -> DeviceResult<std::vector<Property>> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto setTextProperty(
        const std::string& /*deviceName*/, const std::string& /*propertyName*/,
        const std::vector<std::pair<std::string, std::string>>& /*elements*/)
        -> DeviceResult<bool> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto setNumberProperty(
        const std::string& /*deviceName*/, const std::string& /*propertyName*/,
        const std::vector<std::pair<std::string, double>>& /*elements*/)
        -> DeviceResult<bool> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto setSwitchProperty(
        const std::string& /*deviceName*/, const std::string& /*propertyName*/,
        const std::vector<std::pair<std::string, bool>>& /*elements*/)
        -> DeviceResult<bool> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto enableBlob(const std::string& /*deviceName*/, bool /*enable*/,
                    bool /*urlMode*/) -> DeviceResult<bool> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    auto fetchBlobUrl(const std::string& /*url*/)
        -> DeviceResult<std::vector<uint8_t>> {
        return DeviceError{DeviceErrorCode::NotSupported,
                           "INDIGO is not supported on this platform"};
    }

    void setDeviceCallback(DeviceCallback /*callback*/) {}
    void setPropertyDefineCallback(PropertyDefineCallback /*callback*/) {}
    void setPropertyUpdateCallback(PropertyUpdateCallback /*callback*/) {}
    void setConnectionCallback(ConnectionCallback /*callback*/) {}
    void setMessageCallback(MessageCallback /*callback*/) {}

    [[nodiscard]] auto getConfig() const -> const Config& { return config_; }

    auto setConfig(const Config& config) -> DeviceResult<bool> {
        config_ = config;
        return DeviceResult<bool>(true);
    }

    [[nodiscard]] auto getStatistics() const -> json {
        return {{"supported", false}};
    }

private:
    Config config_;
};

#endif  // INDIGO_PLATFORM_SUPPORTED

// ============================================================================
// INDIGOClient public interface delegation
// ============================================================================

INDIGOClient::INDIGOClient() : impl_(std::make_unique<Impl>(Config{})) {}

INDIGOClient::INDIGOClient(const Config& config)
    : impl_(std::make_unique<Impl>(config)) {}

INDIGOClient::~INDIGOClient() = default;

INDIGOClient::INDIGOClient(INDIGOClient&&) noexcept = default;
INDIGOClient& INDIGOClient::operator=(INDIGOClient&&) noexcept = default;

auto INDIGOClient::connect(const std::string& host, int port)
    -> DeviceResult<bool> {
    return impl_->connect(host, port);
}

auto INDIGOClient::disconnect() -> DeviceResult<bool> {
    return impl_->disconnect();
}

auto INDIGOClient::isConnected() const -> bool {
    return impl_->isConnected();
}

auto INDIGOClient::getConnectionState() const -> ConnectionState {
    return impl_->getConnectionState();
}

auto INDIGOClient::getLastError() const -> std::string {
    return impl_->getLastError();
}

auto INDIGOClient::discoverDevices()
    -> DeviceResult<std::vector<DiscoveredDevice>> {
    return impl_->discoverDevices();
}

auto INDIGOClient::getDiscoveredDevices() const -> std::vector<DiscoveredDevice> {
    return impl_->getDiscoveredDevices();
}

auto INDIGOClient::connectDevice(const std::string& deviceName)
    -> DeviceResult<bool> {
    return impl_->connectDevice(deviceName);
}

auto INDIGOClient::disconnectDevice(const std::string& deviceName)
    -> DeviceResult<bool> {
    return impl_->disconnectDevice(deviceName);
}

auto INDIGOClient::getProperty(const std::string& deviceName,
                               const std::string& propertyName)
    -> DeviceResult<Property> {
    return impl_->getProperty(deviceName, propertyName);
}

auto INDIGOClient::getDeviceProperties(const std::string& deviceName)
    -> DeviceResult<std::vector<Property>> {
    return impl_->getDeviceProperties(deviceName);
}

auto INDIGOClient::setTextProperty(
    const std::string& deviceName, const std::string& propertyName,
    const std::vector<std::pair<std::string, std::string>>& elements)
    -> DeviceResult<bool> {
    return impl_->setTextProperty(deviceName, propertyName, elements);
}

auto INDIGOClient::setNumberProperty(
    const std::string& deviceName, const std::string& propertyName,
    const std::vector<std::pair<std::string, double>>& elements)
    -> DeviceResult<bool> {
    return impl_->setNumberProperty(deviceName, propertyName, elements);
}

auto INDIGOClient::setSwitchProperty(
    const std::string& deviceName, const std::string& propertyName,
    const std::vector<std::pair<std::string, bool>>& elements)
    -> DeviceResult<bool> {
    return impl_->setSwitchProperty(deviceName, propertyName, elements);
}

auto INDIGOClient::enableBlob(const std::string& deviceName, bool enable,
                              bool urlMode) -> DeviceResult<bool> {
    return impl_->enableBlob(deviceName, enable, urlMode);
}

auto INDIGOClient::fetchBlobUrl(const std::string& url)
    -> DeviceResult<std::vector<uint8_t>> {
    return impl_->fetchBlobUrl(url);
}

void INDIGOClient::setDeviceCallback(DeviceCallback callback) {
    impl_->setDeviceCallback(std::move(callback));
}

void INDIGOClient::setPropertyDefineCallback(PropertyDefineCallback callback) {
    impl_->setPropertyDefineCallback(std::move(callback));
}

void INDIGOClient::setPropertyUpdateCallback(PropertyUpdateCallback callback) {
    impl_->setPropertyUpdateCallback(std::move(callback));
}

void INDIGOClient::setConnectionCallback(ConnectionCallback callback) {
    impl_->setConnectionCallback(std::move(callback));
}

void INDIGOClient::setMessageCallback(MessageCallback callback) {
    impl_->setMessageCallback(std::move(callback));
}

auto INDIGOClient::getConfig() const -> const Config& {
    return impl_->getConfig();
}

auto INDIGOClient::setConfig(const Config& config) -> DeviceResult<bool> {
    return impl_->setConfig(config);
}

auto INDIGOClient::getStatistics() const -> json {
    return impl_->getStatistics();
}

}  // namespace lithium::client::indigo
