/*
 * dome_parking.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Parking - Parking Control Implementation

*************************************************/

#include "dome_parking.hpp"
#include "../dome_client.hpp"

#include <spdlog/spdlog.h>

DomeParkingManager::DomeParkingManager(INDIDomeClient* client)
    : client_(client) {}

// Parking operations
[[nodiscard]] auto DomeParkingManager::park() -> bool {
    std::scoped_lock lock(parking_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeParking] Not connected to device");
        return false;
    }
    if (is_parked_) {
        spdlog::info("[DomeParking] Dome is already parked");
        return true;
    }
    if (is_parking_) {
        spdlog::info("[DomeParking] Dome is already parking");
        return true;
    }
    if (auto* parkProperty = getDomeParkProperty(); parkProperty) {
        auto* parkWidget = parkProperty->findWidgetByName("PARK");
        if (!parkWidget) {
            parkWidget = parkProperty->findWidgetByName("DOME_PARK");
        }
        if (parkWidget) {
            parkWidget->setState(ISS_ON);
            client_->sendNewProperty(parkProperty);
            is_parking_ = true;
            spdlog::info("[DomeParking] Parking dome");
            notifyParkingStateChange(false, true);
            if (park_position_.has_value()) {
                auto motionManager = client_->getMotionManager();
                if (motionManager) {
                    spdlog::info(
                        "[DomeParking] Moving to park position: {:.2f}°",
                        *park_position_);
                    if (!motionManager->moveToAzimuth(*park_position_)) {
                        spdlog::error(
                            "[DomeParking] Failed to move to park position");
                    }
                }
            }
            return true;
        }
    }
    spdlog::error("[DomeParking] Failed to send park command");
    return false;
}

[[nodiscard]] auto DomeParkingManager::unpark() -> bool {
    std::scoped_lock lock(parking_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeParking] Not connected to device");
        return false;
    }
    if (!is_parked_) {
        spdlog::info("[DomeParking] Dome is not parked");
        return true;
    }
    if (auto* parkProperty = getDomeParkProperty(); parkProperty) {
        auto* unparkWidget = parkProperty->findWidgetByName("UNPARK");
        if (!unparkWidget) {
            unparkWidget = parkProperty->findWidgetByName("DOME_UNPARK");
        }
        if (unparkWidget) {
            unparkWidget->setState(ISS_ON);
            client_->sendNewProperty(parkProperty);
            is_parked_ = false;
            is_parking_ = false;
            spdlog::info("[DomeParking] Unparking dome");
            notifyParkingStateChange(false, false);
            return true;
        }
    }
    spdlog::error("[DomeParking] Failed to send unpark command");
    return false;
}

[[nodiscard]] auto DomeParkingManager::isParked() -> bool { return is_parked_; }

[[nodiscard]] auto DomeParkingManager::isParking() -> bool {
    return is_parking_;
}

// Park position management
[[nodiscard]] auto DomeParkingManager::setParkPosition(double azimuth) -> bool {
    std::scoped_lock lock(parking_mutex_);
    if (azimuth < 0.0 || azimuth >= 360.0) {
        spdlog::error("[DomeParking] Invalid park azimuth: {:.2f}", azimuth);
        return false;
    }
    park_position_ = azimuth;
    spdlog::info("[DomeParking] Set park position to: {:.2f}°", azimuth);
    return true;
}

[[nodiscard]] auto DomeParkingManager::getParkPosition()
    -> std::optional<double> {
    std::scoped_lock lock(parking_mutex_);
    return park_position_;
}

[[nodiscard]] auto DomeParkingManager::getDefaultParkPosition() -> double {
    return default_park_position_;
}

// INDI property handling
void DomeParkingManager::handleParkingProperty(const INDI::Property& property) {
    if (property.getType() == INDI_SWITCH) {
        auto switchProperty = property.getSwitch();
        updateParkingFromProperty(switchProperty);
    }
}

