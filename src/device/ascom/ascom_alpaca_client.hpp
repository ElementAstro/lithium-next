/*
 * ascom_alpaca_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced ASCOM Alpaca REST Client - API Version 9 Compatible

**************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#include <curl/curl.h>
#endif

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

// Use modern JSON library
using json = nlohmann::json;

// HTTP method enumeration
enum class HttpMethod { GET, PUT, POST, DELETE, HEAD, OPTIONS };

// ASCOM Alpaca API version enumeration
enum class AlpacaAPIVersion { V1 = 1, V2 = 2, V3 = 3 };

// ASCOM Device Types (as per API v9)
enum class AscomDeviceType {
    Camera,
    CoverCalibrator,
    Dome,
    FilterWheel,
    Focuser,
    ObservingConditions,
    Rotator,
    SafetyMonitor,
    Switch,
    Telescope
};

// Discovery Protocol Version
enum class DiscoveryProtocol { IPv4, IPv6 };

// ImageBytes transfer format
enum class ImageFormat { Int16Array, Int32Array, DoubleArray, ByteArray };

// ASCOM Error codes (as per API v9)
enum class AscomErrorCode {
    OK = 0x0,
    ActionNotImplemented = 0x40C,
    InvalidValue = 0x401,
    ValueNotSet = 0x402,
    NotConnected = 0x407,
    InvalidWhileParked = 0x408,
    InvalidWhileSlaved = 0x409,
    InvalidOperationException = 0x40B,
    UnspecifiedError = 0x500
};

// Forward declarations
struct AlpacaDevice;
struct AlpacaResponse;
struct AlpacaError;
struct AlpacaManagementInfo;
struct AlpacaConfiguredDevice;
struct ImageBytesMetadata;

// Alpaca error information (API v9 compliant)
struct AlpacaError {
    int error_number;
    std::string message;

    // Helper methods
    bool isSuccess() const { return error_number == 0; }
    bool isRetryable() const {
        return error_number == 0x500 || error_number == 0x407;
    }
    AscomErrorCode getErrorCode() const {
        return static_cast<AscomErrorCode>(error_number);
    }
};

// Enhanced Alpaca device information (API v9)
struct AlpacaDevice {
    std::string device_name;
    std::string device_type;
    int device_number;
    std::string unique_id;
    std::string description;
    std::string driver_info;
    std::string driver_version;
    int interface_version;
    std::vector<int> supported_actions;

    // Device-specific properties
    std::unordered_map<std::string, json> properties;

    // Connection info
    std::string host;
    int port;
    bool ssl_enabled = false;
};

// Management API information (API v9)
struct AlpacaManagementInfo {
    std::string server_name;
    std::string manufacturer;
    std::string manufacturer_version;
    std::string location;
    std::vector<int> supported_api_versions;
};

// Configured device information
struct AlpacaConfiguredDevice {
    std::string device_name;
    std::string device_type;
    int device_number;
    std::string unique_id;
    bool enabled;
    std::unordered_map<std::string, json> configuration;
};

// ImageBytes metadata (API v9)
struct ImageBytesMetadata {
    int client_transaction_id;
    int server_transaction_id;
    int error_number;
    std::string error_message;

    // Image data properties
    int image_element_type;         // Data type code
    int transmission_element_type;  // Transmission format
    int rank;                       // Number of dimensions
    std::vector<int> dimension;     // Size of each dimension

    // Helper methods
    bool isSuccess() const { return error_number == 0; }
    size_t getTotalElements() const {
        size_t total = 1;
        for (int dim : dimension)
            total *= dim;
        return total;
    }
    size_t getElementSize() const {
        switch (transmission_element_type) {
            case 1:
                return 1;  // byte
            case 2:
                return 2;  // int16
            case 3:
                return 4;  // int32
            case 4:
                return 8;  // int64
            case 5:
                return 4;  // float
            case 6:
                return 8;  // double
            default:
                return 0;
        }
    }
};

// Alpaca device discovery response (enhanced)
struct AlpacaDiscoveryResponse {
    std::string alpaca_port;
    std::vector<AlpacaDevice> devices;
    std::string server_name;
    std::string server_version;
    std::string discovery_protocol_version;
    std::chrono::system_clock::time_point discovery_time;
};

// Standard Alpaca API response wrapper (API v9 compliant)
struct AlpacaResponse {
    json value;
    int client_transaction_id;
    int server_transaction_id;
    std::optional<AlpacaError> error_info;

    // Timing information
    std::chrono::system_clock::time_point request_time;
    std::chrono::system_clock::time_point response_time;
    std::chrono::milliseconds response_duration;

    bool isSuccess() const {
        return !error_info.has_value() || error_info->isSuccess();
    }
    std::string getErrorMessage() const {
        return error_info ? error_info->message : "Success";
    }
    int getErrorNumber() const {
        return error_info ? error_info->error_number : 0;
    }
};

// Enhanced HTTP response structure
struct HttpResponse {
    long status_code;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    bool success;
    std::string error_message;

    // Enhanced fields for API v9
    std::chrono::milliseconds response_time;
    size_t content_length;
    std::string content_type;
    std::string server_version;
    bool compressed;

    // SSL information
    bool ssl_used;
    std::string ssl_version;
    std::string ssl_cipher;
};

// Enhanced Alpaca REST client (API v9 compliant)
class ASCOMAlpacaClient {
public:
    ASCOMAlpacaClient();
    ~ASCOMAlpacaClient();

    // Initialization and cleanup
    bool initialize();
    void cleanup();

    // API Version Management
    std::vector<int> getSupportedAPIVersions();
    bool setAPIVersion(AlpacaAPIVersion version);
    AlpacaAPIVersion getCurrentAPIVersion() const { return api_version_; }

    // Connection configuration
    void setServerAddress(const std::string& host, int port);
    void setDeviceInfo(const std::string& deviceType, int deviceNumber);
    void setDeviceInfo(AscomDeviceType deviceType, int deviceNumber);
    void setClientId(int clientId) { client_id_ = clientId; }
    void setTimeout(int timeoutSeconds) { timeout_seconds_ = timeoutSeconds; }
    void setRetryCount(int retryCount) { retry_count_ = retryCount; }

    // Management API (API v9)
    std::optional<AlpacaManagementInfo> getManagementInfo();
    std::vector<AlpacaConfiguredDevice> getConfiguredDevices();
    std::optional<std::string> getDescription();
    std::optional<std::string> getDriverInfo();
    std::optional<std::string> getDriverVersion();
    std::optional<int> getInterfaceVersion();
    std::vector<std::string> getSupportedActions();

    // Device discovery (enhanced)
    std::vector<AlpacaDevice> discoverDevices(
        const std::string& host = "", int port = 11111,
        DiscoveryProtocol protocol = DiscoveryProtocol::IPv4);
    std::optional<AlpacaDevice> findDevice(const std::string& deviceType,
                                           const std::string& deviceName = "");
    std::optional<AlpacaDevice> findDevice(AscomDeviceType deviceType,
                                           const std::string& deviceName = "");

    // Connection management
    bool testConnection();
    bool connect();
    bool disconnect();
    bool isConnected() const { return is_connected_.load(); }

    // Property operations (enhanced)
    std::optional<json> getProperty(const std::string& property);
    bool setProperty(const std::string& property, const json& value);

    // Type-safe property operations
    template <typename T>
    std::optional<T> getTypedProperty(const std::string& property);
    template <typename T>
    bool setTypedProperty(const std::string& property, const T& value);

    // Method invocation (enhanced)
    std::optional<json> invokeMethod(const std::string& method);
    std::optional<json> invokeMethod(
        const std::string& method,
        const std::unordered_map<std::string, json>& parameters);
    std::optional<json> invokeAction(const std::string& action,
                                     const std::string& parameters = "");

    // Batch operations (enhanced)
    std::unordered_map<std::string, json> getMultipleProperties(
        const std::vector<std::string>& properties);
    bool setMultipleProperties(
        const std::unordered_map<std::string, json>& properties);

    // ImageBytes operations (API v9 new feature)
    std::optional<std::vector<uint8_t>> getImageArray();
    std::optional<std::vector<uint16_t>> getImageArrayAsUInt16();
    std::optional<std::vector<uint32_t>> getImageArrayAsUInt32();
    std::optional<std::vector<double>> getImageArrayAsDouble();

    // Enhanced ImageBytes with metadata
    std::pair<ImageBytesMetadata, std::vector<uint8_t>> getImageBytes();
    bool supportsImageBytes();

    // Asynchronous operations (enhanced)
    std::future<std::optional<json>> getPropertyAsync(
        const std::string& property);
    std::future<bool> setPropertyAsync(const std::string& property,
                                       const json& value);
    std::future<std::optional<json>> invokeMethodAsync(
        const std::string& method);
    std::future<std::optional<json>> invokeActionAsync(
        const std::string& action, const std::string& parameters = "");

    // Event polling (enhanced for devices that support events)
    void startEventPolling(
        std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    void stopEventPolling();
    void setEventCallback(
        std::function<void(const std::string&, const json&)> callback);

    // Transaction management (API v9)
    int getNextClientTransactionId();
    void setServerTransactionId(int id) { last_server_transaction_id_ = id; }
    int getLastServerTransactionId() const {
        return last_server_transaction_id_;
    }

    // Error handling (enhanced)
    std::string getLastError() const { return last_error_; }
    int getLastErrorCode() const { return last_error_code_; }
    AscomErrorCode getLastAscomError() const {
        return static_cast<AscomErrorCode>(last_error_code_);
    }
    void clearError();

    // Statistics and monitoring (enhanced)
    size_t getRequestCount() const { return request_count_.load(); }
    size_t getSuccessfulRequests() const { return successful_requests_.load(); }
    size_t getFailedRequests() const { return failed_requests_.load(); }
    double getAverageResponseTime() const;
    double getSuccessRate() const;
    void resetStatistics();

    // Advanced features (enhanced)
    void enableCompression(bool enable) { compression_enabled_ = enable; }
    void enableKeepAlive(bool enable) { keep_alive_enabled_ = enable; }
    void setUserAgent(const std::string& userAgent) { user_agent_ = userAgent; }
    void addCustomHeader(const std::string& name, const std::string& value);
    void removeCustomHeader(const std::string& name);

    // SSL/TLS configuration (enhanced)
    void enableSSL(bool enable) { ssl_enabled_ = enable; }
    void setSSLCertificatePath(const std::string& path) {
        ssl_cert_path_ = path;
    }
    void setSSLKeyPath(const std::string& path) { ssl_key_path_ = path; }
    void setSSLVerifyPeer(bool verify) { ssl_verify_peer_ = verify; }
    void setSSLCipherList(const std::string& ciphers) {
        ssl_cipher_list_ = ciphers;
    }

    // Logging and debugging (enhanced)
    void enableVerboseLogging(bool enable) { verbose_logging_ = enable; }
    void setLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }
    void enableRequestResponseLogging(bool enable) {
        log_requests_responses_ = enable;
    }

    // Caching and optimization
    void enableResponseCaching(
        bool enable, std::chrono::seconds ttl = std::chrono::seconds(30));
    void clearCache();
    void enableRequestQueuing(bool enable) {
        request_queuing_enabled_ = enable;
    }

private:
    // Core HTTP operations (enhanced)
    HttpResponse performRequest(HttpMethod method, const std::string& endpoint,
                                const std::string& params = "",
                                const std::string& body = "");
    HttpResponse performRequestWithRetry(HttpMethod method,
                                         const std::string& endpoint,
                                         const std::string& params = "",
                                         const std::string& body = "");

    // URL building (enhanced)
    std::string buildURL(const std::string& endpoint) const;
    std::string buildManagementURL(const std::string& endpoint) const;
    std::string buildParameters(
        const std::unordered_map<std::string, json>& params) const;
    std::string buildTransactionParameters() const;

    // Response parsing (enhanced)
    std::optional<AlpacaResponse> parseAlpacaResponse(
        const HttpResponse& httpResponse);
    std::optional<json> extractValue(const AlpacaResponse& response);
    ImageBytesMetadata parseImageBytesMetadata(
        const std::vector<uint8_t>& data);
    std::vector<uint8_t> extractImageBytesData(
        const std::vector<uint8_t>& data, const ImageBytesMetadata& metadata);

    // Error handling (enhanced)
    void setError(const std::string& message, int code = 0);
    void setAscomError(AscomErrorCode code, const std::string& message = "");
    void updateStatistics(bool success, std::chrono::milliseconds responseTime);
    bool shouldRetryRequest(const HttpResponse& response) const;

    // Transaction management
    int generateClientTransactionId();
    void updateTransactionIds(const AlpacaResponse& response);

    // Caching
    struct CacheEntry {
        json value;
        std::chrono::system_clock::time_point timestamp;
        std::chrono::seconds ttl;
    };
    std::optional<json> getCachedResponse(const std::string& key);
    void setCachedResponse(const std::string& key, const json& value,
                           std::chrono::seconds ttl);
    std::string generateCacheKey(const std::string& endpoint,
                                 const std::string& params) const;

    // Event polling (enhanced)
    void eventPollingLoop();
    void processEvent(const std::string& eventType, const json& eventData);

    // Device type specific operations
    std::string deviceTypeToString(AscomDeviceType type) const;
    AscomDeviceType stringToDeviceType(const std::string& type) const;

    // Utility methods (enhanced)
    std::string escapeUrl(const std::string& str) const;
    std::string jsonToString(const json& j);
    std::optional<json> stringToJson(const std::string& str);
    std::string formatHttpHeaders(
        const std::unordered_map<std::string, std::string>& headers) const;
    void logRequest(const std::string& method, const std::string& url,
                    const std::string& body = "") const;
    void logResponse(const HttpResponse& response) const;

#ifndef _WIN32
    // cURL specific methods (enhanced)
    bool initializeCurl();
    void cleanupCurl();
    void configureCurlOptions(CURL* curl);
    void configureCurlSSL(CURL* curl);
    void configureCurlHeaders(CURL* curl);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb,
                                std::string* response);
    static size_t headerCallback(
        void* contents, size_t size, size_t nmemb,
        std::unordered_map<std::string, std::string>* headers);
    static size_t progressCallback(void* clientp, curl_off_t dltotal,
                                   curl_off_t dlnow, curl_off_t ultotal,
                                   curl_off_t ulnow);

    CURL* curl_handle_;
    struct curl_slist* curl_headers_;
#endif

    // API Configuration
    AlpacaAPIVersion api_version_;
    std::vector<int> supported_api_versions_;

    // Connection configuration (enhanced)
    std::string host_;
    int port_;
    std::string device_type_;
    AscomDeviceType device_type_enum_;
    int device_number_;
    int client_id_;
    int timeout_seconds_;
    int retry_count_;

    // Transaction management
    std::atomic<int> client_transaction_id_;
    int last_server_transaction_id_;

    // State (enhanced)
    std::atomic<bool> is_connected_;
    std::atomic<bool> initialized_;
    std::string last_error_;
    int last_error_code_;
    std::chrono::system_clock::time_point last_request_time_;
    std::chrono::system_clock::time_point last_response_time_;

    // Event polling (enhanced)
    std::atomic<bool> event_polling_active_;
    std::unique_ptr<std::thread> event_thread_;
    std::chrono::milliseconds event_polling_interval_;
    std::function<void(const std::string&, const json&)> event_callback_;
    std::queue<std::pair<std::string, json>> event_queue_;
    std::mutex event_queue_mutex_;

    // Statistics (enhanced)
    std::atomic<size_t> request_count_;
    std::atomic<size_t> successful_requests_;
    std::atomic<size_t> failed_requests_;
    std::vector<std::chrono::milliseconds> response_times_;
    std::mutex stats_mutex_;

    // HTTP configuration (enhanced)
    bool compression_enabled_;
    bool keep_alive_enabled_;
    std::string user_agent_;
    std::unordered_map<std::string, std::string> custom_headers_;

    // SSL configuration (enhanced)
    bool ssl_enabled_;
    std::string ssl_cert_path_;
    std::string ssl_key_path_;
    bool ssl_verify_peer_;
    std::string ssl_cipher_list_;

    // Logging (enhanced)
    bool verbose_logging_;
    bool log_requests_responses_;
    std::function<void(const std::string&)> log_callback_;

    // Caching system
    bool caching_enabled_;
    std::chrono::seconds default_cache_ttl_;
    std::unordered_map<std::string, CacheEntry> response_cache_;
    std::mutex cache_mutex_;

    // Request queuing
    bool request_queuing_enabled_;
    std::queue<std::function<void()>> request_queue_;
    std::mutex request_queue_mutex_;
    std::condition_variable request_queue_cv_;
    std::unique_ptr<std::thread> request_processor_thread_;

    // Thread safety (enhanced)
    std::mutex request_mutex_;
    std::mutex error_mutex_;
    std::mutex connection_mutex_;

    // Helper methods
    std::string methodToString(HttpMethod method);
    std::vector<AlpacaDevice> queryDevicesFromHost(const std::string& host,
                                                   int port);

    // Device-specific caches
    std::unordered_map<std::string, json> property_cache_;
    std::chrono::system_clock::time_point property_cache_time_;
    std::mutex property_cache_mutex_;
};

// Enhanced Alpaca device discovery helper (API v9 compliant)
class AlpacaDiscovery {
public:
    static std::vector<AlpacaDevice> discoverAllDevices(
        int timeoutSeconds = 5,
        DiscoveryProtocol protocol = DiscoveryProtocol::IPv4);
    static std::vector<std::string> discoverHosts(
        int timeoutSeconds = 5,
        DiscoveryProtocol protocol = DiscoveryProtocol::IPv4);
    static std::vector<AlpacaDiscoveryResponse> discoverServers(
        int timeoutSeconds = 5,
        DiscoveryProtocol protocol = DiscoveryProtocol::IPv4);
    static bool isAlpacaServer(const std::string& host, int port);
    static std::optional<AlpacaManagementInfo> getServerInfo(
        const std::string& host, int port);

    // IPv6 specific discovery
    static std::vector<std::string> discoverHostsIPv6(int timeoutSeconds = 5);
    static std::vector<AlpacaDiscoveryResponse> discoverServersIPv6(
        int timeoutSeconds = 5);

    // Network interface discovery
    static std::vector<std::string> getNetworkInterfaces();
    static std::vector<std::string> getBroadcastAddresses();

private:
    static constexpr int ALPACA_DISCOVERY_PORT = 32227;
    static constexpr const char* ALPACA_DISCOVERY_MESSAGE = "alpacadiscovery1";
    static constexpr const char* ALPACA_DISCOVERY_IPV6_GROUP = "ff12::a1ca";

    // Platform-specific socket operations
    static int createUDPSocket(bool ipv6 = false);
    static bool sendDiscoveryMessage(int socket, const std::string& address,
                                     int port, bool ipv6 = false);
    static std::vector<std::string> receiveDiscoveryResponses(
        int socket, int timeoutSeconds);
    static void closeSocket(int socket);

    // Response parsing
    static std::optional<AlpacaDiscoveryResponse> parseDiscoveryResponse(
        const std::string& response, const std::string& sourceAddress);
};

// Device-specific client classes (API v9)
class AlpacaCameraClient : public ASCOMAlpacaClient {
public:
    AlpacaCameraClient() { setDeviceInfo(AscomDeviceType::Camera, 0); }

    // Camera-specific methods
    std::optional<double> getCCDTemperature();
    bool setCCDTemperature(double temperature);
    std::optional<bool> getCoolerOn();
    bool setCoolerOn(bool on);
    std::optional<int> getBinX();
    bool setBinX(int binning);
    std::optional<int> getBinY();
    bool setBinY(int binning);
    std::optional<double> getExposureTime();
    bool startExposure(double duration, bool light = true);
    bool abortExposure();
    std::optional<bool> getImageReady();

    // Enhanced ImageBytes for cameras
    std::pair<ImageBytesMetadata, std::vector<uint16_t>> getImageArrayUInt16();
    std::pair<ImageBytesMetadata, std::vector<uint32_t>> getImageArrayUInt32();
};

class AlpacaTelescopeClient : public ASCOMAlpacaClient {
public:
    AlpacaTelescopeClient() { setDeviceInfo(AscomDeviceType::Telescope, 0); }

    // Telescope-specific methods
    std::optional<double> getRightAscension();
    std::optional<double> getDeclination();
    std::optional<double> getAzimuth();
    std::optional<double> getAltitude();
    bool slewToCoordinates(double ra, double dec);
    bool slewToAltAz(double altitude, double azimuth);
    bool abortSlew();
    std::optional<bool> getSlewing();
    std::optional<bool> getAtPark();
    bool park();
    bool unpark();
    std::optional<bool> getCanPark();
    std::optional<bool> getCanSlew();
};

class AlpacaFocuserClient : public ASCOMAlpacaClient {
public:
    AlpacaFocuserClient() { setDeviceInfo(AscomDeviceType::Focuser, 0); }

    // Focuser-specific methods
    std::optional<int> getPosition();
    bool move(int position);
    bool halt();
    std::optional<bool> getIsMoving();
    std::optional<int> getMaxStep();
    std::optional<double> getStepSize();
    std::optional<bool> getTempComp();
    bool setTempComp(bool enabled);
    std::optional<double> getTemperature();
};

// Utility functions for ASCOM Alpaca (API v9 enhanced)
namespace AlpacaUtils {
// JSON conversion functions (enhanced)
json toJson(bool value);
json toJson(int value);
json toJson(double value);
json toJson(const std::string& value);
json toJson(const std::vector<std::string>& value);
json toJson(const std::vector<int>& value);
json toJson(const std::vector<double>& value);
json toJson(const std::chrono::system_clock::time_point& value);

template <typename T>
std::optional<T> fromJson(const json& j);

// Image array conversions (enhanced)
std::vector<uint8_t> jsonArrayToUInt8(const json& jsonArray);
std::vector<uint16_t> jsonArrayToUInt16(const json& jsonArray);
std::vector<uint32_t> jsonArrayToUInt32(const json& jsonArray);
std::vector<double> jsonArrayToDouble(const json& jsonArray);

// Binary data conversion for ImageBytes
std::vector<uint8_t> convertImageData(const std::vector<uint16_t>& source);
std::vector<uint8_t> convertImageData(const std::vector<uint32_t>& source);
std::vector<uint8_t> convertImageData(const std::vector<double>& source);

template <typename T>
std::vector<T> convertFromBytes(const std::vector<uint8_t>& bytes);

// ASCOM error handling
std::string getErrorDescription(int errorCode);
std::string getAscomErrorDescription(AscomErrorCode errorCode);
bool isRetryableError(int errorCode);
bool isAscomError(int errorCode);
AscomErrorCode intToAscomError(int errorCode);

// Device type utilities
std::string deviceTypeToString(AscomDeviceType type);
AscomDeviceType stringToDeviceType(const std::string& typeStr);
std::vector<std::string> getSupportedDeviceTypes();
bool isValidDeviceType(const std::string& type);

// URL and parameter utilities
std::string urlEncode(const std::string& value);
std::string urlDecode(const std::string& value);
std::unordered_map<std::string, std::string> parseQueryString(
    const std::string& query);
std::string buildQueryString(
    const std::unordered_map<std::string, std::string>& params);

// Validation utilities
bool isValidClientId(int clientId);
bool isValidTransactionId(int transactionId);
bool isValidDeviceNumber(int deviceNumber);
bool isValidAPIVersion(int version);
bool isValidJSONResponse(const std::string& response);

// Timing utilities
std::string formatTimestamp(const std::chrono::system_clock::time_point& time);
std::chrono::system_clock::time_point parseTimestamp(
    const std::string& timestamp);
std::chrono::milliseconds calculateTimeout(int baseTimeoutSeconds,
                                           int retryCount);

// Network utilities
bool isValidIPAddress(const std::string& ip);
bool isValidPort(int port);
std::string getLocalIPAddress();
std::vector<std::string> getLocalIPAddresses();
bool isLocalAddress(const std::string& address);

// Discovery utilities
std::string formatDiscoveryMessage(const std::string& clientId = "");
std::optional<AlpacaDiscoveryResponse> parseDiscoveryResponse(
    const std::string& response);
bool isValidDiscoveryResponse(const std::string& response);

// Configuration utilities
json createDefaultConfiguration(AscomDeviceType deviceType);
bool validateDeviceConfiguration(const json& config,
                                 AscomDeviceType deviceType);
json mergeConfigurations(const json& base, const json& override);

// Logging utilities
std::string formatLogMessage(const std::string& level,
                             const std::string& message,
                             const std::string& context = "");
void logApiCall(const std::string& method, const std::string& endpoint,
                const std::chrono::milliseconds& duration, bool success);
void logError(const std::string& error, const std::string& context = "");
}  // namespace AlpacaUtils

// Template specializations for type-safe conversions
template <>
std::optional<bool> AlpacaUtils::fromJson<bool>(const json& j);

template <>
std::optional<int> AlpacaUtils::fromJson<int>(const json& j);

template <>
std::optional<double> AlpacaUtils::fromJson<double>(const json& j);

template <>
std::optional<std::string> AlpacaUtils::fromJson<std::string>(const json& j);

template <>
std::optional<std::vector<std::string>>
AlpacaUtils::fromJson<std::vector<std::string>>(const json& j);

template <>
std::optional<std::vector<int>> AlpacaUtils::fromJson<std::vector<int>>(
    const json& j);

template <>
std::optional<std::vector<double>> AlpacaUtils::fromJson<std::vector<double>>(
    const json& j);

// Template implementations for ASCOMAlpacaClient
template <typename T>
std::optional<T> ASCOMAlpacaClient::getTypedProperty(
    const std::string& property) {
    auto result = getProperty(property);
    if (!result.has_value()) {
        return std::nullopt;
    }
    return AlpacaUtils::fromJson<T>(result.value());
}

template <typename T>
bool ASCOMAlpacaClient::setTypedProperty(const std::string& property,
                                         const T& value) {
    json jsonValue = AlpacaUtils::toJson(value);
    return setProperty(property, jsonValue);
}

// Binary data conversion template implementations
template <typename T>
std::vector<T> AlpacaUtils::convertFromBytes(
    const std::vector<uint8_t>& bytes) {
    if (bytes.size() % sizeof(T) != 0) {
        return {};  // Size mismatch
    }

    std::vector<T> result;
    result.reserve(bytes.size() / sizeof(T));

    for (size_t i = 0; i < bytes.size(); i += sizeof(T)) {
        T value;
        std::memcpy(&value, &bytes[i], sizeof(T));
        result.push_back(value);
    }

    return result;
}
