/*
 * ascom_alpaca_client_v9.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-19

Description: Enhanced ASCOM Alpaca REST Client Implementation - API Version 9
New Features

**************************************************/

#include "ascom_alpaca_client.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>

#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

// Implementation of new API v9 methods for ASCOMAlpacaClient

// API Version Management
std::vector<int> ASCOMAlpacaClient::getSupportedAPIVersions() {
    auto response = performRequest(HttpMethod::GET, "management/apiversions");
    if (!response.success || response.status_code != 200) {
        setError("Failed to get supported API versions", response.status_code);
        return supported_api_versions_;  // Return cached versions
    }

    try {
        auto jsonResponse = json::parse(response.body);
        if (jsonResponse.contains("Value") &&
            jsonResponse["Value"].is_array()) {
            std::vector<int> versions;
            for (const auto& version : jsonResponse["Value"]) {
                if (version.is_number_integer()) {
                    versions.push_back(version.get<int>());
                }
            }
            supported_api_versions_ = versions;
            return versions;
        }
    } catch (const std::exception& e) {
        setError("Failed to parse API versions response: " +
                 std::string(e.what()));
    }

    return supported_api_versions_;
}

bool ASCOMAlpacaClient::setAPIVersion(AlpacaAPIVersion version) {
    int versionInt = static_cast<int>(version);

    // Check if version is supported
    auto supportedVersions = getSupportedAPIVersions();
    if (std::find(supportedVersions.begin(), supportedVersions.end(),
                  versionInt) == supportedVersions.end()) {
        setError("API version " + std::to_string(versionInt) +
                 " not supported by server");
        return false;
    }

    api_version_ = version;
    spdlog::info("Set API version to v{}", versionInt);
    return true;
}

// Device type conversion methods
void ASCOMAlpacaClient::setDeviceInfo(AscomDeviceType deviceType,
                                      int deviceNumber) {
    device_type_enum_ = deviceType;
    device_type_ = deviceTypeToString(deviceType);
    device_number_ = deviceNumber;
    spdlog::info("Set device info: {} #{}", device_type_, deviceNumber);
}

std::string ASCOMAlpacaClient::deviceTypeToString(AscomDeviceType type) const {
    static const std::unordered_map<AscomDeviceType, std::string> typeMap = {
        {AscomDeviceType::Camera, "camera"},
        {AscomDeviceType::CoverCalibrator, "covercalibrator"},
        {AscomDeviceType::Dome, "dome"},
        {AscomDeviceType::FilterWheel, "filterwheel"},
        {AscomDeviceType::Focuser, "focuser"},
        {AscomDeviceType::ObservingConditions, "observingconditions"},
        {AscomDeviceType::Rotator, "rotator"},
        {AscomDeviceType::SafetyMonitor, "safetymonitor"},
        {AscomDeviceType::Switch, "switch"},
        {AscomDeviceType::Telescope, "telescope"}};

    auto it = typeMap.find(type);
    return it != typeMap.end() ? it->second : "unknown";
}

AscomDeviceType ASCOMAlpacaClient::stringToDeviceType(
    const std::string& type) const {
    static const std::unordered_map<std::string, AscomDeviceType> typeMap = {
        {"camera", AscomDeviceType::Camera},
        {"covercalibrator", AscomDeviceType::CoverCalibrator},
        {"dome", AscomDeviceType::Dome},
        {"filterwheel", AscomDeviceType::FilterWheel},
        {"focuser", AscomDeviceType::Focuser},
        {"observingconditions", AscomDeviceType::ObservingConditions},
        {"rotator", AscomDeviceType::Rotator},
        {"safetymonitor", AscomDeviceType::SafetyMonitor},
        {"switch", AscomDeviceType::Switch},
        {"telescope", AscomDeviceType::Telescope}};

    auto it = typeMap.find(type);
    return it != typeMap.end() ? it->second : AscomDeviceType::Camera;
}

// Management API implementation
std::optional<AlpacaManagementInfo> ASCOMAlpacaClient::getManagementInfo() {
    auto response = performRequest(HttpMethod::GET, "management/description");
    if (!response.success || response.status_code != 200) {
        setError("Failed to get management info", response.status_code);
        return std::nullopt;
    }

    try {
        auto jsonResponse = json::parse(response.body);
        AlpacaManagementInfo info;

        if (jsonResponse.contains("Value") &&
            jsonResponse["Value"].is_object()) {
            auto value = jsonResponse["Value"];
            info.server_name = value.value("ServerName", "");
            info.manufacturer = value.value("Manufacturer", "");
            info.manufacturer_version = value.value("ManufacturerVersion", "");
            info.location = value.value("Location", "");
        }

        // Get supported API versions
        info.supported_api_versions = getSupportedAPIVersions();

        return info;
    } catch (const std::exception& e) {
        setError("Failed to parse management info: " + std::string(e.what()));
        return std::nullopt;
    }
}

