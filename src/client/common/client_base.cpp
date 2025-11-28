/*
 * client_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Implementation of unified client base class

*************************************************/

#include "client_base.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client {

// ==================== ClientBase Implementation ====================

ClientBase::ClientBase(std::string name, ClientType type)
    : name_(std::move(name)),
      uuid_(atom::utils::UUID().toString()),
      type_(type) {
    spdlog::debug("ClientBase created: {} (UUID: {})", name_, uuid_);
}

ClientBase::~ClientBase() {
    spdlog::debug("ClientBase destroyed: {} (UUID: {})", name_, uuid_);
}

bool ClientBase::configure(const ClientConfig& config) {
    config_ = config;
    spdlog::debug("Client {} configured", name_);
    return true;
}

std::string ClientBase::getTypeName() const {
    switch (type_) {
        case ClientType::Solver:
            return "Solver";
        case ClientType::Guider:
            return "Guider";
        case ClientType::Server:
            return "Server";
        case ClientType::Connector:
            return "Connector";
        case ClientType::Custom:
            return "Custom";
        default:
            return "Unknown";
    }
}

std::string ClientBase::getStateName() const {
    switch (state_.load()) {
        case ClientState::Uninitialized:
            return "Uninitialized";
        case ClientState::Initialized:
            return "Initialized";
        case ClientState::Connecting:
            return "Connecting";
        case ClientState::Connected:
            return "Connected";
        case ClientState::Disconnecting:
            return "Disconnecting";
        case ClientState::Disconnected:
            return "Disconnected";
        case ClientState::Error:
            return "Error";
        default:
            return "Unknown";
    }
}

void ClientBase::setState(ClientState newState) {
    ClientState oldState = state_.exchange(newState);
    if (oldState != newState) {
        spdlog::debug("Client {} state changed: {} -> {}", name_,
                      static_cast<int>(oldState), static_cast<int>(newState));

        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (statusCallback_) {
            try {
                statusCallback_(oldState, newState);
            } catch (const std::exception& e) {
                spdlog::error("Status callback error: {}", e.what());
            }
        }
    }
}

void ClientBase::setError(int code, const std::string& message) {
    lastError_.code = code;
    lastError_.message = message;
    lastError_.timestamp = std::chrono::system_clock::now();
    spdlog::error("Client {} error [{}]: {}", name_, code, message);
    setState(ClientState::Error);
}

void ClientBase::emitEvent(const std::string& event, const std::string& data) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (eventCallback_) {
        try {
            eventCallback_(event, data);
        } catch (const std::exception& e) {
            spdlog::error("Event callback error: {}", e.what());
        }
    }
}

// ==================== ClientRegistry Implementation ====================

ClientRegistry& ClientRegistry::instance() {
    static ClientRegistry registry;
    return registry;
}

bool ClientRegistry::registerClient(const ClientDescriptor& descriptor) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (descriptors_.find(descriptor.name) != descriptors_.end()) {
        spdlog::warn("Client {} already registered, overwriting", descriptor.name);
    }

    descriptors_[descriptor.name] = descriptor;
    spdlog::info("Registered client: {} ({})", descriptor.name, descriptor.description);
    return true;
}

bool ClientRegistry::unregisterClient(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = descriptors_.find(name);
    if (it == descriptors_.end()) {
        spdlog::warn("Client {} not found for unregistration", name);
        return false;
    }

    descriptors_.erase(it);
    spdlog::info("Unregistered client: {}", name);
    return true;
}

std::shared_ptr<ClientBase> ClientRegistry::createClient(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = descriptors_.find(name);
    if (it == descriptors_.end()) {
        spdlog::error("Client {} not found in registry", name);
        return nullptr;
    }

    if (!it->second.factory) {
        spdlog::error("Client {} has no factory function", name);
        return nullptr;
    }

    try {
        auto client = it->second.factory();
        spdlog::debug("Created client instance: {}", name);
        return client;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create client {}: {}", name, e.what());
        return nullptr;
    }
}

std::vector<std::string> ClientRegistry::getRegisteredClients() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(descriptors_.size());
    for (const auto& [name, _] : descriptors_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> ClientRegistry::getClientsByType(ClientType type) const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    for (const auto& [name, desc] : descriptors_) {
        if (desc.type == type) {
            names.push_back(name);
        }
    }
    return names;
}

std::optional<ClientDescriptor> ClientRegistry::getDescriptor(
    const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = descriptors_.find(name);
    if (it == descriptors_.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace lithium::client
