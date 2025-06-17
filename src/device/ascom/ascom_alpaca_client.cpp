/*
 * ascom_alpaca_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: Enhanced ASCOM Alpaca REST Client Implementation

*************************************************/

#include "ascom_alpaca_client.hpp"

#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <thread>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "atom/log/loguru.hpp"

// SimpleJson implementation
std::string SimpleJson::toString() const {
    switch (type_) {
        case JsonType::Null:
            return "null";
        case JsonType::Bool:
            return bool_value_ ? "true" : "false";
        case JsonType::Number:
            return std::to_string(number_value_);
        case JsonType::String:
            return "\"" + string_value_ + "\"";
        case JsonType::Array:
        case JsonType::Object:
        default:
            return "{}";
    }
}

SimpleJson SimpleJson::fromString(const std::string& str) {
    std::string trimmed = str;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed == "null") {
        return SimpleJson();
    } else if (trimmed == "true") {
        return SimpleJson(true);
    } else if (trimmed == "false") {
        return SimpleJson(false);
    } else if (trimmed.front() == '"' && trimmed.back() == '"') {
        return SimpleJson(trimmed.substr(1, trimmed.length() - 2));
    } else {
        try {
            if (trimmed.find('.') != std::string::npos) {
                return SimpleJson(std::stod(trimmed));
            } else {
                return SimpleJson(std::stoi(trimmed));
            }
        } catch (...) {
            return SimpleJson(trimmed);
        }
    }
}

// ASCOMAlpacaClient implementation
ASCOMAlpacaClient::ASCOMAlpacaClient()
    : port_(11111),
      device_number_(0),
      client_id_(1),
      timeout_seconds_(30),
      retry_count_(3),
      is_connected_(false),
      initialized_(false),
      last_error_code_(0),
      transaction_id_(0),
      event_polling_active_(false),
      event_polling_interval_(std::chrono::milliseconds(100)),
      request_count_(0),
      successful_requests_(0),
      failed_requests_(0),
      compression_enabled_(false),
      keep_alive_enabled_(true),
      user_agent_("ASCOM Alpaca Client/1.0"),
      ssl_enabled_(false),
      ssl_verify_peer_(true),
      verbose_logging_(false) {
#ifndef _WIN32
    curl_handle_ = nullptr;
    curl_headers_ = nullptr;
#endif

    LOG_F(INFO, "ASCOMAlpacaClient created");
}

ASCOMAlpacaClient::~ASCOMAlpacaClient() {
    LOG_F(INFO, "ASCOMAlpacaClient destructor called");
    cleanup();
}

bool ASCOMAlpacaClient::initialize() {
    if (initialized_.load()) {
        return true;
    }

    LOG_F(INFO, "Initializing ASCOM Alpaca Client");

#ifndef _WIN32
    if (!initializeCurl()) {
        return false;
    }
#endif

    // Generate random client ID if not set
    if (client_id_ == 0) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        client_id_ = dis(gen);
    }

    initialized_.store(true);
    clearError();
    return true;
}

void ASCOMAlpacaClient::cleanup() {
    if (!initialized_.load()) {
        return;
    }

    LOG_F(INFO, "Cleaning up ASCOM Alpaca Client");

    stopEventPolling();
    disconnect();

#ifndef _WIN32
    cleanupCurl();
#endif

    initialized_.store(false);
}

void ASCOMAlpacaClient::setServerAddress(const std::string& host, int port) {
    host_ = host;
    port_ = port;
    LOG_F(INFO, "Set server address to {}:{}", host, port);
}

void ASCOMAlpacaClient::setDeviceInfo(const std::string& deviceType,
                                      int deviceNumber) {
    device_type_ = deviceType;
    device_number_ = deviceNumber;
    LOG_F(INFO, "Set device info: {} #{}", deviceType, deviceNumber);
}

