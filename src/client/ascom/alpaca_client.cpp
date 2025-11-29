/*
 * alpaca_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: ASCOM Alpaca REST API client implementation

*************************************************/

#include "alpaca_client.hpp"

#include <spdlog/spdlog.h>

#include <sstream>

// Note: In a full implementation, this would use a proper HTTP client library
// like cpp-httplib, cpr, or libcurl. For now, we provide a stub implementation
// that can be extended.

namespace lithium::client::ascom {

AlpacaClient::AlpacaClient(std::string host, int port)
    : host_(std::move(host)), port_(port) {
    spdlog::debug("AlpacaClient created for {}:{}", host_, port_);
}

AlpacaClient::~AlpacaClient() {
    if (connected_) {
        disconnect();
    }
}

bool AlpacaClient::connect(int timeout) {
    std::lock_guard<std::mutex> lock(mutex_);

    timeout_ = timeout;

    // In a full implementation, this would verify server connectivity
    // For now, we just mark as connected
    spdlog::info("AlpacaClient connecting to {}:{}", host_, port_);

    // Try to get server info to verify connection
    // auto info = getServerInfo();
    // if (!info) {
    //     return false;
    // }

    connected_ = true;
    return true;
}

void AlpacaClient::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    connected_ = false;
    spdlog::info("AlpacaClient disconnected from {}:{}", host_, port_);
}

bool AlpacaClient::isConnected() const { return connected_.load(); }

void AlpacaClient::setServer(const std::string& host, int port) {
    std::lock_guard<std::mutex> lock(mutex_);
    host_ = host;
    port_ = port;
}

std::vector<std::string> AlpacaClient::discoverServers(int timeout) {
    std::vector<std::string> servers;

    // ASCOM Alpaca discovery uses UDP broadcast on port 32227
    // In a full implementation, this would:
    // 1. Create UDP socket
    // 2. Send discovery packet to broadcast address
    // 3. Collect responses

    spdlog::debug("Discovering Alpaca servers (timeout: {}ms)", timeout);

    // Stub: return localhost as default
    servers.push_back("localhost:11111");

    return servers;
}

std::optional<AlpacaServerInfo> AlpacaClient::getServerInfo() {
    if (!connected_) {
        return std::nullopt;
    }

    // GET /management/v1/description
    std::string url = "http://" + host_ + ":" + std::to_string(port_) +
                      "/management/v1/description";

    auto response = executeRequest(HttpMethod::GET, url, {});
    if (!response.isSuccess()) {
        return std::nullopt;
    }

    AlpacaServerInfo info;
    if (response.value.is_object()) {
        info.serverName = response.value.value("ServerName", "");
        info.manufacturer = response.value.value("Manufacturer", "");
        info.manufacturerVersion =
            response.value.value("ManufacturerVersion", "");
        info.location = response.value.value("Location", "");
    }

    return info;
}

std::vector<ASCOMDeviceDescription> AlpacaClient::getConfiguredDevices() {
    std::vector<ASCOMDeviceDescription> devices;

    if (!connected_) {
        return devices;
    }

    // GET /management/v1/configureddevices
    std::string url = "http://" + host_ + ":" + std::to_string(port_) +
                      "/management/v1/configureddevices";

    auto response = executeRequest(HttpMethod::GET, url, {});
    if (!response.isSuccess() || !response.value.is_array()) {
        return devices;
    }

    for (const auto& dev : response.value) {
        ASCOMDeviceDescription desc;
        desc.deviceName = dev.value("DeviceName", "");
        desc.deviceType = stringToDeviceType(dev.value("DeviceType", ""));
        desc.deviceNumber = dev.value("DeviceNumber", 0);
        desc.uniqueId = dev.value("UniqueID", "");
        devices.push_back(desc);
    }

    return devices;
}

bool AlpacaClient::connectDevice(ASCOMDeviceType deviceType, int deviceNumber) {
    auto response =
        put(deviceType, deviceNumber, "connected", {{"Connected", "true"}});
    return response.isSuccess();
}

bool AlpacaClient::disconnectDevice(ASCOMDeviceType deviceType,
                                    int deviceNumber) {
    auto response =
        put(deviceType, deviceNumber, "connected", {{"Connected", "false"}});
    return response.isSuccess();
}

bool AlpacaClient::isDeviceConnected(ASCOMDeviceType deviceType,
                                     int deviceNumber) {
    auto response = get(deviceType, deviceNumber, "connected");
    if (response.isSuccess() && response.value.is_boolean()) {
        return response.value.get<bool>();
    }
    return false;
}

std::string AlpacaClient::getDeviceName(ASCOMDeviceType deviceType,
                                        int deviceNumber) {
    auto response = get(deviceType, deviceNumber, "name");
    if (response.isSuccess() && response.value.is_string()) {
        return response.value.get<std::string>();
    }
    return "";
}

std::string AlpacaClient::getDeviceDescription(ASCOMDeviceType deviceType,
                                               int deviceNumber) {
    auto response = get(deviceType, deviceNumber, "description");
    if (response.isSuccess() && response.value.is_string()) {
        return response.value.get<std::string>();
    }
    return "";
}