std::vector<AlpacaConfiguredDevice> ASCOMAlpacaClient::getConfiguredDevices() {
    auto response =
        performRequest(HttpMethod::GET, "management/configureddevices");
    if (!response.success || response.status_code != 200) {
        setError("Failed to get configured devices", response.status_code);
        return {};
    }

    try {
        auto jsonResponse = json::parse(response.body);
        std::vector<AlpacaConfiguredDevice> devices;

        if (jsonResponse.contains("Value") &&
            jsonResponse["Value"].is_array()) {
            for (const auto& deviceJson : jsonResponse["Value"]) {
                AlpacaConfiguredDevice device;
                device.device_name = deviceJson.value("DeviceName", "");
                device.device_type = deviceJson.value("DeviceType", "");
                device.device_number = deviceJson.value("DeviceNumber", 0);
                device.unique_id = deviceJson.value("UniqueID", "");
                device.enabled = deviceJson.value("Enabled", true);

                // Store raw configuration
                device.configuration = deviceJson;

                devices.push_back(device);
            }
        }

        return devices;
    } catch (const std::exception& e) {
        setError("Failed to parse configured devices: " +
                 std::string(e.what()));
        return {};
    }
}

// Transaction ID management
int ASCOMAlpacaClient::generateClientTransactionId() {
    return client_transaction_id_.fetch_add(1, std::memory_order_relaxed);
}

int ASCOMAlpacaClient::getNextClientTransactionId() {
    return client_transaction_id_.load(std::memory_order_relaxed) + 1;
}

void ASCOMAlpacaClient::updateTransactionIds(const AlpacaResponse& response) {
    last_server_transaction_id_ = response.server_transaction_id;
}

// Enhanced URL building for management API
std::string ASCOMAlpacaClient::buildManagementURL(
    const std::string& endpoint) const {
    std::ostringstream url;
    url << (ssl_enabled_ ? "https://" : "http://") << host_ << ":" << port_;
    url << "/api/v" << static_cast<int>(api_version_) << "/management/"
        << endpoint;
    return url.str();
}

// Enhanced error handling
void ASCOMAlpacaClient::setAscomError(AscomErrorCode code,
                                      const std::string& message) {
    last_error_code_ = static_cast<int>(code);
    last_error_ =
        message.empty() ? AlpacaUtils::getAscomErrorDescription(code) : message;

    if (verbose_logging_) {
        spdlog::error("ASCOM Error {}: {}", last_error_code_, last_error_);
    }
}

bool ASCOMAlpacaClient::shouldRetryRequest(const HttpResponse& response) const {
    // Retry on network errors or server errors (5xx)
    if (!response.success || response.status_code >= 500) {
        return true;
    }

    // Retry on specific ASCOM error codes
    if (response.status_code == 200) {
        try {
            auto jsonResponse = json::parse(response.body);
            if (jsonResponse.contains("ErrorNumber")) {
                int errorCode = jsonResponse["ErrorNumber"].get<int>();
                return AlpacaUtils::isRetryableError(errorCode);
            }
        } catch (...) {
            // Parse error, don't retry
        }
    }

    return false;
}

// Enhanced statistics
double ASCOMAlpacaClient::getSuccessRate() const {
    size_t total = request_count_.load();
    if (total == 0)
        return 0.0;

    size_t successful = successful_requests_.load();
    return static_cast<double>(successful) / static_cast<double>(total) * 100.0;
}

// Cache management
void ASCOMAlpacaClient::enableResponseCaching(bool enable,
                                              std::chrono::seconds ttl) {
    caching_enabled_ = enable;
    default_cache_ttl_ = ttl;

    if (!enable) {
        clearCache();
    }

    spdlog::info("Response caching {}, TTL: {}s",
                 enable ? "enabled" : "disabled", ttl.count());
}

void ASCOMAlpacaClient::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    response_cache_.clear();
    spdlog::debug("Response cache cleared");
}

std::optional<json> ASCOMAlpacaClient::getCachedResponse(
    const std::string& key) {
    if (!caching_enabled_)
        return std::nullopt;

    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = response_cache_.find(key);
    if (it == response_cache_.end()) {
        return std::nullopt;
    }

    // Check if cache entry is expired
    auto now = std::chrono::system_clock::now();
    if (now - it->second.timestamp > it->second.ttl) {
        response_cache_.erase(it);
        return std::nullopt;
    }

    return it->second.value;
}

void ASCOMAlpacaClient::setCachedResponse(const std::string& key,
                                          const json& value,
                                          std::chrono::seconds ttl) {
    if (!caching_enabled_)
        return;

    std::lock_guard<std::mutex> lock(cache_mutex_);
    CacheEntry entry;
    entry.value = value;
    entry.timestamp = std::chrono::system_clock::now();
    entry.ttl = ttl;

    response_cache_[key] = entry;
}

std::string ASCOMAlpacaClient::generateCacheKey(
    const std::string& endpoint, const std::string& params) const {
    return endpoint + "?" + params;
}

// Find device overload for AscomDeviceType
std::optional<AlpacaDevice> ASCOMAlpacaClient::findDevice(
    AscomDeviceType deviceType, const std::string& deviceName) {
    return findDevice(deviceTypeToString(deviceType), deviceName);
}