std::vector<AlpacaDevice> ASCOMAlpacaClient::discoverDevices(
    const std::string& host, int port) {
    LOG_F(INFO, "Discovering Alpaca devices on {}:{}",
          host.empty() ? "network" : host, port);

    std::vector<AlpacaDevice> devices;

    if (!host.empty()) {
        // Query specific host
        auto hostDevices = queryDevicesFromHost(host, port);
        devices.insert(devices.end(), hostDevices.begin(), hostDevices.end());
    } else {
        // Use discovery protocol
        auto discoveredHosts = AlpacaDiscovery::discoverHosts(5);
        for (const auto& discoveredHost : discoveredHosts) {
            auto hostDevices = queryDevicesFromHost(discoveredHost, port);
            devices.insert(devices.end(), hostDevices.begin(),
                           hostDevices.end());
        }
    }

    LOG_F(INFO, "Discovered {} Alpaca devices", devices.size());
    return devices;
}

std::optional<AlpacaDevice> ASCOMAlpacaClient::findDevice(
    const std::string& deviceType, const std::string& deviceName) {
    auto devices = discoverDevices();

    for (const auto& device : devices) {
        if (device.device_type == deviceType) {
            if (deviceName.empty() || device.device_name == deviceName) {
                return device;
            }
        }
    }

    return std::nullopt;
}

bool ASCOMAlpacaClient::testConnection() {
    if (host_.empty()) {
        setError("Host not configured");
        return false;
    }

    auto response = performRequest(HttpMethod::GET, "management/apiversions");
    return response.success && response.status_code == 200;
}

bool ASCOMAlpacaClient::connect() {
    if (is_connected_.load()) {
        return true;
    }

    if (!testConnection()) {
        setError("Failed to connect to Alpaca server");
        return false;
    }

    // Set device as connected
    auto response =
        performRequest(HttpMethod::PUT, "connected", "Connected=true");
    if (!response.success || response.status_code != 200) {
        setError("Failed to set device connected", response.status_code);
        return false;
    }

    is_connected_.store(true);
    LOG_F(INFO, "Connected to Alpaca device: {}:{} {}/{}", host_, port_,
          device_type_, device_number_);

    return true;
}

bool ASCOMAlpacaClient::disconnect() {
    if (!is_connected_.load()) {
        return true;
    }

    // Set device as disconnected
    performRequest(HttpMethod::PUT, "connected", "Connected=false");

    is_connected_.store(false);
    LOG_F(INFO, "Disconnected from Alpaca device");

    return true;
}

std::optional<json> ASCOMAlpacaClient::getProperty(
    const std::string& property) {
    if (!is_connected_.load()) {
        setError("Device not connected");
        return std::nullopt;
    }

    auto response = performRequest(HttpMethod::GET, property);
    if (!response.success) {
        return std::nullopt;
    }

    auto alpacaResponse = parseAlpacaResponse(response);
    if (!alpacaResponse || !alpacaResponse->isSuccess()) {
        setError(alpacaResponse ? alpacaResponse->getErrorMessage()
                                : "Failed to parse response");
        return std::nullopt;
    }

    return extractValue(*alpacaResponse);
}

bool ASCOMAlpacaClient::setProperty(const std::string& property,
                                    const json& value) {
    if (!is_connected_.load()) {
        setError("Device not connected");
        return false;
    }

    std::string params;
    switch (value.getType()) {
        case JsonType::Bool:
            params = property + "=" + (value.asBool() ? "true" : "false");
            break;
        case JsonType::Number:
            params = property + "=" + std::to_string(value.asNumber());
            break;
        case JsonType::String:
            params = property + "=" + escapeUrl(value.asString());
            break;
        default:
            params = property + "=" + escapeUrl(value.toString());
            break;
    }

    auto response = performRequest(HttpMethod::PUT, property, params);
    if (!response.success) {
        return false;
    }

    auto alpacaResponse = parseAlpacaResponse(response);
    if (!alpacaResponse || !alpacaResponse->isSuccess()) {
        setError(alpacaResponse ? alpacaResponse->getErrorMessage()
                                : "Failed to parse response");
        return false;
    }

    return true;
}

std::optional<json> ASCOMAlpacaClient::invokeMethod(const std::string& method) {
    std::unordered_map<std::string, json> emptyParams;
    return invokeMethod(method, emptyParams);
}

