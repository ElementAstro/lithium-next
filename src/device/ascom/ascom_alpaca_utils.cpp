/*
 * ascom_alpaca_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-6-19

Description: ASCOM Alpaca Utility Functions Implementation (API v9)

**************************************************/

#include "ascom_alpaca_client.hpp"

#include <spdlog/spdlog.h>
#include <iomanip>
#include <random>
#include <regex>

#ifndef _WIN32
#include <ifaddrs.h>
#include <netdb.h>
#endif

namespace AlpacaUtils {

// JSON conversion functions using nlohmann/json
json toJson(bool value) { return json(value); }

json toJson(int value) { return json(value); }

json toJson(double value) { return json(value); }

json toJson(const std::string& value) { return json(value); }

json toJson(const std::vector<std::string>& value) { return json(value); }

json toJson(const std::vector<int>& value) { return json(value); }

json toJson(const std::vector<double>& value) { return json(value); }

json toJson(const std::chrono::system_clock::time_point& value) {
    auto time_t = std::chrono::system_clock::to_time_t(value);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return json(oss.str());
}

// Template specializations for fromJson
template <>
std::optional<bool> fromJson<bool>(const json& j) {
    if (j.is_boolean()) {
        return j.get<bool>();
    }
    return std::nullopt;
}

template <>
std::optional<int> fromJson<int>(const json& j) {
    if (j.is_number_integer()) {
        return j.get<int>();
    }
    return std::nullopt;
}

template <>
std::optional<double> fromJson<double>(const json& j) {
    if (j.is_number()) {
        return j.get<double>();
    }
    return std::nullopt;
}

template <>
std::optional<std::string> fromJson<std::string>(const json& j) {
    if (j.is_string()) {
        return j.get<std::string>();
    }
    return std::nullopt;
}

template <>
std::optional<std::vector<std::string>> fromJson<std::vector<std::string>>(
    const json& j) {
    if (j.is_array()) {
        std::vector<std::string> result;
        for (const auto& item : j) {
            if (item.is_string()) {
                result.push_back(item.get<std::string>());
            }
        }
        return result;
    }
    return std::nullopt;
}

template <>
std::optional<std::vector<int>> fromJson<std::vector<int>>(const json& j) {
    if (j.is_array()) {
        std::vector<int> result;
        for (const auto& item : j) {
            if (item.is_number_integer()) {
                result.push_back(item.get<int>());
            }
        }
        return result;
    }
    return std::nullopt;
}

template <>
std::optional<std::vector<double>> fromJson<std::vector<double>>(
    const json& j) {
    if (j.is_array()) {
        std::vector<double> result;
        for (const auto& item : j) {
            if (item.is_number()) {
                result.push_back(item.get<double>());
            }
        }
        return result;
    }
    return std::nullopt;
}

// Image array conversions
std::vector<uint8_t> jsonArrayToUInt8(const json& jsonArray) {
    std::vector<uint8_t> result;
    if (!jsonArray.is_array())
        return result;

    for (const auto& item : jsonArray) {
        if (item.is_number_integer()) {
            int value = item.get<int>();
            result.push_back(static_cast<uint8_t>(std::clamp(value, 0, 255)));
        }
    }
    return result;
}

std::vector<uint16_t> jsonArrayToUInt16(const json& jsonArray) {
    std::vector<uint16_t> result;
    if (!jsonArray.is_array())
        return result;

    for (const auto& item : jsonArray) {
        if (item.is_number_integer()) {
            int value = item.get<int>();
            result.push_back(
                static_cast<uint16_t>(std::clamp(value, 0, 65535)));
        }
    }
    return result;
}

std::vector<uint32_t> jsonArrayToUInt32(const json& jsonArray) {
    std::vector<uint32_t> result;
    if (!jsonArray.is_array())
        return result;

    for (const auto& item : jsonArray) {
        if (item.is_number_integer()) {
            result.push_back(item.get<uint32_t>());
        }
    }
    return result;
}

std::vector<double> jsonArrayToDouble(const json& jsonArray) {
    std::vector<double> result;
    if (!jsonArray.is_array())
        return result;

    for (const auto& item : jsonArray) {
        if (item.is_number()) {
            result.push_back(item.get<double>());
        }
    }
    return result;
}

// Binary data conversion for ImageBytes
std::vector<uint8_t> convertImageData(const std::vector<uint16_t>& source) {
    std::vector<uint8_t> result;
    result.reserve(source.size() * sizeof(uint16_t));

    for (uint16_t value : source) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        result.insert(result.end(), bytes, bytes + sizeof(uint16_t));
    }
    return result;
}

