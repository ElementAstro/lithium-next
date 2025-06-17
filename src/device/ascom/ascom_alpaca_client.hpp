/*
 * ascom_alpaca_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced ASCOM Alpaca REST Client

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#include <curl/curl.h>
#endif

#include "atom/log/loguru.hpp"

// HTTP method enumeration
enum class HttpMethod { GET, PUT, POST, DELETE };

// Simple JSON value representation for basic operations
enum class JsonType { Null, Bool, Number, String, Array, Object };

class SimpleJson {
public:
    SimpleJson() : type_(JsonType::Null) {}
    explicit SimpleJson(bool value)
        : type_(JsonType::Bool), bool_value_(value) {}
    explicit SimpleJson(int value)
        : type_(JsonType::Number), number_value_(value) {}
    explicit SimpleJson(double value)
        : type_(JsonType::Number), number_value_(value) {}
    explicit SimpleJson(const std::string& value)
        : type_(JsonType::String), string_value_(value) {}

    JsonType getType() const { return type_; }

    bool asBool() const { return bool_value_; }
    double asNumber() const { return number_value_; }
    const std::string& asString() const { return string_value_; }

    std::string toString() const;
    static SimpleJson fromString(const std::string& str);

private:
    JsonType type_;
    bool bool_value_ = false;
    double number_value_ = 0.0;
    std::string string_value_;
};

using json = SimpleJson;

// Forward declarations
struct AlpacaDevice;
struct AlpacaResponse;
struct AlpacaError;

// Alpaca error information
struct AlpacaError {
    int error_number;
    std::string message;
};

// Alpaca device information
struct AlpacaDevice {
    std::string device_name;
    std::string device_type;
    int device_number;
    std::string unique_id;
};

// Alpaca device discovery response
struct AlpacaDiscoveryResponse {
    std::string alpaca_port;
    std::vector<AlpacaDevice> devices;
};

// Standard Alpaca API response wrapper
struct AlpacaResponse {
    json value;
    int client_transaction_id;
    int server_transaction_id;
    std::optional<AlpacaError> error_info;

    bool isSuccess() const { return !error_info.has_value(); }
    std::string getErrorMessage() const {
        return error_info ? error_info->message : "Success";
    }
};

// HTTP response structure
struct HttpResponse {
    long status_code;
    std::string body;
    std::unordered_map<std::string, std::string> headers;
    bool success;
    std::string error_message;
};

// Advanced Alpaca REST client
class ASCOMAlpacaClient {
public:
    ASCOMAlpacaClient();
    ~ASCOMAlpacaClient();

    // Initialization and cleanup
    bool initialize();
    void cleanup();

    // Connection configuration
    void setServerAddress(const std::string& host, int port);
    void setDeviceInfo(const std::string& deviceType, int deviceNumber);
    void setClientId(int clientId) { client_id_ = clientId; }
    void setTimeout(int timeoutSeconds) { timeout_seconds_ = timeoutSeconds; }
    void setRetryCount(int retryCount) { retry_count_ = retryCount; }

    // Device discovery
    std::vector<AlpacaDevice> discoverDevices(const std::string& host = "",
                                              int port = 11111);
    std::optional<AlpacaDevice> findDevice(const std::string& deviceType,
                                           const std::string& deviceName = "");

    // Connection management
    bool testConnection();
    bool connect();
    bool disconnect();
    bool isConnected() const { return is_connected_.load(); }

    // Property operations
    std::optional<json> getProperty(const std::string& property);
    bool setProperty(const std::string& property, const json& value);

    // Method invocation
    std::optional<json> invokeMethod(const std::string& method);
    std::optional<json> invokeMethod(
        const std::string& method,
        const std::unordered_map<std::string, json>& parameters);

    // Batch operations
    std::unordered_map<std::string, json> getMultipleProperties(
        const std::vector<std::string>& properties);
    bool setMultipleProperties(
        const std::unordered_map<std::string, json>& properties);

    // Image operations (for cameras)
    std::optional<std::vector<uint8_t>> getImageArray();
    std::optional<std::vector<uint16_t>> getImageArrayAsUInt16();
    std::optional<std::vector<uint32_t>> getImageArrayAsUInt32();

    // Asynchronous operations
    std::future<std::optional<json>> getPropertyAsync(
        const std::string& property);
    std::future<bool> setPropertyAsync(const std::string& property,
                                       const json& value);
    std::future<std::optional<json>> invokeMethodAsync(
        const std::string& method);

    // Event polling (for devices that support events)
    void startEventPolling(
        std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    void stopEventPolling();
    void setEventCallback(
        std::function<void(const std::string&, const json&)> callback);

    // Error handling
    std::string getLastError() const { return last_error_; }
    int getLastErrorCode() const { return last_error_code_; }
    void clearError();

    // Statistics and monitoring
    size_t getRequestCount() const { return request_count_.load(); }
    size_t getSuccessfulRequests() const { return successful_requests_.load(); }
    size_t getFailedRequests() const { return failed_requests_.load(); }
    double getAverageResponseTime() const;
    void resetStatistics();

    // Advanced features
    void enableCompression(bool enable) { compression_enabled_ = enable; }
    void enableKeepAlive(bool enable) { keep_alive_enabled_ = enable; }
    void setUserAgent(const std::string& userAgent) { user_agent_ = userAgent; }
    void addCustomHeader(const std::string& name, const std::string& value);
    void removeCustomHeader(const std::string& name);

    // SSL/TLS configuration
    void enableSSL(bool enable) { ssl_enabled_ = enable; }
    void setSSLCertificatePath(const std::string& path) {
        ssl_cert_path_ = path;
    }
    void setSSLKeyPath(const std::string& path) { ssl_key_path_ = path; }
    void setSSLVerifyPeer(bool verify) { ssl_verify_peer_ = verify; }

    // Logging and debugging
    void enableVerboseLogging(bool enable) { verbose_logging_ = enable; }
    void setLogCallback(std::function<void(const std::string&)> callback) {
        log_callback_ = callback;
    }

private:
    // Core HTTP operations
    HttpResponse performRequest(HttpMethod method, const std::string& endpoint,
                                const std::string& params = "",
                                const std::string& body = "");

    // URL building
    std::string buildURL(const std::string& endpoint) const;
    std::string buildParameters(
        const std::unordered_map<std::string, json>& params) const;

    // Response parsing
    std::optional<AlpacaResponse> parseAlpacaResponse(
        const HttpResponse& httpResponse);
    std::optional<json> extractValue(const AlpacaResponse& response);

    // Error handling
    void setError(const std::string& message, int code = 0);
    void updateStatistics(bool success, std::chrono::milliseconds responseTime);

    // Event polling
    void eventPollingLoop();

    // Utility methods
    std::string escapeUrl(const std::string& str) const;
    std::string jsonToString(const json& j);
    std::optional<json> stringToJson(const std::string& str);

#ifndef _WIN32
    // cURL specific methods
    bool initializeCurl();
    void cleanupCurl();
    static size_t writeCallback(void* contents, size_t size, size_t nmemb,
                                std::string* response);
    static size_t headerCallback(
        void* contents, size_t size, size_t nmemb,
        std::unordered_map<std::string, std::string>* headers);

    CURL* curl_handle_;
    struct curl_slist* curl_headers_;
#endif

    // Connection configuration
    std::string host_;
    int port_;
    std::string device_type_;
    int device_number_;
    int client_id_;
    int timeout_seconds_;
    int retry_count_;

    // State
    std::atomic<bool> is_connected_;
    std::atomic<bool> initialized_;
    std::string last_error_;
    int last_error_code_;
    int transaction_id_;

    // Event polling
    std::atomic<bool> event_polling_active_;
    std::unique_ptr<std::thread> event_thread_;
    std::chrono::milliseconds event_polling_interval_;
    std::function<void(const std::string&, const json&)> event_callback_;

    // Statistics
    std::atomic<size_t> request_count_;
    std::atomic<size_t> successful_requests_;
    std::atomic<size_t> failed_requests_;
    std::vector<std::chrono::milliseconds> response_times_;
    std::mutex stats_mutex_;

    // HTTP configuration
    bool compression_enabled_;
    bool keep_alive_enabled_;
    std::string user_agent_;
    std::unordered_map<std::string, std::string> custom_headers_;

    // SSL configuration
    bool ssl_enabled_;
    std::string ssl_cert_path_;
    std::string ssl_key_path_;
    bool ssl_verify_peer_;

    // Logging
    bool verbose_logging_;
    std::function<void(const std::string&)> log_callback_;

    // Thread safety
    std::mutex request_mutex_;
    std::mutex error_mutex_;

    std::string methodToString(HttpMethod method);
    std::vector<AlpacaDevice> queryDevicesFromHost(const std::string& host,
                                                   int port);
};

// Alpaca device discovery helper
class AlpacaDiscovery {
public:
    static std::vector<AlpacaDevice> discoverAllDevices(int timeoutSeconds = 5);
    static std::vector<std::string> discoverHosts(int timeoutSeconds = 5);
    static bool isAlpacaServer(const std::string& host, int port);

private:
    static constexpr int ALPACA_DISCOVERY_PORT = 32227;
    static constexpr const char* ALPACA_DISCOVERY_MESSAGE = "alpacadiscovery1";
};

// Utility functions for JSON conversion
namespace AlpacaUtils {
// Convert various data types to/from JSON for Alpaca API
json toJson(bool value);
json toJson(int value);
json toJson(double value);
json toJson(const std::string& value);
json toJson(const std::vector<std::string>& value);
json toJson(const std::vector<int>& value);
json toJson(const std::vector<double>& value);

template <typename T>
std::optional<T> fromJson(const json& j);

// Image array conversions
std::vector<uint16_t> jsonArrayToUInt16(const json& jsonArray);
std::vector<uint32_t> jsonArrayToUInt32(const json& jsonArray);
std::vector<double> jsonArrayToDouble(const json& jsonArray);

// Error code mappings
std::string getErrorDescription(int errorCode);
bool isRetryableError(int errorCode);
}  // namespace AlpacaUtils

// Template specializations
template <>
std::optional<bool> AlpacaUtils::fromJson<bool>(const json& j);

template <>
std::optional<int> AlpacaUtils::fromJson<int>(const json& j);

template <>
std::optional<double> AlpacaUtils::fromJson<double>(const json& j);

template <>
std::optional<std::string> AlpacaUtils::fromJson<std::string>(const json& j);
