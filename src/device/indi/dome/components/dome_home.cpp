/*
 * dome_home.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-6-16

Description: INDI Dome Home - Home Position Management Implementation

*************************************************/

#include "dome_home.hpp"
#include "../dome_client.hpp"

#include <spdlog/spdlog.h>
#include <string_view>
#include <thread>
#include <utility>
using namespace std::chrono_literals;

DomeHomeManager::DomeHomeManager(INDIDomeClient* client) : client_(client) {}

[[nodiscard]] auto DomeHomeManager::findHome() -> bool {
    std::scoped_lock lock(home_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeHomeManager] Device not connected");
        return false;
    }
    if (home_finding_in_progress_.exchange(true)) {
        spdlog::warn("[DomeHomeManager] Home finding already in progress");
        return false;
    }
    // Check if dome is moving
    auto motionManager = client_->getMotionManager();
    if (motionManager && motionManager->isMoving()) {
        spdlog::error(
            "[DomeHomeManager] Cannot find home while dome is moving");
        home_finding_in_progress_ = false;
        return false;
    }
    spdlog::info("[DomeHomeManager] Starting home position discovery");
    // Try INDI home discovery property first
    if (auto* discoverProp = getHomeDiscoverProperty(); discoverProp) {
        auto* discoverWidget = discoverProp->findWidgetByName("DOME_HOME_FIND");
        if (!discoverWidget) {
            discoverWidget = discoverProp->findWidgetByName("HOME_FIND");
        }
        if (discoverWidget) {
            discoverProp->reset();
            discoverWidget->setState(ISS_ON);
            client_->sendNewProperty(discoverProp);
            spdlog::info(
                "[DomeHomeManager] Home discovery command sent to device");
            return true;
        }
    }
    // Fallback: Perform manual home finding
    bool result = performHomeFinding();
    home_finding_in_progress_ = false;
    return result;
}

[[nodiscard]] auto DomeHomeManager::setHome() -> bool {
    std::scoped_lock lock(home_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeHomeManager] Device not connected");
        return false;
    }
    auto motionManager = client_->getMotionManager();
    if (!motionManager) {
        spdlog::error("[DomeHomeManager] Motion manager not available");
        return false;
    }
    double currentAz = motionManager->getCurrentAzimuth();
    if (auto* setProp = getHomeSetProperty(); setProp) {
        auto* setWidget = setProp->findWidgetByName("DOME_HOME_SET");
        if (!setWidget) {
            setWidget = setProp->findWidgetByName("HOME_SET");
        }
        if (setWidget) {
            setProp->reset();
            setWidget->setState(ISS_ON);
            client_->sendNewProperty(setProp);
        }
    }
    home_position_ = currentAz;
    spdlog::info("[DomeHomeManager] Home position set to: {:.2f}°", currentAz);
    notifyHomeEvent(true, currentAz);
    return true;
}

[[nodiscard]] auto DomeHomeManager::gotoHome() -> bool {
    std::scoped_lock lock(home_mutex_);
    if (!client_->isConnected()) {
        spdlog::error("[DomeHomeManager] Device not connected");
        return false;
    }
    if (!home_position_) {
        spdlog::error("[DomeHomeManager] Home position not set");
        return false;
    }
    spdlog::info("[DomeHomeManager] Moving to home position: {:.2f}°",
                 *home_position_);
    if (auto* gotoProp = getHomeGotoProperty(); gotoProp) {
        auto* gotoWidget = gotoProp->findWidgetByName("DOME_HOME_GOTO");
        if (!gotoWidget) {
            gotoWidget = gotoProp->findWidgetByName("HOME_GOTO");
        }
        if (gotoWidget) {
            gotoProp->reset();
            gotoWidget->setState(ISS_ON);
            client_->sendNewProperty(gotoProp);
            return true;
        }
    }
    auto motionManager = client_->getMotionManager();
    if (motionManager) {
        return motionManager->moveToAzimuth(*home_position_);
    }
    spdlog::error(
        "[DomeHomeManager] No method available to move to home position");
    return false;
}