std::vector<uint8_t> convertImageData(const std::vector<uint32_t>& source) {
    std::vector<uint8_t> result;
    result.reserve(source.size() * sizeof(uint32_t));

    for (uint32_t value : source) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        result.insert(result.end(), bytes, bytes + sizeof(uint32_t));
    }
    return result;
}

std::vector<uint8_t> convertImageData(const std::vector<double>& source) {
    std::vector<uint8_t> result;
    result.reserve(source.size() * sizeof(double));

    for (double value : source) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        result.insert(result.end(), bytes, bytes + sizeof(double));
    }
    return result;
}

// ASCOM error handling
std::string getErrorDescription(int errorCode) {
    static const std::unordered_map<int, std::string> errorDescriptions = {
        {0x0, "Success"},
        {0x401,
         "Invalid value - The value is invalid for this property or method"},
        {0x402, "Value not set - The value has not been set"},
        {0x407, "Not connected - The device is not connected"},
        {0x408, "Invalid while parked - Cannot perform operation while parked"},
        {0x409,
         "Invalid while slaved - Cannot perform operation while slaved to "
         "another application"},
        {0x40B,
         "Invalid operation - The requested operation cannot be performed"},
        {0x40C,
         "Action not implemented - The requested action is not implemented"},
        {0x500, "Unspecified error - An unspecified error has occurred"}};

    auto it = errorDescriptions.find(errorCode);
    return it != errorDescriptions.end() ? it->second : "Unknown error";
}

std::string getAscomErrorDescription(AscomErrorCode errorCode) {
    return getErrorDescription(static_cast<int>(errorCode));
}

bool isRetryableError(int errorCode) {
    // These errors might be temporary and worth retrying
    return errorCode == 0x500 || errorCode == 0x407;
}

bool isAscomError(int errorCode) {
    return (errorCode >= 0x400 && errorCode <= 0x4FF) || errorCode == 0x500;
}

AscomErrorCode intToAscomError(int errorCode) {
    switch (errorCode) {
        case 0x0:
            return AscomErrorCode::OK;
        case 0x401:
            return AscomErrorCode::InvalidValue;
        case 0x402:
            return AscomErrorCode::ValueNotSet;
        case 0x407:
            return AscomErrorCode::NotConnected;
        case 0x408:
            return AscomErrorCode::InvalidWhileParked;
        case 0x409:
            return AscomErrorCode::InvalidWhileSlaved;
        case 0x40B:
            return AscomErrorCode::InvalidOperationException;
        case 0x40C:
            return AscomErrorCode::ActionNotImplemented;
        case 0x500:
            return AscomErrorCode::UnspecifiedError;
        default:
            return AscomErrorCode::UnspecifiedError;
    }
}

// Device type utilities
std::string deviceTypeToString(AscomDeviceType type) {
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

AscomDeviceType stringToDeviceType(const std::string& typeStr) {
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

    auto it = typeMap.find(typeStr);
    return it != typeMap.end() ? it->second : AscomDeviceType::Camera;
}

std::vector<std::string> getSupportedDeviceTypes() {
    return {"camera",      "covercalibrator", "dome",
            "filterwheel", "focuser",         "observingconditions",
            "rotator",     "safetymonitor",   "switch",
            "telescope"};
}

bool isValidDeviceType(const std::string& type) {
    auto types = getSupportedDeviceTypes();
    return std::find(types.begin(), types.end(), type) != types.end();
}

// URL and parameter utilities
std::string urlEncode(const std::string& value) {
    std::ostringstream encoded;
    encoded.fill('0');
    encoded << std::hex;

    for (char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else {
            encoded << std::uppercase;
            encoded << '%' << std::setw(2)
                    << static_cast<int>(static_cast<unsigned char>(c));
            encoded << std::nouppercase;
        }
    }

    return encoded.str();
}

std::string urlDecode(const std::string& value) {
    std::string decoded;
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '%' && i + 2 < value.length()) {
            int hex = std::stoi(value.substr(i + 1, 2), nullptr, 16);
            decoded += static_cast<char>(hex);
            i += 2;
        } else if (value[i] == '+') {
            decoded += ' ';
        } else {
            decoded += value[i];
        }
    }
    return decoded;
}

