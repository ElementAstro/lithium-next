/*
 * device_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Device factory implementation

*************************************************/

#include "device_factory.hpp"

#include "atom/log/spdlog_logger.hpp"

// INDI device includes
#ifdef HAVE_INDI
#include "client/indi/indi_camera.hpp"
#include "client/indi/indi_telescope.hpp"
#include "client/indi/indi_focuser.hpp"
#include "client/indi/indi_filterwheel.hpp"
#include "client/indi/indi_dome.hpp"
#include "client/indi/indi_rotator.hpp"
#include "client/indi/indi_weather.hpp"
#endif

// ASCOM device includes
#ifdef HAVE_ASCOM
#include "client/ascom/ascom_camera.hpp"
#include "client/ascom/ascom_telescope.hpp"
#include "client/ascom/ascom_focuser.hpp"
#include "client/ascom/ascom_filterwheel.hpp"
#include "client/ascom/ascom_dome.hpp"
#include "client/ascom/ascom_rotator.hpp"
#include "client/ascom/ascom_observingconditions.hpp"
#endif

namespace lithium::device {

void DeviceFactory::initializeDefaultCreators() {
    LOG_INFO("DeviceFactory: Initializing default device creators");

#ifdef HAVE_INDI
    using namespace lithium::client::indi;

    // INDI Camera
    registerCreator("INDI", DeviceType::Camera,
        [](const std::string& name, const DiscoveredDevice& info) {
            auto camera = std::make_shared<INDICamera>(name);
            return std::static_pointer_cast<AtomDriver>(camera);
        });

    // INDI Telescope
    registerCreator("INDI", DeviceType::Telescope,
        [](const std::string& name, const DiscoveredDevice& info) {
            auto telescope = std::make_shared<INDITelescope>(name);
            return std::static_pointer_cast<AtomDriver>(telescope);
        });

    // INDI Focuser
    registerCreator("INDI", DeviceType::Focuser,
        [](const std::string& name, const DiscoveredDevice& info) {
            auto focuser = std::make_shared<INDIFocuser>(name);
            return std::static_pointer_cast<AtomDriver>(focuser);
        });

    // INDI FilterWheel
    registerCreator("INDI", DeviceType::FilterWheel,
        [](const std::string& name, const DiscoveredDevice& info) {
            auto filterwheel = std::make_shared<INDIFilterWheel>(name);
            return std::static_pointer_cast<AtomDriver>(filterwheel);
        });

    // INDI Dome
    registerCreator("INDI", DeviceType::Dome,
        [](const std::string& name, const DiscoveredDevice& info) {
            auto dome = std::make_shared<INDIDome>(name);
            return std::static_pointer_cast<AtomDriver>(dome);
        });

    // INDI Rotator
    registerCreator("INDI", DeviceType::Rotator,
        [](const std::string& name, const DiscoveredDevice& info) {
            auto rotator = std::make_shared<INDIRotator>(name);
            return std::static_pointer_cast<AtomDriver>(rotator);
        });

    // INDI Weather
    registerCreator("INDI", DeviceType::Weather,
        [](const std::string& name, const DiscoveredDevice& info) {
            auto weather = std::make_shared<INDIWeather>(name);
            return std::static_pointer_cast<AtomDriver>(weather);
        });

    LOG_INFO("DeviceFactory: Registered INDI device creators");
#endif

#ifdef HAVE_ASCOM
    // ASCOM Camera
    registerCreator("ASCOM", DeviceType::Camera,
        [](const std::string& name, const DiscoveredDevice& info) {
            int deviceNumber = 0;
            if (info.customProperties.contains("deviceNumber")) {
                deviceNumber = info.customProperties["deviceNumber"].get<int>();
            }
            auto camera = std::make_shared<lithium::client::ascom::ASCOMCamera>(name, deviceNumber);
            return std::static_pointer_cast<AtomDriver>(camera);
        });

    // ASCOM Telescope
    registerCreator("ASCOM", DeviceType::Telescope,
        [](const std::string& name, const DiscoveredDevice& info) {
            int deviceNumber = 0;
            if (info.customProperties.contains("deviceNumber")) {
                deviceNumber = info.customProperties["deviceNumber"].get<int>();
            }
            auto telescope = std::make_shared<lithium::client::ascom::ASCOMTelescope>(name, deviceNumber);
            return std::static_pointer_cast<AtomDriver>(telescope);
        });

    // ASCOM Focuser
    registerCreator("ASCOM", DeviceType::Focuser,
        [](const std::string& name, const DiscoveredDevice& info) {
            int deviceNumber = 0;
            if (info.customProperties.contains("deviceNumber")) {
                deviceNumber = info.customProperties["deviceNumber"].get<int>();
            }
            auto focuser = std::make_shared<lithium::client::ascom::ASCOMFocuser>(name, deviceNumber);
            return std::static_pointer_cast<AtomDriver>(focuser);
        });

    // ASCOM FilterWheel
    registerCreator("ASCOM", DeviceType::FilterWheel,
        [](const std::string& name, const DiscoveredDevice& info) {
            int deviceNumber = 0;
            if (info.customProperties.contains("deviceNumber")) {
                deviceNumber = info.customProperties["deviceNumber"].get<int>();
            }
            auto filterwheel = std::make_shared<lithium::client::ascom::ASCOMFilterWheel>(name, deviceNumber);
            return std::static_pointer_cast<AtomDriver>(filterwheel);
        });

    // ASCOM Dome
    registerCreator("ASCOM", DeviceType::Dome,
        [](const std::string& name, const DiscoveredDevice& info) {
            int deviceNumber = 0;
            if (info.customProperties.contains("deviceNumber")) {
                deviceNumber = info.customProperties["deviceNumber"].get<int>();
            }
            auto dome = std::make_shared<lithium::client::ascom::ASCOMDome>(name, deviceNumber);
            return std::static_pointer_cast<AtomDriver>(dome);
        });

    // ASCOM Rotator
    registerCreator("ASCOM", DeviceType::Rotator,
        [](const std::string& name, const DiscoveredDevice& info) {
            int deviceNumber = 0;
            if (info.customProperties.contains("deviceNumber")) {
                deviceNumber = info.customProperties["deviceNumber"].get<int>();
            }
            auto rotator = std::make_shared<lithium::client::ascom::ASCOMRotator>(name, deviceNumber);
            return std::static_pointer_cast<AtomDriver>(rotator);
        });

    // ASCOM ObservingConditions (Weather)
    registerCreator("ASCOM", DeviceType::Weather,
        [](const std::string& name, const DiscoveredDevice& info) {
            int deviceNumber = 0;
            if (info.customProperties.contains("deviceNumber")) {
                deviceNumber = info.customProperties["deviceNumber"].get<int>();
            }
            auto weather = std::make_shared<lithium::client::ascom::ASCOMObservingConditions>(name, deviceNumber);
            return std::static_pointer_cast<AtomDriver>(weather);
        });

    LOG_INFO("DeviceFactory: Registered ASCOM device creators");
#endif

    LOG_INFO("DeviceFactory: Default device creators initialized");
}

}  // namespace lithium::device
