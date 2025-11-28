/*
 * server_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Implementation of server client base class

*************************************************/

#include "server_client.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client {

ServerClient::ServerClient(std::string name)
    : ClientBase(std::move(name), ClientType::Server) {
    setCapabilities(ClientCapability::Connect | ClientCapability::Scan |
                    ClientCapability::Configure | ClientCapability::StatusQuery |
                    ClientCapability::EventCallback);
    spdlog::debug("ServerClient created: {}", getName());
}

ServerClient::~ServerClient() {
    spdlog::debug("ServerClient destroyed: {}", getName());
}

bool ServerClient::configureServer(const ServerConfig& config) {
    serverConfig_ = config;
    spdlog::debug("Server {} configured: {}:{}", getName(),
                  config.host, config.port);
    return true;
}

bool ServerClient::setProperties(const std::string& device,
                                 const std::unordered_map<std::string, std::string>& properties) {
    bool allSuccess = true;
    for (const auto& [prop, value] : properties) {
        // Parse property.element format
        auto dotPos = prop.find('.');
        std::string propName = prop;
        std::string elemName = "";
        if (dotPos != std::string::npos) {
            propName = prop.substr(0, dotPos);
            elemName = prop.substr(dotPos + 1);
        }
        if (!setProperty(device, propName, elemName, value)) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

std::unordered_map<std::string, PropertyValue> ServerClient::getProperties(
    const std::string& device) const {
    auto deviceOpt = getDevice(device);
    if (deviceOpt) {
        return deviceOpt->properties;
    }
    return {};
}

void ServerClient::registerServerEventCallback(ServerEventCallback callback) {
    std::lock_guard<std::mutex> lock(serverEventMutex_);
    serverEventCallback_ = std::move(callback);
}

void ServerClient::unregisterServerEventCallback() {
    std::lock_guard<std::mutex> lock(serverEventMutex_);
    serverEventCallback_ = nullptr;
}

nlohmann::json ServerClient::getServerStatus() const {
    nlohmann::json status;
    status["name"] = getName();
    status["backend"] = getBackendName();
    status["running"] = isServerRunning();
    status["connected"] = isConnected();
    status["config"] = serverConfig_.toJson();
    
    auto devices = getDevices();
    status["deviceCount"] = devices.size();
    
    nlohmann::json deviceList = nlohmann::json::array();
    for (const auto& dev : devices) {
        deviceList.push_back(dev.toJson());
    }
    status["devices"] = deviceList;
    
    auto drivers = getRunningDrivers();
    status["runningDriverCount"] = drivers.size();
    
    return status;
}

void ServerClient::emitServerEvent(const ServerEvent& event) {
    std::lock_guard<std::mutex> lock(serverEventMutex_);
    if (serverEventCallback_) {
        serverEventCallback_(event);
    }
}

}  // namespace lithium::client