std::optional<json> ASCOMAlpacaClient::invokeMethod(
    const std::string& method,
    const std::unordered_map<std::string, json>& parameters) {
    if (!is_connected_.load()) {
        setError("Device not connected");
        return std::nullopt;
    }

    std::string params = buildParameters(parameters);
    auto response = performRequest(HttpMethod::PUT, method, params);

    if (!response.success) {
        return std::nullopt;
    }

    auto alpacaResponse = parseAlpacaResponse(response);
    if (!alpacaResponse || !alpacaResponse->isSuccess()) {
        setError(alpacaResponse ? alpacaResponse->getErrorMessage()
                                : "Failed to parse response");
        return std::nullopt;
    }

    return extractValue(*alpacaResponse);
}

std::unordered_map<std::string, json> ASCOMAlpacaClient::getMultipleProperties(
    const std::vector<std::string>& properties) {
    std::unordered_map<std::string, json> results;

    for (const auto& property : properties) {
        auto value = getProperty(property);
        if (value) {
            results[property] = *value;
        }
    }

    return results;
}

bool ASCOMAlpacaClient::setMultipleProperties(
    const std::unordered_map<std::string, json>& properties) {
    bool allSuccess = true;

    for (const auto& [property, value] : properties) {
        if (!setProperty(property, value)) {
            allSuccess = false;
            LOG_F(ERROR, "Failed to set property: {}", property);
        }
    }

    return allSuccess;
}

std::optional<std::vector<uint8_t>> ASCOMAlpacaClient::getImageArray() {
    auto response = performRequest(HttpMethod::GET, "imagearray");
    if (!response.success) {
        return std::nullopt;
    }

    // Parse base64 encoded image data
    auto alpacaResponse = parseAlpacaResponse(response);
    if (!alpacaResponse || !alpacaResponse->isSuccess()) {
        return std::nullopt;
    }

    // TODO: Implement base64 decoding
    // For now, return empty vector as placeholder
    return std::vector<uint8_t>();
}

std::optional<std::vector<uint16_t>>
ASCOMAlpacaClient::getImageArrayAsUInt16() {
    // TODO: Implement 16-bit image array retrieval
    return std::nullopt;
}

std::optional<std::vector<uint32_t>>
ASCOMAlpacaClient::getImageArrayAsUInt32() {
    // TODO: Implement 32-bit image array retrieval
    return std::nullopt;
}

std::future<std::optional<json>> ASCOMAlpacaClient::getPropertyAsync(
    const std::string& property) {
    return std::async(std::launch::async,
                      [this, property]() { return getProperty(property); });
}

std::future<bool> ASCOMAlpacaClient::setPropertyAsync(
    const std::string& property, const json& value) {
    return std::async(std::launch::async, [this, property, value]() {
        return setProperty(property, value);
    });
}

std::future<std::optional<json>> ASCOMAlpacaClient::invokeMethodAsync(
    const std::string& method) {
    return std::async(std::launch::async,
                      [this, method]() { return invokeMethod(method); });
}

void ASCOMAlpacaClient::startEventPolling(std::chrono::milliseconds interval) {
    if (event_polling_active_.load()) {
        return;
    }

    event_polling_interval_ = interval;
    event_polling_active_.store(true);
    event_thread_ = std::make_unique<std::thread>(
        &ASCOMAlpacaClient::eventPollingLoop, this);

    LOG_F(INFO, "Started event polling with {}ms interval", interval.count());
}

void ASCOMAlpacaClient::stopEventPolling() {
    if (!event_polling_active_.load()) {
        return;
    }

    event_polling_active_.store(false);
    if (event_thread_ && event_thread_->joinable()) {
        event_thread_->join();
    }
    event_thread_.reset();

    LOG_F(INFO, "Stopped event polling");
}

void ASCOMAlpacaClient::setEventCallback(
    std::function<void(const std::string&, const json&)> callback) {
    event_callback_ = callback;
}

void ASCOMAlpacaClient::clearError() {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_.clear();
    last_error_code_ = 0;
}

double ASCOMAlpacaClient::getAverageResponseTime() const {
    std::lock_guard lock(stats_mutex_);

    if (response_times_.empty()) {
        return 0.0;
    }

    auto total = std::chrono::milliseconds(0);
    for (const auto& time : response_times_) {
        total += time;
    }

    return static_cast<double>(total.count()) / response_times_.size();
}

void ASCOMAlpacaClient::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    request_count_.store(0);
    successful_requests_.store(0);
    failed_requests_.store(0);
    response_times_.clear();
}