void DomeParkingManager::updateParkingFromProperty(
    const INDI::PropertySwitch& property) {
    std::lock_guard<std::mutex> lock(parking_mutex_);

    for (int i = 0; i < property.count(); ++i) {
        auto widget = property.at(i);
        std::string widgetName = widget->getName();
        ISState state = widget->getState();

        if (widgetName == "PARK" || widgetName == "DOME_PARK") {
            if (state == ISS_ON) {
                if (!is_parked_) {
                    is_parked_ = true;
                    is_parking_ = false;
                    spdlog::info("[DomeParking] Dome parked");
                    notifyParkingStateChange(true, false);
                }
            } else {
                if (is_parked_) {
                    is_parked_ = false;
                    is_parking_ = false;
                    spdlog::info("[DomeParking] Dome unparked");
                    notifyParkingStateChange(false, false);
                }
            }
        } else if (widgetName == "PARKING" || widgetName == "DOME_PARKING") {
            if (state == ISS_ON) {
                if (!is_parking_) {
                    is_parking_ = true;
                    is_parked_ = false;
                    spdlog::info("[DomeParking] Dome parking in progress");
                    notifyParkingStateChange(false, true);
                }
            } else {
                if (is_parking_) {
                    is_parking_ = false;
                    // Check if parking completed successfully
                    auto parkWidget = property.findWidgetByName("PARK");
                    if (parkWidget && parkWidget->getState() == ISS_ON) {
                        is_parked_ = true;
                        spdlog::info("[DomeParking] Parking completed");
                        notifyParkingStateChange(true, false);
                    } else {
                        spdlog::info("[DomeParking] Parking stopped");
                        notifyParkingStateChange(false, false);
                    }
                }
            }
        }
    }
}

void DomeParkingManager::synchronizeWithDevice() {
    if (!client_->isConnected()) {
        return;
    }

    auto parkProperty = getDomeParkProperty();
    if (parkProperty) {
        updateParkingFromProperty(parkProperty);
    }
}

void DomeParkingManager::setParkingCallback(ParkingCallback callback) {
    std::lock_guard<std::mutex> lock(parking_mutex_);
    parking_callback_ = std::move(callback);
}

// Internal methods
void DomeParkingManager::notifyParkingStateChange(bool parked, bool parking) {
    if (parking_callback_) {
        try {
            parking_callback_(parked, parking);
        } catch (const std::exception& ex) {
            spdlog::error("[DomeParking] Parking callback error: {}",
                          ex.what());
        }
    }
}

// INDI property helpers
auto DomeParkingManager::getDomeParkProperty() -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected()) {
        return nullptr;
    }

    auto& device = client_->getBaseDevice();

    // Try common property names
    std::vector<std::string> propertyNames = {"DOME_PARK", "TELESCOPE_PARK",
                                              "PARK", "DOME_PARKING_CONTROL"};

    for (const auto& propName : propertyNames) {
        auto property = device.getProperty(propName.c_str());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }

    return nullptr;
}

void DomeParkingManager::updateParkingFromProperty(
    INDI::PropertyViewSwitch* property) {
    if (!property) {
        return;
    }

    std::lock_guard<std::mutex> lock(parking_mutex_);

    // Check parking state widgets
    auto parkWidget = property->findWidgetByName("PARK");
    auto unparkWidget = property->findWidgetByName("UNPARK");

    if (!parkWidget) {
        parkWidget = property->findWidgetByName("DOME_PARK");
    }
    if (!unparkWidget) {
        unparkWidget = property->findWidgetByName("DOME_UNPARK");
    }

    if (parkWidget && parkWidget->getState() == ISS_ON) {
        is_parked_ = true;
        is_parking_ = false;
        notifyParkingStateChange(true, false);
    } else if (unparkWidget && unparkWidget->getState() == ISS_ON) {
        is_parked_ = false;
        is_parking_ = false;
        notifyParkingStateChange(false, false);
    }
}
