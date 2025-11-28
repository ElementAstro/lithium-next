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

}  // namespace lithium::client
