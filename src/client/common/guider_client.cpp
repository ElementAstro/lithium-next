/*
 * guider_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Implementation of guider client base class

*************************************************/

#include "guider_client.hpp"

#include <spdlog/spdlog.h>

namespace lithium::client {

GuiderClient::GuiderClient(std::string name)
    : ClientBase(std::move(name), ClientType::Guider) {
    setCapabilities(ClientCapability::Connect | ClientCapability::Configure |
                    ClientCapability::AsyncOperation | ClientCapability::StatusQuery |
                    ClientCapability::EventCallback);
    spdlog::debug("GuiderClient created: {}", getName());
}

GuiderClient::~GuiderClient() {
    spdlog::debug("GuiderClient destroyed: {}", getName());
}

std::string GuiderClient::getGuiderStateName() const {
    switch (guiderState_.load()) {
        case GuiderState::Stopped:
            return "Stopped";
        case GuiderState::Looping:
            return "Looping";
        case GuiderState::Calibrating:
            return "Calibrating";
        case GuiderState::Guiding:
            return "Guiding";
        case GuiderState::Settling:
            return "Settling";
        case GuiderState::Paused:
            return "Paused";
        case GuiderState::LostStar:
            return "LostStar";
        default:
            return "Unknown";
    }
}

}  // namespace lithium::client