std::unordered_map<std::string, std::string> parseQueryString(
    const std::string& query) {
    std::unordered_map<std::string, std::string> params;
    std::regex paramRegex("([^&=]+)=([^&]*)");
    std::sregex_iterator iter(query.begin(), query.end(), paramRegex);
    std::sregex_iterator end;

    for (; iter != end; ++iter) {
        std::string key = urlDecode((*iter)[1].str());
        std::string value = urlDecode((*iter)[2].str());
        params[key] = value;
    }

    return params;
}

std::string buildQueryString(
    const std::unordered_map<std::string, std::string>& params) {
    std::ostringstream query;
    bool first = true;

    for (const auto& [key, value] : params) {
        if (!first)
            query << "&";
        query << urlEncode(key) << "=" << urlEncode(value);
        first = false;
    }

    return query.str();
}

// Validation utilities
bool isValidClientId(int clientId) {
    return clientId >= 0 && clientId <= 65535;
}

bool isValidTransactionId(int transactionId) { return transactionId >= 0; }

bool isValidDeviceNumber(int deviceNumber) {
    return deviceNumber >= 0 && deviceNumber <= 2147483647;
}

bool isValidAPIVersion(int version) { return version >= 1 && version <= 3; }

bool isValidJSONResponse(const std::string& response) {
    try {
        auto parsed = json::parse(response);
        return true;
    } catch (...) {
        return false;
    }
}

// Timing utilities
std::string formatTimestamp(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point parseTimestamp(
    const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::chrono::milliseconds calculateTimeout(int baseTimeoutSeconds,
                                           int retryCount) {
    // Exponential backoff with jitter
    int timeout = baseTimeoutSeconds * (1 << std::min(retryCount, 5));
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(timeout * 800, timeout * 1200);
    return std::chrono::milliseconds(dis(gen));
}

// Network utilities
bool isValidIPAddress(const std::string& ip) {
    std::regex ipv4Regex(
        R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    std::regex ipv6Regex(R"(^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$)");

    return std::regex_match(ip, ipv4Regex) || std::regex_match(ip, ipv6Regex);
}

bool isValidPort(int port) { return port > 0 && port <= 65535; }

std::string getLocalIPAddress() {
    // This is a simplified implementation
    // In a real implementation, you'd enumerate network interfaces
    return "127.0.0.1";
}

std::vector<std::string> getLocalIPAddresses() {
    std::vector<std::string> addresses;

#ifndef _WIN32
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        return addresses;
    }

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr)
            continue;

        int family = ifa->ifa_addr->sa_family;
        if (family == AF_INET) {
            char host[NI_MAXHOST];
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host,
                                NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
            if (s == 0) {
                addresses.push_back(std::string(host));
            }
        }
    }

    freeifaddrs(ifaddr);
#else
    addresses.push_back("127.0.0.1");
#endif

    return addresses;
}

bool isLocalAddress(const std::string& address) {
    return address == "127.0.0.1" || address == "localhost" || address == "::1";
}

// Logging utilities
std::string formatLogMessage(const std::string& level,
                             const std::string& message,
                             const std::string& context) {
    std::ostringstream ss;
    ss << "[" << level << "]";
    if (!context.empty()) {
        ss << " [" << context << "]";
    }
    ss << " " << message;
    return ss.str();
}

void logApiCall(const std::string& method, const std::string& endpoint,
                const std::chrono::milliseconds& duration, bool success) {
    spdlog::info("API Call: {} {} - {}ms - {}", method, endpoint,
                 duration.count(), success ? "SUCCESS" : "FAILED");
}

void logError(const std::string& error, const std::string& context) {
    if (context.empty()) {
        spdlog::error("{}", error);
    } else {
        spdlog::error("[{}] {}", context, error);
    }
}

}  // namespace AlpacaUtils