void ASCOMAlpacaClient::addCustomHeader(const std::string& name,
                                        const std::string& value) {
    custom_headers_[name] = value;
}

void ASCOMAlpacaClient::removeCustomHeader(const std::string& name) {
    custom_headers_.erase(name);
}

// Private implementation methods
HttpResponse ASCOMAlpacaClient::performRequest(HttpMethod method,
                                               const std::string& endpoint,
                                               const std::string& params,
                                               const std::string& body) {
    std::lock_guard<std::mutex> lock(request_mutex_);

    auto startTime = std::chrono::steady_clock::now();
    HttpResponse response;
    response.success = false;

    std::string url = buildURL(endpoint);
    std::string fullParams = params;

    // Add client transaction ID
    if (!fullParams.empty()) {
        fullParams += "&";
    }
    fullParams += "ClientID=" + std::to_string(client_id_);
    fullParams += "&ClientTransactionID=" + std::to_string(++transaction_id_);

    if (verbose_logging_) {
        LOG_F(DEBUG, "Alpaca request: {} {} with params: {}",
              methodToString(method), url, fullParams);
    }

#ifndef _WIN32
    if (!curl_handle_) {
        response.error_message = "cURL not initialized";
        updateStatistics(false, std::chrono::milliseconds(0));
        return response;
    }

    // Reset cURL handle
    curl_easy_reset(curl_handle_);

    // Set basic options
    curl_easy_setopt(curl_handle_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle_, CURLOPT_TIMEOUT, timeout_seconds_);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_handle_, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl_handle_, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl_handle_, CURLOPT_HEADERDATA, &response.headers);

    // Set user agent
    curl_easy_setopt(curl_handle_, CURLOPT_USERAGENT, user_agent_.c_str());

    // Set method-specific options
    switch (method) {
        case HttpMethod::GET:
            if (!fullParams.empty()) {
                std::string getUrl = url + "?" + fullParams;
                curl_easy_setopt(curl_handle_, CURLOPT_URL, getUrl.c_str());
            }
            break;
        case HttpMethod::PUT:
            curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, "PUT");
            if (!fullParams.empty()) {
                curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS,
                                 fullParams.c_str());
            }
            break;
        case HttpMethod::POST:
            curl_easy_setopt(curl_handle_, CURLOPT_POST, 1L);
            if (!fullParams.empty()) {
                curl_easy_setopt(curl_handle_, CURLOPT_POSTFIELDS,
                                 fullParams.c_str());
            }
            break;
        case HttpMethod::DELETE:
            curl_easy_setopt(curl_handle_, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
    }

    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(
        headers, "Content-Type: application/x-www-form-urlencoded");

    for (const auto& [name, value] : custom_headers_) {
        std::string header = name + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }

    if (headers) {
        curl_easy_setopt(curl_handle_, CURLOPT_HTTPHEADER, headers);
    }

    // SSL options
    if (ssl_enabled_) {
        curl_easy_setopt(curl_handle_, CURLOPT_SSL_VERIFYPEER,
                         ssl_verify_peer_ ? 1L : 0L);
        if (!ssl_cert_path_.empty()) {
            curl_easy_setopt(curl_handle_, CURLOPT_SSLCERT,
                             ssl_cert_path_.c_str());
        }
        if (!ssl_key_path_.empty()) {
            curl_easy_setopt(curl_handle_, CURLOPT_SSLKEY,
                             ssl_key_path_.c_str());
        }
    }

    // Compression
    if (compression_enabled_) {
        curl_easy_setopt(curl_handle_, CURLOPT_ACCEPT_ENCODING,
                         "gzip, deflate");
    }

    // Perform request
    CURLcode res = curl_easy_perform(curl_handle_);

    if (headers) {
        curl_slist_free_all(headers);
    }

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl_handle_, CURLINFO_RESPONSE_CODE,
                          &response.status_code);
        response.success =
            (response.status_code >= 200 && response.status_code < 300);

        if (!response.success) {
            response.error_message =
                "HTTP " + std::to_string(response.status_code);
        }
    } else {
        response.error_message = curl_easy_strerror(res);
        setError("cURL error: " + response.error_message,
                 static_cast<int>(res));
    }
#else
    // Windows implementation placeholder
    response.error_message = "Windows HTTP client not implemented";