[[nodiscard]] auto DomeHomeManager::getHomePosition() -> std::optional<double> {
    std::scoped_lock lock(home_mutex_);
    return home_position_;
}

[[nodiscard]] auto DomeHomeManager::isHomeSet() -> bool {
    std::scoped_lock lock(home_mutex_);
    return home_position_.has_value();
}

[[nodiscard]] auto DomeHomeManager::enableAutoHome(bool enable) -> bool {
    std::unique_lock lock(home_mutex_);
    auto_home_enabled_ = enable;
    spdlog::info("[DomeHomeManager] {} auto-home functionality",
                 enable ? "Enabled" : "Disabled");
    if (enable && !home_position_ && client_->isConnected()) {
        spdlog::info(
            "[DomeHomeManager] Auto-home enabled, attempting to find home "
            "position");
        lock.unlock();
        [[maybe_unused]] bool _ = findHome();
        lock.lock();
    }
    return true;
}

[[nodiscard]] auto DomeHomeManager::isAutoHomeEnabled() -> bool {
    return auto_home_enabled_.load();
}

[[nodiscard]] auto DomeHomeManager::setAutoHomeOnStartup(bool enable) -> bool {
    auto_home_on_startup_ = enable;
    spdlog::info("[DomeHomeManager] {} auto-home on startup",
                 enable ? "Enabled" : "Disabled");
    return true;
}

[[nodiscard]] auto DomeHomeManager::isAutoHomeOnStartupEnabled() -> bool {
    return auto_home_on_startup_.load();
}

void DomeHomeManager::handleHomeProperty(const INDI::Property& property) {
    if (!property.isValid())
        return;
    std::string_view propertyName = property.getName();
    if (propertyName.find("HOME") != std::string_view::npos) {
        if (property.getType() == INDI_SWITCH) {
            auto* switchProp = property.getSwitch();
            if (switchProp) {
                auto* findWidget =
                    switchProp->findWidgetByName("DOME_HOME_FIND");
                if (!findWidget)
                    findWidget = switchProp->findWidgetByName("HOME_FIND");
                if (findWidget && findWidget->getState() == ISS_OFF) {
                    std::scoped_lock lock(home_mutex_);
                    if (home_finding_in_progress_) {
                        home_finding_in_progress_ = false;
                        auto motionManager = client_->getMotionManager();
                        if (motionManager) {
                            double currentAz =
                                motionManager->getCurrentAzimuth();
                            home_position_ = currentAz;
                            spdlog::info(
                                "[DomeHomeManager] Home position discovered "
                                "at: {:.2f}°",
                                currentAz);
                            notifyHomeEvent(true, currentAz);
                        }
                    }
                }
            }
        } else if (property.getType() == INDI_NUMBER &&
                   propertyName.find("POSITION") != std::string_view::npos) {
            auto* numberProp = property.getNumber();
            if (numberProp) {
                for (int i = 0; i < numberProp->count(); ++i) {
                    auto* widget = numberProp->at(i);
                    std::string_view widgetName = widget->getName();
                    if (widgetName.find("HOME") != std::string_view::npos ||
                        widgetName.find("AZ") != std::string_view::npos) {
                        double homeAz = widget->getValue();
                        std::scoped_lock lock(home_mutex_);
                        home_position_ = homeAz;
                        spdlog::info(
                            "[DomeHomeManager] Home position updated from "
                            "device: {:.2f}°",
                            homeAz);
                        break;
                    }
                }
            }
        }
    }
}

void DomeHomeManager::synchronizeWithDevice() {
    if (!client_->isConnected())
        return;
    if (auto* homeProp = getHomeProperty(); homeProp) {
        auto property =
            client_->getBaseDevice().getProperty(homeProp->getName());
        if (property.isValid())
            handleHomeProperty(property);
    }
    auto posProp =
        client_->getBaseDevice().getProperty("DOME_ABSOLUTE_POSITION");
    if (posProp.isValid())
        handleHomeProperty(posProp);
    if (auto_home_on_startup_ && !home_position_) {
        spdlog::info("[DomeHomeManager] Performing auto-home on startup");
        std::thread([this]() {
            std::this_thread::sleep_for(2s);  // Give device time to initialize
            [[maybe_unused]] bool _ = findHome();
        }).detach();
    }
    spdlog::debug("[DomeHomeManager] Synchronized with device");
}

