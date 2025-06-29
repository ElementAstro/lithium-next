/*
 * dome_shutter.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Shutter - Shutter Control Implementation

*************************************************/

#include "dome_shutter.hpp"
#include "../dome_client.hpp"

#include <spdlog/spdlog.h>
#include <string_view>

DomeShutterManager::DomeShutterManager(INDIDomeClient* client)
    : client_(client) {}

// Shutter control
[[nodiscard]] auto DomeShutterManager::openShutter() -> bool {
    std::scoped_lock lock(shutter_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeShutter] Not connected to device");
        return false;
    }
    if (!canOpenShutter()) {
        spdlog::error(
            "[DomeShutter] Cannot open shutter - safety check failed");
        return false;
    }
    if (current_state_ == ShutterState::OPEN) {
        spdlog::info("[DomeShutter] Shutter is already open");
        return true;
    }
    if (auto* shutterProperty = getDomeShutterProperty(); shutterProperty) {
        constexpr std::string_view openNames[] = {"SHUTTER_OPEN", "OPEN"};
        for (auto name : openNames) {
            if (auto* openWidget =
                    shutterProperty->findWidgetByName(name.data());
                openWidget) {
                openWidget->setState(ISS_ON);
                client_->sendNewProperty(shutterProperty);
                current_state_ = ShutterState::OPENING;
                incrementOperationCount();
                spdlog::info("[DomeShutter] Opening shutter");
                notifyShutterStateChange(ShutterState::OPENING);
                return true;
            }
        }
    }
    spdlog::error("[DomeShutter] Failed to send shutter open command");
    return false;
}

[[nodiscard]] auto DomeShutterManager::closeShutter() -> bool {
    std::scoped_lock lock(shutter_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeShutter] Not connected to device");
        return false;
    }
    if (current_state_ == ShutterState::CLOSED) {
        spdlog::info("[DomeShutter] Shutter is already closed");
        return true;
    }
    if (auto* shutterProperty = getDomeShutterProperty(); shutterProperty) {
        constexpr std::string_view closeNames[] = {"SHUTTER_CLOSE", "CLOSE"};
        for (auto name : closeNames) {
            if (auto* closeWidget =
                    shutterProperty->findWidgetByName(name.data());
                closeWidget) {
                closeWidget->setState(ISS_ON);
                client_->sendNewProperty(shutterProperty);
                current_state_ = ShutterState::CLOSING;
                incrementOperationCount();
                spdlog::info("[DomeShutter] Closing shutter");
                notifyShutterStateChange(ShutterState::CLOSING);
                return true;
            }
        }
    }
    spdlog::error("[DomeShutter] Failed to send shutter close command");
    return false;
}

[[nodiscard]] auto DomeShutterManager::abortShutter() -> bool {
    std::scoped_lock lock(shutter_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeShutter] Not connected to device");
        return false;
    }
    if (auto* shutterProperty = getDomeShutterProperty(); shutterProperty) {
        constexpr std::string_view abortNames[] = {"SHUTTER_ABORT", "ABORT"};
        for (auto name : abortNames) {
            if (auto* abortWidget =
                    shutterProperty->findWidgetByName(name.data());
                abortWidget) {
                abortWidget->setState(ISS_ON);
                client_->sendNewProperty(shutterProperty);
                spdlog::info("[DomeShutter] Shutter operation aborted");
                return true;
            }
        }
    }
    spdlog::error("[DomeShutter] Failed to send shutter abort command");
    return false;
}

[[nodiscard]] auto DomeShutterManager::getShutterState() -> ShutterState {
    return current_state_;
}

[[nodiscard]] auto DomeShutterManager::isShutterMoving() -> bool {
    return current_state_ == ShutterState::OPENING ||
           current_state_ == ShutterState::CLOSING;
}

// Safety checks
auto DomeShutterManager::canOpenShutter() -> bool {
    if (!isSafeToOperate()) {
        return false;
    }

    // Check weather conditions if weather manager is available
    auto weatherManager = client_->getWeatherManager();
    if (weatherManager && weatherManager->isWeatherMonitoringEnabled()) {
        if (!weatherManager->isWeatherSafe()) {
            spdlog::warn(
                "[DomeShutter] Cannot open shutter - unsafe weather "
                "conditions");
            return false;
        }
    }

    return true;
}

auto DomeShutterManager::isSafeToOperate() -> bool {
    // Check if dome is parked
    auto parkingManager = client_->getParkingManager();
    if (parkingManager && parkingManager->isParked()) {
        spdlog::warn("[DomeShutter] Cannot operate shutter - dome is parked");
        return false;
    }

    return true;
}

// Statistics
auto DomeShutterManager::getShutterOperations() -> uint64_t {
    std::lock_guard<std::mutex> lock(shutter_mutex_);
    return shutter_operations_;
}