#endif

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    updateStatistics(response.success, duration);

    if (verbose_logging_) {
        LOG_F(DEBUG, "Alpaca response: {} ({}ms) - {}", response.status_code,
              duration.count(),
              response.success ? "SUCCESS" : response.error_message);
    }

    return response;
}

std::string ASCOMAlpacaClient::buildURL(const std::string& endpoint) const {
    std::ostringstream oss;
    oss << (ssl_enabled_ ? "https" : "http") << "://" << host_ << ":" << port_
        << "/api/v1/" << device_type_ << "/" << device_number_ << "/"
        << endpoint;
    return oss.str();
}

std::string ASCOMAlpacaClient::buildParameters(
    const std::unordered_map<std::string, json>& params) const {
    std::ostringstream oss;
    bool first = true;

    for (const auto& [key, value] : params) {
        if (!first) {
            oss << "&";
        }
        first = false;

        oss << escapeUrl(key) << "=";

        switch (value.getType()) {
            case JsonType::Bool:
                oss << (value.asBool() ? "true" : "false");
                break;
            case JsonType::Number:
                oss << value.asNumber();
                break;
            case JsonType::String:
                oss << escapeUrl(value.asString());
                break;
            default:
                oss << escapeUrl(value.toString());
                break;
        }
    }

    return oss.str();
}

std::optional<AlpacaResponse> ASCOMAlpacaClient::parseAlpacaResponse(
    const HttpResponse& httpResponse) {
    if (!httpResponse.success) {
        return std::nullopt;
    }

    // Parse JSON response - simplified implementation
    // In production, would use a proper JSON parser
    AlpacaResponse response;

    // Extract basic fields using simple parsing
    std::string body = httpResponse.body;

    // Look for error information
    size_t errorPos = body.find("\"ErrorNumber\":");
    if (errorPos != std::string::npos) {
        // Extract error number
        size_t start = body.find(":", errorPos) + 1;
        size_t end = body.find_first_of(",}", start);
        if (end != std::string::npos) {
            std::string errorNumStr = body.substr(start, end - start);
            errorNumStr.erase(0, errorNumStr.find_first_not_of(" \t"));
            errorNumStr.erase(errorNumStr.find_last_not_of(" \t") + 1);

            int errorNum = std::stoi(errorNumStr);
            if (errorNum != 0) {
                AlpacaError error;
                error.error_number = errorNum;

                // Extract error message
                size_t msgPos = body.find("\"ErrorMessage\":");
                if (msgPos != std::string::npos) {
                    size_t msgStart = body.find("\"", msgPos + 15) + 1;
                    size_t msgEnd = body.find("\"", msgStart);
                    if (msgEnd != std::string::npos) {
                        error.message =
                            body.substr(msgStart, msgEnd - msgStart);
                    }
                }

                response.error_info = error;
            }
        }
    }

    // Extract value field
    size_t valuePos = body.find("\"Value\":");
    if (valuePos != std::string::npos) {
        size_t start = body.find(":", valuePos) + 1;
        size_t end = body.find_first_of(",}", start);
        if (end != std::string::npos) {
            std::string valueStr = body.substr(start, end - start);
            valueStr.erase(0, valueStr.find_first_not_of(" \t"));
            valueStr.erase(valueStr.find_last_not_of(" \t") + 1);

            response.value = SimpleJson::fromString(valueStr);
        }
    }

    return response;
}

std::optional<json> ASCOMAlpacaClient::extractValue(
    const AlpacaResponse& response) {
    if (!response.isSuccess()) {
        return std::nullopt;
    }

    return response.value;
}

void ASCOMAlpacaClient::setError(const std::string& message, int code) {
    std::lock_guard<std::mutex> lock(error_mutex_);
    last_error_ = message;
    last_error_code_ = code;
    LOG_F(ERROR, "Alpaca Client Error: {} (Code: {})", message, code);
}

void ASCOMAlpacaClient::updateStatistics(
    bool success, std::chrono::milliseconds responseTime) {
    request_count_.fetch_add(1);

    if (success) {
        successful_requests_.fetch_add(1);
    } else {
        failed_requests_.fetch_add(1);
    }

    std::lock_guard<std::mutex> lock(stats_mutex_);
    response_times_.push_back(responseTime);

    // Keep only last 100 response times for average calculation
    if (response_times_.size() > 100) {
        response_times_.erase(response_times_.begin());
    }
}

