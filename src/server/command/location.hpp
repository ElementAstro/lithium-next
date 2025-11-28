#ifndef LITHIUM_SERVER_MIDDLEWARE_LOCATION_HPP
#define LITHIUM_SERVER_MIDDLEWARE_LOCATION_HPP

#include "config/config.hpp"

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/print.hpp"

#include "constant/constant.hpp"
#include "utils/macro.hpp"

namespace lithium::middleware {
inline void saveCurrentLocation(double latitude, double longitude) {
    LOG_INFO(
        "saveCurrentLocation: Entering function with latitude={}, longitude={}",
        latitude, longitude);

    LITHIUM_GET_REQUIRED_PTR(messageBus, atom::async::MessageBus,
                             Constants::MESSAGE_BUS);
    LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager,
                             Constants::CONFIG_MANAGER);

    try {
        configManager->set("/quarcs/location/latitude", latitude);
        configManager->set("/quarcs/location/longitude", longitude);
        LOG_INFO(
              "saveCurrentLocation: Successfully saved location: latitude={}, "
              "longitude={}",
              latitude, longitude);
    } catch (const std::exception& e) {
        LOG_ERROR( "saveCurrentLocation: Failed to save location: {}",
              e.what());
    }

    LOG_INFO( "saveCurrentLocation: Exiting function");
}

inline void getCurrentLocation() {
    LOG_INFO( "getCurrentLocation: Entering function");

    LITHIUM_GET_REQUIRED_PTR(messageBus, atom::async::MessageBus,
                             Constants::MESSAGE_BUS);
    LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager,
                             Constants::CONFIG_MANAGER);

    try {
        auto latitudeJ = configManager->get("/quarcs/location/latitude");
        auto longitudeJ = configManager->get("/quarcs/location/longitude");

        if (latitudeJ && longitudeJ) {
            auto latitude = latitudeJ.value().get<double>();
            auto longitude = longitudeJ.value().get<double>();
            LOG_INFO(
                  "getCurrentLocation: Current location: latitude={}, "
                  "longitude={}",
                  latitude, longitude);
            messageBus->publish(
                "quarcs", "SetCurrentLocation:{}:{}"_fmt(latitude, longitude));
        } else {
            LOG_WARN( "getCurrentLocation: Location data not found");
        }
    } catch (const std::exception& e) {
        LOG_ERROR( "getCurrentLocation: Failed to get current location: {}",
              e.what());
    }

    LOG_INFO( "getCurrentLocation: Exiting function");
}
}  // namespace lithium::middleware

#endif