std::string AlpacaClient::getDriverInfo(ASCOMDeviceType deviceType,
                                        int deviceNumber) {
    auto response = get(deviceType, deviceNumber, "driverinfo");
    if (response.isSuccess() && response.value.is_string()) {
        return response.value.get<std::string>();
    }
    return "";
}

std::string AlpacaClient::getDriverVersion(ASCOMDeviceType deviceType,
                                           int deviceNumber) {
    auto response = get(deviceType, deviceNumber, "driverversion");
    if (response.isSuccess() && response.value.is_string()) {
        return response.value.get<std::string>();
    }
    return "";
}

int AlpacaClient::getInterfaceVersion(ASCOMDeviceType deviceType,
                                      int deviceNumber) {
    auto response = get(deviceType, deviceNumber, "interfaceversion");
    if (response.isSuccess() && response.value.is_number_integer()) {
        return response.value.get<int>();
    }
    return 0;
}

std::vector<std::string> AlpacaClient::getSupportedActions(
    ASCOMDeviceType deviceType, int deviceNumber) {
    std::vector<std::string> actions;
    auto response = get(deviceType, deviceNumber, "supportedactions");
    if (response.isSuccess() && response.value.is_array()) {
        for (const auto& action : response.value) {
            if (action.is_string()) {
                actions.push_back(action.get<std::string>());
            }
        }
    }
    return actions;
}

AlpacaResponse AlpacaClient::get(
    ASCOMDeviceType deviceType, int deviceNumber, const std::string& method,
    const std::unordered_map<std::string, std::string>& params) {
    std::string url = buildUrl(deviceType, deviceNumber, method);
    return executeRequest(HttpMethod::GET, url, params);
}

AlpacaResponse AlpacaClient::put(
    ASCOMDeviceType deviceType, int deviceNumber, const std::string& method,
    const std::unordered_map<std::string, std::string>& params) {
    std::string url = buildUrl(deviceType, deviceNumber, method);
    return executeRequest(HttpMethod::PUT, url, params);
}

std::string AlpacaClient::action(ASCOMDeviceType deviceType, int deviceNumber,
                                 const std::string& actionName,
                                 const std::string& parameters) {
    auto response = put(deviceType, deviceNumber, "action",
                        {{"Action", actionName}, {"Parameters", parameters}});
    if (response.isSuccess() && response.value.is_string()) {
        return response.value.get<std::string>();
    }
    return "";
}

void AlpacaClient::commandBlind(ASCOMDeviceType deviceType, int deviceNumber,
                                const std::string& command, bool raw) {
    put(deviceType, deviceNumber, "commandblind",
        {{"Command", command}, {"Raw", raw ? "true" : "false"}});
}

bool AlpacaClient::commandBool(ASCOMDeviceType deviceType, int deviceNumber,
                               const std::string& command, bool raw) {
    auto response =
        put(deviceType, deviceNumber, "commandbool",
            {{"Command", command}, {"Raw", raw ? "true" : "false"}});
    if (response.isSuccess() && response.value.is_boolean()) {
        return response.value.get<bool>();
    }
    return false;
}

std::string AlpacaClient::commandString(ASCOMDeviceType deviceType,
                                        int deviceNumber,
                                        const std::string& command, bool raw) {
    auto response =
        put(deviceType, deviceNumber, "commandstring",
            {{"Command", command}, {"Raw", raw ? "true" : "false"}});
    if (response.isSuccess() && response.value.is_string()) {
        return response.value.get<std::string>();
    }
    return "";
}

void AlpacaClient::setTimeout(int timeout) { timeout_ = timeout; }

int AlpacaClient::getNextTransactionId() { return transactionId_.fetch_add(1); }

std::string AlpacaClient::buildUrl(ASCOMDeviceType deviceType, int deviceNumber,
                                   const std::string& method) const {
    std::ostringstream oss;
    oss << "http://" << host_ << ":" << port_ << "/api/v1/"
        << deviceTypeToString(deviceType) << "/" << deviceNumber << "/"
        << method;
    return oss.str();
}

AlpacaResponse AlpacaClient::executeRequest(
    HttpMethod method, const std::string& url,
    const std::unordered_map<std::string, std::string>& params) {
    // Add transaction IDs to params
    std::unordered_map<std::string, std::string> fullParams = params;
    fullParams["ClientID"] = "1";
    fullParams["ClientTransactionID"] = std::to_string(getNextTransactionId());

    // In a full implementation, this would use an HTTP client library
    // For now, return a stub response
    spdlog::debug("Alpaca {} request to {}",
                  method == HttpMethod::GET ? "GET" : "PUT", url);

    AlpacaResponse response;
    response.clientTransactionId = std::stoi(fullParams["ClientTransactionID"]);
    response.serverTransactionId = 0;
    response.errorNumber = 0;
    response.errorMessage = "";

    // Stub: return success with empty value
    // In production, this would make actual HTTP requests

    return response;
}

AlpacaResponse AlpacaClient::parseResponse(const std::string& responseBody) {
    try {
        json j = json::parse(responseBody);
        return AlpacaResponse::fromJson(j);
    } catch (const std::exception& e) {
        AlpacaResponse response;
        response.errorNumber = ASCOMErrorCode::UnspecifiedError;
        response.errorMessage =
            std::string("Failed to parse response: ") + e.what();
        return response;
    }
}

}  // namespace lithium::client::ascom