void ASCOMAlpacaClient::eventPollingLoop() {
    while (event_polling_active_.load()) {
        if (is_connected_.load() && event_callback_) {
            // Poll for device events - implementation depends on device type
            // This is a placeholder for event polling logic

            try {
                // Example: poll device state
                auto state = getProperty("connected");
                if (state) {
                    event_callback_("connected", *state);
                }
            } catch (const std::exception& e) {
                LOG_F(WARNING, "Event polling error: {}", e.what());
            }
        }

        std::this_thread::sleep_for(event_polling_interval_);
    }
}

std::string ASCOMAlpacaClient::escapeUrl(const std::string& str) const {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase;
            escaped << '%' << std::setw(2)
                    << static_cast<int>(static_cast<unsigned char>(c));
            escaped << std::nouppercase;
        }
    }

    return escaped.str();
}

std::string ASCOMAlpacaClient::jsonToString(const json& j) {
    return j.toString();
}

std::optional<json> ASCOMAlpacaClient::stringToJson(const std::string& str) {
    try {
        return SimpleJson::fromString(str);
    } catch (...) {
        return std::nullopt;
    }
}

std::string ASCOMAlpacaClient::methodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET:
            return "GET";
        case HttpMethod::PUT:
            return "PUT";
        case HttpMethod::POST:
            return "POST";
        case HttpMethod::DELETE:
            return "DELETE";
        default:
            return "UNKNOWN";
    }
}

std::vector<AlpacaDevice> ASCOMAlpacaClient::queryDevicesFromHost(
    const std::string& host, int port) {
    std::vector<AlpacaDevice> devices;

    // Temporarily set host and port
    std::string originalHost = host_;
    int originalPort = port_;

    setServerAddress(host, port);

    // Query management API for device list
    auto response =
        performRequest(HttpMethod::GET, "management/configureddevices");

    if (response.success) {
        // Parse device list from response
        // This is a simplified implementation
        // In production, would properly parse JSON array

        AlpacaDevice device;
        device.device_name = "Sample Device";
        device.device_type = device_type_;
        device.device_number = 0;
        device.unique_id =
            host + ":" + std::to_string(port) + "/" + device_type_ + "/0";

        devices.push_back(device);
    }

    // Restore original settings
    setServerAddress(originalHost, originalPort);

    return devices;
}

#ifndef _WIN32
bool ASCOMAlpacaClient::initializeCurl() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle_ = curl_easy_init();

    if (!curl_handle_) {
        LOG_F(ERROR, "Failed to initialize cURL");
        return false;
    }

    LOG_F(INFO, "cURL initialized successfully");
    return true;
}

void ASCOMAlpacaClient::cleanupCurl() {
    if (curl_headers_) {
        curl_slist_free_all(curl_headers_);
        curl_headers_ = nullptr;
    }

    if (curl_handle_) {
        curl_easy_cleanup(curl_handle_);
        curl_handle_ = nullptr;
    }

    curl_global_cleanup();
}