auto DomeShutterManager::resetShutterOperations() -> bool {
    std::lock_guard<std::mutex> lock(shutter_mutex_);

    shutter_operations_ = 0;
    spdlog::info("[DomeShutter] Shutter operation count reset");
    return true;
}

// INDI property handling
void DomeShutterManager::handleShutterProperty(const INDI::Property& property) {
    if (property.getType() == INDI_SWITCH) {
        auto switchProperty = property.getSwitch();
        updateShutterFromProperty(switchProperty);
    }
}

void DomeShutterManager::updateShutterFromProperty(
    const INDI::PropertySwitch& property) {
    std::scoped_lock lock(shutter_mutex_);
    for (int i = 0; i < property.count(); ++i) {
        auto widget = property.at(i);
        std::string_view widgetName = widget->getName();
        ISState state = widget->getState();
        if (widgetName == std::string_view("SHUTTER_OPEN") ||
            widgetName == std::string_view("OPEN")) {
            if (state == ISS_ON && current_state_ != ShutterState::OPEN) {
                current_state_ = ShutterState::OPEN;
                spdlog::info("[DomeShutter] Shutter opened");
                notifyShutterStateChange(ShutterState::OPEN);
            }
        } else if (widgetName == std::string_view("SHUTTER_CLOSE") ||
                   widgetName == std::string_view("CLOSE")) {
            if (state == ISS_ON && current_state_ != ShutterState::CLOSED) {
                current_state_ = ShutterState::CLOSED;
                spdlog::info("[DomeShutter] Shutter closed");
                notifyShutterStateChange(ShutterState::CLOSED);
            }
        } else if (widgetName == std::string_view("SHUTTER_OPENING") ||
                   widgetName == std::string_view("OPENING")) {
            if (state == ISS_ON && current_state_ != ShutterState::OPENING) {
                current_state_ = ShutterState::OPENING;
                spdlog::info("[DomeShutter] Shutter opening");
                notifyShutterStateChange(ShutterState::OPENING);
            }
        } else if (widgetName == std::string_view("SHUTTER_CLOSING") ||
                   widgetName == std::string_view("CLOSING")) {
            if (state == ISS_ON && current_state_ != ShutterState::CLOSING) {
                current_state_ = ShutterState::CLOSING;
                spdlog::info("[DomeShutter] Shutter closing");
                notifyShutterStateChange(ShutterState::CLOSING);
            }
        }
    }
}

void DomeShutterManager::updateShutterFromProperty(
    INDI::PropertyViewSwitch* property) {
    if (!property) {
        return;
    }

    std::lock_guard<std::mutex> lock(shutter_mutex_);

    // Check shutter state widgets
    auto openWidget = property->findWidgetByName("SHUTTER_OPEN");
    auto closeWidget = property->findWidgetByName("SHUTTER_CLOSE");

    if (openWidget && openWidget->getState() == ISS_ON) {
        current_state_ = ShutterState::OPEN;
        notifyShutterStateChange(current_state_);
    } else if (closeWidget && closeWidget->getState() == ISS_ON) {
        current_state_ = ShutterState::CLOSED;
        notifyShutterStateChange(current_state_);
    }
}

void DomeShutterManager::synchronizeWithDevice() {
    if (!client_->isConnected()) {
        return;
    }

    auto shutterProperty = getDomeShutterProperty();
    if (shutterProperty) {
        updateShutterFromProperty(shutterProperty);
    }
}

void DomeShutterManager::setShutterCallback(ShutterCallback callback) {
    std::lock_guard<std::mutex> lock(shutter_mutex_);
    shutter_callback_ = std::move(callback);
}

// Internal methods
void DomeShutterManager::notifyShutterStateChange(ShutterState state) {
    if (shutter_callback_) {
        try {
            shutter_callback_(state);
        } catch (const std::exception& ex) {
            spdlog::error("[DomeShutter] Shutter callback error: {}",
                          ex.what());
        }
    }
}

void DomeShutterManager::incrementOperationCount() {
    shutter_operations_++;
    spdlog::debug("[DomeShutter] Shutter operation count: {}",
                  shutter_operations_);
}

// INDI property helpers
auto DomeShutterManager::getDomeShutterProperty() -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected()) {
        return nullptr;
    }

    auto& device = client_->getBaseDevice();

    // Try common property names
    std::vector<std::string> propertyNames = {
        "DOME_SHUTTER", "SHUTTER_CONTROL", "DOME_SHUTTER_CONTROL", "SHUTTER"};

    for (const auto& propName : propertyNames) {
        auto property = device.getProperty(propName.c_str());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }

    return nullptr;
}

auto DomeShutterManager::convertShutterState(ISState state) -> ShutterState {
    return (state == ISS_ON) ? ShutterState::OPEN : ShutterState::CLOSED;
}

auto DomeShutterManager::convertToISState(bool value) -> ISState {
    return value ? ISS_ON : ISS_OFF;
}