void DomeHomeManager::setHomeCallback(HomeCallback callback) {
    std::scoped_lock lock(home_mutex_);
    home_callback_ = std::move(callback);
}

void DomeHomeManager::notifyHomeEvent(bool homeFound, double homePosition) {
    if (home_callback_) {
        try {
            home_callback_(homeFound, homePosition);
        } catch (const std::exception& ex) {
            spdlog::error("[DomeHomeManager] Home callback error: {}",
                          ex.what());
        }
    }
}

[[nodiscard]] auto DomeHomeManager::performHomeFinding() -> bool {
    if (!client_->isConnected())
        return false;
    auto motionManager = client_->getMotionManager();
    if (!motionManager) {
        spdlog::error(
            "[DomeHomeManager] Motion manager not available for home finding");
        return false;
    }
    spdlog::info("[DomeHomeManager] Performing manual home finding procedure");
    constexpr double startPosition = 0.0;
    if (!motionManager->moveToAzimuth(startPosition)) {
        spdlog::error(
            "[DomeHomeManager] Failed to move to start position for home "
            "finding");
        return false;
    }
    constexpr int maxWaitTime = 60;
    int waitTime = 0;
    while (motionManager->isMoving() && waitTime < maxWaitTime) {
        std::this_thread::sleep_for(1s);
        ++waitTime;
    }
    if (waitTime >= maxWaitTime) {
        spdlog::error(
            "[DomeHomeManager] Timeout waiting for dome to reach start "
            "position");
        return false;
    }
    constexpr double homePosition = 0.0;
    home_position_ = homePosition;
    spdlog::info("[DomeHomeManager] Manual home finding completed at: {:.2f}°",
                 homePosition);
    notifyHomeEvent(true, homePosition);
    return true;
}

[[nodiscard]] auto DomeHomeManager::getHomeProperty()
    -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected())
        return nullptr;
    constexpr std::string_view propertyNames[] = {"DOME_HOME", "HOME_POSITION",
                                                  "DOME_HOME_POSITION"};
    for (auto propName : propertyNames) {
        auto property = client_->getBaseDevice().getProperty(propName.data());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }
    return nullptr;
}

[[nodiscard]] auto DomeHomeManager::getHomeDiscoverProperty()
    -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected())
        return nullptr;
    constexpr std::string_view propertyNames[] = {
        "DOME_HOME_FIND", "HOME_DISCOVER", "DOME_DISCOVER_HOME", "FIND_HOME"};
    for (auto propName : propertyNames) {
        auto property = client_->getBaseDevice().getProperty(propName.data());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }
    return nullptr;
}

[[nodiscard]] auto DomeHomeManager::getHomeSetProperty()
    -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected())
        return nullptr;
    constexpr std::string_view propertyNames[] = {"DOME_HOME_SET", "HOME_SET",
                                                  "SET_HOME_POSITION"};
    for (auto propName : propertyNames) {
        auto property = client_->getBaseDevice().getProperty(propName.data());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }
    return nullptr;
}

[[nodiscard]] auto DomeHomeManager::getHomeGotoProperty()
    -> INDI::PropertyViewSwitch* {
    if (!client_->isConnected())
        return nullptr;
    constexpr std::string_view propertyNames[] = {"DOME_HOME_GOTO", "HOME_GOTO",
                                                  "GOTO_HOME_POSITION"};
    for (auto propName : propertyNames) {
        auto property = client_->getBaseDevice().getProperty(propName.data());
        if (property.isValid() && property.getType() == INDI_SWITCH) {
            return property.getSwitch();
        }
    }
    return nullptr;
}