size_t ASCOMAlpacaClient::writeCallback(void* contents, size_t size,
                                        size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t ASCOMAlpacaClient::headerCallback(
    void* contents, size_t size, size_t nmemb,
    std::unordered_map<std::string, std::string>* headers) {
    size_t totalSize = size * nmemb;
    std::string header(static_cast<char*>(contents), totalSize);

    size_t colonPos = header.find(':');
    if (colonPos != std::string::npos) {
        std::string name = header.substr(0, colonPos);
        std::string value = header.substr(colonPos + 1);

        // Trim whitespace
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        (*headers)[name] = value;
    }

    return totalSize;
}
#endif

// AlpacaDiscovery implementation
std::vector<AlpacaDevice> AlpacaDiscovery::discoverAllDevices(
    int timeoutSeconds) {
    std::vector<AlpacaDevice> allDevices;

    auto hosts = discoverHosts(timeoutSeconds);
    for (const auto& host : hosts) {
        if (isAlpacaServer(host, 11111)) {
            // Query devices from this host
            ASCOMAlpacaClient client;
            client.initialize();
            client.setServerAddress(host, 11111);

            auto devices = client.discoverDevices(host, 11111);
            allDevices.insert(allDevices.end(), devices.begin(), devices.end());
        }
    }

    return allDevices;
}

std::vector<std::string> AlpacaDiscovery::discoverHosts(int timeoutSeconds) {
    std::vector<std::string> hosts;

#ifndef _WIN32
    // UDP broadcast discovery
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        return hosts;
    }

    int broadcast = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct timeval timeout;
    timeout.tv_sec = timeoutSeconds;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;
    broadcastAddr.sin_port = htons(ALPACA_DISCOVERY_PORT);

    // Send discovery message
    sendto(sockfd, ALPACA_DISCOVERY_MESSAGE, strlen(ALPACA_DISCOVERY_MESSAGE),
           0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));

    // Receive responses
    char buffer[1024];
    struct sockaddr_in responseAddr;
    socklen_t addrLen = sizeof(responseAddr);

    while (true) {
        ssize_t received = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                    (struct sockaddr*)&responseAddr, &addrLen);
        if (received <= 0) {
            break;  // Timeout or error
        }

        buffer[received] = '\0';

        // Parse response to extract host IP
        char* hostIP = inet_ntoa(responseAddr.sin_addr);
        if (hostIP) {
            hosts.push_back(std::string(hostIP));
        }
    }

    close(sockfd);
#endif

    return hosts;
}

bool AlpacaDiscovery::isAlpacaServer(const std::string& host, int port) {
    ASCOMAlpacaClient client;
    client.initialize();
    client.setServerAddress(host, port);

    return client.testConnection();
}

// AlpacaUtils implementation
namespace AlpacaUtils {

json toJson(bool value) { return SimpleJson(value); }

json toJson(int value) { return SimpleJson(value); }

json toJson(double value) { return SimpleJson(value); }

json toJson(const std::string& value) { return SimpleJson(value); }

json toJson(const std::vector<std::string>& value) {
    // Simplified - would need proper array support
    return SimpleJson("array");
}

json toJson(const std::vector<int>& value) { return SimpleJson("array"); }

json toJson(const std::vector<double>& value) { return SimpleJson("array"); }

template <>
std::optional<bool> fromJson<bool>(const json& j) {
    if (j.getType() == JsonType::Bool) {
        return j.asBool();
    }
    return std::nullopt;
}

template <>
std::optional<int> fromJson<int>(const json& j) {
    if (j.getType() == JsonType::Number) {
        return static_cast<int>(j.asNumber());
    }
    return std::nullopt;
}

template <>
std::optional<double> fromJson<double>(const json& j) {
    if (j.getType() == JsonType::Number) {
        return j.asNumber();
    }
    return std::nullopt;
}

template <>
std::optional<std::string> fromJson<std::string>(const json& j) {
    if (j.getType() == JsonType::String) {
        return j.asString();
    }
    return std::nullopt;
}

std::vector<uint16_t> jsonArrayToUInt16(const json& jsonArray) {
    // TODO: Implement array parsing
    return {};
}

std::vector<uint32_t> jsonArrayToUInt32(const json& jsonArray) {
    // TODO: Implement array parsing
    return {};
}

std::vector<double> jsonArrayToDouble(const json& jsonArray) {
    // TODO: Implement array parsing
    return {};
}

std::string getErrorDescription(int errorCode) {
    switch (errorCode) {
        case 0x400:
            return "Bad Request";
        case 0x401:
            return "Unauthorized";
        case 0x404:
            return "Not Found";
        case 0x500:
            return "Internal Server Error";
        case 0x800:
            return "Not Implemented";
        case 0x801:
            return "Invalid Value";
        case 0x802:
            return "Value Not Set";
        case 0x803:
            return "Not Connected";
        case 0x804:
            return "Invalid While Parked";
        case 0x805:
            return "Invalid While Slaved";
        case 0x806:
            return "Invalid Coordinates";
        case 0x807:
            return "Invalid While Moving";
        default:
            return "Unknown Error";
    }
}

bool isRetryableError(int errorCode) {
    // Network errors that might be temporary
    return (errorCode >= 500 && errorCode < 600) || errorCode == 0x803;
}

}  // namespace AlpacaUtils
